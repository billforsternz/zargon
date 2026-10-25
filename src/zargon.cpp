//***********************************************************
//
//               SARGON
//
// Sargon is a computer chess playing program designed
// and coded by Dan and Kathe Spracklen.  Copyright 1978. All
// rights reserved.  No part of this publication may be
// reproduced without the prior written permission.
//***********************************************************

// See the section "How Sargon Works" at the bottom of this
// file to understand how Sargon calculates chess moves.

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <vector>
#include <map>
#include <algorithm>

#include "main.h"
#include "trace.h"
#include "hash_lookup.h"
#include "zargon_functions.h"
#include "zargon.h"

// Up to 64K of emulated memory
Zargon gbl_zargon;        // Now made available as simple global
#if 0
static Zargon &z = gbl_zargon;    // This is good practice, but slow
#else
#define z gbl_zargon              // This is bad practice, but fast
#endif

//***********************************************************
// EQUATES
//***********************************************************
//
#define PAWN    1
#define KNIGHT  2
#define BISHOP  3
#define ROOK    4
#define QUEEN   5
#define KING    6
#define WHITE   0
#define BLACK   0x80
#define BPAWN   (BLACK+PAWN)

//**********************************************************
// PROGRAM CODE SECTION
//**********************************************************

//**********************************************************
// BOARD SETUP ROUTINE
//***********************************************************
// FUNCTION:   To initialize the board array, setting the
//             pieces in their initial positions for the
//             start of the game.
//
// CALLED BY:  DRIVER
//
// CALLS:      None
//
// ARGUMENTS:  None
//***********************************************************

void INITBD()
{
    // Pre-fill board with -1's
    memset( &z.BOARDA[0], -1, sizeof(z.BOARDA) );

    // Init the 64 squares within the 120 byte BOARD[] array
    uint8_t *dst = &z.BOARDA[SQ_a1];
    uint8_t *src = &z.pieces[0];
    for( int i=0; i<8; i++ )
    {
        *dst      = *src;               // White RNBQKBNR
        *(dst+10) = PAWN;
        *(dst+20) = 0;                  // empty squares
        *(dst+30) = 0;
        *(dst+40) = 0;
        *(dst+50) = 0;
        *(dst+60) = BPAWN;
        *(dst+70) = (BLACK | *src);     // Black RNBQKBNR
        src++;
        dst++;
    }

    //  Init King/Queen position list
    z.POSK[0] = SQ_e1;
    z.POSK[1] = SQ_e8;
    z.POSQ[0] = SQ_d1;
    z.POSQ[1] = SQ_d8;
}

//***********************************************************
// PATH ROUTINE
//***********************************************************
// FUNCTION:   To generate a single possible move for a given
//             piece along its current path of motion including:
//
//                Fetching the contents of the board at the new
//                position, and setting a flag describing the
//                contents:
//                          0  --  New position is empty
//                          1  --  Encountered a piece of the
//                                 opposite color
//                          2  --  Encountered a piece of the
//                                 same color
//                          3  --  New position is off the
//                                 board
//
// CALLED BY:  MPIECE
//             ATTACK
//             PINFND
//
// CALLS:      None
//
// ARGUMENTS:  Direction from the direction array giving the
//             constant to be added for the new position.
//***********************************************************

inline path_t PATH( int8_t dir )
{
    trace_func(FE_PATH);

    // Step along the path
    uint8_t piece = z.BOARDA[z.M2+=dir];

    // Off board?
    //  return 3 if off board
    if( piece == ((uint8_t)-1) )
        return PATH_OFF_BOARD;

    // Set P2=piece and T2=piece type (=piece&7)
    //  return 0 if empty
    else if(  0 == (z.T2 = (z.P2=piece)&7) )
        return PATH_EMPTY;

    // Else we have run into non zero piece P2
    //  return 1 if opposite color to origin piece P1
    //  return 2 if same color as origin piece P1
    return IS_SAME_COLOR(z.P2,z.P1) ? PATH_SAME_COLOR : PATH_OPPOSITE_COLOR;
}

//***********************************************************
// PIECE MOVER ROUTINE
//***********************************************************
// FUNCTION:   To generate all the possible legal moves for a
//             given piece.
//
// CALLED BY:  GENMOV
//
// CALLS:      PATH
//             ADMOVE
//             CASTLE
//             ENPSNT
//
// ARGUMENTS:  The piece to be moved.
//***********************************************************

void MPIECE( uint8_t piece )
{
    trace_func(FE_MPIECE);

    // Clear DOUBLE, PROMOTION and FIRST/CASTLED flag bits
    piece &= 0x87;
    bool empty = false;

    //  Decrement black pawns (so pawns, the only directional type, are 0 black and 1 white
    //   from now on in this function)
    if( piece == BPAWN )
        piece--;
    piece &= 7;      // clear colour flag to isolate piece type
    z.T1 = piece;

    // For minimax experiments, suppress king moves
    if( callback_suppress_king_moves(piece) )
        return;

    /*

    How the piece moving tables work by example

    The tables:

        int8_t direct[24] = {   // idx: type of row
            +9,+11,-11,-9,      // 0:   diagonals
            +10,-10,+1,-1,      // 4:   ranks and files
            -21,-12,+8,+19,     // 8:   knight moves
            +21,+12,-8,-19,     // 12:  knight moves
            +10,+10,+11,+9,     // 16:  white pawn moves
            -10,-10,-11,-9      // 20:  black pawn moves
        };

        uint8_t dpoint[7] = {
            20,16,8,0,4,0,0     // black pawn->20, white pawn->16, knight->8,
                                //  bishop->0, rook->4, queen->0, king->0
        };

        uint8_t dcount[7] = {
            4,4,8,4,4,8,8       // knights, queens and kings 2 rows,
                                //  pawns, bishops and rooks 1 row
        };


    */

    // Loop through the direction vectors for the given piece
    //  eg Black pawn dir_idx   = 20,21,22,23
    //                dir_count =  4, 3, 2, 1
    //                move_dir  = -10, -10, -11, -9

    z.INDX2 = z.dpoint[z.T1];       //  Get direction pointer
    for( uint8_t dir_count = z.dcount[z.T1], dir_idx = z.INDX2;
         dir_count != 0;
         dir_count--, dir_idx++
       )
    {
        uint8_t move_dir = z.direct[dir_idx];

        // From position
        z.M2 = z.M1;

        // Loop through steps in each direction vector
        bool stepping = true;
        while( stepping )
        {
            stepping = false;   // further steps are actually exceptional
                                // (BISHOPS, ROOKS, QUEENS plus pawn double step)

            path_t path_result = PATH( move_dir );   //  Calculate next position
            // 0  --  New position is empty
            // 1  --  Encountered a piece of the
            //        opposite color
            // 2  --  Encountered a piece of the
            //        same color
            // 3  --  New position is off the
            //        board

            // Can move to empty squares or squares of the same color.
            if( path_result <= PATH_OPPOSITE_COLOR )
            {

                // Empty destination ?
                empty = (path_result==PATH_EMPTY);

                //  Is it a Piece?
                if( piece >= KNIGHT )
                {

                    //  Add move to list
                    ADMOVE();

                    // Keep stepping if not capture and it's a long range piece
                    if( empty && (piece==BISHOP || piece==ROOK || piece==QUEEN) )
                        stepping = true;
                }

                // It's a pawn, more complicated than pieces!
                else
                {

                    // For pawns, dir count 4 is single square
                    if( dir_count == 4 )
                    {

                        // Skip over dir_count==3 in the outer vector loop,
                        //  it's a special case that's handled in the inner
                        //  PATH() loop only. (See below *)
                        dir_idx++;
                        dir_count--;

                        // Is destination available?
                        if( empty)
                        {

                            // Check promotion
                            if( z.M2<=SQ_h1 || z.M2>=SQ_a8 )    // destination on 1st or 8th rank?
                                SET_PROMOTION(z.P2);            // set promotion flag

                            // Add single step move to move list
                            ADMOVE();

                            // Check Pawn moved flag, for double step
                            if( HAS_NOT_MOVED(z.P1) )
                            {

                                // This is the only pawn path that continues the inner PATH() stepping
                                //  loop (for just one more step - the possible double step)
                                stepping = true;
                            }
                        }
                    }

                    // For pawns, dir_count 3 indicates double step, but it's actually an
                    //  extension of the dir_count == 4 vector above rather than a vector
                    //  in it's own right, which is why we arrange to skip over it in the
                    //  outer vector loop. (See above *)
                    else if( dir_count == 3 )
                    {
                        // Is forward square empty ?
                        if( empty )
                            ADMOVE();
                    }

                    // For pawns, dir_count 1 and 2 are diagonal moves
                    else if( dir_count < 3 )
                    {
                        if( empty )
                            ENPSNT();               //  Try en passant capture
                        else
                        {
                            if( z.M2<=SQ_h1 || z.M2>=SQ_a8 )    // destination on 1st or 8th rank?
                                SET_PROMOTION(z.P2);            // set promotion flag
                            ADMOVE();                           // add to move list
                        }
                    }
                }
            }
        }
    }

    // Try Castling
    if( piece ==  KING )
        CASTLE();
 }

//***********************************************************
// EN PASSANT ROUTINE
//***********************************************************
// FUNCTION:   --  To test for en passant Pawn capture and
//                 to add it to the move list if it is
//                 legal.
//
// CALLED BY:  --  MPIECE
//
// CALLS:      --  ADMOVE
//                 ADJPTR
//
// ARGUMENTS:  --  None
//***********************************************************
void ENPSNT()
{
    trace_func(FE_ENPSNT);

    // Pawn position
    uint8_t idx = z.M1;

    // Add 10 for black
    if( IS_BLACK(z.P1) )
        idx += 10;

    // Check on en-passant capture rank
    if( idx<SQ_a5 || SQ_h5<idx )  // fifth rank a5-h5 ? So white captures black pawn
        return;  // not on fifth rank

    // Previous move must be first move for that piece
    if( !IS_FIRST_MOVE(z.MLPTRJ->flags) )
        return;

    // Get "to" position for previous move
    z.M4 = z.MLPTRJ->to;
    uint8_t piece = z.BOARDA[z.M4];
    z.P3 = piece;

    // Check that previous move was a pawn move
    piece &= 7;
    if( piece != PAWN )
        return;

    // Compare "to" position for previous and current moves
    int8_t diff = z.M4-z.M2;
    if( diff!=10 && diff!=-10) // should be one rank different
        return;

    // Set double move flag and add en-passant move in two steps
    SET_DOUBLE_MOVE(z.P2);
    ADMOVE();       // first step, move the pawn
    z.M3 = z.M1;    // save initial pawn position

    // Dummy pawn move, to effect capture, as second part of move
    z.M1 = z.M4;    // dummy pawn move "from"
    z.M2 = z.M4;    // dummy pawn move "to"
    z.P2 = z.P3;    // captured pawn
    ADMOVE();       // second step capture the pawn

    // Restore "from" position
    z.M1 = z.M3;
    ADJPTR();
}

//***********************************************************
// ADJUST MOVE LIST POINTER FOR DOUBLE MOVE
//***********************************************************
// FUNCTION:   --  To adjust move list pointer to link around
//                 second move in double move.
//
// CALLED BY:  --  ENPSNT
//                 CASTLE
//                 (This mini-routine is not really called,
//                 but is jumped to to save time.)
//
// CALLS:      --  None
//
// ARGUMENTS:  --  None
//***********************************************************
inline void ADJPTR()
{
    trace_func(FE_ADJPTR);

    // Adjust list pointer to point at previous move
    z.MLLST--;

    // Zero link
    z.MLLST->link_ptr = 0;
}

//***********************************************************
// CASTLE ROUTINE
//***********************************************************
// FUNCTION:   --  To determine whether castling is legal
//                 (Queen side, King side, or both) and add it
//                 to the move list if it is.
//
// CALLED BY:  --  MPIECE
//
// CALLS:      --  ATTACK
//                 ADMOVE
//                 ADJPTR
//
// ARGUMENTS:  --  None
//***********************************************************
void CASTLE()
{
    trace_func(FE_CASTLE);

    // If king has moved return
    if( HAS_MOVED(z.P1) )
        return;

    // If king is in check return
    if( z.CKFLG )
        return;

    // Check both sides, kingside first
    for( uint8_t i=0; i<2; i++ )
    {

        // King rook is king+3, Queen rook is king-4
        z.M3 = (uint8_t)(i==0 ? z.M1+3 : z.M1-4);

        // Check that it is an unmoved rook
        uint8_t piece = z.BOARDA[z.M3];
        piece &= 0x7f;
        if( piece == ROOK )
        {

            // Step from king rook left towards king, queen rook right towards king
            uint8_t step = (uint8_t)(i==0 ? (-1) : 1);

            // Save start rook position
            uint8_t starting_rook_pos = z.M3;

            // Step from the rook to the king, checking each square between
            for(;;)
            {

                // Use M3 as a rover, from rook to king
                z.M3 += step;
                piece = z.BOARDA[z.M3];

                // Must be empty (this will always stop loop when we reach the king)
                if( piece != 0 )
                    break;

                // Must not be under attack (except for b1 and b8)
                if( z.M3 != SQ_b1 && z.M3 != SQ_b8 && ATTACK() )
                    break;
            }

            // If we didn't stop *BEFORE* reaching king, we have found a legal
            //  castling move, represent it as a double move, king then rook
            if( z.M3 == z.M1 )
            {

                // M1 is king "from" position, M2 is the king "to" position
                z.M2 = z.M3;
                z.M2 -= step;
                z.M2 -= step;
                z.P2 = 0;
                SET_DOUBLE_MOVE(z.P2);    // set double move flag
                ADMOVE();       // add to list

                // M1 is rook "from" position, M2 is rook "to" position, one step from king
                z.M1 = starting_rook_pos;
                z.M2 = z.M3 - step;
                z.P2 = 0;       // clear move flags
                ADMOVE();       // add to list
                ADJPTR();       // re-adjust move list pointer

                // Restore King position
                z.M1 = z.M3;
            }
        }
    }
}

//***********************************************************
// ADMOVE ROUTINE
//***********************************************************
// FUNCTION:   --  To add a move to the move list
//
// CALLED BY:  --  MPIECE
//                 ENPSNT
//                 CASTLE
//
// CALLS:      --  None
//
// ARGUMENT:  --  None
//***********************************************************

void ADMOVE()
{
    trace_func_void(FE_ADMOVE);

    // Address of next location in move list
    ML *ml = z.MLNXT;

    // Check that we haven't run out of memory
    if( ml > (ML *)&z.MLEND )
    {

        // TODO - Maybe this is probably what was intended in the original code,
        //  certainly writing 0 to the *difference* of two pointers makes no sense
        //  This is hopeless though, probably should panic and quit
        printf( "PANIC - Sargon out of memory\n" );
        fprintf( stderr, "PANIC - Sargon out of memory\n" );
        z.MLNXT = &z.MLIST[0];
        return;
    }

    // Address of previous move
    ML *previous_move = z.MLLST;

    // Save next as previous
    z.MLLST = ml;

    // Store as link address
    previous_move->link_ptr = ml;

    // If piece hasn't moved before set first move flag
    if( HAS_NOT_MOVED(z.P1) )
        SET_FIRST_MOVE(z.P2);

    // Now write move details
    ml->link_ptr = 0;           // store zero in link address
    ml->from  = z.M1;           // store "from" move position
    ml->to    = z.M2;           // store "to" move position
    ml->flags = z.P2;           // store move flags/capt. piece
    ml->val   = 0;              // store initial move value

    // Save next slot address
    z.MLNXT = ml + 1;
}

//***********************************************************
// GENERATE MOVE ROUTINE
//***********************************************************
// FUNCTION:  --  To generate the move set for all of the
//                pieces of a given color.
//
// CALLED BY: --  FNDMOV
//
// CALLS:     --  MPIECE
//                INCHK
//
// ARGUMENTS: --  None
//***********************************************************

void GENMOV()
{
    trace_func(FE_GENMOV);

    // Test for King in check
    bool inchk = INCHK(z.COLOR);
    z.CKFLG = inchk;

    // Next ply
    z.PLYIX_nxt[z.NPLY++] = z.MLNXT;    // save end of move list ptr
    z.MLPTRI = &z.PLYIX[z.NPLY];        // start of move linked list
    z.MLLST  = (ML *)z.MLPTRI;          // current end of list

    // Loop through the board
    for( uint8_t pos=SQ_a1; pos<=SQ_h8; pos++ )
    {
        z.M1 = pos;
        uint8_t piece = z.BOARDA[pos];

        // If piece not empty or border
        if( piece!=0 && piece!=0xff )
        {
            z.P1 = piece;

            // If piece color is side to move, gen moves
            if( IS_SAME_COLOR(z.COLOR,piece) )
            {

                // MPIECE() now uses piece including colour is z.P1
                //  rather than recreating it. Not sure why original
                //  code didn't do that
                MPIECE(z.P1);
            }
        }
    }
}

//***********************************************************
// CHECK ROUTINE
//***********************************************************
// FUNCTION:   --  To determine whether or not the
//                 King is in check.
//
// CALLED BY:  --  GENMOV
//                 FNDMOV
//                 EVAL
//
// CALLS:      --  ATTACK
//
// ARGUMENTS:  --  Color of king
//***********************************************************

inline bool INCHK( uint8_t color )
{

    // Set index M3 to point at White or Black king, Set P1 as piece
    //  with flags and T1 as piece type (piece&7)
	const uint8_t *p = &z.POSK[color?1:0];
	z.P1 = z.BOARDA[z.M3 = *p];
	z.T1 = z.P1 & 7;

    // Return true if there are attackers to the king
	return ATTACK();
}

//***********************************************************
// ATTACK ROUTINE
//***********************************************************
// FUNCTION:   --  To find all attackers on a given square
//                 by scanning outward from the square
//                 until a piece is found that attacks
//                 that square, or a piece is found that
//                 doesn't attack that square, or the edge
//                 of the board is reached.
//
//                 In determining which pieces attack
//                 a square, this routine also takes into
//                 account the ability of certain pieces to
//                 attack through another attacking piece. (For
//                 example a queen lined up behind a bishop
//                 of her same color along a diagonal.) The
//                 bishop is then said to be transparent to the
//                 queen, since both participate in the
//                 attack.
//
//                 In the case where this routine is called
//                 by CASTLE or INCHK, the routine is
//                 terminated as soon as an attacker of the
//                 opposite color is encountered.
//
// CALLED BY:  --  POINTS
//                 PINFND
//                 CASTLE
//                 INCHK
//
// CALLS:      --  PATH
//                 ATKSAV
//
// ARGUMENTS:  --  None
//***********************************************************

bool ATTACK()
{
    trace_func(FE_ATTACK);

    // Loop over the 16 directions in the direct[] table
    z.INDX2 = 0;
    const int8_t *dir_ptr = (int8_t *)z.direct;
    for( uint8_t dir_count=16; dir_count>0; dir_count-- )
    {
        int8_t dir = *dir_ptr++;
        uint8_t scan_count = 0;
        z.M2 = z.M3;

        // Stepping loop
        for(;;)
        {
            scan_count++;
            path_t path_result = PATH(dir);

            // PATH() return values
            // 0  --  New position is empty
            // 1  --  Encountered a piece of the opposite color
            // 2  --  Encountered a piece of the same color
            // 3  --  New position is off the board
            if( path_result==PATH_EMPTY && dir_count>=9 )
                continue; // empty square, not a knight, keep stepping
            if( path_result==PATH_EMPTY || path_result==PATH_OFF_BOARD )
                break;  // break to stop stepping

            // 1 -- Encountered a piece of the opposite color
            if( path_result == PATH_OPPOSITE_COLOR )
            {
                // Same color found already ?
                if( (scan_count&0x40) != 0 )
                    break;

                // Set opposite color found flag
                scan_count |= 0x20;
            }

            // 2 -- Encountered a piece of the same color
            else
            {
                // Opposite color found already?
                if( (scan_count&0x20) != 0 )
                    break;

                // Set same color found flag
                scan_count |= 0x40;
            }

            // Encountered a piece of either colour, determine if
            //  piece attacks square
            uint8_t piece = z.T2;

            // Knight moves?
            if( dir_count < 9 )
            {
                if( piece != KNIGHT )
                    break;
            }

            // Queen is good for ranks and diagonals
            else if( piece == QUEEN )
            {
                scan_count |= 0x80;
            }

            // King is good for ranks and diagonals for one step
            else if( (scan_count&0x0f) == 1 && piece == KING )
                ;

            // Ranks and files and rook?
            else if( dir_count < 13 )
            {
                if( piece != ROOK )
                    break;   // ranks and files and not rook
            }

            // Diagonals and bishop?
            else if( piece == BISHOP )
                ;

            // Diagonals and pawn
            else
            {

                // Count must be 1, pawn reach
                if( (scan_count&0x0f) != 1 || piece != PAWN )
                    break;

                // Pawn attack logic
                if( IS_WHITE(z.P2) )
                {
                    if( dir_count >= 15 )
                        break;   // not the right direction for this colour
                }
                else
                {
                    if( dir_count < 15 )
                        break;   // not the right direction for this colour
                }
            }

            // Get here if we've identified a suitable piece on the vector
            //
            // Check attacked piece type 1-6. Use 7 as a special sentinel
            //  value to indicate called from POINTS()
            if( z.T1 == 7 )
            {
                // If called from POINTS(), save attacker in attack list
                ATKSAV(scan_count,dir);
            }

            // Else if piece is opposite color, attacker found
            else if( (scan_count&0x20) != 0 )
            {
                return true;
            }

            // Keep stepping unless single step piece
            if( z.T2==KING || z.T2==KNIGHT )
                break;
        }
    }

    // No attackers
    return false;
}

//***********************************************************
// ATTACK SAVE ROUTINE
//***********************************************************
// FUNCTION:   --  To save an attacking piece value in the
//                 attack list, and to increment the attack
//                 count for that color piece.
//
//                 The pin piece list is checked for the
//                 attacking piece, and if found there, the
//                 piece is not included in the attack list.
//
// CALLED BY:  --  ATTACK
//
// CALLS:      --  PNCK
//
// ARGUMENTS:  --  None
//***********************************************************

inline void ATKSAV( uint8_t scan_count, int8_t dir )
{
    trace_func(FE_ATKSAV);

     /*

     How this works

     This routine collects an ordered list of up to 12 attackers (or
     defenders) of a square. The attackers (or defenders, but from
     now on I'll just say attackers even though it could also be
     defenders) are organised into 6 double entry slots, one for
     each piece type. The slots are byte sized at PAWN, KNIGHT,
     BISHOP, ROOK, QUEEN, KING (1,2,3,4,5,6 repectively) offset from
     the start of the wact or bact 7 byte array, leaving offset 0
     for the attacker count in a 7 byte data structure. Two entries
     are packed into one slot by using the two nibbles of a byte.
     The values stored are 1,3,3,5,9,10 respectively for each
     piece type, i.e. the conventional pawn value of the pieces
     with 10 used for the king. Under normal circumstances up to
     two pieces of each type are present, sufficient for all but
     edge cases as only two pawns can ever combine attacks on
     one square and Sargon doesn't really do underpromotion. A
     zero value is used to indicate an unused entry in the table.
     Basically the attacking values are placed into position in
     a sparse matrix so that ordered values are available without
     needing any sorting.

     Some examples;

     5,0x01,0x00,0x03,0x05,0x09,0x0a -> PBRQK

     A square is attacked by a pawn, a bishop, a rook, a queen and
     a king. This is also the order, it's setup this way because
     in chess you usually prefer to capture with your lowest value
     piece first when you have a choice. Sargon was written way
     before master level computer chess was possible and it was
     happy to get it right more often than not, without worrying
     about unusual edge cases.

     5,0x01,0x33,0x00,0x05,0x09a -> PNNRQ

     Both entries of the knight slot are used, indicating two
     capturing knights.

     3,0x00,0x00,0x03,0x00,0x59,0x00 -> BQR

     A bishop, queen and rook in that order. The ATKSAV algorithm
     and data structure is set up to accommodate the situation
     where you cannot capture in the preferred lowest value piece
     first order because you have either rooks or bishops lined
     up behind a queen. Here a rook is behind the queen. The
     least significant nibble is ordered ahead of the most
     significant nibble, so 0x59 not 0x95. In fact 0x95 never
     occurs because a rook and queen only share one slot in the
     table in this specific situation where the rook is behind
     the queen so the queen must come first.

     4,0x00,0x00,0x03,0x00,0x59,0x05 -> BQRR

     What if two rooks are lined up behind a queen? (two bishops
     are a little implausible, especially in the no
     underpromotion world of Sargon). In that case one of the
     rooks slides over into the king's slot.

     5,0x00,0x00,0x03,0x00,0x59,0xa5 -> BQRRK

     Finally, a situation where the king also attacks the square,
     no problem there is rook and queen in the queen slot and
     another rook and the king in the king's slot. The king's
     slot may have 0xa5 indicating rook followed by king but
     never 0x5a indicating king followed by rook, similar to
     (although the other way around) to the way the queen's
     slot may have 0x59 but never 0x95.

     These complications are handled with some surprisingly
     compact code - translated directly from the Sargon 1978
     original which made good use of special Z80 nibble
     opcodes. See comments below. However, the code is not
     set up to deal with promotion, I suspect the trade-off
     was maximise speed and efficiency for all but extreme
     edge cases, similar to the ROYALT() situation (one king
     plus one queen) which is fine, but we can make a very
     worthwhile improvement by allocating one extra byte
     (so 8 bytes total not 7) to avoid a very unfortunate
     buffer overflow in the following situation

     5,0x00,0x00,0x00,0x00,0x99,0x55,0x0a

     Here two queens and two rooks fully occupy the king and
     queen slots, before a king is added. In that case the
     king is the piece that triggers the p++ overflow in the
     code below, and an extra byte beyond the king slot
     should be added to accommodate this. I will tag this
     with ATKSAV_BUG2, for search purposes. Note that this
     is an edge of an edge case, if the king is encountered
     first it will be shifted out of the king slot by a
     second rook and lost, a different and less serious problem
     (in general ATKSAV() doesn't necessarily fully cope with
     promotion edge cases, ATKSAV_BUG2 is judged special and
     worthy of a fix simply because it involves buffer
     overflow)

     */

    // If there are pins, check whether this attacking piece is
    //  pinned and therefore not worthy of being saved to the list
    if( z.NPINS )
    {
        bool invalid_attacker = PNCK(z.NPINS,dir);
        if( invalid_attacker )
            return;
        // scan_count = dir;   // reproduce bug in original Sargon
        // ATKSAV_BUG is "fixed" (more accurately, absent) by simply omitting or commenting out "scan_count = dir" here
    }

    // Point at White or Black attack list
    uint8_t *p = IS_BLACK(z.P2) ? z.bact : z.wact;

    // First byte of attack list is count, increment it
    (*p)++;

    // Rest of list is 6 slots, one for each piece type
    uint8_t offset = z.P2 & 0x07;   // get piece type

    // If queen previously found on this vector use Queen slot as shown
    //  in the rook (or bishop, or queen actually) behind queen examples
    //  in "How this works" above
    if( (scan_count&0x80) != 0 )   // Queen found this scan ?
        offset = QUEEN;
    p += offset;    // point at piece type slot

    // Add the value of the piece as a nibble to attack list
    uint8_t piece_value = z.pvalue[z.T2-1];     // 1,3,3,5,9 or 10 for king
    uint8_t hi = *p&0xf0;
    uint8_t lo = *p&0x0f;
    if( lo!=0 && hi==0 )
    {

        // Add second attacker into high nibble
        *p = (piece_value<<4) + lo;

        // In the tricky rooks or bishops behind queen
        // scenario, the Queen must already be in the
        // queen slot, so it will correctly always be
        // in the low nibble (ordered first) position,
        // so 0x59 not 0x95 as discussed above

        // If we add a king into the king slot, after
        // another piece has overflowed into the king
        // slot, this clause will correctly add the
        // king into the high nibble, leaving the
        // overflow piece as the low nibble (ordered
        // first, the king should always be ordered
        // last). So 0xa5 instead of the incorrect
        // 0x5a as discussed above.
    }
    else
    {

        // If lo != 0 then hi cannot be 0 either [because
        //  we didn't take if( lo!=0 && hi==0 ) above]
        if( lo != 0 )   // if both nibbles occupied  
            p++;        //     use the next slot

        // Add new attacker into low nibble while shifting
        //  possible existing attacker into high nibble
        *p = (*p<<4) + piece_value;

        // In the tricky rooks or bishops behind queen
        // scenario, the Queen must already be in the
        // queen slot, so if we take this else{} block
        // then lo != 0 (because queen in queen slot)
        // which implies in turn that p++; above has
        // happened, overflowing us into the king slot.
        // If the king value (0x0a) is already present
        // it will correctly be shifted into the high
        // nibble, so that it is ordered last, so 0xa5
        // instead of the incorrect 0x5a as discussed
        // above.

        // ATKSAV_BUG2 occurs if both entries of the
        // king slot are used by overflows from the
        // queen slot and then the king tries to use
        // the king slot, a p++ overflow into an 8th
        // byte in the buffer occurs, which is a
        // problem if we only allocated 7 bytes as
        // in Sargon 1978. Note that this bug is an
        // extreme edge case, eg two rooks need to be
        // lined up behind two queens, nothing like
        // ATKSAV_BUG in terms of overall
        // significance
    }
}

//***********************************************************
// PIN CHECK ROUTINE
//***********************************************************
// FUNCTION:   --  Checks to see if the attacker is in the
//                 pinned piece list. If so he is not a valid
//                 attacker unless the direction in which he
//                 attacks is the same as the direction along
//                 which he is pinned. If the piece is
//                 found to be invalid as an attacker, the
//                 return to the calling routine is aborted
//                 and this routine returns directly to ATTACK.
//
// CALLED BY:  --  ATKSAV
//
// CALLS:      --  None
//
// ARGUMENTS:  --  The direction of the attack. The
//                 pinned piece counnt.
//***********************************************************

// Returns true if attacker is not a valid attacker
inline bool PNCK( uint16_t pin_count, int8_t attack_direction )
{
    trace_func(FE_PNCK);

    // Loop over the pin list
    bool not_first_find=false;
    uint8_t *p = &z.PLISTA[0];  // Pin list address
    bool expired = false;
    while(!expired)
    {

        // Search list for position of piece (Z80_CPIR)
        bool found = false;
        while( !found )
        {
            found = (z.M2 == *p);
            p++;
            pin_count--;
            if( pin_count == 0 )
                break;
        }
        expired = (pin_count==0);

        //  If not found, then the attacker is not pinned and invalid
        if( !found )
            return false;

        // If it's not the first find, then the attacker is pinned
        if( not_first_find )
            return true;
        not_first_find = true;

        // Check whether pin direction is same as attacking direction
        int8_t dir = *(int8_t *)(p+9);
        if( dir != attack_direction && (0-dir) != attack_direction  )

            // If it's the first find, and pin direction is not
            //  the attack direction, then the attacker is pinned
            //  and invalid
            return true;
    }

    // If expired (end of list), then the attacker is not pinned
    return false;
}

//***********************************************************
// PIN FIND ROUTINE
//***********************************************************
// FUNCTION:   --  To produce a list of all pieces pinned
//                 against the King or Queen, for both white
//                 and black.
//
// CALLED BY:  --  FNDMOV
//                 EVAL
//
// CALLS:      --  PATH
//                 ATTACK
//
// ARGUMENTS:  --  None
//***********************************************************

void PINFND()
{
    trace_func(FE_PINFND);
    z.NPINS = 0;

    // Loop over 4 royal pieces
    for( uint8_t i=0; i<4; i++ )
    {
        uint8_t *p = i<2 ? &z.POSK[i] : &z.POSQ[i-2];

        // Check piece is on the board
        if( *p == 0 )
            continue;

        // Loop over all directions
        z.M3 = *p;              //  Save position as board index
        z.P1 = z.BOARDA[z.M3];
        z.INDX2 = 0;
        int8_t *y = &z.direct[z.INDX2];
        for( uint8_t dir_count=8; dir_count>0; dir_count-- )
        {
            int8_t dir = *y++;      // get direction of scan
            z.M2 = z.M3;            // get King/Queen position
            z.M4 = 0;               // clear pinned piece saved pos

            // Step through squares in this direction
            bool stepping = true;
            while( stepping )
            {
                stepping = false;
                path_t path_result = PATH(dir);  // next position

                // If empty, carry on stepping
                if( path_result == PATH_EMPTY )
                {
                    stepping = true;
                }

                // Piece of same color it might be a pinned piece
                else if( path_result==PATH_SAME_COLOR && z.M4==0 )
                {

                    // Register a possible pin position
                    z.M4 = z.M2;
                    stepping= true;  // keep stepping
                }

                // Else piece of different colour and we have a potential pinned piece
                else if( path_result==PATH_OPPOSITE_COLOR && z.M4 != 0 )
                {
                    bool save_pinned_piece = false;

                    // Bishop pins piece to king or queen ?
                    if( dir_count >=5 && z.T2 == BISHOP )
                    {
                        save_pinned_piece = true;
                    }

                    // Rook pins piece to king or queen ?
                    else if( dir_count <5 && z.T2 == ROOK )
                    {
                        save_pinned_piece = true;
                    }

                    // Queen pins piece to king or queen ?
                    else if( z.T2 == QUEEN )
                    {

                        // If king, yes that's a pin
                        if( (z.P1&7) == KING )
                            save_pinned_piece = true;
                        else
                        {

                            // If queen do an attackers v defenders calculation
                            z.T1 = 7;
                            memset( z.wact, 0, sizeof(z.wact) );
                            memset( z.bact, 0, sizeof(z.bact) );
                            ATTACK();
                            int8_t defenders_minus_attackers;
                            int8_t *wact = (int8_t *)z.wact;
                            int8_t *bact = (int8_t *)z.bact;
                            if( IS_BLACK(z.P1) )   // Is queen black ?
                            {
                                defenders_minus_attackers = *bact - *wact;
                            }
                            else
                            {
                                defenders_minus_attackers = *wact - *bact;
                            }
                            if( defenders_minus_attackers < 1 )
                                save_pinned_piece = true;
                        }
                    }

                    // Save direction of pin and position of pinned piece
                    if( save_pinned_piece )
                    {
                        z.plistd[z.NPINS]   = dir;
                        z.PLISTA[z.NPINS++] = z.M4;
                    }
                }
            }
        }
    }
}

//***********************************************************
// EXCHANGE ROUTINE
//***********************************************************
// FUNCTION:   --  To determine the exchange value of a
//                 piece on a given square by examining all
//                 attackers and defenders of that piece.
//
// CALLED BY:  --  POINTS
//
// CALLS:      --  NEXTAD
//
// ARGUMENTS:  --  None.
//***********************************************************
void XCHNG( int8_t &points, int8_t &piece_val )
{
    trace_func(FE_XCHNG);

/*
    How this works

    We are calculating the profit or (loss) of capturing the
     piece on the current square by a series of exchanges.
     The material units are half pawns, so pawn=2, knight=6
     etc.

     It doesn't matter which side is to move. (So the capture
     might not be possible until next move). During the
     capturing sequence, the color of capturing and defending
     pieces swaps. In terms of points accumulation, we add
     the value of captured pieces of the same color as the
     original piece on the square (and subtract the value
     of captured attackers of the opposite color to the
     original piece on the square).

    Main algorithm is;
    Loop until run out of defenders or attackers
      Add value of initial captured piece and defenders to points
      Subtract value of captured attackers from points

    Some examples, note the '@' symbols, which are explained below.

    Rook attacks a rook, net points +10
        Step  Attacker   Attacked  Defender  Points
        1     Rook(10)   Rook(10)  -         +10

    Bishop attacks a rook defended by a pawn, net points 4
        Step  Attacker   Attacked  Defender  Points
        1     Bishop(6)  Rook(10)  Pawn(2)   +10
        2     Pawn(2)    Bishop(6) -         -6

    Rook attacks a rook defended by a pawn, net points 0
        Step  Attacker   Attacked  Defender  Points
        1     Rook(10)   Rook(10)  Pawn(2)   +10
        2     Pawn(2)    Rook(10)  -         -10

    Knight and bishop attack a rook defended by knight and bishop, net points +4
        Step  Attacker   Attacked  Defender  Points
        1     Knight(6)  Rook(10)  Knight(6) +10
        2     Knight(6)  Knight(6) Bishop(6) -6
        3     Bishop(6)  Knight(6) Bishop(6) +6
        4     Bishop(6)  Bishop(6) -         -6

    Queen ahead of rook attacks rook defended by rook, net points +2
        Step  Attacker   Attacked  Defender  Points
    @   1     Queen(18)  Rook(10)  Rook(10)  +10
        2     Rook(10)   Queen(18) Rook(10)  -18
        3     Rook(10)   Rook(10)   -        +10

    Knight and bishop attack a pawn defended by knight, net points +2
        Step  Attacker   Attacked  Defender  Points
    @   1     Knight(6)  Pawn(2)   Knight(6) +2
        2     Knight(6)  Knight(6) Bishop(6) -6
        3     Bishop(6)  Knight(6) -         +6

    Knight and bishop attack a pawn defended by knight and bishop, net points -4
        Step  Attacker   Attacked  Defender  Points
    @   1     Knight(6)  Pawn(2)   Knight(6) +2
        2     Knight(6)  Knight(6) Bishop(6) -6
        3     Bishop(6)  Knight(6) Bishop(6) +6
        4     Bishop(6)  Bishop(6) -         -6

    Knight and bishop attack a pawn defended by rook and king, net points 0
        Step  Attacker   Attacked  Defender  Points
    @   1     Knight(6)  Pawn(2)   Rook(10)  +2
        2     Rook(10)   Knight(6) Bishop(6) -6
        3     Bishop(6)  Rook(10)  King(20)  +10
        4     King(20)   Bishop(6) -         -6

    Two rooks ahead of a queen attack a rook defended by rook and king, net points 20
        Step  Attacker   Attacked  Defender  Points
        1     Rook(10)   Rook(10)  Rook(10)  +10
        2     Rook(10)   Rook(10)  Rook(10)  -10
        3     Rook(10)   Rook(10)  King(20)  +10
    @   4     King(20)   Rook(10)  Queen(18) -10
        5     Queen(18)  King(20)  -         +20

    A queen ahead of two rooks attacks a rook defended by rook and king, net points 12
        Step  Attacker   Attacked  Defender  Points
    @   1     Queen(18)  Rook(10)  Rook(10)  +10
        2     Rook(10)   Queen(18) Rook(10)  -18
        3     Rook(10)   Rook(10)  King(20)  +10
        4     King(20)   Rook(10)  Rook(18)  -10
        5     Rook(18)   King(20)  -         +20

    The algorithm works reasonably well, however let's introduce an
    improvement. The @ symbols show points where we refine things by
    switching to a different algorithm as follows;

    If have attacker > attacked and there is a defender
        Don't proceed with remaining net points loop, end now and
        this attacked value might be applied to net points (or not).
        To determine whether this attacked value is applied (YES) or
        not (NO);
        Secondary loop
            If defender value < attacker value end and NO
            Get next attacker, if absent end and NO
            Get next defender, if absent end and YES

    Note that in this secondary loop, we get a new attacker and defender in
    every iteration, rather than swapping sides with defenders becoming
    attackers.

    The logic of this is that a valuable attacker attacking a low value
    defended piece can normally be dismissed as not viable immediately.
    But if a series of defenders that are just as valuable as their
    corresponding attackers are eventually outnumbered by attackers
    that is an exception and we judge the initial capture as viable,
    although the sequence of exchanges (trades) is likely to end with
    the initial capture by the valuable attacker.

    Applying this refinment to the earlier examples gives improved
    results I think we can agree;

    Queen ahead of rook attacks rook defended by rook, net points 0
        Step  Attacker   Attacked  Defender  Points
    @   1     Queen(18)  Rook(10)  Rook(10)  +10 NO (defender < attacker)

    Knight and bishop attack a pawn defended by knight, net points +2
        Step  Attacker   Attacked  Defender  Points
    @   1     Knight(6)  Pawn(2)   Knight(6) +2 YES (defender absent at step 2)
        2     Bishop(6)            -            

    Knight and bishop attack a pawn defended by knight and bishop, net points 0
        Step  Attacker   Attacked  Defender  Points
    @   1     Knight(6)  Pawn(2)   Knight(6) +2 NO (attacker absent at step 3)
        2     Bishop(6)            Bishop(6)    
        3     -                                  

    Knight and bishop attack a pawn defended by rook and king, net points 0
        Step  Attacker   Attacked  Defender  Points
    @   1     Knight(6)  Pawn(2)   Rook(10)  +2 NO (attacker absent at step 3)
        2     Bishop(6)            King(20)             
        3     -                                  

    Two rooks ahead of a queen attack a rook defended by rook and king, net points 10
        Step  Attacker   Attacked  Defender  Points
        1     Rook(10)   Rook(10)  Rook(10)  +10
        2     Rook(10)   Rook(10)  Rook(10)  -10
        3     Rook(10)   Rook(10)  King(20)  +10
    @   4     King(20)   Rook(10)  Queen(18) -10 NO (defender < attacker)

    A queen ahead of two rooks attacks a rook defended by rook and king, net points 0
        Step  Attacker   Attacked  Defender  Points
    @   1     Queen(18)  Rook(10)  Rook(10)  +10 NO (defender < attacker)

*/

    // Initialise points won or lost
    points = 0;

    bool black = IS_BLACK(z.P1);
    bool have_reversed_sides = false;

    uint8_t *wact = z.wact;
    uint8_t *bact = z.bact;

    uint8_t count_white = *wact;
    uint8_t count_black = *bact;

    int8_t attacked_val = z.pvalue[z.T3-1];
    attacked_val = attacked_val+attacked_val;   // units of value are half pawns
    piece_val = (uint8_t)attacked_val;  

    // Retrieve first attacker
    int8_t attacker_val = black ? NEXTAD( count_white, wact )
                                : NEXTAD( count_black, bact );

    // Return if none
    if( attacker_val == 0 )
        return;

    // Loop through attackers and defenders
    // Main algorithm is;
    // Loop until run out of defenders or attackers
    //   Add value of initial captured piece and its defenders to points
    //   Subtract value of captured attackers from points
    //   Alternate sides, defender becomes attacker at next iteration
    for(;;)
    {

        // Get next defender
        int8_t defender_val = black ? NEXTAD( count_black, bact )
                                    : NEXTAD( count_white, wact );

        // If have a defender and attacker > attacked
        //     Don't proceed with remaining net points loop, end now and
        //     this attacked value might be applied to net points (or not).
        //     To determine whether this attacked value is applied (YES) or
        //     not (NO);
        //     Secondary loop
        //         If defender value < attacker value end and NO
        //         Get next attacker, if absent end and NO
        //         Get next defender, if absent end and YES
        bool attacker_gt_attacked = (attacker_val>attacked_val);
        while( defender_val!=0 && attacker_gt_attacked )
        {

            // Defender less than attacker ?
            if( defender_val < attacker_val )
                return; // yes - return

            // Get next attacker
            attacker_val = black ? NEXTAD( count_white, wact )
                                 : NEXTAD( count_black, bact );
            if( attacker_val == 0 )
                return; // return if none

            //  Get next defender
            defender_val = black ? NEXTAD( count_black, bact )
                                 : NEXTAD( count_white, wact );
            // if defender_val == 0, fall through to add or subtract 
            //  attacked_val to points then end
        }

        // Add values of captured defenders, subtract values
        //  of captured attackers (in the context of the
        //  starting position)
        if( have_reversed_sides )
            points -= attacked_val;
        else
            points += attacked_val;

        // Return at end of list
        if( defender_val == 0 )
            return; 

        // Prev attacker becomes defender
        attacked_val = attacker_val;
        have_reversed_sides = !have_reversed_sides;
        black = !black;
    }
}

//***********************************************************
// NEXT ATTACKER/DEFENDER ROUTINE
//***********************************************************
// FUNCTION:   --  To retrieve the next attacker or defender
//                 piece value from the attack list, and delete
//                 that piece from the list.
//
// CALLED BY:  --  XCHNG
//
// CALLS:      --  None
//
// ARGUMENTS:  --  Attack list addresses.
//                 Side flag
//                 Attack list counts
//***********************************************************

inline uint8_t NEXTAD( uint8_t& count, uint8_t* &p )
{
    trace_func(FE_NEXTAD);
    uint8_t val = 0;

    // Not at end of list ?
    if( count != 0 )
    {

        // Decrement list count
        count--;

        // Find next piece slot
        do
        {
            p++;               // increment list pointer
        } while( *p == 0 );    // keep going if empty

        // Get nibble value from list and remove it
        val = *p & 0x0f;    // order is low nibble first
        *p = *p >> 4;

        // Double it
        val = val+val;
        p--;    // decrement list pointer to retrieve second
                // (high) nibble (if present) next
    }
    return val;
}

//***********************************************************
// POINT EVALUATION ROUTINE
//***********************************************************
//FUNCTION:   --  To perform a static board evaluation and
//                derive a score for a given board position
//
// CALLED BY:  --  FNDMOV
//                 EVAL
//
// CALLS:      --  ATTACK
//                 XCHNG
//                 LIMIT
//
// ARGUMENTS:  --  None
//***********************************************************

void POINTS()
{
    trace_func_void(FE_POINTS);

/*
    How this Works

    As mentioned elsewhere, the basic score (or points)
    calculation is

    points = 4*LIMIT(29,net_material_gain) + LIMIT(6,net_board_control_gain)

    Where units of material are half pawns and units of
    board control are squares attacked.

    The LIMIT() constraints restrict the score dynamic
    range so that 8 bit variables can be used.

    net_material_gain is the change in net_material since
    the ply 0 base position, where net_material is
    white material - black material.

    net_board_control_gain is the change in net_board_control
    since the ply 0 base position, where net_board_control
    is squares attacked by white - squares attacked by black.
    
    Both these values are adjusted in various ways as
    discussed below.

    Board control is adjusted by bonuses and penalties as
    follows;

    Unmoved bishop or knight, 2 point penalty.
    Castling, 6 point bonus.
    King has not castled but has moved, 2 point penalty.
    Rook or Queen moves before move 7, 2 point penalty.
    Moving a piece to an enprise location, sets board control to zero.

    Material is adjusted based on en-prise pieces of
    either color. Every piece is judged en-prise (or not) by
    routine XCHNG().

    XCHNG() makes its best attempt to do a deep en-prise evaluation,
    without search, by ordering attackers and defenders and judging
    whether the piece can be safely (profitably) taken. It judges
    a piece en-prise if a positive trading profit is available, eg
    if a rook can be captured at the expense of a minor piece, then
    the rook is en-prise and the available profit will be 4 (so 5-3
    = 2 pawns, units are half pawns). 

    The first material adjustment based on the XCHNG() calculation
    is reduction of half a pawn in the value of any piece of
    either color that is en-prise. The piece is judged unstable I
    suppose.
    
    There are also points lost and points won material adjustments,
    these names are relative to the side that moved to create the
    current position. So points lost refers to pieces that can be
    taken by the other side, that is the side that moves next. The
    points lost adjustment is the largest trading profit available
    to the other side, minus the half pawn adjustment that the
    lost piece has already accumulated in the basic net material
    calculation.

    The points won adjustment is less immediate, since these points
    are not available immediately, since the side that just moved
    has to wait for the opponent to move first.

    The points won adjustment is the second greatest trading
    profit available to the side that just moved, minus the
    half point adjustment, divided by two. The adjustment is
    zero unless at least two pieces are en-prise. The logic
    of this is that the opponent has a move to move or
    defend one vulnerable piece, but may not be able to cope
    with two attacks. The division by two reflects the
    uncertainty inherant in applying this reasoning without
    search.

    Moving a piece to an enprise location, which has already
    incurred a board control penalty, is also penalised by
    removing any points won adjustment.

    The basic net material and net board control variables
    are MTRL and BRDC respectively. The adjustment variables
    are PTSL (points lost), PTSW1, PTSW2 (points won) and
    PTSCK (used to determine whether the last move moved
    a piece into an en-prise position).

    The base MTRL and BRDC are values before applying the
    points won and lost adjustments, and the zero board
    control when moving a piece to an en-prise position
    adjustment. They are both net values, in the sense
    that they are White - Black values.

    POINTS() is run on the base ply 0 position, and the
    resulting MTRL and BRDC values are saved as MTRL0 and
    BRDC0, the base of the subsequent material and board
    control *GAINED* subtraction.

    All adjustments are made (except the zero board
    control when moving a piece to an en-prise position
    adjustment which is made after the base value
    subtraction) then the MTRL0 and BRDC0 base values
    subtracted, then the basic formula applied;

    points = 4*LIMIT(29,net_material_gain) + LIMIT(6,net_board_control_gain)

    Finally 128 is added to shift from a signed range to
    an unsigned range, as discussed at length elsewhere,
    and the points value stored with the move that
    created the current position.

*/

    int8_t *wact = (int8_t *)z.wact;
    int8_t *bact = (int8_t *)z.bact;

    // Init points algorithm variables
    z.MTRL  = 0;
    z.BRDC  = 0;
    z.PTSL  = 0;
    z.PTSW1 = 0;
    z.PTSW2 = 0;
    z.PTSCK = 0;

    // Attacked piece type 1-6 for pawn to king. Use 7 as a special sentinel
    //  value to indicate to ATTACK() it's being called by POINTS()
    z.T1 = 7;

    // Loop around board
    for( z.M3=SQ_a1; z.M3<=SQ_h8; z.M3++ )
    {
        uint8_t piece = z.BOARDA[z.M3];

        //  Off board edge ?
        if( piece == 0xff )
            continue;

        // Piece with flags
        z.P1 = piece;

        // Piece without flags
        piece &= 7;
        z.T3 = piece;

        // Pawn (or empty) ?
        if( piece < KNIGHT )
            ;

        // Bishop or knight
        else if( piece < ROOK )
        {

            // If bishop or knight has not moved, 2 point penalty
            if( HAS_NOT_MOVED(z.P1) )
            {
                int8_t bonus = -2;
                if( IS_BLACK(z.P1) )
                    bonus = 2;  // 2 point penalty for Black
                z.BRDC += bonus;
            }
        }

        // King
        else if( piece == KING )
        {

            // 6 point bonus for castling
            if( IS_CASTLED(z.P1) )
            {
                int8_t bonus = +6;
                if( IS_BLACK(z.P1) )
                    bonus = -6;     // 6 point bonus for Black
                z.BRDC += bonus;
            }

            // Else if king has not castled but has moved, 2 point penalty
            else if( HAS_MOVED(z.P1) )
            {
                int8_t bonus = -2;
                if( IS_BLACK(z.P1) )
                    bonus = 2;  // 2 point penalty for Black
                z.BRDC += bonus;
            }
        }

        // Rook or queen early in game
        else if( z.MOVENO < 7 )
        {

            // If rook or queen early in the game HAS moved, 2 point penalty
            if( HAS_MOVED(z.P1) )
            {
                int8_t bonus = -2;
                if( IS_BLACK(z.P1) )
                    bonus = 2;  // 2 point penalty for Black
                z.BRDC += bonus;
            }
        }

        // Zero out attack lists
        memset( z.wact, 0, sizeof(z.wact) );
        memset( z.bact, 0, sizeof(z.bact) );

        // Build attack list for square
        ATTACK();

        // Get (White attacker count - Black attacker count) and accumulate
        //  into board control score
        z.BRDC += (*wact-*bact);

        // If square is empty continue loop
        if( z.P1 == 0 )   // get piece on current square
            continue;

        // Check for an en-prise piece on this square, for either color.
        //  XCHNG() makes its best attempt to do a deep en-prise evaluation,
        //  without search, by ordering attackers and defenders and judging
        //  whether the piece can be safely (profitably) taken. It sets
        //  points to a positive profit if that is available, eg if a rook
        //  can be captured at the expense of a minor piece, then points
        //  will be 4 (so 5-3 = 2 pawns, units are half pawns) while
        //  attacked_piece_val will be 10 (Rook = 5 pawns)
        int8_t points;
        int8_t attacked_piece_val;
        XCHNG( points, attacked_piece_val );

        // If piece is nominally lost
        if( points != 0 )
        {

            //  Devalue an unstable piece by half a Pawn value in z.MTRL calculation
            attacked_piece_val--;

            // z.COLOR is still set to the side that just moved.If color of piece
            //  under attack matches color of side that just moved, then it can
            //  be taken next move, we set "points lost" by the side that just moved
            //  to (almost) the full value of this loss, almost as much of a negative
            //  as if the material had already been lost
            if( IS_SAME_COLOR(z.COLOR,z.P1) )
            {

                // Save the worst such loss
                if( points >= z.PTSL )
                {
                    z.PTSL = points;

                    // Is the lost piece the one moving ?
                    if( z.M3 == z.MLPTRJ->to )
                    {

                        // Yes, save position as a flag, in the final
                        //  points tally we will adjust the points to
                        //  punish / discourage moving a piece to an
                        //  en-prise location
                        z.PTSCK = z.M3; 
                    }
                }
            }

            // Else color of piece under attack doesn't match color of the side that
            //  just moved. In this case the side that just moved is now attacking
            //  an en-prise piece. We call such material "points won" by analogy to the
            //  previously discussed "points lost", but since this material is not won
            //  immediately the positive effect is much less than the full material
            //  loss of points lost, this attack is merely a future possiblility of
            //  winning material. The method chosen by Sargon to discount points won is
            //  to only count a positive contribution if two separate pieces are
            //  en-prise to attacks by the side just moved, (eg a fork), and to count
            //  half the least valued of the two pieces.
            else
            {

                // Points won
                int8_t temp = points;

                // PTSW1 is set to the largest points won value
                if( points >= z.PTSW1 )
                {

                    // Update max points won (but save previous value)
                    temp = z.PTSW1;
                    z.PTSW1 = points;
                }

                // PTSW2 is set to the second largest points won value
                if( temp >= z.PTSW2 )
                {
                    z.PTSW2 = temp;
                }
            }
        }

        // Accumulate total material
        if( IS_BLACK(z.P1) )
            attacked_piece_val = 0-attacked_piece_val;    // negate piece value if Black
        z.MTRL += attacked_piece_val;
    }

    // Punish moving a piece into an en-prise location firstly by devaluing
    //  points won (even more)
    if( z.PTSCK != 0 )
    {
        z.PTSW1 = 0;
    }

    // Adjust points lost to reflect the half pawn reduction in material already
    //  accumulated into z.MTRL
    int8_t ptsl = z.PTSL;
    if( ptsl != 0 )
        ptsl--;

    // As discussed earlier points won is far less tangible than points lost, it's
    //  just a hope for the future. Calculate its actual value as half of the lesser
    //  valued of the two most valuble en-prise attacks, if and only if there are
    //  at least two such attacks, and and only after discounting a half pawn's
    //  value as we did for points lost.
    int8_t ptsw = (z.PTSW1!=0 && z.PTSW2!=0) ? (z.PTSW2-1)/2 : 0;

    // Points won net = points won - points lost
    ptsw -= ptsl;

    // Harmonise conventions:
    //  Material and board control are bigger is better for White
    //  Points won net are bigger is better for the side that just moved
    //  Harmonise on "bigger is better for White" for now
    if( IS_BLACK(z.COLOR) )
        ptsw = 0-ptsw;  // so negate points won net if Black just moved

    // Add net material on board
    ptsw += z.MTRL;

    // Subtract material at ply 0
    ptsw -= z.MV0;

    // Limit to +/- 30 (Sargon refinement => 29 see asterisked comments
    //  below about higher weighting for mates in fewer moves)
    int8_t points = LIMIT(29,ptsw);

    // Board control points minus board control at ply zero
    int8_t bcp = z.BRDC - z.BC0;

    // Punish moving a piece into an en-prise location a second time by
    //  setting board control points to zero
    if( z.PTSCK != 0 )
        bcp = 0;

    // Limit to +/- 6
    bcp = LIMIT(6,bcp);

    // Add material*4
    points = points*4 + bcp;

    // Change convention from "bigger is better for White"
    //  to "smaller is better for the side that just moved"
    // If White just moved negate a score that was bigger better for White
    // If Black just moved a score that was bigger better for White is already smaller is
    //  better for Black
    if( IS_WHITE(z.COLOR) )
        points = 0-points;

    // Rescale score (neutral = 0x80)
    // Convention is still "smaller is better for the side that just moved"
    //  but scale is different;
    //   before: -127 is smallest value, +127 is largest, 0 is neutral
    //   after:  1 is smallest value, 255 is largest, 128 is neutral
    // As discussed in "How Sargon Works" this has two benefits, Z80
    // assembly does unsigned comparisons more easily than signed comparisons
    // and 0 becomes available as a magic or sentinel value.
    points += 0x80;
    callback_end_of_points(points);

    // Save score value
    z.VALM = (uint8_t)points;

    // Save score value to move pointed to by current move ptr
    // Note that this val field is never updated from this initial static
    // evaluation to a backed up refined minimax evaluation.
    z.MLPTRJ->val = z.VALM;
}

//***********************************************************
// LIMIT ROUTINE
//***********************************************************
// FUNCTION:   --  To limit the magnitude of a given value
//                 to another given value.
//
// CALLED BY:  --  POINTS
//
// CALLS:      --  None
//
// ARGUMENTS:  --  Input  - Value, to be limited in the B
//                          register.
//                        - Value to limit to in the A register
//                 Output - Limited value in the A register.
//***********************************************************

int8_t LIMIT( int8_t limit, int8_t val)
{

    // Negative values, eg limit=6, val=-7 => -6
    if( val < 0 )
    {
        limit = 0-limit;    // make limit negative too
        if( val < limit )   // if val is more negative than limit
            val = limit;
    }

    // Positive values, eg limit=6, val=7 => 6
    else
    {
        if( val > limit )
            val = limit;
    }
    return val;
}

//***********************************************************
// MOVE ROUTINE
//***********************************************************
// FUNCTION:   --  To execute a move from the move list on the
//                 board array.
//
// CALLED BY:  --  CPTRMV
//                 PLYRMV
//                 EVAL
//                 FNDMOV
//                 VALMOV
//
// CALLS:      --  None
//
// ARGUMENTS:  --  None
//***********************************************************

void MOVE()
{
    trace_func(FE_MOVE);

    //  Load move list pointer
    ML *p = z.MLPTRJ;

    // Loop for possible double move
    for(;;)
    {

        // "From" position
        z.M1 = p->from;

        // "To" position
        z.M2 = p->to;

        // Get captured piece plus flags
        uint8_t captured_piece_plus_flags = p->flags;

        // Get piece moved from "from" pos board index
        uint8_t piece = z.BOARDA[z.M1];

        // Promote pawn if required
        if( IS_PROMOTION(captured_piece_plus_flags) )
        {
            piece |= 4; // change pawn to a queen 0x01->0x05 or 0x81->0x85
        }

        // Update queen position if required
        else if( (piece&7) == QUEEN )
        {
            uint8_t *q = &z.POSQ[0];    // addr of saved queen position
            if( IS_BLACK(piece) )       // is queen black ?
                q++;                    // increment to black queen pos
            *q = z.M2;                  // set new queen position
        }

        // Update king position if required
        else if( (piece&7) == KING )
        {
            uint8_t *q = &z.POSK[0];    // addr of saved king position
            if( IS_DOUBLE_MOVE(captured_piece_plus_flags) )  // castling ?
                SET_CASTLED(piece);
            if( IS_BLACK(piece) )       // is king black ?
                q++;                    // increment to black king pos
            *q = z.M2;                  // set new king position
        }

        // Set piece moved flag
        SET_MOVED(piece);

        // Insert piece at new position
        z.BOARDA[z.M2] = piece;

        // Empty previous position
        z.BOARDA[z.M1] = 0;

        // Double move ?
        if( IS_DOUBLE_MOVE(captured_piece_plus_flags) )
        {
            p = z.MLPTRJ;  // reload move list pointer
            p++;           // go to second move of double move
        }
        else
        {

            // Was captured piece a queen ?
            if( (captured_piece_plus_flags & 7) == QUEEN )
            {

                // Clear queen royalty position after capture
                uint8_t *q = &z.POSQ[0];
                if( IS_BLACK(captured_piece_plus_flags) )  // is queen black ?
                    q++; // increment to black queen pos
                *q = 0;
            }
            return;
        }
    }
}

//***********************************************************
// UN-MOVE ROUTINE
//***********************************************************
// FUNCTION:   --  To reverse the process of the move routine,
//                 thereby restoring the board array to its
//                 previous position.
//
// CALLED BY:  --  VALMOV
//                 EVAL
//                 FNDMOV
//                 ASCEND
//
// CALLS:      --  None
//
// ARGUMENTS:  --  None
//***********************************************************

void UNMOVE()
{
    trace_func(FE_UNMOVE);

    //  Load move list pointer
    ML *p = z.MLPTRJ;

    // Loop for possible double move
    for(;;)
    {

        // "From" position
        z.M1 = p->from;

        // "To" position
        z.M2 = p->to;

        // Get captured piece plus flags
        uint8_t captured_piece_plus_flags = p->flags;

        // Get piece moved from "to" pos board index
        uint8_t piece = z.BOARDA[z.M2];

        // Unpromote pawn if required
        if( IS_PROMOTION(captured_piece_plus_flags) )
        {
            piece &= 0xfb; // change queen to a pawn 0x05->0x01 or 0x85->0x81
        }

        // Update queen position if required
        else if( (piece&7) == QUEEN )
        {
            uint8_t *q = &z.POSQ[0];    // addr of saved queen position
            if( IS_BLACK(piece) )       // is queen black ?
                q++;                    // increment to black queen pos
            *q = z.M1;                  // set previous queen position
        }

        // Update king position if required
        else if( (piece&7) == KING )
        {
            uint8_t *q = &z.POSK[0];    // addr of saved king position
            if( IS_DOUBLE_MOVE(captured_piece_plus_flags) )  // castling ?
                CLR_CASTLED(piece);     // clear castled flag
            if( IS_BLACK(piece) )       // is king black ?
                q++;                    // increment to black king pos
            *q = z.M1;                  // set previous king position
        }

        // If this is the first move for piece, clear piece moved flag
        if( IS_FIRST_MOVE(captured_piece_plus_flags) )
        {
            CLR_MOVED(piece);
        }

        // Return piece to previous board position
        z.BOARDA[z.M1] = piece;

        // Restore captured piece with cleared flags
        uint8_t temp = captured_piece_plus_flags & 0x8f;
        z.BOARDA[z.M2] = temp;

        // Double move ?
        if( IS_DOUBLE_MOVE(captured_piece_plus_flags) )
        {
            p = z.MLPTRJ;  // reload move list pointer
            p++;           // go to second move of double move
        }
        else
        {

            // Was captured piece a queen ?
            if( (captured_piece_plus_flags & 7) == QUEEN )
            {

                // Restore queen royalty position after capture
                uint8_t *q = &z.POSQ[0];
                if( IS_BLACK(captured_piece_plus_flags) )  // is queen black ?
                    q++; // increment to black queen pos
                *q = z.M2;
            }
            return;
        }
    }
}

//***********************************************************
// SORT ROUTINE
//***********************************************************
// FUNCTION:   --  To sort the move list in order of
//                 increasing move value scores.
//
// CALLED BY:  --  FNDMOV
//
// CALLS:      --  EVAL
//
// ARGUMENTS:  --  None
//***********************************************************
bool compare_points(const ML& p1, const ML& p2) { return p1.val < p2.val; }

void SORTM()
{
    trace_func(FE_SORTM);

    // Alternative implementation of SORTM() using the standard C++
    //  library to do the sorting. It works, but it's actually a little
    //  slower than the standard Sargon merge sort below.
    #if 0

    // Input to SORTM() is a list of adjacent MLs (moves) pointed to by
    //  the ply pointer MLPTRI and ending with the last move MLLST. The
    //  moves are unscored, so start by looping through them and scoring
    //  them with EVAL() whilst also establishing conventional begin and
    //  end ptrs (by convention end points one beyond the last element)
    ML *ml_begin = z.MLPTRI->link_ptr;
    ML *ml = ml_begin;
    while( ml <= z.MLLST )
    {   
        z.MLPTRJ = ml;
        EVAL();
        if( IS_DOUBLE_MOVE(ml->flags) )
        {
            uint8_t val = ml->val;
            ml++;
            ml->val = val;  // put the move val into the second half of the move
                            // so it has the same value as the first half, and
                            // use std::stable_sort() not std::sort() so the
                            // tied first and second half will retain their
                            // order relative to each other and other possibly
                            // tied moves
        }
        ml++;
    }
    ML *ml_end = ml;

    // Now sort them in place
    std::stable_sort( ml_begin, ml_end, compare_points );

    // Now link the sorted moves into a linked list
    for( ml=ml_begin; ml && ml<ml_end; ml = ml->link_ptr )
    {
        ML *ml_next = ml+1;
        if( IS_DOUBLE_MOVE(ml->flags) )
        {
            ml_next->val = 0;
            ml_next++;
        }
        if( ml_next >= ml_end )
            ml->link_ptr = NULL;
        else
            ml->link_ptr = ml_next;
    }

    // Leave MLPTRJ pointing at the best move and MLLST pointing at the
    //  worst move, just to match the Sargon SORTM() behaviour
    z.MLPTRJ           = ml_begin;
    z.MLLST            = ml_end-1;

    #else
    // The original Sargon implementation is by necessity more complicated,
    //  given the lack of a standard Z80 sort facility!
    //  The sort technique is merge sort. The list of moves stays in the
    //  original order (unlike the C++ sort above), and the sort property
    //  is obtained by threading a linked list through the stationary moves.
    //  The list is divided conceptually into two sections, a sorted section
    //  followed by an unsorted section. Initially the sorted section is
    //  zero sized. We loop through the list of moves scoring them one by
    //  one with EVAL() and immediately merging the newly scored move into
    //  the sorted section increasing its length by one and simultaneously
    //  decreasing the length of the unsorted section by one.
    //  The first merge is trivial ('merging' an element into an empty list
    //  is just an append operation). Subsequent merges involve stepping
    //  through the sorted portion of the list to find the right place to
    //  link the move in to extend the sorted chain by one.

    // Loop over the unsorted list starting at the ply pointer MLPTRI. The
    // link_ptr can be used by both the unsorted and sorted lists because
    // a move is on one list or the other, never both.
    // This means that as soon as the unsorted rover advances it can lose
    // its connection to the ply pointer MLPTRI, and sorted moves are in
    // fact linked into a new sorted list reusing MLPTRI as the head pointer
    //
    // If the list is originally say;
    //  KDJHKAFHKF
    //
    // We divide it into two with a sorted/unsorted partition; Initially
    //  |KDJHKAFHKF   nbr sorted = 0
    //
    // After 'sorting' one element
    //  K|DJHKAFHKF   nbr sorted = 1
    //
    // After sorting two elements
    //  DK|JHKAFHKF   nbr sorted = 2
    //
    // After sorting three elements
    //  DJK|HKAFHKF   nbr sorted = 3
    //
    // After sorting all elements
    //  ADFFHHJKKK|   nbr sorted = 10
    //

    // Example used in line line by line comments below
    // After sorting three elements
    //  DJK|HKAFHKF   nbr sorted = 3
    // (|KDJHKAFHKF   original list)
    //
    //  u = unsorted list (pointers are straight line left to right)
    //  s = MLPTRI sorted list (pointers hop about)  
    //  +---+
    //  | s |   u
    //  | | |   |
    //  V V |   V
    //  K D J | H-> K-> A-> F-> H-> K-> F   nbr sorted = 3
    //    | ^
    //    | |
    //    +-+
    //
    ML *sorted_tail = 0;   // sorted tail needs to be relinked into
                           // sorted list after each sort step
    ML_HEAD *unsorted_rover = z.MLPTRI;

    // Loop over the unsorted list, EVAL() and sort one move at a time
    // Example from above, we are just finishing up sorting J and then
    //  we are going to do H
    for(;;)
    {
        // Next unsorted element
        // Example: (unsorted_rover = J)->H->K->A->F->H->K->F
        // Example: unsorted_move = H
        ML *unsorted_move = unsorted_rover->link_ptr; 

        // Extend sorted list with the sorted tail
        // Example: the previous time round J was promoted at the
        //  expense of K
        //  So MLPTRI->D->K changed to MLPTRI->D->J and K was unlinked
        //  So link K after J, so now MLPTRI->D->J->K is the sorted list
        //  See also the *asterisked example comment at the bottom of
        //  the loop
        unsorted_rover->link_ptr = sorted_tail;
                                        
        // Return if end of list
        if( unsorted_move == 0 )
        {
            return;
        }

        // Evaluate this unsorted move
        // Example: unsorted_move = H
        z.MLPTRJ = unsorted_move;               
        EVAL();

        // Search loop, find the right point in the sorted list
        ML_HEAD *sorted_rover = z.MLPTRI;  // sorted moves replace unsorted moves in ply list MLPTRI

        // Example: MLPTRI -> D -> J -> K   
        for(;;)
        {

            // Get next move from sorted list
            // Example iteration 1: D
            // Example iteration 2: J
            sorted_tail = sorted_rover->link_ptr;

            // End of list ?
            // Example iteration 1: D == null? (NO)
            // Example iteration 2: J == null? (NO)
            if( sorted_tail == 0 )
            {
                break;
            }

            // Compare value to list value (note z.VALM = z.MLPTRJ->val)
            // Example iteration 1: H < D? (NO)
            // Example iteration 2: H < J? (YES)
            if( z.VALM < sorted_tail->val )   // is unsorted better than sorted?
            {
                break;  // Yes sorted_rover->link_ptr will point to the new unsorted
                        // element instead of the inferior old sorted element
            }

            // Continue search through sorted loop
            sorted_rover = (ML_HEAD *)sorted_tail;
        }

        // Link new move into list
        // Example before: MLPTRI -> D->J->K 
        // Example now   : MLPTRI -> D->H   (J->K the sorted_tail is unlinked)
        // J->K has been unlinked, it will be linked to H at the *top of the loop
        //  but only after the next unsorted move is retrieved from H's current
        //  link ptr
        sorted_rover->link_ptr = unsorted_move;

        // Continue unsorted loop
        unsorted_rover = (ML_HEAD *)unsorted_move;
    }
    #endif
}

//***********************************************************
// EVALUATION ROUTINE
//***********************************************************
// FUNCTION:   --  To evaluate a given move in the move list.
//                 It first makes the move on the board, then if
//                 the move is legal, it evaluates it, and then
//                 restores the board position.
//
// CALLED BY:  --  SORT
//
// CALLS:      --  MOVE
//                 INCHK
//                 PINFND
//                 POINTS
//                 UNMOVE
//
// ARGUMENTS:  --  None
//***********************************************************

void EVAL()
{
    trace_func(FE_EVAL);

    // Make move on the board array
    MOVE();

    // Determine if move is legal
    bool inchk = INCHK(z.COLOR);

    // Score of zero for illegal move
    if( inchk )
        z.VALM = 0;
    else
    {

        // Compile pinned list
        PINFND();

        // Assign points to move
        POINTS();
    }

    // Restore board array
    UNMOVE();
}

//***********************************************************
// FIND MOVE ROUTINE
//***********************************************************
// FUNCTION:   --  To determine the computer's best move by
//                 performing a depth first tree search using
//                 the techniques of alpha-beta pruning.
//
// CALLED BY:  --  CPTRMV
//
// CALLS:      --  PINFND
//                 POINTS
//                 GENMOV
//                 SORTM
//                 ASCEND
//                 UNMOVE
//
// ARGUMENTS:  --  None
//***********************************************************

void FNDMOV()
{
    trace_func(FE_FNDMOV);

    // Book move ?
    if( z.MOVENO == 1 )
    {
        BOOK();
        return;
    }

    //  Initialize best move
    z.NPLY  = 0;
    z.BESTM = 0;

    //  Initialize ply list pointers
    z.MLNXT  = z.MLIST;
    z.MLPTRI = z.PLYIX;
    
    // The original Sargon code actually sets it this way which is
    //  a tricky way of saving one element in the PLYIX array
    //z.MLPTRI = &z.PLYIX[-1];    // don't worry, it's incremented just before first ptr store

    // Initialise color
    z.COLOR = z.KOLOR;

    // Initialize score table
    memset( z.SCORE, 0, sizeof(z.SCORE) );

    // Init board control and material
    z.BC0 = 0;
    z.MV0 = 0;

    //  Compile pin list
    PINFND();

    //  Evaluate board at ply 0
    TOGGLE(z.COLOR);    // POINTS() expects z.COLOR to be side just moved, not side to move
    POINTS();
    TOGGLE(z.COLOR);    // Restore z.COLOR
    z.BC0 = z.BRDC;     // Strictly speaking we didn't need to toggle color, and Sargon Z80
    z.MV0 = z.MTRL;     //  doesn't bother, because the calculation of both z.BRDC and z.MTRL
                        //  in POINTS() does not depend on z.COLOR

    // Generate move list for ply 1, from start position
    z.MATEF = true;             // Assume mate unless legal move found
    GENMOV();                   // Generate list of moves
    callback_after_genmov();
    if( z.PLYMAX > z.NPLY )
        SORTM();                // If not at max ply, score and sort (for alpha-beta) the moves
    z.MLPTRJ = (ML *)z.MLPTRI;  // Current move is first move in the move list for this ply

    // Loop through the moves
    for(;;)
    {
        bool is_leaf_node = false;

        //  If more moves in move list
        if( z.MLPTRJ->link_ptr != 0 )
        {

            z.MLPTRJ           = z.MLPTRJ->link_ptr;    // next move in move list
            z.MLPTRI->link_ptr = z.MLPTRJ;              // update ply pointer to point current move

            // Not yet at max depth, make the move
            if( z.NPLY < z.PLYMAX )
            {

                // If score is zero (illegal move) continue looping
                if( z.MLPTRJ->val == 0 )
                    continue;

                // Execute move on board array
                MOVE();
            }

            // Else max depth
            else
            {

                //  Execute move on board array
                MOVE();

                // Check move legality
                bool inchk = INCHK(z.COLOR); // illegal if we are in check after making move

                //  If move not legal, restore board position and continue looping
                if( inchk )
                {
                    UNMOVE();
                    continue;
                }

                // At max depth we normally make the leaf POINTS() calculation that's
                //  subsequently backed up to non-leaf nodes by minimax (plus alpha-beta)
                // But
                //  If move gives check, go one ply deeper to check for mate
                // So
                //  leaf node if move does not check or if we have already gone one move
                //  deeper
                if( z.NPLY > z.PLYMAX || !INCHK(z.COLOR^0x80) )
                {
                    is_leaf_node = true;
                }
            }

            // If not leaf node generate a move list for this node
            if( !is_leaf_node )
            {

                // Toggle color
                TOGGLE(z.COLOR);

                // If new colour is white increment move number
                if( IS_WHITE(z.COLOR) )
                    z.MOVENO++;

                // Update score
                z.SCORE[z.NPLY+1] = z.SCORE[z.NPLY-1];
                tracef( "Descending, initialise ply %d score with ply %d score %u\n", z.NPLY+1, z.NPLY-1, z.SCORE[z.NPLY-1] );

                // Generate moves at next ply
                z.MATEF = true;                 // assume mate unless legal move found
                GENMOV();                       // generate list of moves
                callback_after_genmov();

                // While we are descending we sort moves into best to worst order
                // using the scoring function (SORTM() calls the scoring function
                // POINTS()) and immediately continue descending. The sort is only
                // performed to speed up the algorithm by making alpha-beta cutoffs
                // more likely. No need to sort at max depth (there's not going to
                // be alpha-beta cutoffs at the leaves)
                if( z.PLYMAX > z.NPLY )
                    SORTM();                    // not at max ply, so call sort
                z.MLPTRJ = (ML *)z.MLPTRI;      // set current move to first move in
                                                //  the move list for this ply

                // Continue loop to iterate through the new move list
                continue;   
            }
        }

        // Get to this point if
        //  if( z.MLPTRJ->link_ptr == 0 )   // No more moves in move list
        //  OR if a leaf node
        // Apply minimax and alpha beta and then ASCEND() up tree

        // Score the node
        uint8_t score_smaller_is_better = 0;
        uint8_t score_bigger_is_better  = 0;

        // Is it a leaf node
        if( is_leaf_node )
        {
            // Compile pin list
            PINFND();

            // Evaluate move
            POINTS();

            // Restore board position
            UNMOVE();
            score_smaller_is_better = z.VALM;             // get value of move
            tracef( "Leaf node, score_smaller_is_better=%u\n", score_smaller_is_better );
            z.MATEF = false;            // it's not mate
        }

        // Else if legal move found, so not mate or stalemate, ASCEND
        else if( !z.MATEF )
        {
            if( z.NPLY == 1 )           // at top of tree ?
                return;                 // yes
            ASCEND();                   // ascend one ply in tree

            // Get backed up score (score of the best reply) from
            //  the ply one down we just ascended from. Note that
            //  scores in the ply table are bigger_is_better but
            //  bigger_is_better for replies is smaller_is_better
            //  for the move the replies are replying to
            score_smaller_is_better = z.SCORE[z.NPLY+1];
        }

        // Else if mate or stalemate
        else
        {
            score_smaller_is_better = 128;   // stalemate score
            if( z.CKFLG )   // test check flag
            {
                score_smaller_is_better = 0xff;   // if in check, then checkmate score
                                         //  (player to move has worse possible score)

                // Sargon refinement* - weight mates higher if they take
                //  less moves. Mate in 1 is 0xff remains the highest
                //  score. Reduce this value by up to 4. The 4 score
                //  slots opened up are obtained by changing the basic
                //  score formula
                //  from
                //    4*LIMIT(30,material) + LIMIT(6,board control)
                //    (maxes out at 126)
                //  to
                //    4*LIMIT(29,material) + LIMIT(6,board control)
                //    (maxes out at 122)
                uint8_t reduce = (z.NPLY-2)/2;
                if( reduce > 4 )
                    reduce = 4;
                score_smaller_is_better -= reduce;
                // Further notes;
                // Sargon mates on the move is detected at NPLY=2, reduce=0
                // Sargon mates in 2 is detected at NPLY=4, reduce=1
                // Sargon mates in 3 is detected at NPLY=6, reduce=2
                // Sargon mates in 4 is detected at NPLY=8, reduce=3
                // Sargon mates in 5 (or more) is detected at NPLY=10 (or more), reduce=4
                // Sargon mated next move is detected at NPLY=3, reduce=0
                // Sargon mated in 2 is detected at NPLY=5, reduce=1 etc
                tracef( "Mate detected, NPLY=%d, reduce=%d\n", z.NPLY, reduce );
                z.PMATE= z.MOVENO;
            }
            z.MATEF = false;     // mate flag has served its purpose
        }

        // Alpha Beta cutoff ?
        // How to think about Alpha Beta:
        // Minimax is minimising or maximising to determine the best move
        // in a generated list. Maybe the generated list is the replies
        // to the move D where D is itself in the higher level list
        // A,B,C,D... Minimax will be trying each reply to D to see which
        // is best.
        // Alpha Beta instead checks whether we can optimise by abandoning
        //  this list of replies because a reply refutes move D.
        //  "Wait a minute, this reply already means that move D is no good,
        //   because it proves the opponent can do better against D than
        //   against the best to date (B say). So stop wasting time on D"

        // Score table scores are flipped, so bigger is better
        const uint8_t *table_bigger_is_better = &z.SCORE[z.NPLY-1];
        //tracef( "Alpha-beta cutoff if score_smaller_is_better<=ply above: %s (score_smaller_is_better=%u, ply above=%u)\n",
        //        score_smaller_is_better<=*table_bigger_is_better?"YES":"NO", score_smaller_is_better, *table_bigger_is_better );
        trace_alpha_beta( score_smaller_is_better, *table_bigger_is_better );
        callback_alpha_beta_cutoff( score_smaller_is_better, table_bigger_is_better );

        // If this reply to D (for example) is better (or equal) than the best score
        //  so far for A,B and C then D is refuted. To see the best score so far
        //  for A,B and C we look into the table one ply higher. But table
        //  scores have the convention bigger_is_better aren't they incompatible
        //  with smaller_is_better scores? No because table scores are
        //  bigger_is_better for alternating colours, so a score that's
        //  smaller_is_better for White at this ply, can be compared to a score
        //  that's bigger_is_better for Black at the adjacent ply (since bigger
        //  is better for Black means smaller is better for White).
        //  See the explanation and worked example in "How Sargon Works?" for more
        //  discussion.
        if( score_smaller_is_better <= *table_bigger_is_better )  // compare to score above
        {

            // Alpha-beta cutoff
            ASCEND();  // ascend one ply in tree, abandoning these replies
            continue;
        }

        // Flip score (twos complement)
        score_bigger_is_better = (score_smaller_is_better^0xff)+1;
        tracef( "is the best move if score_bigger_is_better > ply score: %s (score_smaller_is_better=%u, score_bigger_is_better=%u, ply score=%u)\n",
                      (score_bigger_is_better > z.SCORE[z.NPLY])?"YES":"NO", (score_bigger_is_better^0xff)+1, score_bigger_is_better, z.SCORE[z.NPLY] );

        // Is this the best reply to date?
        bool score_greater = (z.SCORE[z.NPLY] < score_bigger_is_better);
        callback_no_best_move( score_bigger_is_better, &z.SCORE[z.NPLY] );
        if( !score_greater )
            continue;   // continue unless score is greater

        // Save bigger_is_better score in table
        z.SCORE[z.NPLY] = score_bigger_is_better;
        //tracef( "SCORE updated, score_bigger_is_better=%u\n", score_bigger_is_better );
        trace_score_updated( score_bigger_is_better );
        callback_yes_best_move();

        // At top of tree ?
        if( z.NPLY != 1 )
            continue;   // no - loop

        // Set best move pointer = current move pointer
        z.BESTM = z.MLPTRJ;

        //  Was it mate on the move ?
        if( z.SCORE[1] != 0xff )
            continue;   // no - loop

        // Subtract 2 from maximum ply number
        //  (Presumably so Sargon will find the mate faster when it is
        //   its turn to play next - but this is not really necessary
        //   or desirable in Sargon x86 / C++ ports so commented out)
        // z.PLYMAX -= 2;

        // Calculate checkmate move number and return
        if( IS_BLACK(z.KOLOR) )  // if computer's color is black decrement
            z.PMATE--;
        return;
    }
}

//***********************************************************
// ASCEND TREE ROUTINE
//***********************************************************
// FUNCTION:  --  To adjust all necessary parameters to
//                ascend one ply in the tree.
//
// CALLED BY: --  FNDMOV
//
// CALLS:     --  UNMOVE
//
// ARGUMENTS: --  None
//***********************************************************

void ASCEND()
{
    trace_func(FE_ASCEND);
    tracef( "ASCEND()\n" );

    // Toggle color
    TOGGLE(z.COLOR);

    // If new colour is Black, decrement move number
    if( IS_BLACK(z.COLOR) )
        z.MOVENO--;

    // Decrement ply counter and update pointers
    z.NPLY--;
    z.MLNXT  = z.PLYIX_nxt[z.NPLY];     // end of moves at this ply
    z.MLPTRI = &(z.PLYIX[z.NPLY]);      // linked list head
    z.MLPTRJ = z.MLPTRI->link_ptr;      // current move

    // Restore board to previous ply
    UNMOVE();
}

//***********************************************************
// ONE MOVE BOOK OPENING
// **********************************************************
// FUNCTION:   --  To provide an opening book of a single
//                 move.
//
// CALLED BY:  --  FNDMOV
//
// CALLS:      --  None
//
// ARGUMENTS:  --  None
//***********************************************************

void BOOK()
{
    // Set score to zero
    uint8_t *p = (uint8_t *)&z.SCORE;
    *(p+1) = 0;

    // Point to 4 individual first move book moves, in order 1.e4, 1.d4, 1...e5, 1...d5
    //  For each move only the bytes at MLFRP, MLTOP, MLFG respectively (from, to, flags)
    //  are stored (so 3 bytes per move)
    p = &z.BMOVES[0];

    // Adjust p so that MLFRP points at "from" (then "to", then "flags")
    p -= offsetof(ML,from);

    // Named pointers to each of the four book moves;
    uint8_t *e4=p, *d4=p+3, *e5=p+6, *d5=p+9;

    // If computer is White
    if( z.KOLOR == 0 )
    {

        // Get a random number
        uint8_t rand;
        callback_ldar(rand);

        // Play e4 or d4 randomly
        if( (rand&1) == 0 )
            z.BESTM = (ML *)e4;
        else
            z.BESTM = (ML *)d4;
    }

    // Else computer is Black
    else
    {
        uint8_t from = z.MLPTRJ->from;   // get "from" square for opponent's 1st move

        // Play d5 after all White first moves except a,b,c or e pawn moves
        bool play_d5 = true;
        if( from==SQ_a2 || from==SQ_b2 || from==SQ_c2 || from==SQ_e2 )
            play_d5 = false;
        if( play_d5 )
            z.BESTM = (ML *)d5;
        else
            z.BESTM = (ML *)e5;
    }
}

//
//  Omit some Z80 User Interface code, eg
//  user interface data including graphics data and text messages
//  Also macros
//      CARRET, CLRSCR, PRTLIN, PRTBLK and EXIT
//  and functions
//      DRIVER and INTERR
//

//***********************************************************
// COMPUTER MOVE ROUTINE
//***********************************************************
// FUNCTION:   --  To control the search for the computers move
//                 and the display of that move on the board
//                 and in the move list.
//
// CALLED BY:  --  DRIVER
//
// CALLS:      --  FNDMOV
//                 FCDMAT
//                 MOVE
//                 EXECMV
//                 BITASN
//                 INCHK
//
// MACRO CALLS:    PRTBLK
//                 CARRET
//
// ARGUMENTS:  --  None
//***********************************************************

void CPTRMV()
{

    //  Select best move
    FNDMOV();

    // Save best move
    z.MLPTRJ = z.BESTM;

    // Make the move
    MOVE();

    // Toggle color
    TOGGLE(z.COLOR);

    // Unnecessary Z80 User interface stuff, including EXECMV (graphics move)
    // has now been removed
    //
}

//
//  Omit some more Z80 user interface stuff, functions
//  FCDMAT, TBPLCL, TBCPCL, TBPLMV and TBCPMV
//

//***********************************************************
// BOARD INDEX TO ASCII SQUARE NAME
//***********************************************************
// FUNCTION:   --  To translate a hexadecimal index in the
//                 board array into an ascii description
//                 of the square in algebraic chess notation.
//
// CALLED BY:  --  CPTRMV
//
// CALLS:      --  DIVIDE
//
// ARGUMENTS:  --  Board index input in register D and the
//                 Ascii square name is output in register
//                 pair HL.
//***********************************************************

//
// Board size is 120 bytes, 12 rows of 10
// x is padding, s is square
//
//  xxxxxxxxxx
//  xxxxxxxxxx
//  xssssssssx    <--- offset of a1, first s = 21
//  xssssssssx
//  xssssssssx
//  xssssssssx
//  xssssssssx
//  xssssssssx
//  xssssssssx
//  xssssssssx    <--- offset of h8, last s = 98
//  xxxxxxxxxx
//  xxxxxxxxxx

uint16_t BITASN( uint8_t idx )
{
    uint8_t rank = idx/10;         // rank 2-9
    uint8_t file = idx%10;         // file 1-8
    rank = (rank-2) + '1';         // ascii '1'-'8'
    file = (file-1) + 'a';         // ascii 'a'-'h'
    uint16_t temp = rank;
    temp = (temp<<8) + file;
    return temp;
}

//
//  Omit some more Z80 user interface stuff, function
//  PLYRMV
//

//***********************************************************
// ASCII SQUARE NAME TO BOARD INDEX
//***********************************************************
// FUNCTION:   --  To convert an algebraic square name in
//                 Ascii to a hexadecimal board index.
//                 This routine also checks the input for
//                 validity.
//
// CALLED BY:  --  PLYRMV
//
// CALLS:      --  MLTPLY
//
// ARGUMENTS:  --  Accepts the square name in register pair HL
//                 and outputs the board index in register A.
//                 Register B = 0 if ok. Register B = Register
//                 A if invalid.
//***********************************************************

uint8_t ASNTBI( uint8_t file, uint8_t rank )
{

    // If not okay, return 0
    bool ok = ('1'<=rank && rank<='8' && 'A'<=file && file<='H' );
    if( !ok )
        return 0;

    // Otherwise return offset from 21-98
    rank -= '0';            // 1 - 8
    rank++;                 // 2 - 9
    file -= 'A';            // 0 - 7
    file++;                 // 1 - 8
    uint8_t offset = rank*10 + file;    // 21-98 (SQ_a1 - SQ_h8)
    return offset;
}

//***********************************************************
// VALIDATE MOVE SUBROUTINE
//***********************************************************
// FUNCTION:   --  To check a players move for validity.
//
// CALLED BY:  --  PLYRMV
//
// CALLS:      --  GENMOV
//                 MOVE
//                 INCHK
//                 UNMOVE
//
// ARGUMENTS:  --  Returns flag in register A, 0 for valid
//                 and 1 for invalid move.
//***********************************************************
bool VALMOV()
{
    // Return bool ok
    bool ok=false;

    //  Save last move pointer
    ML *mlptrj_save = z.MLPTRJ;

    // Set user color to opposite of computer's color
    z.COLOR = IS_WHITE(z.KOLOR) ? BLACK : WHITE;

    // Load move list index
    ML_HEAD *p = z.PLYIX;
    p--;
    z.MLPTRI = p;

    // Next available list pointer
    ML *ml = z.MLIST;
    ml += 1024/sizeof(ML);      // FIXME this is a mystery
    z.MLNXT = ml;

    // Generate opponent's moves
    GENMOV();

    // Search for move, "From" position (offset 0) "To" position (offset 1)
    uint8_t *q = (uint8_t *)&z.MVEMSG;
    for(;;)
    {

        // Move found ?
        if( *q == ml->from && *(q+1) == ml->to )
            break;  // yes

        //  Pointer to next list move
        ml = ml->link_ptr;
        if( ml == 0 )   // at end of list ?
        {

            // Invalid move, restore last move pointer
            z.MLPTRJ = mlptrj_save;
            return ok;  // false
        }
    }

    //  Save opponents move pointer
    z.MLPTRJ = ml;

    //  Make move on board array
    MOVE();

    //  Was it a legal move ?
    bool inchk = INCHK(z.COLOR);
    ok = !inchk;
    if( !ok )
    {

        // Not legal so Undo move on board array
        UNMOVE();

        //  Restore last move pointer
        z.MLPTRJ = mlptrj_save;
    }
    return ok;
}

//
//  Omit some more Z80 user interface stuff, functions
//  CHARTR, PGIFND, MATED, ANALYS
//

//***********************************************************
// UPDATE POSITIONS OF ROYALTY
//***********************************************************
// FUNCTION:   --  To update the positions of the Kings
//                 and Queen after a change of board position
//                 in ANALYS.
//
// CALLED BY:  --  ANALYS
//
// CALLS:      --  None
//
// ARGUMENTS:  --  None
//***********************************************************

void ROYALT()
{

    // Clear royalty array
    z.POSK[0] = 0;
    z.POSK[1] = 0;
    z.POSQ[0] = 0;
    z.POSQ[1] = 0;

    // Board idx = a1 to h8 inclusive
    for( int idx=SQ_a1; idx<=SQ_h8; idx++ )
    {
        uint8_t piece = z.BOARDA[idx];

        // Update king or queen position if king or queen found
        if( (piece&7) == KING )
        {
            z.POSK[IS_BLACK(piece)?1:0] = idx;
        }
        else if( (piece&7) == QUEEN )
        {
            z.POSQ[IS_BLACK(piece)?1:0] = idx;
        }
    }

    // leave M1 at h8, not padding beyond h8 [probably not needed]
    z.M1 = SQ_h8;
}

//
//  Omit some more Z80 user interface stuff, functions
//  DSPBRD, BSETUP, INSPCE, CONVRT, DIVIDE, MLTPLY, BLNKER, EXECMV
//

// How Sargon Works
// ================

/*

Sargon calculates good chess moves using the classic minimax and
alpha-beta algorithms.

Minimax
-------

To calculate a move by minimax, first we need a scoring function which
takes as input any chess position and provides as output a numerical
estimate of which side stands better and by how much.

With that in hand, to work out a good move in any position, generate
every move available, then score the positions arising after each move
and take the best (maximum). Now we not only have a provisional best
move, we have an improved and refined score for the opening position.
It will be a more accurate score than the static scoring function alone
provides.

That is already minimax, for 1 ply (or level). To go one ply deeper we
simply get refined scores rather than static scores for each of the
positions we reached at one ply. This process can be continued
indefinitely, the score of any position can be improved by replacing it
with the best score of any position that can be reached in one move. We
can't actually continue indefinitely, at a certain point (or depth) we
use the static scores from the scoring function instead of going yet
another ply deeper.

The net result is a tree of analysis, and the positions where we stop
looking deeper are called leaves. The static scores from those leaves
are backed up through each level of refinement until we have our final
score and final move at the root of the tree.

As we look deeper into a position we score White to play and Black to
play positions alternately at successive plies and so we are
alternately maximising and minimising the numeric score to find the
move at each level. The name minimax refers to alternately minimising
and maximising. It actually completely summarises a two ply search
where the program seeks to minimise the maximum the opponent can
achieve.

Alpha-Beta
----------

The score of every position in the tree of analysis is either a static
score at a leaf position or a backed up score anywhere else. So it
might seem we only need to apply the static analyser at the leaf
positions. It turns out though that ironically enough the fastest and
most efficient path to the final minimax score involves statically
scoring all the positions. We use an optimisation technique called
alpha-beta pruning that works wonders for minimax calculation
efficiency if the move lists in every position are roughly sorted from
strongest to weakest move using the static scoring function.

Alpha-beta pruning comes into play when analysing a move after at least
one alternative move has been fully analysed. Sargon does depth first
tree searching, so it lends itself to this, the very first move in
every move list at every level is fully analysed to maximum depth
before work starts on the second move. Imagine we've completed both the
first and second move and we are now working on the third move, working
through the list of replies to the third move searching for the best
reply. Alpha-beta asks of each reply, "does this reply already refute
the third move?" If the opponent can do at least as well as they are
doing against the best of the earlier alternatives (either the first or
second move) by playing that reply, then the third move is effectively
refuted and we can save time by skipping further replies and going
straight on to the fourth move.

The skipped replies each involve a whole (sub)tree of analysis, so
this can be tremendously effective, but only if good alternatives tend
to be analysed first, and in turn good replies to those alternatives
are also analysed first.

Tracing Sargon's Processing
---------------------------

I have added a trace facility to Zargon that allows us to trace the
progress of the move calculation algorithm by showing the state of the
analysis tree as it develops. The main trace function is tracef(),
it can be found in file trace.cpp.

Sargon uses a very simple and efficient system to store the move tree
effectively as a single long list of moves. The list is divided into
sections. Ply 1 moves come first, then ply 2, then ply 3 etc. Although
the sections have different lengths they don't ever need to be shifted
around or reallocated in any way. The ply 1 moves are laid down first
and remain unchanged for the entire calculation. The ply 2 moves are
stacked into place immediately after ply 1. The entire section of old
ply 2 moves are popped off the stack and replaced with new ply 2 moves
as each ply 1 move is considered in turn. One set of ply 2 moves hosts
a series of ply 3 sections in the same way. And so it continues to any
required depth.

Our trace facility shows the sections of the move tree as lines of
descriptive text. The following discussion uses similar diagrams, with
simplifications and elaborations as it suits the explanation.

As mentioned, the generated moves are stored in one big list in memory.
The list starts with all the moves at ply 1. One and only one of those
moves is the current move, the list continues with all available ply 2
moves after the ply 1 current move is played. Then one of these ply 2
moves is the current move at that ply giving us a current position
after 2 moves and the list continues with all available ply moves in
that position.

Illustrative Example, Opening Moves
-----------------------------------

As an example, imagine Sargon is trying to calculate the best move in
the opening position, at depth 4 ply (with no opening book).

First it generates all the moves in the opening position and statically
sorts them in place using a linked list. This is provisional, rough
sorting that we later refine with search, but let's imagine it's pretty
smart and gives us an order like e4,d4,c4,Nf3 ... f3 (Is 1.f3 the worst
opening move? Not sure).

Then it internally "plays" the first move (so 1.e4) and generates and
sorts all the ply two moves after that. Hopefully something like
e5,c5,c6,e6,g6,Nf6. Remember all these moves are stored in one big
list, it's actually a very efficient way to build a move tree.

The process continues, again we "play" the first move in this ply 2 list
so we now have e4 e5 on the board, and we generate and sort all legal
moves at ply 3 storing them immediately after the ply 2 moves. Of
course we keep a stack of pointers to keep track of where each ply's
move list ends and the next ply's start. Now we have something at ply 3
like Nf3,Nc3,Bc4,f4... etc for the position after e4 e5. Next we play
Nf3 and generate the ply 4 moves after e4 e5 Nf3 adding them to the end
of the one big list (of course). At this point we deviate from the
process so far. We are now at ply 4, the specified search depth and we
don't sort the moves. Sorting at the earlier plies speeds up Alpha-Beta
pruning but isn't useful at the final ply so it's omitted to make
things as fast as possible.

This is a picture of how our big list looks now;

1: ->e4,d4,c4,Nf3,g3,b3,f4 ... f3
2: ->e5,c5,c6,e6,g6,Nf3,d6 ... b5
3: ->Nf3,Nc3,Bc4,f4,d4,c3,Be2 ... b4
4u: ->a5,a6,b5,b6 ... Ba3,Ne7,Nf6,Nh6

The u in 4u indicates unsorted, chess players will immediately recognise
the weird order, compared to the earlier plies.

Sargon keeps two stacks of ply pointers, one for the current location in
each ply (shown with ->) and one for the end of storage of each ply.

At the moment the current location corresponds to the start of each ply
list, but that situation is only temporary. Note that Sargon's end of
ply storage pointers don't correspond to the end of the sorted lines,
because the sort is performed in place with a linked list (the moves
themselves don't move, only the link pointers). So for example the end
of ply 1 storage will be after h4, the last move generated at ply 1.
The end of ply 4 storage will be after Nh6. Since ply 4 (and only ply
4) isn't sorted, the end of ply 4 storage does show in our diagram.

Sargon doesn't actually keep pointers to the start of the ply lists, it
doesn't need them. To trace and debug Sargon's workings it is useful to
have these pointers though, so there is now debug only code to
calculate these positions. Sargon does keep track of the end of the ply
lists of course, using a conventional NULL link pointer to indicate end
of list.

Returning to Sargon's depth 4 search for the best White opening move,
Sargon now statically evaluates each 'leaf' position after each ply 4
move in turn, to find the best such Black move. The score of this best
move becomes the backed up (refined) score of White move Nf3 in the
sequence e4 e5 Nf3.

Ascending and Descending
------------------------

Up until now we've always been going in one direction, we've
been 'descending' down the tree. To make further progress it's now time
to do our first 'ascending' up the tree.

Sargon undoes Nf3 at ply 3 and plays Nc3, the second move of the ply 3
list instead. Then the old Nf3 ply 4 moves are efficiently flushed by
popping the last end of ply storage pointer off the stack and using
that location to generate and store a new list of Nc3 ply 4 moves.

Sargon evaluates all these ply 4 moves to get a backed up score for Nc3
in the sequence e4 e5 Nc3.

I'm sure you see where this is going. Sargon rinses and repeats until it
has backed up scores for all of Nf3, Nc3, Bc4 etc. The best score for
that White move establishes the backed up score for Black move e5 in
the sequence e4 e5.

Then Sargon will undo e5 and try c5 instead. That will require a new ply
3 list for the first time, and a whole series of ply 4 lists. But
eventually we get the score for c5 then all the other ply 2 moves
yielding the best Black reply to e4 which establishes the score for
e4.

Once that's done Sargon will undo e4 and try d4 at ply 1, and for the
first time we get a new ply 2 list, and of course many ply 3 lists and
many many ply 4 lists.

A bit later still, the moves in memory will look like this;

1: e4,d4,->c4,Nf3,g3,b3,f4 ... f3
2: Nf6,->e5,c5,e6,c6,d6,g6 ... f6
3: g3,->Nc3,e3 ... f4
4u: a5,a6,b5,->b6 ... Ba3,Ne7,Nf6,Nh6

Note that Sargon is done with the moves before arrows. So 1.e4 and 1.d4
have been completely analysed, that is their final backed up score has
been established, and the best one kept (this diagram doesn't show the
scores of 1.e4 or 1.d4 so we don't know which one is the best move so
far).

At ply 2 c4 Nf6 has been completely analysed and its final backed up
score established. This means that we have a partial score for 1.c4, it
will be the score of c4 Nf6 or worse if Sargon finds a better reply
than Nf6. Sargon is currently working on c4 e5 to see if it is better
than c4 Nf6. If c4 e5 is better than c4 Nf6 then we know the score for
1.c4 will be score of c4 e5 or worse. This is minimax, the score of
1.c4 gets worse as the score to the responses to 1.c4 get better.

Note that the final backed up scores of each of c4 e5 g3 and c4 e5 Nc3
a5 and c4 e5 Nc3 a6 and c4 e5 Nc3 b5 have all been established as we
work on the partially complete c4 e5 score. Every node in the tree is
either completely backed up (we know it's score), partially backed
up (we know it's score is equal to or worse than the best score found
so far), or it's provisional (only the static score has been
calculated).

To track partial and complete scores we only need to store the best
score at each ply, so for this 4 ply search we need an array of four
scores.

If we imagine the 1.d4 score is better than the 1.e4 score then the
entry in the array for ply 1 is the score for 1.d4.

The entry in the array for ply 2 is the c4 Nf6 score. It will be
replaced by the score of c4 e5 if c4 e5 scores better than c4 Nf6.

The entry in the array for ply 3 is the c4 e5 g3 score. It will be
replaced by the score of c4 e5 Nc3 if c4 e5 Nc3 scores better than c4
e5 g3.

The entry in the array for ply 4 is whichever one of the c4 e5 Nc3 a5,
c4 e5 Nc3 a6, c4 e5 Nc3 b5 scores is best. It might be about to be
replaced by the score of c4 e5 Nc3 b6 if that turns out to be the best
ply 4 score so far.

So the ply 1 score is the best opening position score (so far), the ply
2 score is the best c4 score (so far) the ply 3 score is the best c4 e5
score so far, the ply 4 score is the best c4 e5 Nc3 score so far.

Sorry to belabour these points but a complete and clear understanding of
this process is essential to really understanding the Sargon code.

Now imagine that the score for c4 e5 when we finish the checking all the
backed up ply 3 White moves is great for Black (bad for White). Then
the score for c4 e5 replaces the score for c4 Nf6 at ply 2. But wait
there's more. We should also routinely compare this score to the ply 1
score above (currently the score for 1.d4). The score for 1.c4 is going
to be this (bad for White) score for 1.c4 e5 or even worse. So 1.c4 is
refuted already, it will not supplant 1.d4 we can abort the full
analysis of 1.c4 already and move on to 1.Nf3.

This is Alpha Beta pruning, the implementation is almost trivially easy
in Sargon - a real advantage over more conventional recursive
implementations of minimax and alpha-beta (which can be very mind
bending in my opinion).

A detail worth noting is that the alpha beta comparison is a better than
or equal comparison rather than a better than comparison (eg is c4 e5
is better than or equal to d4?). This is an optimisation, it's worth
pruning c4 analysis now rather than continuing when we know c4 can
perhaps match d4 but definitely can't beat it.

It's important to understand that minimax and alpha-beta are inherently
recursive, even though Sargon doesn't use recursive function calls to
implement it. As previously mentioned one ply 2 section of moves hosts
multiple ply 3 sections of moves, just as the ply 1 section hosts
multiple ply 2 sections. If we were doing really deep search we might
have a ply 8 section hosting multiple ply 9 sections (each of which, in
its time will host .... ). This means, for example that Alpha-beta will
happily be applied to ply 9 replies to ply 8 moves that already have
fully backed up ply 8 alternatives.

Alpha beta often arises in routine exchanges. White to move can exchange
minor pieces for no nominal gain, only one Black piece can recapture.
When ply 1 is the capture, the ply 2 list will presumably start with
the recapture. The recapture will be completely analysed and will
establish the initial ply 1 score in the scoring table. The
alternatives to recapture will then be considered in turn but will
likely all be abandoned by Alpha-Beta very quickly because they are so
easy to refute (any White reply that keeps the free material).

Sargon keeps an array of best scores but it doesn't keep an array of
best moves - in other words it doesn't remember the variation that it
considers best play from the start position. Sargon was originally just
a player not an analyst, it just remembered the best move at ply
1 (1.d4 in the partially complete calculation we are following).

However we have extended the basic Sargon code to track the
PV (principal variation) but the algorithm is out of scope for this
introduction.

In the original Sargon code the comments talk of the Minimax comparison
being to the score one ply above, and the Alpha-Beta comparison being
to the score two plies above. This simply means the Minimax comparison
is establishing a score for a move by finding the best reply (one ply
difference between a move and its replies). At the same time the
Alpha-Beta comparison is checking the reply against the comparison
between the move and it's alternatives (one ply higher, so two plies
total) for each reply in turn to see whether that reply is sufficient
to refute the move early and allow us to abandon it without checking
any more replies.

The Score Table
---------------

We have been considering this scenario;

1: [+0.3] e4,BESTM=d4,->c4,Nf3,g3,b3,f4 ... f3
2: [+0.4] Nf6,e5,->c5,e6,c6,d6,g6 ... f6
3: [+0.2] Nc3,->Nf3,g3,e3 ... h3
4u: [+1.1] MLPTRI->a5,a6,b5,MLPTRJ->b6, ... Qb6,Qa5,Nf6,Nh6

The arrow -> indicators establish a node in the total tree of moves
arising from the initial position, currently that node is c4 c5 Nf3
b6.

I have added a few things compared to the earlier diagram of the same
scenario. The scoring table is shown in [brackets]. At ply 1 +0.3 is
the best score of the fully analysed first moves to date (so e4 and
d4). At ply 2 +0.4 is the best score of the fully analysed replies to
c4 to date (so c4 Nf6 and and c4 e5). At ply 3 +0.2 is the best score
of the fully analysed replies to c4 c5 to date (so c4 c5 Nc3 only). And
finally at ply 4 +1.1 is the best score of the fully analysed replies
to c4 c5 Nf3 to date (so c4 c5 Nf3 a5, c4 c5 Nf3 a6 and c4 c5 Nf3 b5).

We need to keep track of which particular move is associated with the
best score at ply 1 only, so that Sargon knows what move to play. I've
indicated that with BESTM= where BESTM is the name of the Sargon
variable used for this purpose. So d4 is the best move found to date,
with a fully backed up score of +0.3.

I've also added an indicator for the Sargon variable MLPTRJ (pointer
to the current move at the current depth).

We are not always at a leaf node like this of course. A bit later the
situation might look like this;

1: [+0.3] e4,BESTM=d4,c4,->Nf3,g3,b3,f4 ... f3
2: [+0.4] d5,Nf6,c5,e6,c6,MLPTRJ->d6,g6 ... f6

The node under consideration is now Nf3 d6. The score table and move
lists for plies 3 and 4 are not shown because the current values of
that data is irrelevant, Sargon has finished processing that
information, in the same way that it has finished with the e4, d4 and
c4 at ply 1 and d5 Nf6, c5, e6 and c6 at ply 2.

Compared to the previous picture, Sargon has finished with 1.c4 and it
evidently did not replace 1.d4 as the best move to date. So far 1.Nf3
is doing a bit better than 1.d4 with [+0.4] after processing the
replies d5, Nf6, c5, e6 and c6. If any of these replies had been as
good or better for Black than +0.3 (so less than or equal to 0.3) then
1.Nf3 would have been considered refuted by Alpha-Beta pruning and
Sargon would have continued to 1.g3 immediately.

I'm using this example because of the familiarity of opening move
sequences, the actual scoring is not representative of Sargon at all,
it is not as subtle as these illustrative scores.

The Sargon algorithm will next generate a list of all White moves
available in the position after c4 d6 and sort them. It will then step
to the position after the first move in that list and generate all the
moves in that position etc. etc. In other words it will perform exactly
the same procedure in node c4 d6 to calculate the best move it can find
after that position as it performed, or strictly speaking is
performing, to calculate the best move in the starting position.
Although Sargon doesn't implement the minimax search using a function
calling itself, it is in essence a recursive algorithm.

Pseudo Code
-----------

Putting these ideas together gives us FNDMOV() (calculate best move)
pseudo code, the heart of Sargon. This pseudo code ignores some
details, including an extra ply beyond PLYMAX whenever a move gives
check, to check for checkmate;

Set NPLY = 1 and generate move ply 1 list
If not at max ply, score and sort the moves
Loop through the current move list
     If no more moves in move list
        If NPLY == 1 return
        Get SCORE (a) from score ply table and Ascend (NPLY--, undo move)
     Else if more moves in move list
        Make the move
        If not yet at max depth
            Generate moves at next ply and Descend (NPLY++)
            If not at max ply, score and sort the moves
            Continue loop to iterate through the new move list
        Else if max depth
            Evaluate SCORE (b) at leaf node using POINTS()
            Unmake the move
    SCORE available, from (a [bubble up]) or (b [leaf])
    If score is better or equal to score above in the score ply table
        Alpha Beta cutoff, Ascend (NPLY--, undo move). This abandons
         the current move list, this reply has refuted the current
         move a ply higher. That move will not be the best move at
         that ply, we prefer a previously analysed alternative and
         its final score. There is no need to analyse the other
         replies in the current move list.
    Else if score is better than score in current score ply table
        Update score ply table
        If NPLY == 1
             Update best move found to date, if it gives mate return
End loop

Score Representation
--------------------

Sargon does not use floating point to score positions of course (I used
it in my example above for illustrative purposes only). In fact the
Sargon points system is squeezed into 1 byte of dynamic range, for ease
of programming on Z80. Zargon retains this, but some useful
simplification could be achieved by changing to int16 or int32
representaton. For now the 1 byte system is retained and the details
break down as follows;

Initially the score is calculated as a signed 8 bit integer. Such values
are intrinsically limited to the range -128 to 127, Sargon uses almost
all of this range for its points calculation (specifically -126 to
126).

The value is calculated as 4*LIMIT(30,material) + LIMIT(6,board_control)
The LIMIT(n,x) function is a saturation function that returns either
the value of x, or n if x is greater than n, or -n if x is less than
-n. Therefore initially at least scores are constrained to the range
-126 to 126 (because 4*30+6 = 126). More positive scores favour White.

Material and board control are calculated separately. Board control is
the number of squares controlled, material is the amount of
material (using a 1,3,3,5,9 convention) in half pawns. Both of these
are 'net' values. In fact two netting calculations are made for each
variable, first a White minus Black subtraction, then a relative to
initial position calculation, i.e. a current position minus initial
position subtraction.

So the material and board control variables are effectively extra
material gained by White and extra board control gained by White.
Negative numbers indicate gains by Black. If we used absolute numbers
instead of relative numbers the tiny dynamic range of a one byte number
would saturate if one side had a massive advantage in the starting
position, and Sargon would calculate more or less random moves instead
of trying to make (or resist) further progress.

Importantly, Sargon uses a rudimentary SOMA (Swopping Off Material
Analyzer, see routine XCHNG() and the pin and attack list routines that
enable it to do its work) to adjust the material count to reflect
material that's en-prise. This is an attempt to calculate complete
routine material exchanges in the absense of extra plies that could do
that more comprehensively and accurately. It attempts to account for at
least some pins but it is of course vulnerable to zwichenzugs and other
tactical nuances messing up its evaluations. This is an inevitable
downside of simple, fixed depth search.

The board control variable is also adjusted somewhat, to encourage
castling for example. In general though the Sargon static analyser is
pretty simple, basically a material calculation plus a few
adjustments (as I mentioned earlier, it is not subtle).

The Minimax and Alpha Beta algorithms operate on unsigned 8 bit scores
rather than signed 8 bit scores. The main reason for this is that the
Z80 can compare unsigned 8 bit numbers more efficiently than signed 8
bit numbers. To get the unsigned scores, Sargon adds 128 to the score,
shifting range -126 to 126 into the unsigned range 2 to 254 (midpoint
128). A second reason for this is that it is convenient for zero to be
a sentinel value rather than a valid score value.

In the original Z80 code Sargon somewhat confusingly uses the Z80 negate
opcode to operate on these numbers that it is clearly treating as
unsigned 8 bit values. This confused me for a very long time! It turns
out that negating signed integers is accomplished by the neat
'twos complement' trick of inverting every bit and adding 1. This does
something useful even if we are treating our bits as an unsigned
integer;

0 -> 0         (0x00 -> 0x00)
1 -> 255       (0x01 -> 0xff)
2 -> 254       (0x02 -> 0xfe)
3 -> 253       (0x03 -> 0xfd)
...
127 -> 129     (0x7f -> 0x81)
128 -> 128     (0x80 -> 0x80)
129 -> 127     (0x81 -> 0x7f)
...
253 -> 3       (0xfd -> 0x03)
254 -> 2       (0xfe -> 0x02)
255 -> 1       (0xff -> 0x01)

So for our purposes 'negate' does a chesswise negate on the scores,
scores that are good for White and bad for Black are flipped around to
be good for Black and bad for White. I will subsequently use the word
'flip' rather than 'negate', because I don't like to talk about negating
unsigned numbers!

The material and board control algorithm generates scores in the range
2-254 as we have seen. Score values 1 and 255 are used to indicate mate
for one side or the other. Both values are required so that flipping
the mating score is possible, if White has a mating advantage Black has
a corresponding mating disadvantage and vice-versa. 0 is reserved as a
sentinel, usually to indicate a score that has not yet been
calculated.

It can be very confusing accounting for which way around scores are in
any given scenario. A score of 128 always corresponds to 0.0. But
127 (or 129) might mean a 1/8 pawn advantage to White, or Black, or the
side to move, or the other side (not to move) at different points in
the algorithm.

Let's illustrate the conventions used with the ply diagram we have been
using, with the original example we started with; the starting position
of standard chess after reaching the first leaf position e4,e5,Nf3,a5.
Remember the perhaps unexpected move a5 is due to the leaf nodes not
being sorted. The annotations this time highlight which way around the
unsigned 8 bit scoring is.

0: (B) [0]
1: W [bigger_is_better score] (smaller_is_better: ->e4,d4,c4,Nf3,g3,b3,f4 ... f3)
2: B [bigger_is_better score] (smaller_is_better: ->e5,c5,c6,e6,g6,Nf3,d6 ... b5)
3: W [bigger_is_better score] (smaller_is_better: ->Nf3,Nc3,Bc4,f4,d4,c3,Be2 ... b4)
4u: B [bigger_is_better score] (smaller_is_better: ->a5,a6,b5,b6 ... Nh6)

I've added ply 0, the root position. There is no ply 0 move list, but
there is a ply 0 score that stays at 0 (sentinel value) through the
whole algorithm. Strictly speaking it would be consistent to assign it
with a bigger is better score for Black (so smaller is better score for
White) for the root position at the very end once Sargon has calculated
and scored the move it is going to play at ply 1.

I am using conventional C arrays to implement these data structures so
they are quite properly 0 based, and unfortunately the 0th element of
the move lists is wasted. The variable NPLY serves as the index into
the score table and the move lists.

I have labelled the plies explicitly with an alternating colour W(hite),
B(lack), W, B etc. Ply 0 the root ply gets the Black label for
consistency with the alternating pattern, you can think of it as a
virtual Black move creating the start position for Ply 1 where it is
White to play.

The move lists are sorted according to the static position evaluation
scores, these use the smaller is better convention as shown so the best
static scores have the lowest numbers and are listed first. Perhaps I
should mention again that the Sargon static evaluator is nowhere near
good enough to determine opening move ordering as plausible as the
opening moves I list here, it's illustrative only! Don't worry, a
realistic actual example is not too far away.

If Sargon is playing Black the ply labels are reversed and the static
move scores flipped so they are still smallest number is better, and
the ply table scores still have the bigger is better convention;

0: (W) [0]
1: B [bigger_is_better score] (smaller_is_better: ...
2: W [bigger_is_better score] (smaller_is_better: ...
3: B [bigger_is_better score] (smaller_is_better: ...
4u: W [bigger_is_better score] (smaller_is_better: ...

With these ideas now hopefully firmly established we can now nail down
the numerical conventions at the heart of the Sargon FNDMOV()
algorithm. From the pseudo code previously we have;

SCORE available, from (a [backing up]) or (b [leaf])
If score is better or equal to score above in the score ply table
    Alpha Beta cutoff, Ascend (NPLY--, undo move). This abandons
     the current move list, this reply has refuted the current
     move a ply higher. That move will not be the best move at
     that ply, we prefer a previously analysed alternative and
     its final score. There is no need to analyse the other
     replies in the current move list.
Else if score is better than score in current score ply table
    Update score ply table

The specific comparison calculations are as follows;

score_smaller_is_better available, from (a backing up) or (b leaf)
If score_smaller_is_better <= bigger_is_better score in table one ply above
    Alpha Beta cutoff
Else
    Flip score_smaller_is_better => score_bigger_is_better
    If score_bigger_is_better > bigger_is_better score in ply table
        bigger_is_better score in ply table = score_bigger_is_better

The 'if score_smaller_is_better <= bigger_is_better' comparison makes
sense because these are adjacent plies which correspond to different
sides to move. So for example if score_smaller_is_better is for a White
ply it is also a score_bigger_is_better type score for Black and can be
directly compared to a Black ply score_bigger_is_better score.

The 0 value at ply 0 in the score table serves to allow us to avoid a
check for ply 2 or greater when doing an Alpha Beta check, that would
be unnecessary code in the hot path. Instead we just have a
harmless 'is score_smaller_is_better <= 0' check at ply 1 which never
triggers and only executes comparitively rarely (for every N ply 1
operations there are N*N ply 2 operations, N*N*N ply 3 operations
etc.).

Detailed Working Example
------------------------

Just for the really dedicated here is a simple working PLYMAX=2
example.

The test FEN is '8/8/k4rpr/4N1p1/5p1p/3b4/PP6/K7 w - - 0 1'
White to move;

    ........
    ........
    k....rpr
    ....N.p.
    .....p.p
    ...b....
    PP......
    K.......

White has a knight and two pawns versus Black's two rooks, bishop and
four pawns and for the record is totally lost. But White does have two
sensible material winning moves, and the example shows Sargon selecting
the best, or at least the greediest, of those with a PLYMAX = 2
calculation. The two good moves are Ne5xBd3 grabbing a free piece and
Ne5-g4 forking the two rooks and actually winning a whole rook not just
an exchange. I've organised the pawns to avoid the Black rooks
defending each other or giving checks. In fact the example position is
chosen to avoid all complications. There are just the two useful
initial White moves, all other moves for both sides are just do nothing
moves or simple one move material losing blunders.

The static evaluator ranks Ne5xBd3 grabbing the piece as best which is
no surprise. The complete merits of Ne5-g4 forking the rooks is beyond
the static analyser, but it does like a couple of attacks and so it is
ranked second followed by all the do nothing moves and blunders.

The following is edited extracts from the debug log, with a descriptive
annotation after each extract.

4> Nxd3 f3 is the best move if score_bigger_is_better > ply score: YES (score_smaller_is_better=154, score_bigger_is_better=102, ply score=0)
1: [0]  (a1b1) ->Nxd3(102) Ng4(110) a3(128) a4(128) b3(128) b4(128) Nf3(128) Nd7(130) Nf7(148) Nxg6(148) Nc4(156) Nc6(156)
2u: [0]  MLPTRJ->f3(154) h3(0) g4(0) Kb7(0) Kb5(0) Ka7(0) Ka5(0) Kb6(0) Rf7(0) Rf8(0) Rf5(0) Re6(0) Rd6(0) Rc6(0) Rb6(0) Rh7(0) Rh8(0) Rh5(0)

Start considering replies to 1.Nxd3. Only a two ply search so we are
immediately at the unsorted leaf nodes. The first one is 1...f3 and it
establishes the initial score to beat.

41> Nxd3 Rd6 is the best move if score_bigger_is_better > ply score: YES (score_smaller_is_better=150, score_bigger_is_better=106, ply score=102)
1: [0]  (a1b1) ->Nxd3(102) Ng4(110) a3(128) a4(128) b3(128) b4(128) Nf3(128) Nd7(130) Nf7(148) Nxg6(148) Nc4(156) Nc6(156)
2u: [102]  f3(154) h3(154) g4(154) Kb7(154) Kb5(154) Ka7(154) Ka5(154) Kb6(154) Rf7(154) Rf8(154) Rf5(154) Re6(154) MLPTRJ->Rd6(150) Rc6(0) Rb6(0) Rh7(0) Rh8(0) Rh5(0)

Continue considering replies to 1.Nxd3. The first 12 replies are all do
nothing moves that score 102, the 13th is 1...Rd6 which at least
attacks the knight so it scores slightly better with 106.

59> Nxd3 Alpha-beta dummy check, never cuts off at ply 1
1: [0]  (a1b1) MLPTRJ->Nxd3(102) Ng4(110) a3(128) a4(128) b3(128) b4(128) Nf3(128) Nd7(130) Nf7(148) Nxg6(148) Nc4(156) Nc6(156)

60> Nxd3 is the best move if score_bigger_is_better > ply score: YES (score_smaller_is_better=106, score_bigger_is_better=150, ply score=0)
1: [0]  (a1b1) MLPTRJ->Nxd3(102) Ng4(110) a3(128) a4(128) b3(128) b4(128) Nf3(128) Nd7(130) Nf7(148) Nxg6(148) Nc4(156) Nc6(156)

We now have a backed up score for Nxd3, the first move in the move list
for the root position. The backed up score is the score of the best
replies, it's score_bigger_is_better=106 when it's a Black reply score
but score_smaller_is_better=106 for the White move Black is replying
to. Notice that its score_smaller_is_better of 106 is slightly
different to its static score of 102. At ply 1 we never do an alpha
beta cutoff, we aren't considering replies to a move that alpha beta
can refute.

Nxd3 does establish the score of the move to beat at ply 1. It is
106 (smaller_is_better) and 150 (bigger_is_better)

65> Ng4 Bc4 is the best move if score_bigger_is_better > ply score: YES (score_smaller_is_better=165, score_bigger_is_better=91, ply score=0)
1: [150]  (a1b1) BESTM->Nxd3(102) ->Ng4(110) a3(128) a4(128) b3(128) b4(128) Nf3(128) Nd7(130) Nf7(148) Nxg6(148) Nc4(156) Nc6(156)
2u: [0]  MLPTRJ->Bc4(165) Bb5(0) Be4(0) Bf5(0) Bc2(0) Bb1(0) Be2(0) Bf1(0) f3(0) h3(0) Kb7(0) Kb5(0) Ka7(0) Ka5(0) Kb6(0) Rf7(0) Rf8(0) Rf5(0) Re6(0) Rd6(0) Rc6(0) Rb6(0) Rh7(0) Rh8(0) Rh5(0)

Start considering replies to 1.Ng4. Only a two ply search so we are
immediately at the unsorted leaf nodes. The first one is 1...Bc4 and it
establishes 91 as the (unimpressive!) initial score_bigger_is_better to
beat.

72> Ng4 Be4 is the best move if score_bigger_is_better > ply score: YES (score_smaller_is_better=164, score_bigger_is_better=92, ply score=91)
1: [150]  (a1b1) BESTM->Nxd3(102) ->Ng4(110) a3(128) a4(128) b3(128) b4(128) Nf3(128) Nd7(130) Nf7(148) Nxg6(148) Nc4(156) Nc6(156)
2u: [91]  Bc4(165) Bb5(167) MLPTRJ->Be4(164) Bf5(0) Bc2(0) Bb1(0) Be2(0) Bf1(0) f3(0) h3(0) Kb7(0) Kb5(0) Ka7(0) Ka5(0) Kb6(0) Rf7(0) Rf8(0) Rf5(0) Re6(0) Rd6(0) Rc6(0) Rb6(0) Rh7(0) Rh8(0) Rh5(0)

1...Be4 is a marginal one point improvement over 1...Bc4, 91 becomes 92
but so far the replies to 1...Ng4 are unimpressive,

97> Ng4 Kb7 is the best move if score_bigger_is_better > ply score: YES (score_smaller_is_better=163, score_bigger_is_better=93, ply score=92)
1: [150]  (a1b1) BESTM->Nxd3(102) ->Ng4(110) a3(128) a4(128) b3(128) b4(128) Nf3(128) Nd7(130) Nf7(148) Nxg6(148) Nc4(156) Nc6(156)
2u: [92]  Bc4(165) Bb5(167) Be4(164) Bf5(164) Bc2(168) Bb1(172) Be2(164) Bf1(169) f3(165) h3(165) MLPTRJ->Kb7(163) Kb5(0) Ka7(0) Ka5(0) Kb6(0) Rf7(0) Rf8(0) Rf5(0) Re6(0) Rd6(0) Rc6(0) Rb6(0) Rh7(0) Rh8(0) Rh5(0)

The 11th reply considered is 1...Kb7, marginal improvement 92->93

113> Ng4 Rf7 is the best move if score_bigger_is_better > ply score: YES (score_smaller_is_better=161, score_bigger_is_better=95, ply score=93)
1: [150]  (a1b1) BESTM->Nxd3(102) ->Ng4(110) a3(128) a4(128) b3(128) b4(128) Nf3(128) Nd7(130) Nf7(148) Nxg6(148) Nc4(156) Nc6(156)
2u: [93]  Bc4(165) Bb5(167) Be4(164) Bf5(164) Bc2(168) Bb1(172) Be2(164) Bf1(169) f3(165) h3(165) Kb7(163) Kb5(164) Ka7(166) Ka5(166) Kb6(164) MLPTRJ->Rf7(161) Rf8(0) Rf5(0) Re6(0) Rd6(0) Rc6(0) Rb6(0) Rh7(0) Rh8(0) Rh5(0)

The 16th reply considered is 1...Rf7, marginal improvement 93->95

123> Ng4 Re6 is the best move if score_bigger_is_better > ply score: YES (score_smaller_is_better=159, score_bigger_is_better=97, ply score=95)
1: [150]  (a1b1) BESTM->Nxd3(102) ->Ng4(110) a3(128) a4(128) b3(128) b4(128) Nf3(128) Nd7(130) Nf7(148) Nxg6(148) Nc4(156) Nc6(156)
2u: [95]  Bc4(165) Bb5(167) Be4(164) Bf5(164) Bc2(168) Bb1(172) Be2(164) Bf1(169) f3(165) h3(165) Kb7(163) Kb5(164) Ka7(166) Ka5(166) Kb6(164) Rf7(161) Rf8(161) Rf5(163) MLPTRJ->Re6(159) Rd6(0) Rc6(0) Rb6(0) Rh7(0) Rh8(0) Rh5(0)

The 19th reply considered is 1...Re6, marginal improvement 95->97

136> Ng4 Rh7 is the best move if score_bigger_is_better > ply score: YES (score_smaller_is_better=158, score_bigger_is_better=98, ply score=97)
1: [150]  (a1b1) BESTM->Nxd3(102) ->Ng4(110) a3(128) a4(128) b3(128) b4(128) Nf3(128) Nd7(130) Nf7(148) Nxg6(148) Nc4(156) Nc6(156)
2u: [97]  Bc4(165) Bb5(167) Be4(164) Bf5(164) Bc2(168) Bb1(172) Be2(164) Bf1(169) f3(165) h3(165) Kb7(163) Kb5(164) Ka7(166) Ka5(166) Kb6(164) Rf7(161) Rf8(161) Rf5(163) Re6(159) Rd6(161) Rc6(159) Rb6(160) MLPTRJ->Rh7(158) Rh8(0) Rh5(0)

The 23rd reply considered is 1...Rh7, marginal improvement 97->98 All of
the replies are pretty much equivalent because they all leave a whole
rook en-prise.

145> Ng4 Alpha-beta dummy check, never cuts off at ply 1
1: [150]  (a1b1) BESTM->Nxd3(102) MLPTRJ->Ng4(110) a3(128) a4(128) b3(128) b4(128) Nf3(128) Nd7(130) Nf7(148) Nxg6(148) Nc4(156) Nc6(156)

146> Ng4 is the best move if score_bigger_is_better > ply score: YES (score_smaller_is_better=98, score_bigger_is_better=158, ply score=150)
1: [150]  (a1b1) BESTM->Nxd3(102) MLPTRJ->Ng4(110) a3(128) a4(128) b3(128) b4(128) Nf3(128) Nd7(130) Nf7(148) Nxg6(148) Nc4(156) Nc6(156)

The replies to Ng4 have all been considered and it has a backed up
smaller_is_better score of 98 (as for Nxd3 the backed up
smaller_is_better score is the best bigger_is_better score of the
replies). As always there is no Alpha-beta cutoff at ply 1.

Going one ply deeper has improved Ng4's static score_smaller_is_better
of 110 to 98, a more significant refinement than the Nxd3's 102 to 106.
So Ng4 usurps Nxd3 as the best move to date, a triumph indeed for
minimax.

150> a3 Bc4 refutes a3 Alpha-beta cutoff, score_smaller_is_better<=ply above: YES (score_smaller_is_better=148, Ng4 is ply above=158)
1: [158]  (a1b1) Nxd3(102) BESTM->Ng4(110) ->a3(128) a4(128) b3(128) b4(128) Nf3(128) Nd7(130) Nf7(148) Nxg6(148) Nc4(156) Nc6(156)
2u: [0]  MLPTRJ->Bc4(148) Bb5(0) Be4(0) Bf5(0) Bc2(0) Bb1(0) Be2(0) Bf1(0) f3(0) h3(0) g4(0) Kb7(0) Kb5(0) Ka7(0) Ka5(0) Kb6(0) Rf7(0) Rf8(0) Rf5(0) Re6(0) Rd6(0) Rc6(0) Rb6(0) Rh7(0) Rh8(0) Rh5(0)

Alpha Beta comes into its own now as the alternatives to best move 1.Ng4
are considered in turn. They are all quickly rejected, usually after
checking just one reply because any Black reply that simply keeps the
free material White is eschewing with a move other than 1.Ng4 is
sufficient to refute the White move. Here 1...Bc4 is sufficient to
refute 1.a3, even though 1...Bc4 actually blunders a bishop, even
that's better than losing a rook to 1.Ng4

154> a4 Bc4 refutes a4 Alpha-beta cutoff, score_smaller_is_better<=ply above: YES (score_smaller_is_better=148, Ng4 is ply above=158)
1: [158]  (a1b1) Nxd3(102) BESTM->Ng4(110) a3(128) ->a4(128) b3(128) b4(128) Nf3(128) Nd7(130) Nf7(148) Nxg6(148) Nc4(156) Nc6(156)
2u: [0]  MLPTRJ->Bc4(148) Bb5(0) Be4(0) Bf5(0) Bc2(0) Bb1(0) Be2(0) Bf1(0) f3(0) h3(0) g4(0) Kb7(0) (a6b5) Ka7(0) Ka5(0) Kb6(0) Rf7(0) Rf8(0) Rf5(0) Re6(0) Rd6(0) Rc6(0) Rb6(0) Rh7(0) Rh8(0) Rh5(0)

1...Bc4 is also sufficient to refute 1.a4, even though like 1.a3 Bc4 it
simply blunders a bishop.

... There are many more alpha-beta cutoffs of lame alternatives to Nxd3
    and Ng4 ...

285> Nc6 Bc4 refutes Nc6 Alpha-beta cutoff, score_smaller_is_better<=ply above: YES (score_smaller_is_better=121, Ng4 is ply above=158)
1: [158]  (a1b1) Nxd3(102) BESTM->Ng4(110) a3(128) a4(128) b3(128) b4(128) Nf3(128) Nd7(130) Nf7(148) Nxg6(148) Nc4(156) ->Nc6(156)
2u: [0]  MLPTRJ->Bc4(121) Bb5(0) Be4(0) Bf5(0) Bc2(0) Bb1(0) Be2(0) Bf1(0) f3(0) h3(0) g4(0) Kb7(0) Kb5(0) (a6a7) (a6a5) Kb6(0) Rf7(0) Rf8(0) Rf5(0) Re6(0) Rd6(0) Rxc6(0) Rh7(0) Rh8(0) Rh5(0)

1.Nc6 is the last alternative first move considered, it is a truly
terrible move blundering the knight instead of taking free material
like Nxd3 or winning material with a fork like Ng4. 1...Bc4 doesn't
blunder the bishop this time, although it doesn't take the knight
either. It is still more than sufficient to refute 1.Nc6 with
alpha-beta cutoff. Once all the ply 1 moves are considered, FNDMOV()
returns with its conclusion that the best move is 1.Ng4.

Fixing Sargon so it Prefers Quicker Mates
=========================================

One problem with unmodified Sargon is that it considers all mates to be
equal, a mate discovered at ply 3 is not weighted more highly than a
mate at ply 6 (say). It plays the mate it finds first, not the quickest
mate. This can be quite annoying and I was happy to apply a fairly
simple fix in Zargon. We start by modifying the basic points formula
slightly

   points = 4*LIMIT(29,material) + LIMIT(6,board control)

I decided distinguishing between 14.5 and 15 pawns of extra material is
rarely important (certainly not in any of my test positions). Note that
some of Sargon's material adjustment calculations do introduce half
pawn values, so 14.5 is not the same as 14, despite the basic 1,3,3,5,9
material convention.

The benefit of limiting to 29, rather than 30, is that the points values
are now in the range -122 to +122 (signed) and 6 to 250 (unsigned).
This makes room for additional mate sentinel values 254,253,252 and 251
and their corresponding flipped values 2,3,4 and 5.

The mate score at plies 1 and 2 remains 255/1. Then each pair of deeper
plies gets a slightly less powerful mating score. So 3 and 4 -> 254/2,
5 and 6 -> 253/3, 7 and 8 -> 252/4, 9 and higher -> 251/5. We can
allocate two plies to each mating score because White moves don't
compete against Black moves in minimax calculations so the same mating
score doesn't create a conflict.

*/


