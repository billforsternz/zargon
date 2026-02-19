//***********************************************************
//
//               SARGON
//
// Sargon is a computer chess playing program designed
// and coded by Dan and Kathe Spracklen.  Copyright 1978. All
// rights reserved.  No part of this publication may be
// reproduced without the prior written permission.
//***********************************************************

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "main.h"
#include "sargon-asm-interface.h"
#include "bridge.h"
#include "zargon_functions.h"
#include "zargon.h"

// Up to 64K of emulated memory
emulated_memory gbl_emulated_memory;        // Now made available as simple global
#if 0
static emulated_memory &m = gbl_emulated_memory;    // This is good practice, but slow
#else
#define m gbl_emulated_memory                       // This is bad practice, but fast
#endif
emulated_memory *zargon_get_ptr_emulated_memory() {return &m;}

// Temp probes
void probe_write( int tag, mig_t val );
void probe_read( int tag );

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
    memset( &m.BOARDA[0], -1, sizeof(m.BOARDA) );

    // Init the 64 squares within the 120 byte BOARD[] array
    uint8_t *dst = &m.BOARDA[SQ_a1];
    uint8_t *src = &m.pieces[0];
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
    m.POSK[0] = SQ_e1;
    m.POSK[1] = SQ_e8;
    m.POSQ[0] = SQ_d1;
    m.POSQ[1] = SQ_d8;
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
    callback_zargon_bridge(CB_PATH);

    // Step along the path
    uint8_t piece = m.BOARDA[m.M2+=dir];

    // Off board?
    //  return 3 if off board
    if( piece == ((uint8_t)-1) )
        return PATH_OFF_BOARD;

    // Set P2=piece and T2=piece type (=piece&7)
    //  return 0 if empty
    else if(  0 == (m.T2 = (m.P2=piece)&7) )
        return PATH_EMPTY;

    // Else we have run into non zero piece P2
    //  return 1 if opposite color to origin piece P1
    //  return 2 if same color as origin piece P1
    return IS_SAME_COLOR(m.P2,m.P1) ? PATH_SAME_COLOR : PATH_OPPOSITE_COLOR;
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

void MPIECE()
{
    callback_zargon_bridge(CB_MPIECE);

    // TODO: Maybe make piece a named parameter
    uint8_t piece = m.P1;
    piece &= 0x87;   // clear DOUBLE, PROMOTION and FIRST/CASTLED flag bits
    bool empty = false;

    //  Decrement black pawns (so pawns, the only directional type, are 0 black and 1 white
    //   from now on in this function)
    if( piece == BPAWN )
        piece--;
    piece &= 7;      // clear colour flag to isolate piece type
    m.T1 = piece;

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

    m.INDX2 = m.dpoint[m.T1];       //  Get direction pointer
    for( uint8_t dir_count = m.dcount[m.T1], dir_idx = m.INDX2;
         dir_count != 0;
         dir_count--, dir_idx++
       )
    {
        uint8_t move_dir = m.direct[dir_idx];

        // From position
        m.M2 = m.M1;

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
                            if( m.M2<=SQ_h1 || m.M2>=SQ_a8 )    // destination on 1st or 8th rank?
                                SET_PROMOTION(m.P2);            // set promotion flag

                            // Add single step move to move list
                            ADMOVE();

                            // Check Pawn moved flag, for double step
                            if( HAS_NOT_MOVED(m.P1) )
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
                            if( m.M2<=SQ_h1 || m.M2>=SQ_a8 )    // destination on 1st or 8th rank?
                                SET_PROMOTION(m.P2);            // set promotion flag
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
    callback_zargon_bridge(CB_ENPSNT);

    // Pawn position
    uint8_t idx = m.M1;

    // Add 10 for black
    if( IS_BLACK(m.P1) )
        idx += 10;

    // Check on en-passant capture rank
    if( idx<SQ_a5 || SQ_h5<idx )  // fifth rank a5-h5 ? So white captures black pawn
        return;  // not on fifth rank

    // Previous move must be first move for that piece
    if( !IS_FIRST_MOVE(m.MLPTRJ->flags) )
        return;

    // Get "to" position for previous move
    m.M4 = m.MLPTRJ->to;
    uint8_t piece = m.BOARDA[m.M4];
    m.P3 = piece;

    // Check that previous move was a pawn move
    piece &= 7;
    if( piece != PAWN )
        return;

    // Compare "to" position for previous and current moves
    int8_t diff = m.M4-m.M2;
    if( diff!=10 && diff!=-10) // should be one rank different
        return;

    // Set double move flag and add en-passant move in two steps
    SET_DOUBLE_MOVE(m.P2);
    ADMOVE();       // first step, move the pawn
    m.M3 = m.M1;    // save initial pawn position

    // Dummy pawn move, to effect capture, as second part of move
    m.M1 = m.M4;    // dummy pawn move "from"
    m.M2 = m.M4;    // dummy pawn move "to"
    m.P2 = m.P3;    // captured pawn
    ADMOVE();       // second step capture the pawn

    // Restore "from" position
    m.M1 = m.M3;
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
    callback_zargon_bridge(CB_ADJPTR);

    // Adjust list pointer to point at previous move
    m.MLLST -= 1;

    // Zero link
    m.MLLST->link_ptr = 0;
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
    callback_zargon_bridge(CB_CASTLE);

    // If king has moved return
    if( HAS_MOVED(m.P1) )
        return;

    // If king is in check return
    if( m.CKFLG != 0 )
        return;

    // Check both sides, kingside first
    for( uint8_t i=0; i<2; i++ )
    {

        // King rook is king+3, Queen rook is king-4
        m.M3 = (uint8_t)(i==0 ? m.M1+3 : m.M1-4);

        // Check that it is an unmoved rook
        uint8_t piece = m.BOARDA[m.M3];
        piece &= 0x7f;
        if( piece == ROOK )
        {

            // Step from king rook left towards king, queen rook right towards king
            uint8_t step = (uint8_t)(i==0 ? (-1) : 1);

            // Save start rook position
            uint8_t starting_rook_pos = m.M3;

            // Step from the rook to the king, checking each square between
            for(;;)
            {

                // Use M3 as a rover, from rook to king
                m.M3 += step;
                piece = m.BOARDA[m.M3];

                // Must be empty (this will always stop loop when we reach the king)
                if( piece != 0 )
                    break;

                // Must not be under attack (except for b1 and b8)
                if( m.M3 != SQ_b1 && m.M3 != SQ_b8 && ATTACK() )
                    break;
            }

            // If we didn't stop *BEFORE* reaching king, we have found a legal
            //  castling move, represent it as a double move, king then rook
            if( m.M3 == m.M1 )
            {

                // M1 is king "from" position, M2 is the king "to" position
                m.M2 = m.M3;
                m.M2 -= step;
                m.M2 -= step;
                m.P2 = 0;
                SET_DOUBLE_MOVE(m.P2);    // set double move flag
                ADMOVE();       // add to list

                // M1 is rook "from" position, M2 is rook "to" position, one step from king
                m.M1 = starting_rook_pos;
                m.M2 = m.M3 - step;
                m.P2 = 0;       // clear move flags
                ADMOVE();       // add to list
                ADJPTR();       // re-adjust move list pointer

                // Restore King position
                m.M1 = m.M3;
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
    callback_zargon_bridge(CB_ADMOVE);

    // Address of next location in move list
    ML *ml = m.MLNXT;

    // Check that we haven't run out of memory
    if( ml > (ML *)&m.MLEND )
    {

        // TODO - Maybe this is probably what was intended in the original code,
        //  certainly writing 0 to the *difference* of two pointers makes no sense
        m.MLNXT = 0;
        return;
    }

    // Address of previous move
    ML *previous_move = m.MLLST;

    // Save next as previous
    m.MLLST = ml;

    // Store as link address
    previous_move->link_ptr = ml;

    // If piece hasn't moved before set first move flag
    if( HAS_NOT_MOVED(m.P1) )
        SET_FIRST_MOVE(m.P2);

    // Now write move details
    ml->link_ptr = 0;           // store zero in link address
    ml->from  = m.M1;           // store "from" move position
    ml->to    = m.M2;           // store "to" move position
    ml->flags = m.P2;           // store move flags/capt. piece
    ml->val   = 0;              // store initial move value

    // Save next slot address
    m.MLNXT = ml + 1;
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
    callback_zargon_bridge(CB_GENMOV);

    // Test for King in check
    bool inchk = INCHK(m.COLOR);
    m.CKFLG = inchk;

    // Setup move list pointers
    ML *mlnxt    = m.MLNXT;             // addr of next avail list space
    probe_read(4);
    ML **mig_hl = (ML **)m.MLPTRI;      // ply list pointer index
    mig_hl++;                           // increment to next ply

    // Save move list pointer
    uint8_t *p = MIG_TO_PTR(mig_hl);
    *mig_hl = mlnxt;
    mig_hl++; 
    probe_write(0,(mig_t)mig_hl);
    m.MLPTRI = (mig_t)mig_hl;           // save new index
    m.MLLST  = (ML *)mig_hl;            // last pointer for chain init.

    // Loop through the board
    for( uint8_t pos=SQ_a1; pos<=SQ_h8; pos++ )
    {
        m.M1 = pos;
        uint8_t piece = m.BOARDA[pos];

        // If piece not empty or border
        if( piece!=0 && piece!=0xff )
        {
            m.P1 = piece;

            // If piece color is side to move, gen moves
            if( IS_SAME_COLOR(m.COLOR,piece) )
            {

                // MPIECE() now uses piece including colour is m.P1
                //  rather than recreating it. Not sure why original
                //  code didn't do that
                MPIECE();
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
	const uint8_t *p = &m.POSK[color?1:0];
	m.P1 = m.BOARDA[m.M3 = *p];
	m.T1 = m.P1 & 7;

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
    callback_zargon_bridge(CB_ATTACK);

    // Loop over the 16 directions in the direct[] table
    m.INDX2 = 0;
    const int8_t *dir_ptr = (int8_t *)m.direct;
    for( uint8_t dir_count=16; dir_count>0; dir_count-- )
    {
        int8_t dir = *dir_ptr++;
        uint8_t scan_count = 0;
        m.M2 = m.M3;

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
            uint8_t piece = m.T2;

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
                if( IS_WHITE(m.P2) )
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
            if( m.T1 == 7 )
            {
                // If called from POINTS(), save attacker in attack list
                ATKSAV(scan_count,dir);
            }

            // Else if piece is opposite color attacker found
            else if( (scan_count&0x20) != 0 )
            {
                return true;
            }

            // Keep stepping unless single step piece
            if( m.T2 == KING )
                break;
            if( m.T2 == KNIGHT )
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
    callback_zargon_bridge(CB_ATKSAV);

    // If there are pins, check whether this attacking piece is
    //  pinned and therefore not worthy of being saved to the list
    if( m.NPINS )
    {
        bool invalid_attacker = PNCK(m.NPINS,dir);
        if( invalid_attacker )
            return;
        scan_count = dir;   // reproduce bug in original Sargon
    }

    // Point at White or Black attack list
    uint8_t *p = IS_BLACK(m.P2) ? m.bact : m.wact;

    // First byte of attack list is count, increment it
    (*p)++;

    // Rest of list is 6 slots, one for each piece type
    uint8_t offset = m.P2 & 0x07;   // get piece type

    // If queen previously found use Queen slot
    //  (not sure why)
    if( (scan_count&0x80) != 0 )         // Queen found this scan ?
        offset = QUEEN;
    p += offset;    // point at piece type slot

    // Add the value of the piece as a nibble to attack list
    //  If both nibbles in slot non-zero overflow to next slot
    //  I think this means there should be room for 7 pieces
    //  not 6 in the list right?
    uint8_t *pvalue = &m.pvalue[0] - 1;
    uint8_t hi = *p&0xf0;
    uint8_t lo = *p&0x0f;
    if( lo!=0 && hi==0 )
    {
        *p = (pvalue[m.T2]<<4) + lo;
    }
    else
    {
        if( lo != 0 )
            p++;
        *p = (*p<<4) + (pvalue[m.T2]&0x0f);
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
    callback_zargon_bridge(CB_PNCK);

    // Loop over the pin list
    bool not_first_find=false;
    uint8_t *p = &m.PLISTA[0];  // Pin list address
    bool expired = false;
    while(!expired)
    {

        // Search list for position of piece (Z80_CPIR)
        bool found = false;
        while( !found )
        {
            found = (m.M2 == *p);
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
    callback_zargon_bridge(CB_PINFND);
    m.NPINS = 0;

    // Loop over 4 royal pieces
    for( uint8_t i=0; i<4; i++ )
    {
        uint8_t *p = i<2 ? &m.POSK[i] : &m.POSQ[i-2];

        // Check piece is on the board
        if( *p == 0 )
            continue;

        // Loop over all directions
        m.M3 = *p;              //  Save position as board index
        m.P1 = m.BOARDA[m.M3];
        m.INDX2 = 0;
        int8_t *y = &m.direct[m.INDX2];
        for( uint8_t dir_count=8; dir_count>0; dir_count-- )
        {
            int8_t dir = *y++;      // get direction of scan
            m.M2 = m.M3;            // get King/Queen position
            m.M4 = 0;               // clear pinned piece saved pos

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
                else if( path_result==PATH_SAME_COLOR && m.M4==0 )
                {

                    // Register a possible pin position
                    m.M4 = m.M2;
                    stepping= true;  // keep stepping
                }

                // Else piece of different colour and we have a potential pinned piece
                else if( path_result==PATH_OPPOSITE_COLOR && m.M4 != 0 )
                {
                    bool save_pinned_piece = false;

                    // Bishop pins piece to king or queen ?
                    if( dir_count >=5 && m.T2 == BISHOP )
                    {
                        save_pinned_piece = true;
                    }

                    // Rook pins piece to king or queen ?
                    else if( dir_count <5 && m.T2 == ROOK )
                    {
                        save_pinned_piece = true;
                    }

                    // Queen pins piece to king or queen ?
                    else if( m.T2 == QUEEN )
                    {

                        // If king, yes that's a pin
                        if( (m.P1&7) == KING )
                            save_pinned_piece = true;
                        else
                        {

                            // If queen do an attackers v defenders calculation
                            m.T1 = 7;
                            memset( m.wact, 0, sizeof(m.wact) );
                            memset( m.bact, 0, sizeof(m.bact) );
                            ATTACK();
                            int8_t defenders_minus_attackers;
                            int8_t *wact = (int8_t *)m.wact;
                            int8_t *bact = (int8_t *)m.bact;
                            if( IS_BLACK(m.P1) )   // Is queen black ?
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
                        m.plistd[m.NPINS]   = dir;
                        m.PLISTA[m.NPINS++] = m.M4;
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
void XCHNG( int8_t &points, int8_t &attacked_piece_val )
{
    callback_zargon_bridge(CB_XCHNG);
    bool black = IS_BLACK(m.P1);
    bool side_flag = true;

    uint8_t *wact = m.wact;
    uint8_t *bact = m.bact;

    uint8_t count_white = *wact;
    uint8_t count_black = *bact;

    // Init points lost count
    points = 0;

    // Get attacked piece value
    uint8_t piece_val = m.pvalue[m.T3-1];
    piece_val = piece_val+piece_val;
    attacked_piece_val = piece_val;

    // Retrieve first attacker
    uint8_t val = black ? NEXTAD( count_white, wact )
                        : NEXTAD( count_black, bact );

    // Return if none
    if( val==0 )
        return;

    // Loop through attackers and defenders
    for(;;)
    {

        // Save attacker value
        uint8_t save_val = val;

        // Get next defender
        val = black ? NEXTAD( count_black, bact )
                    : NEXTAD( count_white, wact );

        // Toggle
        black = !black;
        side_flag = !side_flag;

        // If have a defender and attacked < attacker
        bool attacked_lt_attacker = (piece_val<save_val);
        if( val!=0 && attacked_lt_attacker )
        {

            // Evaluate this as special case
            for(;;)
            {

                // Defender less than attacker ?
                if( save_val>val )
                    return; //  Yes - return

                // Get next attacker
                val = black ? NEXTAD( count_black, bact )
                            : NEXTAD( count_white, wact );
                if( val == 0 )
                    return; // Return if none

                // Save attacker value
                save_val = val;

                //  Get next defender
                val = black ? NEXTAD( count_white, wact )
                            : NEXTAD( count_black, bact );
                if( val == 0 )
                    break; // End subloop if none
            }
        }

        // Get value of attacked piece
        int8_t temp = (int8_t)piece_val;

        //  Attacker or defender ?
        if( side_flag )
            temp = 0-temp;  // negate value for attacker

        // Accumulate total points lost
        points += temp;

        // Save total
        if( val == 0 )
            return; // return at end of list

        // Prev attacker becomes defender
        piece_val = save_val;
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
    callback_zargon_bridge(CB_NEXTAD);
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
        val = *p &0x0f;
        *p = *p >> 4;

        // Double it
        val = val+val;
        p--;    // decrement list pointer to retrieve second
                // nibble (if present) next
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
    callback_zargon_bridge(CB_POINTS);
    int8_t *wact = (int8_t *)m.wact;
    int8_t *bact = (int8_t *)m.bact;

    // Init points algorithm variables
    m.MTRL  = 0;
    m.BRDC  = 0;
    m.PTSL  = 0;
    m.PTSW1 = 0;
    m.PTSW2 = 0;
    m.PTSCK = 0;

    // Attacked piece type 1-6 for pawn to king. Use 7 as a special sentinel
    //  value to indicate to ATTACK() it's being called by POINTS()
    m.T1 = 7;

    // Loop around board
    for( m.M3=SQ_a1; m.M3<=SQ_h8; m.M3++ )
    {
        uint8_t piece = m.BOARDA[m.M3];            //  Save as board index

        //  Off board edge ?
        if( piece == 0xff )
            continue;

        // Piece with flags
        m.P1 = piece;

        // Piece without flags
        piece &= 7;
        m.T3 = piece;

        // Pawn (or empty) ?
        if( piece < KNIGHT )
            ;

        // Bishop or knight
        else if( piece < ROOK )
        {

            // If bishop or knight has not moved, 2 point penalty
            if( HAS_NOT_MOVED(m.P1) )
            {
                int8_t bonus = -2;
                if( IS_BLACK(m.P1) )
                    bonus = 2;  // 2 point penalty for Black
                m.BRDC += bonus;
            }
        }

        // King
        else if( piece == KING )
        {

            // 6 point bonus for castling
            if( IS_CASTLED(m.P1) )
            {
                int8_t bonus = +6;
                if( IS_BLACK(m.P1) )
                    bonus = -6;     // 6 point bonus for Black
                m.BRDC += bonus;
            }

            // Else if king has not castled but has moved, 2 point penalty
            else if( HAS_MOVED(m.P1) )
            {
                int8_t bonus = -2;
                if( IS_BLACK(m.P1) )
                    bonus = 2;  // 2 point penalty for Black
                m.BRDC += bonus;
            }
        }

        // Rook or queen early in game
        else if( m.MOVENO < 7 )
        {

            // If rook or queen early in the game HAS moved, 2 point penalty
            if( HAS_MOVED(m.P1) )
            {
                int8_t bonus = -2;
                if( IS_BLACK(m.P1) )
                    bonus = 2;  // 2 point penalty for Black
                m.BRDC += bonus;
            }
        }

        // Zero out attack lists
        memset( m.wact, 0, sizeof(m.wact) );
        memset( m.bact, 0, sizeof(m.bact) );

        // Build attack list for square
        ATTACK();

        // Get (White attacker count - Black attacker count) and accumulate
        //  into board control score
        m.BRDC += (*wact-*bact);

        // If square is empty continue loop
        if( m.P1 == 0 )            //  Get piece on current square
            continue;

        // Evaluate exchange, if any
        int8_t points;
        int8_t attacked_piece_val;
        XCHNG( points, attacked_piece_val );

        // If piece is nominally lost
        if( points != 0 )
        {

            //  Deduct half a Pawn value
            attacked_piece_val--;

            // If color of piece under attack matches color of side just moved
            if( IS_SAME_COLOR(m.COLOR,m.P1) )
            {

                // If points lost >= previous max points lost
                if( points >= m.PTSL )
                {

                    // Store new value as max lost
                    m.PTSL = points;

                    // Is the lost piece the one moving ?
                    if( m.M3 == m.MLPTRJ->to )
                        m.PTSCK = m.M3; // yes, save position as a flag
                }
            }

            // Else color of piece under attack doesn't match color of side just moved
            else
            {

                // Points won
                int8_t temp = points;

                // If points won >= previous maximum points won
                if( points >= m.PTSW1 )
                {

                    // Update max points won (but save previous value)
                    temp = m.PTSW1;
                    m.PTSW1 = points;
                }

                // Update 2nd max points won with old value of max points won
                if( temp >= m.PTSW2 )
                    m.PTSW2 = temp;
            }
        }

        // Accumulate total material
        if( IS_BLACK(m.P1) )
            attacked_piece_val = 0-attacked_piece_val;    // negate piece value if Black
        m.MTRL += attacked_piece_val;
    }

    // If moving piece lost
    if( m.PTSCK != 0 )
    {

        // Shift two element max points stack
        m.PTSW1 = m.PTSW2;
        m.PTSW2 = 0;
    }

    // Adjust max points lost (decrement if non-zero)
    int8_t ptsl = m.PTSL;
    if( ptsl != 0 )
        ptsl--;

    // Get max points won
    int8_t ptsw = m.PTSW1;

    // Experiment: Both PTSW1 and PTSW2 are small integers in the 0-20 (ish) range
    //  See commit bd4b1b3db9ecf5c529325ceb49be2df7ba09414e for the scatter()
    //  source code we used to probe that (and other "Experiment:" results below)

    // Adjust max points won if both 1st and 2nd max points won are non-zero
    //  Value is;
    //      0 if 1st max points won is 0
    //      else 2nd max points won
    //      except then if not zero adjusted val = (val-1)/2
    // I don't currently understand the reasoning behind this
    if( ptsw != 0 )
    {
        ptsw = m.PTSW2;
        if( ptsw != 0 )
        {
            ptsw--;

            // Experiment: ptsw is also a small integer in the 0-20 range
            ptsw = ptsw/2;
        }
    }

    // Points won net = points won - points lost
    ptsw -= ptsl;

    // Color of side just moved
    if( IS_BLACK(m.COLOR) )
        ptsw = 0-ptsw;  // negate for black

    // Add net material on board
    ptsw += m.MTRL;

    // Subtract material at ply 0
    ptsw -= m.MV0;

    // Limit to +/- 30
    int8_t points = LIMIT(30,ptsw);

    // Board control points minus board control at ply zero
    int8_t bcp = m.BRDC - m.BC0;

    // If moving piece lost, set board control points to zero
    if( m.PTSCK != 0 )
        bcp = 0;

    // Limit to +/- 6
    bcp = LIMIT(6,bcp);

    // Add material*4
    points = points*4 + bcp;

    // Color of side just moved
    if( IS_WHITE(m.COLOR) )
        points = 0-points;  // negate for white

    // Rescale score (neutral = 0x80)
    // Experiment: points here is a balanced signed value, so 0 = even +126=very good, -126=very bad
    points += 0x80;
    callback_end_of_points(points);

    // Save score value
    // Experiment: after rescale -126->2 ... -1->127, 0->128, 1->129 ... 126->254
    m.VALM = points;

    // Save score value to move pointed to by current move ptr
    m.MLPTRJ->val = m.VALM;
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
    callback_zargon_bridge(CB_MOVE);

    //  Load move list pointer
    ML *p = (ML *)m.MLPTRJ;

    // Loop for possible double move
    for(;;)
    {

        // "From" position
        m.M1 = p->from;

        // "To" position
        m.M2 = p->to;

        // Get captured piece plus flags
        uint8_t captured_piece_plus_flags = p->flags;

        // Get piece moved from "from" pos board index
        uint8_t piece = m.BOARDA[m.M1];

        // Promote pawn if required
        if( IS_PROMOTION(captured_piece_plus_flags) )
        {
            piece |= 4; // change pawn to a queen 0x01->0x05 or 0x81->0x85
        }

        // Update queen position if required
        else if( (piece&7) == QUEEN )
        {
            uint8_t *q = &m.POSQ[0];    // addr of saved queen position
            if( IS_BLACK(piece) )       // is queen black ?
                q++;                    // increment to black queen pos
            *q = m.M2;                  // set new queen position
        }

        // Update king position if required
        else if( (piece&7) == KING )
        {
            uint8_t *q = &m.POSK[0];    // addr of saved king position
            if( IS_DOUBLE_MOVE(captured_piece_plus_flags) )  // castling ?
                SET_CASTLED(piece);
            if( IS_BLACK(piece ) )      // is king black ?
                q++;                    // increment to black king pos
            *q = m.M2;                  // set new king position
        }

        // Set piece moved flag
        SET_MOVED(piece);

        // Insert piece at new position
        m.BOARDA[m.M2] = piece;

        // Empty previous position
        m.BOARDA[m.M1] = 0;

        // Double move ?
        if( IS_DOUBLE_MOVE(captured_piece_plus_flags) )
        {
            p = (ML *)m.MLPTRJ;  // reload move list pointer
            p++;                 // go to second move of double move
        }
        else
        {

            // Was captured piece a queen
            if( (captured_piece_plus_flags & 7) == QUEEN )
            {

                // Clear queen royalty position after capture
                uint8_t *q = &m.POSQ[0];
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
    callback_zargon_bridge(CB_UNMOVE);

    //  Load move list pointer
    ML *p = (ML *) m.MLPTRJ;

    // Loop for possible double move
    for(;;)
    {

        // "From" position
        m.M1 = p->from;

        // "To" position
        m.M2 = p->to;

        // Get captured piece plus flags
        uint8_t captured_piece_plus_flags = p->flags;

        // Get piece moved from "to" pos board index
        uint8_t piece = m.BOARDA[m.M2];

        // Unpromote pawn if required
        if( IS_PROMOTION(captured_piece_plus_flags) )
        {
            piece &= 0xfb; // change queen to a pawn 0x05->0x01 or 0x85->0x81
        }

        // Update queen position if required
        else if( (piece&7) == QUEEN )
        {
            uint8_t *q = &m.POSQ[0];    // addr of saved queen position
            if( IS_BLACK(piece) )       // is queen black ?
                q++;                    // increment to black queen pos
            *q = m.M1;                  // set previous queen position
        }

        // Update king position if required
        else if( (piece&7) == KING )
        {
            uint8_t *q = &m.POSK[0];    // addr of saved king position
            if( IS_DOUBLE_MOVE(captured_piece_plus_flags) )  // castling ?
                CLR_CASTLED(piece);     // clear castled flag
            if( IS_BLACK(piece) )       // is king black ?
                q++;                    // increment to black king pos
            *q = m.M1;                  // set previous king position
        }

        // If this is the first move for piece, clear piece moved flag
        if( IS_FIRST_MOVE(captured_piece_plus_flags) )
        {
            CLR_MOVED(piece);
        }

        // Return piece to previous board position
        m.BOARDA[m.M1] = piece;

        // Restore captured piece with cleared flags
        m.BOARDA[m.M2] = captured_piece_plus_flags & 0x8f;

        // Double move ?
        if( IS_DOUBLE_MOVE(captured_piece_plus_flags) )
        {
            p = (ML *)m.MLPTRJ;  // reload move list pointer
            p++;                 // go to second move of double move
        }
        else
        {

            // Was captured piece a queen
            if( (captured_piece_plus_flags & 7) == QUEEN )
            {

                // Restore queen royalty position after capture
                uint8_t *q = &m.POSQ[0];
                if( IS_BLACK(captured_piece_plus_flags) )  // is queen black ?
                    q++; // increment to black queen pos
                *q = m.M2;
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

void SORTM()
{
    callback_zargon_bridge(CB_SORTM);

    // Init working pointers
    probe_read(5);
    ML *mig_bc = (ML *)m.MLPTRI;       //  Move list begin pointer
    ML *mig_de = 0;

    // Loop
    for(;;)
    {
        ML *mig_hl = mig_bc;

        // Get link to next move
        mig_bc = mig_hl->link_ptr;

        // Make linked list
        mig_hl->link_ptr = mig_de;

        // Return if end of list
        if( mig_bc == 0 )
            return;

        // Save list pointer
        m.MLPTRJ = mig_bc;

        // Evaluate move
        EVAL();
        probe_read(6);
        mig_hl = (ML *)m.MLPTRI;    // beginning of move list
        mig_bc = m.MLPTRJ;          // restore list pointer

        // Next move loop
        for(;;)
        {

            // Get next move
            mig_de = mig_hl->link_ptr;

            // End of list ?
            if( mig_de == 0 )
                break;

            // Compare value to list value
            ML *ml = mig_de;
            if( m.VALM < ml->val )
                break;

            // Swap pointers if value not less than list value
            mig_hl = mig_de;
        }

        // Link new move into list
        mig_hl->link_ptr = mig_bc;
    }
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
    callback_zargon_bridge(CB_EVAL);

    // Make move on the board array
    MOVE();

    // Determine if move is legal
    bool inchk = INCHK(m.COLOR);

    // Score of zero for illegal move
    if( inchk )
        m.VALM = 0;
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
    callback_zargon_bridge(CB_FNDMOV);

    // Book move ?
    if( m.MOVENO == 1 )
    {
        BOOK();
        return;
    }

    //  Initialize ply number, best move
    m.NPLY  = 0;
    m.BESTM = 0;

    //  Initialize ply list pointers
    ML *ml  = (&m.MLIST[0]);
    m.MLNXT = ml;
    uint8_t *q = (uint8_t *)(&m.PLYIX);
    q -= sizeof(mig_t);
    probe_write(1,(mig_t)q);
    m.MLPTRI = PTR_TO_MIG(q);

    // Initialise color
    m.COLOR = m.KOLOR;

    // Initialize score index and clear table
    uint8_t *p = (uint8_t *)(&m.SCORE);
    m.SCRIX = p;
    for( int i=0; i<m.PLYMAX+2; i++ )
        *p++ = 0;

    // Init board control and material
    m.BC0 = 0;
    m.MV0 = 0;

    //  Compile pin list
    PINFND();

    //  Evaluate board at ply 0
    POINTS();

    // Update board control and material
    m.BC0 = m.BRDC;
    m.MV0 = m.MTRL;

    // Loop
    bool genmove_needed = true;
    for(;;)
    {

        // Generate moves
        if( genmove_needed )
        {
            genmove_needed = false;
            m.NPLY++;                   //  Increment ply count
            m.MATEF = 0;                //  Initialize mate flag
            GENMOV();                   //  Generate list of moves
            callback_after_genmov();
            if( m.PLYMAX > m.NPLY )
                SORTM();                    // not at max ply, so call sort
            probe_read(7);
            m.MLPTRJ = (ML *)m.MLPTRI;      // last move pointer = load ply index pointer
        }

        // Traverse move list
        uint8_t score = 0;
        int8_t iscore = 0;
        ML *ml = m.MLPTRJ;   // load last move pointer

        //  End of move list ?
        bool points_needed = false;
        if( ml->link_ptr != 0 )
        {
            m.MLPTRJ = ml->link_ptr;        // save current move pointer
            probe_read(8);
            ml = (ML *)m.MLPTRI;            // save in ply pointer list
            ml->link_ptr = m.MLPTRJ;

            // Max depth reached ?
            if( m.NPLY >= m.PLYMAX )
            {

                //  Execute move on board array
                MOVE();

                // Check move legality
                bool inchk = INCHK(m.COLOR);             //  Check for legal move

                //  If move not legal, restore board position and continue looping
                if( inchk )       //  Is move legal
                {
                    UNMOVE();
                    continue;
                }

                // If beyond max ply or king not in check time to calculate points
                if( m.NPLY != m.PLYMAX )
                    points_needed = true;
                inchk = INCHK(m.COLOR^0x80);
                if( !inchk )
                    points_needed = true;
            }
            else
            {

                // Load move pointer
                ml = m.MLPTRJ;

                // If score is zero (illegal move) continue looping
                if( ml->val == 0 )
                    continue;

                // Execute move on board array
                MOVE();
            }

            // points_needed flag avoids a goto from where the flag is set
            if( !points_needed )
            {

                // Toggle color
                TOGGLE(m.COLOR);

                // If new colour is white increment move number
                if( IS_WHITE(m.COLOR) )
                    m.MOVENO++;

                // Load score table pointer
                p = m.SCRIX;

                //  Get score two plys above
                score = *p;
                p++;                   // increment to current ply
                p++;

                // Save score as initial value
                *p = score;
                p--;                        // decrement pointer
                m.SCRIX = p;                // save it
                genmove_needed = true;      // go to top of loop
                continue;
            }
        }


        // Common case, don't evaluate points yet, not mate or stalemate
        score = 0;
        if( !points_needed && m.MATEF!=0 )
        {
            if( m.NPLY == 1 )       // at top of tree ?
                return;             // yes
            ASCEND();               // ascend one ply in tree
            p = m.SCRIX;            // load score table pointer
            p++;                    // increment to current ply
            p++;                    //
            score = *p;             // get score
            p--;                    // restore pointer
            p--;                    //
        }

        // Time to evaluate points ?
        else if( points_needed )
        {

            //  Compile pin list
            PINFND();

            // Evaluate move
            POINTS();

            // Restore board position
            UNMOVE();
            score = m.VALM;             // get value of move
            m.MATEF |= 1;               // set mate flag
            p = m.SCRIX;                // load score table pointer
        }

        // Else if terminal position ?
        else if( m.MATEF == 0 ) // checkmate or stalemate ?
        {
            score = 0x80;       // stalemate score
            if( m.CKFLG != 0 )  // test check flag
            {
                score = 0xff;   // if in check, then checkmate score
                m.PMATE= m.MOVENO;
            }
            m.MATEF |= 1;               // set mate flag
            p = m.SCRIX;                // load score table pointer
        }

        // Alpa Beta cutoff ?
        callback_alpha_beta_cutoff( score, p );
        if( score <= *p )  // compare to score 2 ply above
        {
            ASCEND();  // ascend one ply in tree
            continue;
        }

        // Negate score
        iscore = (int8_t)score;
        iscore = 0-iscore;
        score = (uint8_t) iscore;

        // Compare to score 1 ply above
        p++;
        bool score_greater = (*p < score);
        callback_no_best_move( score, p );
        if( !score_greater )
            continue;   // continue unless score is greater
        *p = score;     // save as new score 1 ply above
        callback_yes_best_move();

        // At top of tree ?
        if( m.NPLY != 1 )
            continue;   // no - loop

        // Set best move pointer = current move pointer
        m.BESTM = m.MLPTRJ;

        // Point to best move score
        p = 1 + (uint8_t *)&m.SCORE;

        //  Was it a checkmate ?
        if( *p != 0xff )
            continue;   // no - loop

        // Subtract 2 from maximum ply number
        m.PLYMAX -= 2;

        // Calculate checkmate move number and return
        if( IS_BLACK(m.KOLOR) )  // if computer's color is black decrement
            m.PMATE--;
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
    callback_zargon_bridge(CB_ASCEND);

    //  Toggle color
    TOGGLE(m.COLOR);

    // If new colour is Black, decrement move number
    if( IS_BLACK(m.COLOR) )
        m.MOVENO--;

    // Decrement score table index
    m.SCRIX--;

    // Decrement ply counter
    m.NPLY--;

    // Get ply list pointer
    probe_read(9);
    ML **pp = (ML **)m.MLPTRI;

    // Decrement by ptr size
    pp--;

    // Update move list avail ptr
    m.MLNXT = *pp;

    // Get ptr to next move to undo
    pp--;
    m.MLPTRJ = *pp;

    // Save new ply list pointer
    probe_write(2,(mig_t)pp);
    m.MLPTRI = (mig_t)pp;

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
    uint8_t *p = (uint8_t *)&m.SCORE;
    *(p+1) = 0;

    // Point to 4 individual first move book moves, in order 1.e4, 1.d4, 1...e5, 1...d5
    //  For each move only the bytes at MLFRP, MLTOP, MLFG respectively (from, to, flags)
    //  are stored (so 3 bytes per move)
    p = &m.BMOVES[0];

    // Adjust p so that MLFRP points at "from" (then "to", then "flags")
    p -= offsetof(ML,from);

    // Named pointers to each of the four book moves;
    uint8_t *e4=p, *d4=p+3, *e5=p+6, *d5=p+9;

    // If computer is White
    if( m.KOLOR == 0 )
    {

        // Get a random number
        uint8_t rand;
        callback_ldar(rand);

        // Play e4 or d4 randomly
        if( (rand&1) == 0 )
            m.BESTM = (ML *)e4;
        else
            m.BESTM = (ML *)d4;
    }

    // Else computer is Black
    else
    {
        uint8_t from = m.MLPTRJ->from;   // get "from" square for opponent's 1st move

        // Play d5 after all White first moves except a,b,c or e pawn moves
        bool play_d5 = true;
        if( from==SQ_a2 || from==SQ_b2 || from==SQ_c2 || from==SQ_e2 )
            play_d5 = false;
        if( play_d5 )
            m.BESTM = (ML *)d5;
        else
            m.BESTM = (ML *)e5;
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
    callback_zargon_bridge(CB_AFTER_FNDMOV);

    // Save best move
    m.MLPTRJ = m.BESTM;

    // Make the move
    MOVE();

    // Toggle color
    TOGGLE(m.COLOR);

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
    ML *mlptrj_save = m.MLPTRJ;

    // Set user color to opposite of computer's color
    m.COLOR = IS_WHITE(m.KOLOR) ? BLACK : WHITE;

    // Load move list index
    uint8_t *p = (uint8_t *)(&m.PLYIX[0]);
    p -= sizeof(mig_t);
    probe_write(3,(mig_t)p);
    m.MLPTRI = PTR_TO_MIG(p);

    // Next available list pointer
    ML *ml = (ML *)m.MLIST;
    ml += 1024/sizeof(ML);      // FIXME this is a mystery
    m.MLNXT = ml;

    // Generate opponent's moves
    GENMOV();

    // Search for move, "From" position (offset 0) "To" position (offset 1)
    uint8_t *q = (uint8_t *)&m.MVEMSG;
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
            m.MLPTRJ = mlptrj_save;
            return ok;  // false
        }
    }

    //  Save opponents move pointer
    m.MLPTRJ = ml;

    //  Make move on board array
    MOVE();

    //  Was it a legal move ?
    bool inchk = INCHK(m.COLOR);
    ok = !inchk;
    if( !ok )
    {

        // Not legal so Undo move on board array
        UNMOVE();

        //  Restore last move pointer
        m.MLPTRJ = mlptrj_save;
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
    m.POSK[0] = 0;
    m.POSK[1] = 0;
    m.POSQ[0] = 0;
    m.POSQ[1] = 0;

    // Board idx = a1 to h8 inclusive
    for( int idx=SQ_a1; idx<=SQ_h8; idx++ )
    {
        uint8_t piece = m.BOARDA[idx];

        // Update king or queen position if king or queen found
        if( (piece&7) == KING )
        {
            m.POSK[IS_BLACK(piece)?1:0] = idx;
        }
        else if( (piece&7) == QUEEN )
        {
            m.POSQ[IS_BLACK(piece)?1:0] = idx;
        }
    }

    // leave M1 at h8, not padding beyond h8 [probably not needed]
    m.M1 = SQ_h8;
}

//
//  Omit some more Z80 user interface stuff, functions
//  DSPBRD, BSETUP, INSPCE, CONVRT, DIVIDE, MLTPLY, BLNKER, EXECMV
//

static bool have_written=false;
static mig_t previous;
static uint64_t trace;
static int tag_most_recent_write;

struct STAT
{
    bool read;
    uint64_t hits;
    uint64_t tag_most_recent_write[10];
    uint64_t nil_hits;
    uint64_t list_hits;
    uint64_t plyix_hits;
    uint64_t other_hits;
};

static STAT stats[10];

void probe_write( int tag, mig_t val )
{
    return;
    if( tag > 9 )
    {
        printf( "Unexpected tag\n" );
        exit(0);
    }
    if( !have_written )
        have_written = true;
    else
    {
        if( previous != m.MLPTRI )
            printf( "*** %llu PROBE_WRITE() *** %d %p %p\n", ++trace, tag, previous, m.MLPTRI );
    }
    STAT *p = &stats[tag];
    p->read = false;
    p->hits++;
    if( val == 0 )
        p->nil_hits++;
    bool is_list_ptr = (val>= (mig_t)m.MLIST);
    bool is_plyix_ptr = (val>= (mig_t)&m.PLYIX[-1] && val<(mig_t)&m.PLYIX[40]);
    if( is_list_ptr )
        p->list_hits++;
    else if( is_plyix_ptr )
        p->plyix_hits++;
    else
        p->other_hits++;
    tag_most_recent_write = tag;
    previous = val;
}

void probe_read( int tag )
{
    return;
    if( tag > 9 )
    {
        printf( "Unexpected tag\n" );
        exit(0);
    }
    if( have_written )
    {
        if( previous != m.MLPTRI )
            printf( "*** %llu PROBE_READ() *** %d %p %p\n", ++trace, tag, previous, m.MLPTRI );
    }
    STAT *p = &stats[tag];
    p->read = true;
    p->hits++;
    mig_t val = m.MLPTRI;
    if( val == 0 )
        p->nil_hits++;
    bool is_list_ptr = (val>= (mig_t)m.MLIST);
    bool is_plyix_ptr = (val>= (mig_t)&m.PLYIX[-1] && val<(mig_t)&m.PLYIX[40]);
    if( is_list_ptr )
        p->list_hits++;
    else if( is_plyix_ptr )
        p->plyix_hits++;
    else
        p->other_hits++;
    (p->tag_most_recent_write[tag_most_recent_write]) ++;
}

void probe_report()
{
    return;
    for( int tag=0; tag<10; tag++ )
    {
        STAT *p = &stats[tag];
        printf( "%d %s\n", tag, p->read?"Read":"Write" );
        printf( "  %llu hits\n", p->hits );
        for( int i=0; i<10; i++ )
        {
            if( p->tag_most_recent_write[i] > 0 )
                printf("    most recent write tag %d: %llu hits\n", i, p->tag_most_recent_write[i] );
        }
        printf( "  %llu nil hits\n", p->nil_hits );
        printf( "  %llu list hits \n", p->list_hits );
        printf( "  %llu plyix hits \n", p->plyix_hits );
        printf( "  %llu other hits\n", p->other_hits );
    }
}

