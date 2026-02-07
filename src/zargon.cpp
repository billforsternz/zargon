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
#include "z80_cpu.h"
#include "z80_opcodes.h"  // include last, uses aggressive macros
                          //  that might upset other .h files

// Emulate Z80 CPU and opcodes
z80_cpu     gbl_z80_cpu;

// Up to 64K of emulated memory
static emulated_memory m;
emulated_memory *zargon_get_ptr_emulated_memory() {return &m;}

// Regenerate defines for sargon-asm-interface.h as needed
zargon_data_defs_check_and_regen regen;

// Transitional (maybe) pointer manipulation macros
//  Note: a "bin" for our purposes is the binary representation of
//        a Sargon pointer, it is a uint16_t offset from the start
//        of Sargon's memory
#define BIN_TO_PTR(bin)     ((uint8_t*) ((bin) + ((uint8_t*)(&m))))
#define PTR_TO_BIN(p)       (uint16_t)(((uint8_t*)(p)) - (uint8_t*)(&m))
#define HI(bin)             ((bin>>8)&0xff)
#define LO(bin)             (bin&0xff)
inline uint16_t RD_BIN( const uint8_t *p) { uint16_t temp=*(p+1); return (temp<<8)|*p; }
inline void     WR_BIN( uint8_t *p, uint16_t bin ) { *p = LO(bin); *(p+1) = HI(bin); }

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

//
// 1) TABLES
//

// Data section now actually defined in zargon.h, for now at least
//  we reproduce it here for comparison to original Sargon Z80 code
#ifndef ZARGON_H_INCLUDED
//***********************************************************
// TABLES SECTION
//***********************************************************
//
//
#define TBASE 0x100     // The following tables begin at this
                        //  low but non-zero page boundary in
                        //  in our 64K emulated memory
struct emulated_memory {
uint8_t padding[TBASE];

//There are multiple tables used for fast table look ups
//that are declared relative to TBASE. In each case there
//is a table (say DIRECT) and one or more variables that
//index into the table (say INDX2). The table is declared
//as a relative offset from the TBASE like this;
//
//DIRECT = .-TBASE  ;In this . is the current location
//                  ;($ rather than . is used in most assemblers)
//
//The index variable is declared as;
//INDX2    .WORD TBASE
//
//TBASE itself is page aligned, for example TBASE = 100h
//Although 2 bytes are allocated for INDX2 the most significant
//never changes (so in our example it's 01h). If we want
//to index 5 bytes into DIRECT we set the low byte of INDX2
//to 5 (now INDX2 = 105h) and load IDX2 into an index
//register. The following sequence loads register C with
//the 5th byte of the DIRECT table (Z80 mnemonics)
//        LD      A,5
//        LD      [INDX2],A
//        LD      IY,[INDX2]
//        LD      C,[IY+DIRECT]
//
//It's a bit like the little known C trick where array[5]
//can also be written as 5[array].
//
//The Z80 indexed addressing mode uses a signed 8 bit
//displacement offset (here DIRECT) in the range -128
//to 127. Sargon needs most of this range, which explains
//why DIRECT is allocated 80h bytes after start and 80h
//bytes *before* TBASE, this arrangement sets the DIRECT
//displacement to be -80h bytes (-128 bytes). After the 24
//byte DIRECT table comes the DPOINT table. So the DPOINT
//displacement is -128 + 24 = -104. The final tables have
//positive displacements.
//
//The negative displacements are not necessary in X86 where
//the equivalent mov reg,[di+offset] indexed addressing
//is not limited to 8 bit offsets, so in the X86 port we
//put the first table DIRECT at the same address as TBASE,
//a more natural arrangement I am sure you'll agree.
//
//In general it seems Sargon doesn't want memory allocated
//in the first page of memory, so we start TBASE at 100h not
//at 0h. One reason is that Sargon extensively uses a trick
//to test for a NULL pointer; it tests whether the hi byte of
//a pointer == 0 considers this as a equivalent to testing
//whether the whole pointer == 0 (works as long as pointers
//never point to page 0).
//
//Also there is an apparent bug in Sargon, such that MLPTRJ
//is left at 0 for the root node and the MLVAL for that root
//node is therefore written to memory at offset 5 from 0 (so
//in page 0). It's a bit wasteful to waste a whole 256 byte
//page for this, but it is compatible with the goal of making
//as few changes as possible to the inner heart of Sargon.
//In the X86 port we lock the uninitialised MLPTRJ bug down
//so MLPTRJ is always set to zero and rendering the bug
//harmless (search for MLPTRJ to find the relevant code).
//
//**********************************************************
// DIRECT  --  Direction Table.  Used to determine the dir-
//             ection of movement of each piece.
//***********************************************************
#define DIRECT (addr(direct)-TBASE)
int8_t direct[24] = {
    +9,+11,-11,-9,
    +10,-10,+1,-1,
    -21,-12,+8,+19,
    +21,+12,-8,-19,
    +10,+10,+11,+9,
    -10,-10,-11,-9
};
//***********************************************************
// DPOINT  --  Direction Table Pointer. Used to determine
//             where to begin in the direction table for any
//             given piece.
//***********************************************************
#define DPOINT (addr(dpoint)-TBASE)
uint8_t dpoint[7] = {
    20,16,8,0,4,0,0
};

//***********************************************************
// DCOUNT  --  Direction Table Counter. Used to determine
//             the number of directions of movement for any
//             given piece.
//***********************************************************
#define DCOUNT (addr(dcount)-TBASE)
uint8_t dcount[7] = {
    4,4,8,4,4,8,8
};

//***********************************************************
// PVALUE  --  Point Value. Gives the point value of each
//             piece, or the worth of each piece.
//***********************************************************
#define PVALUE (addr(pvalue)-TBASE-1)  //-1 because PAWN is 1 not 0
uint8_t pvalue[6] = {
    1,3,3,5,9,10
};

//***********************************************************
// PIECES  --  The initial arrangement of the first rank of
//             pieces on the board. Use to set up the board
//             for the start of the game.
//***********************************************************
#define PIECES  (addr(pieces)-TBASE)
uint8_t pieces[8] = {
    4,2,3,5,6,3,2,4
};

//***********************************************************
// BOARD   --  Board Array.  Used to hold the current position
//             of the board during play. The board itself
//             looks like:
//             FFFFFFFFFFFFFFFFFFFF
//             FFFFFFFFFFFFFFFFFFFF
//             FF0402030506030204FF
//             FF0101010101010101FF
//             FF0000000000000000FF
//             FF0000000000000000FF
//             FF0000000000000060FF
//             FF0000000000000000FF
//             FF8181818181818181FF
//             FF8482838586838284FF
//             FFFFFFFFFFFFFFFFFFFF
//             FFFFFFFFFFFFFFFFFFFF
//             The values of FF form the border of the
//             board, and are used to indicate when a piece
//             moves off the board. The individual bits of
//             the other bytes in the board array are as
//             follows:
//             Bit 7 -- Color of the piece
//                     1 -- Black
//                     0 -- White
//             Bit 6 -- Not used
//             Bit 5 -- Not used
//             Bit 4 --Castle flag for Kings only
//             Bit 3 -- Piece has moved flag
//             Bits 2-0 Piece type
//                     1 -- Pawn
//                     2 -- Knight
//                     3 -- Bishop
//                     4 -- Rook
//                     5 -- Queen
//                     6 -- King
//                     7 -- Not used
//                     0 -- Empty Square
//***********************************************************
#define BOARD (addr(BOARDA)-TBASE)
uint8_t BOARDA[120];

//***********************************************************
// ATKLIST -- Attack List. A two part array, the first
//            half for white and the second half for black.
//            It is used to hold the attackers of any given
//            square in the order of their value.
//
// WACT   --  White Attack Count. This is the first
//            byte of the array and tells how many pieces are
//            in the white portion of the attack list.
//
// BACT   --  Black Attack Count. This is the eighth byte of
//            the array and does the same for black.
//***********************************************************
#define WACT addr(ATKLST)
#define BACT (addr(ATKLST)+7)
union
{
    uint16_t    ATKLST[7];
        uint8_t     wact[7];
/*    struct wact_bact
    {
        uint8_t     wact[7];
        uint8_t     bact[7];
    }; */
};

//***********************************************************
// PLIST   --  Pinned Piece Array. This is a two part array.
//             PLISTA contains the pinned piece position.
//             PLISTD contains the direction from the pinned
//             piece to the attacker.
//***********************************************************
#define PLIST (addr(PLISTA)-TBASE-1)    ///TODO -1 why?
#define PLISTD (PLIST+10)
uint8_t     PLISTA[10];     // pinned pieces
uint8_t     plistd[10];     // corresponding directions

//***********************************************************
// POSK    --  Position of Kings. A two byte area, the first
//             byte of which hold the position of the white
//             king and the second holding the position of
//             the black king.
//
// POSQ    --  Position of Queens. Like POSK,but for queens.
//***********************************************************
uint8_t     POSK[2] = {
    25,95
};
uint8_t     POSQ[2] = {
    24,94
};
int8_t padding2 = -1;

//***********************************************************
// SCORE   --  Score Array. Used during Alpha-Beta pruning to
//             hold the scores at each ply. It includes two
//             "dummy" entries for ply -1 and ply 0.
//***********************************************************
uint8_t padding3[44];
uint16_t    SCORE[20] = {
    0,0,0,0,0,0,0,0,0,0,                // Z80 max 6 ply
    0,0,0,0,0,0,0,0,0,0                 // x86 max 20 ply
};

//***********************************************************
// PLYIX   --  Ply Table. Contains pairs of pointers, a pair
//             for each ply. The first pointer points to the
//             top of the list of possible moves at that ply.
//             The second pointer points to which move in the
//             list is the one currently being considered.
//***********************************************************
uint8_t padding4[2];
uint16_t    PLYIX[20] = {
    0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0
};

//
// 2) PTRS to TABLES
//

//***********************************************************
// 2) TABLE INDICES SECTION
//
// M1-M4   --  Working indices used to index into
//             the board array.
//
// T1-T3   --  Working indices used to index into Direction
//             Count, Direction Value, and Piece Value tables.
//
// INDX1   --  General working indices. Used for various
// INDX2       purposes.
//
// NPINS   --  Number of Pins. Count and pointer into the
//             pinned piece list.
//
// MLPTRI  --  Pointer into the ply table which tells
//             which pair of pointers are in current use.
//
// MLPTRJ  --  Pointer into the move list to the move that is
//             currently being processed.
//
// SCRIX   --  Score Index. Pointer to the score table for
//             the ply being examined.
//
// BESTM   --  Pointer into the move list for the move that
//             is currently considered the best by the
//             Alpha-Beta pruning process.
//
// MLLST   --  Pointer to the previous move placed in the move
//             list. Used during generation of the move list.
//
// MLNXT   --  Pointer to the next available space in the move
//             list.
//
//***********************************************************
uint8_t padding5[174];
uint16_t M1      =      TBASE;
uint16_t M2      =      TBASE;
uint16_t M3      =      TBASE;
uint16_t M4      =      TBASE;
uint16_t T1      =      TBASE;
uint16_t T2      =      TBASE;
uint16_t T3      =      TBASE;
uint16_t INDX1   =      TBASE;
uint16_t INDX2   =      TBASE;
uint16_t NPINS   =      TBASE;
uint16_t MLPTRI  =      addr(PLYIX);
uint16_t MLPTRJ  =      0;
uint16_t SCRIX   =      0;
uint16_t BESTM   =      0;
uint16_t MLLST   =      0;
uint16_t MLNXT   =      addr(MLIST);

//
// 3) MISC VARIABLES
//

//***********************************************************
// VARIABLES SECTION
//
// KOLOR   --  Indicates computer's color. White is 0, and
//             Black is 80H.
//
// COLOR   --  Indicates color of the side with the move.
//
// P1-P3   --  Working area to hold the contents of the board
//             array for a given square.
//
// PMATE   --  The move number at which a checkmate is
//             discovered during look ahead.
//
// MOVENO  --  Current move number.
//
// PLYMAX  --  Maximum depth of search using Alpha-Beta
//             pruning.
//
// NPLY    --  Current ply number during Alpha-Beta
//             pruning.
//
// CKFLG   --  A non-zero value indicates the king is in check.
//
// MATEF   --  A zero value indicates no legal moves.
//
// VALM    --  The score of the current move being examined.
//
// BRDC    --  A measure of mobility equal to the total number
//             of squares white can move to minus the number
//             black can move to.
//
// PTSL    --  The maximum number of points which could be lost
//             through an exchange by the player not on the
//             move.
//
// PTSW1   --  The maximum number of points which could be won
//             through an exchange by the player not on the
//             move.
//
// PTSW2   --  The second highest number of points which could
//             be won through a different exchange by the player
//             not on the move.
//
// MTRL    --  A measure of the difference in material
//             currently on the board. It is the total value of
//             the white pieces minus the total value of the
//             black pieces.
//
// BC0     --  The value of board control(BRDC) at ply 0.
//
// MV0     --  The value of material(MTRL) at ply 0.
//
// PTSCK   --  A non-zero value indicates that the piece has
//             just moved itself into a losing exchange of
//             material.
//
// BMOVES  --  Our very tiny book of openings. Determines
//             the first move for the computer.
//
//***********************************************************
uint8_t KOLOR   =      0;               //
uint8_t COLOR   =      0;               //
uint8_t P1      =      0;               //
uint8_t P2      =      0;               //
uint8_t P3      =      0;               //
uint8_t PMATE   =      0;               //
uint8_t MOVENO  =      0;               //
uint8_t PLYMAX  =      2;               //
uint8_t NPLY    =      0;               //
uint8_t CKFLG   =      0;               //
uint8_t MATEF   =      0;               //
uint8_t VALM    =      0;               //
uint8_t BRDC    =      0;               //
uint8_t PTSL    =      0;               //
uint8_t PTSW1   =      0;               //
uint8_t PTSW2   =      0;               //
uint8_t MTRL    =      0;               //
uint8_t BC0     =      0;               //
uint8_t MV0     =      0;               //
uint8_t PTSCK   =      0;               //
uint8_t BMOVES[12] = {
    35,55,0x10,
    34,54,0x10,
    85,65,0x10,
    84,64,0x10
};
uint8_t LINECT = 0;
char MVEMSG[5]; // = {'a','1','-','a','1'};
char O_O[3];    //    = { '0', '-', '0' };
char O_O_O[5];  //  = { '0', '-', '0', '-', '0' };

//
// 4) MOVE ARRAY
//

//***********************************************************
// MOVE LIST SECTION
//
// MLIST   --  A 2048 byte storage area for generated moves.
//             This area must be large enough to hold all
//             the moves for a single leg of the move tree.
//
// MLEND   --  The address of the last available location
//             in the move list.
//
// MLPTR   --  The Move List is a linked list of individual
//             moves each of which is 6 bytes in length. The
//             move list pointer(MLPTR) is the link field
//             within a move.
//
// MLFRP   --  The field in the move entry which gives the
//             board position from which the piece is moving.
//
// MLTOP   --  The field in the move entry which gives the
//             board position to which the piece is moving.
//
// MLFLG   --  A field in the move entry which contains flag
//             information. The meaning of each bit is as
//             follows:
//             Bit 7  --  The color of any captured piece
//                        0 -- White
//                        1 -- Black
//             Bit 6  --  Double move flag (set for castling and
//                        en passant pawn captures)
//             Bit 5  --  Pawn Promotion flag; set when pawn
//                        promotes.
//             Bit 4  --  When set, this flag indicates that
//                        this is the first move for the
//                        piece on the move.
//             Bit 3  --  This flag is set is there is a piece
//                        captured, and that piece has moved at
//                        least once.
//             Bits 2-0   Describe the captured piece.  A
//                        zero value indicates no capture.
//
// MLVAL   --  The field in the move entry which contains the
//             score assigned to the move.
//
//***********************************************************
uint8_t padding6[178];
struct ML {
    uint16_t    MLPTR_;
    uint8_t     MLFRP_;
    uint8_t     MLTOP_;
    uint8_t     MLFLG_;
    uint8_t     MLVAL_;
}  MLIST[10000];
uint8_t MLEND;
};
#endif // ZARGON_H_INCLUDED

// Macros not actually defined in zargon.h to avoid namespace pollution
#define MLPTR 0
#define MLFRP 2
#define MLTOP 3
#define MLFLG 4
#define MLVAL 5
#define DIRECT (addr(direct)-TBASE)
#define DPOINT (addr(dpoint)-TBASE)
#define DCOUNT (addr(dcount)-TBASE)
#define PVALUE (addr(pvalue)-TBASE-1)
#define PIECES  (addr(pieces)-TBASE)
#define BOARD (addr(BOARDA)-TBASE)
#define WACT addr(ATKLST)
#define BACT (addr(ATKLST)+7)
#define PLIST (addr(PLISTA)-TBASE-1)
#define PLISTD (PLIST+10)


//**********************************************************
// PROGRAM CODE SECTION
//**********************************************************

//
// Miscellaneous stubs
//
void FCDMAT()  {}
void TBCPMV()  {}
void MAKEMV()  {}
void PRTBLK( const char *name, int len ) {}
void CARRET()  {}

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
    uint8_t *dst = &m.BOARDA[21];
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
    m.POSK[0] = 25;
    m.POSK[1] = 95;
    m.POSQ[0] = 24;
    m.POSQ[1] = 94;
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

inline uint8_t PATH( int8_t dir )
{
    callback_zargon_bridge(CB_PATH);

    // Step along the path
    uint8_t piece = m.BOARDA[m.M2+=dir];

    // Off board?
    //  return 3 if off board
    if( piece == ((uint8_t)-1) )
        return 3;

    // Set P2=piece and T2=piece type (=piece&7)
    //  return 0 if empty
    else if(  0 == (m.T2 = (m.P2=piece)&7) )
        return 0;

    // Else we have run into non zero piece P2
    //  return 1 if opposite color to origin piece P1
    //  return 2 if same color as origin piece P1
    return ((m.P2 ^ m.P1)&0x80) ? 1 : 2;
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
    piece &= 0x87;   // clear flag bits
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

            uint8_t path_result = PATH( move_dir );   //  Calculate next position
            // 0  --  New position is empty
            // 1  --  Encountered a piece of the
            //        opposite color
            // 2  --  Encountered a piece of the
            //        same color
            // 3  --  New position is off the
            //        board

            // Hit our own piece or off the board ?
            if( path_result < 2 )
            {

                // Empty destination ?
                empty = (path_result==0);

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
                            if( m.M2<=28 || m.M2>=91 )  // destination on 1st or 8th rank?
                                m.P2 |= 0x20;         //  Set promote flag

                            // Add single step move to move list
                            ADMOVE();

                            // Check Pawn moved flag, for double step
                            if( (m.P1 & 0x08) == 0 )
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
                            if( m.M2<=28 || m.M2>=91 )  // destination on 1st or 8th rank?
                                m.P2 |= 0x20;           // set promote flag
                            ADMOVE();                   // add to move list
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
    if( m.P1 & 0x80 )
        idx += 10;

    // Check on en-passant capture rank
    if( idx<61 || 68<idx )  // fifth rank a5-h5 ? So white captures black pawn
        return;

    // Test pointer to previous move
    uint8_t *p = BIN_TO_PTR(m.MLPTRJ);

    // Must be first move for that piece
    if( (*(p+MLFLG) & 0x10) == 0)
        return;

    // Get "to" position for previous move
    m.M4 = *(p+MLTOP);
    p = BIN_TO_PTR(m.M4);
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
    m.P2 |= 0x40;
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
    uint8_t *p = BIN_TO_PTR(m.MLLST -= 6);

    // Zero link = first two bytes
    WR_BIN(p,0);
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
    if( m.P1 & 0x08 )
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
                if( m.M3 != 22 && m.M3 != 92 && ATTACK() )
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
                m.P2 = 0x40;    // set double move flag
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
    uint16_t bin = m.MLNXT;
    uint8_t *p = BIN_TO_PTR(bin);

    // Check that we haven't run out of memory
    uint8_t *q = &m.MLEND;
    if( p > q )
    {

        // TODO - Maybe this is probably what was intended in the original code,
        //  certainly writing 0 to the *difference* of two pointers makes no sense
        m.MLNXT = 0;
        return;
    }

    // Address of previous list area
    p = BIN_TO_PTR(m.MLLST);

    // Save next as previous
    m.MLLST = bin;

    // Store as link address
    WR_BIN( p, bin );

    // Address of moved piece
    p = &m.P1;

    // If it hasn't moved before set first move flag
    if( (*p & 0x08) == 0 )
    {
        q = &m.P2;          //  Address of move flags
        *q |= 0x10;         //  Set first move flag
    }

    // Now write move details
    p = BIN_TO_PTR(bin);
    WR_BIN(p,0);                // store zero in link address
    p += 2;
    *p++ = m.M1;                // store "from" move position
    *p++ = m.M2;                // store "to" move position
    *p++ = m.P2;                // store move flags/capt. piece
    *p++ = 0;                   // store initial move value

    // Save next slot address
    m.MLNXT = PTR_TO_BIN(p);
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
    uint16_t bin_de = m.MLNXT;  // addr of next avail list space
    uint16_t bin_hl = m.MLPTRI; // ply list pointer index
    bin_hl += 2;                // increment to next ply

    // Save move list pointer
    uint8_t *p = BIN_TO_PTR(bin_hl);
    WR_BIN(p,bin_de);
    bin_hl += 2;
    m.MLPTRI = bin_hl;   // save new index
    m.MLLST  = bin_hl;   // last pointer for chain init.

    // Loop through the board
    for( uint8_t pos=21; pos<=98; pos++ )
    {
        m.M1 = pos;
        uint8_t piece = m.BOARDA[pos];

        // If piece not empty or border
        if( piece!=0 && piece!=0xff )
        {
            m.P1 = piece;

            // If piece color is side to move, gen moves
            if( (m.COLOR&0x80) == (piece&0x80) )
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
            uint8_t path_result = PATH(dir);

            // PATH() return values
            // 0  --  New position is empty
            // 1  --  Encountered a piece of the opposite color
            // 2  --  Encountered a piece of the same color
            // 3  --  New position is off the board
            if( path_result==0 && dir_count>=9 )
                continue; // empty square, not a knight, keep stepping
            if( path_result==0 || path_result==3 )
                break;  // break to stop stepping

            // 1 -- Encountered a piece of the opposite color
            if( path_result == 1)
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
                if( (m.P2&0x80) == 0 )  // Black?
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

    // Point at White attack list
    uint8_t *p = &m.wact[0];    // Attack list

    // Skip to Black attack list if Black
    if( (m.P2&0x80) != 0 )
        p += sizeof(m.ATKLST)/2;    // (bact)

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
    for( uint8_t i=0, *p = &m.POSK[0]; i<4; i++, p++ )
    {

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
                int8_t path_result = PATH(dir);  // next position

                // If empty, carry on stepping
                if( path_result == 0 )
                {
                    stepping = true;
                }

                // Piece of same color it might be a pinned piece
                else if( path_result==2 && m.M4==0 )
                {

                    // Register a possible pin position
                    m.M4 = m.M2;
                    stepping= true;  // keep stepping
                }

                // Else piece of different colour and we have a potential pinned piece
                else if( path_result==1 && m.M4 != 0 )
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
                            memset( m.ATKLST, 0, sizeof(m.ATKLST) );
                            ATTACK();
                            int8_t defenders_minus_attackers;
                            int8_t *wact = (int8_t *)&(m.ATKLST);
                            int8_t *bact = wact + sizeof(m.ATKLST)/2;
                            if( m.P1 & 0x80 )   // Is queen black ?
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

void XCHNG()
{
    callback_zargon_bridge(CB_XCHNG);
    bool black = (m.P1 & 0x80) != 0;
    bool side_flag = true;

    uint8_t *wact = (uint8_t *)&(m.ATKLST);
    uint8_t *bact = wact + sizeof(m.ATKLST)/2;

    uint8_t count_white = *wact;
    uint8_t count_black = *bact;

    // Init points lost count (XCHNG() returns registers e + d)
    e = 0;

    // Get attacked piece value (XCHNG() returns registers e + d)
    d = m.pvalue[m.T3-1];
    d = d+d;                        // double it
    uint8_t piece_val = d;          // save

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
        int8_t points = (int8_t)piece_val;

        //  Attacker or defender ?
        if( side_flag )
            points = 0-points;  // negate value for attacker

        // Accumulate total points lost
        points += (int8_t)e;

        // Save total
        e = (uint8_t)points;
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
    int8_t *wact = (int8_t *)&(m.ATKLST);
    int8_t *bact = wact + sizeof(m.ATKLST)/2;

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
    for( m.M3=21; m.M3<=98; m.M3++ )
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
            if( (m.P1&0x08) == 0 )  // if piece not moved
            {
                int8_t bonus = -2;
                if( (m.P1&0x80) != 0 )  // if Black
                    bonus = 2;          // 2 point penalty for Black
                m.BRDC += bonus;
            }
        }

        // King
        else if( piece == KING )
        {

            // 6 point bonus for castling
            if( (m.P1&0x10) != 0 )
            {
                int8_t bonus = +6;
                if( (m.P1&0x80) != 0 )  // if Black
                    bonus = -6;         // 6 point bonus for Black
                m.BRDC += bonus;
            }

            // Else if king has not castled but has moved, 2 point penalty
            else if( (m.P1&0x08) != 0 )  // if piece has moved
            {
                int8_t bonus = -2;
                if( (m.P1&0x80) != 0 )  // if Black
                    bonus = 2;          // 2 point penalty for Black
                m.BRDC += bonus;
            }
        }

        // Rook or queen early in game
        else if( m.MOVENO < 7 )
        {

            // If rook or queen early in the game HAS moved, 2 point penalty
            if( (m.P1&0x08) != 0 )  // if piece has moved
            {
                int8_t bonus = -2;
                if( (m.P1&0x80) != 0 )  // if Black
                    bonus = 2;          // 2 point penalty for Black
                m.BRDC += bonus;
            }
        }

        // Zero out attack list
        memset( m.ATKLST, 0, sizeof(m.ATKLST) );

        // Build attack list for square
        ATTACK();

        // Get (White attacker count - Black attacker count) and accumulate
        //  into board control score
        m.BRDC += (*wact-*bact);

        // If square is empty continue loop
        if( m.P1 == 0 )            //  Get piece on current square
            continue;

        // Evaluate exchange, if any
        XCHNG();
        int8_t points    = e;  //  XCHNG returns registers e, d
        int8_t piece_val = d;  //  e=points, d=attacked piece val (I think, later - formalise this)

        // If piece is nominally lost
        if( points != 0 )
        {

            //  Deduct half a Pawn value
            piece_val--;

            // If color of piece under attack matches color of side just moved
            if( (m.COLOR&0x80) == (m.P1&0x80) )
            {

                // If points lost >= previous max points lost
                if( points >= m.PTSL )
                {

                    // Store new value as max lost
                    m.PTSL = points;

                    // Load pointer to this move
                    uint8_t *p = BIN_TO_PTR(m.MLPTRJ);

                    // Is the lost piece the one moving ?
                    if( m.M3 == *(p+MLTOP) )
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
        if( (m.P1&0x80) != 0 )
            piece_val = 0-piece_val;    // negate piece value if Black
        m.MTRL += piece_val;
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
            ptsw = ptsw/2;
        }
    }

    // Points won net = points won - points lost
    ptsw -= ptsl;

    // Color of side just moved
    if( (m.COLOR &0x80) != 0 )
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
    if( (m.COLOR&0x80) == 0 )
        points = 0-points;  // negate for white

    // Rescale score (neutral = 0x80)
    points += 0x80;
    callback_end_of_points(points);

    // Save score value
    m.VALM = points;

    // Save score value to move pointed to by current move ptr
    uint8_t *p = BIN_TO_PTR( m.MLPTRJ );
    *(p+MLVAL) = m.VALM;
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
    uint8_t *p = BIN_TO_PTR( m.MLPTRJ);
    p += 2;     // increment past link bytes

    // Loop for possible double move
    for(;;)
    {

        // "From" position
        m.M1 = *p++;

        // "To" position
        m.M2 = *p++;

        // Get captured piece plus flags
        uint8_t captured_piece_flags = *p;

        // Get piece moved from "from" pos board index
        uint8_t piece = m.BOARDA[m.M1];

        // Promote pawn if required
        if( captured_piece_flags & 0x20 )
        {
            piece |= 4; // change pawn to a queen
        }

        // Update queen position if required
        else if( (piece&7) == QUEEN )
        {
            uint8_t *q = &m.POSQ[0];    // addr of saved queen position
            if( (piece & 0x80) != 0 )   // is queen black ?
                q++;                    // increment to black queen pos
            *q = m.M2;                  // set new queen position
        }

        // Update king position if required
        else if( (piece&7) == KING )
        {
            uint8_t *q = &m.POSK[0];    // addr of saved king position
            if( (captured_piece_flags & 0x40) != 0 )  // castling ?
                piece |= 0x10;
            if( (piece & 0x80) != 0 )   // is king black ?
                q++;                    // increment to black king pos
            *q = m.M2;                  // set new king position
        }

        // Set piece moved flag
        piece |= 0x08;

        // Insert piece at new position
        m.BOARDA[m.M2] = piece;

        // Empty previous position
        m.BOARDA[m.M1] = 0;

        // Double move ?
        if( (captured_piece_flags & 0x40) != 0 )
        {
            p = BIN_TO_PTR( m.MLPTRJ);  // reload move list pointer
            p += 8;                     //  jump to next move
        }
        else
        {

            // Was captured piece a queen
            if( (captured_piece_flags & 7) == QUEEN )
            {

                // Clear queen royalty position after capture
                uint8_t *q = &m.POSQ[0];
                if( (captured_piece_flags & 0x80) != 0 )  // is queen black ?
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
    uint8_t *p = BIN_TO_PTR( m.MLPTRJ);
    p += 2;     // increment past link bytes

    // Loop for possible double move
    for(;;)
    {

        // "From" position
        m.M1 = *p++;

        // "To" position
        m.M2 = *p++;

        // Get captured piece plus flags
        uint8_t captured_piece_flags = *p;

        // Get piece moved from "to" pos board index
        uint8_t piece = m.BOARDA[m.M2];

        // Unpromote pawn if required
        if( captured_piece_flags & 0x20 )
        {
            piece &= 0xfb; // change queen to a pawn
        }

        // Update queen position if required
        else if( (piece&7) == QUEEN )
        {
            uint8_t *q = &m.POSQ[0];    // addr of saved queen position
            if( (piece & 0x80) != 0 )   // is queen black ?
                q++;                    // increment to black queen pos
            *q = m.M1;                  // set previous queen position
        }

        // Update king position if required
        else if( (piece&7) == KING )
        {
            uint8_t *q = &m.POSK[0];    // addr of saved king position
            if( (captured_piece_flags & 0x40) != 0 )  // castling ?
                piece &= 0xef;  // clear castled flag
            if( (piece & 0x80) != 0 )   // is king black ?
                q++;                    // increment to black king pos
            *q = m.M1;                  // set previous king position
        }

        // If this is the first move for piece, clear piece moved flag
        if( (captured_piece_flags&0x10) != 0 )
        {
            piece &= 0xf7;
        }

        // Return piece to previous board position
        m.BOARDA[m.M1] = piece;

        // Restore captured piece with cleared flags
        m.BOARDA[m.M2] = captured_piece_flags & 0x8f;

        // Double move ?
        if( (captured_piece_flags & 0x40) != 0 )
        {
            p = BIN_TO_PTR( m.MLPTRJ);  // reload move list pointer
            p += 8;                     //  jump to next move
        }
        else
        {

            // Was captured piece a queen
            if( (captured_piece_flags & 7) == QUEEN )
            {

                // Restore queen royalty position after capture
                uint8_t *q = &m.POSQ[0];
                if( (captured_piece_flags & 0x80) != 0 )  // is queen black ?
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
    uint16_t bin_bc = m.MLPTRI;       //  Move list begin pointer
    uint16_t bin_de = 0;

    // Loop
    for(;;)
    {
        uint16_t bin_hl = bin_bc;

        // Get link to next move
        uint8_t *p = BIN_TO_PTR(bin_hl);
        bin_bc = RD_BIN(p);

        // Make linked list
        WR_BIN( p, bin_de );

        // Return if end of list
        if( HI(bin_bc) == 0 )
            return;

        // Save list pointer
        m.MLPTRJ = bin_bc;

        // Evaluate move
        EVAL();
        bin_hl = m.MLPTRI;       //  Beginning of move list
        bin_bc = m.MLPTRJ;       //  Restore list pointer

        // Next move loop
        for(;;)
        {

            // Get next move
            p = BIN_TO_PTR(bin_hl);
            bin_de = RD_BIN(p);

            // End of list ?
            if( HI(bin_de) == 0 )
                break;

            // Compare value to list value
            uint8_t *q = BIN_TO_PTR(bin_de);
            if( m.VALM < *(q+MLVAL) )
                break;

            // Swap pointers if value not less than list value
            bin_hl = bin_de;
        }

        // Link new move into list
        WR_BIN(p,bin_bc);
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
    uint8_t *p = (uint8_t *)(&m.MLIST[0]);
    m.MLNXT = PTR_TO_BIN(p);
    p = (uint8_t *)(&m.PLYIX);
    p -= 2;
    m.MLPTRI = PTR_TO_BIN(p);

    // Initialise color
    m.COLOR = m.KOLOR;

    // Initialize score index and clear table
    p = (uint8_t *)(&m.SCORE);
    m.SCRIX = PTR_TO_BIN(p);
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
                SORTM();                    // Not at max ply, so call sort
            m.MLPTRJ = m.MLPTRI;            //  last move pointer = oad ply index pointer
        }

        // Traverse move list
        uint8_t score = 0;
        int8_t iscore = 0;
        p = BIN_TO_PTR(m.MLPTRJ);   // load last move pointer
        uint16_t bin = RD_BIN(p);

        //  End of move list ?
        bool points_needed = false;
        if( HI(bin) != 0 )
        {
            m.MLPTRJ = bin;             // save current move pointer
            p = BIN_TO_PTR(m.MLPTRI);   // save in ply pointer list
            WR_BIN(p,bin);

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

                //  Load move pointer
                p = BIN_TO_PTR(m.MLPTRJ);

                //  If score is zero (illegal move) continue looping
                if( *(p+MLVAL) == 0 )
                    continue;

                //  Execute move on board array
                MOVE();
            }

            // points_needed flag avoids a goto from where the flag is set
            if( !points_needed )
            {

                // Toggle color
                uint8_t hi = m.COLOR & 0x80;
                hi = (hi==0x80 ? 0 : 0x80);
                m.COLOR = (m.COLOR&0x7f) | hi;

                // If new colour is white increment move number
                bool white = (hi==0x00 );
                if( white )
                    m.MOVENO++;

                //  Load score table pointer
                p = BIN_TO_PTR(m.SCRIX);

                //  Get score two plys above
                score = *p;
                p++;                   // increment to current ply
                p++;

                // Save score as initial value
                *p = score;
                p--;                        // decrement pointer
                m.SCRIX = PTR_TO_BIN(p);    // save it
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
            p = BIN_TO_PTR(m.SCRIX);// load score table pointer
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
            p = BIN_TO_PTR(m.SCRIX);    // load score table pointer
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
            p = BIN_TO_PTR(m.SCRIX);    // load score table pointer
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
        CY = (*p > score);
        Z  = (*p == score);
        callback_no_best_move( score, p );
        if( CY || Z )
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
        if( (m.KOLOR&0x80 ) != 0 )  // if computer's color is black decrement
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
    uint8_t hi = m.COLOR & 0x80;
    hi = (hi==0x80 ? 0 : 0x80);
    m.COLOR = (m.COLOR&0x7f) | hi;

    // If new colour is Black, decrement move number
    bool white = (hi==0x00 );
    if( !white )
        m.MOVENO--;

    // Decrement score table index
    m.SCRIX--;

    // Decrement ply counter
    m.NPLY--;

    // Get ply list pointer
    uint8_t *p = BIN_TO_PTR(m.MLPTRI);

    // Decrement by ptr size
    p -= sizeof(int16_t);

    // Update move list avail ptr
    m.MLNXT = RD_BIN(p);

    // Get ptr to next move to undo
    p -= sizeof(int16_t);
    m.MLPTRJ = RD_BIN(p);

    // Save new ply list pointer
    m.MLPTRI = PTR_TO_BIN(p);

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

    // Adjust p so that p+MLFRP points at "from" (then "to", then "flags")
    p -= MLFRP;

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
            m.BESTM = PTR_TO_BIN(e4);
        else
            m.BESTM = PTR_TO_BIN(d4);
    }

    // Else computer is Black
    else
    {
        p = BIN_TO_PTR(m.MLPTRJ);       //  Pointer to opponents 1st move
        uint8_t from = *(p+MLFRP);      //  Get "from" position

        // Play d5 after all White first moves except a,b,c or e pawn moves
        bool play_d5 = true;
        if( from==31 || from==32 || from==33 || from==35 )
            play_d5 = false;
        if( play_d5 )
            m.BESTM = PTR_TO_BIN(d5);
        else
            m.BESTM = PTR_TO_BIN(e5);
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

    // Unnecessary Z80 User interface stuff has now been removed
    //

    // Make move on board array
    MOVE();

    // Return info about it
    EXECMV();

    // Toggle color
    uint8_t color = m.COLOR & 0x80;
    color = (color==0x80 ? 0 : 0x80);
    uint8_t temp = m.COLOR;
    m.COLOR = (m.COLOR&0x7f) | color;
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

// TODO, give this proper C parameters and return code
void ASNTBI()
{

    // Input from registers h,l
    uint8_t file = h;       // 'A' - 'H'
    uint8_t rank = l;       // '1' - '8'

    // If not okay, return register b=1
    bool ok = ('1'<=rank && rank<='8' && 'A'<=file && file<='H' );
    if( !ok )
    {
        a = 1;
        b = 1;
        return;
    }

    // Otherwise calculate offset from 21-98
    rank -= '0';            // 1 - 8
    rank++;                 // 2 - 9
    file -= 'A';            // 0 - 7
    file++;                 // 1 - 8
    uint8_t offset = rank*10 + file;    // 21-98
    a = offset;
    b = 0;  //ok
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
    uint16_t mlptrj_save = m.MLPTRJ;

    // Toggle computer's color
    uint8_t temp = m.KOLOR & 0x80;
    temp = (temp==0x80 ? 0 : 0x80);
    m.COLOR = (m.KOLOR&0x7f) | temp;    // store as current COLOR

    // Load move list index
    uint8_t *p = (uint8_t *)(&m.PLYIX[0]);
    p -= 2;
    m.MLPTRI = PTR_TO_BIN(p);

    // Next available list pointer
    p = (uint8_t *)(&m.MLIST[0]);
    p += 1024;
    m.MLNXT = PTR_TO_BIN(p);

    // Generate opponent's moves
    GENMOV();

    // Search for move, "From" position (offset 0) "To" position (offset 1)
    uint8_t *q = (uint8_t *)&m.MVEMSG;
    for(;;)
    {

        // Move found ?
        if( *q == *(p+MLFRP) && *(q+1) == *(p+MLTOP) )
            break;  // yes

        //  Pointer to next list move
        uint16_t bin = RD_BIN(p+MLPTR);
        p = BIN_TO_PTR(bin);
        if( HI(bin) == 0 ) // At end of list ?
        {

            // Invalid move, restore last move pointer
            m.MLPTRJ = mlptrj_save;
            return ok;  // false
        }
    }

    //  Save opponents move pointer
    m.MLPTRJ = PTR_TO_BIN(p);

    //  Make move on board array
    MOVE();

    //  Was it a legal move ?
    bool inchk = INCHK(m.COLOR);
    ok = !inchk;
    if( !ok )
    {

        // Not legal Undo move on board array
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
    memset( &m.POSK[0], 0, 4 );

    // Board idx = a1 to h8 inclusive
    for( int idx=21; idx<=98; idx++ )
    {
        //  Address of White king position
        uint8_t *p = &m.POSK[0];
        uint8_t piece = m.BOARDA[idx];

        // Is it a black piece ?
        if( (piece&0x80) != 0 )
            p++;    // yes, increment to address of Black king position

        // Delete flags, leave piece
        piece &= 7;

        // Update king or queen position if king or queen found
        if( piece == KING )
            *p = idx;
        else if( piece == QUEEN )
        {

            // Queen position is 2 bytes after corresponding king position
            p++;
            p++;
            *p = idx;  // Update Queen position
        }
    }

    // leave M1 at h8, not padding beyond h8 [probably not needed]
    m.M1 = 98;
}

//
//  Omit some more Z80 user interface stuff, functions
//  DSPBRD, BSETUP, INSPCE, CONVRT, DIVIDE, MLTPLY, BLNKER
//

//***********************************************************
// EXECUTE MOVE SUBROUTINE
//***********************************************************
// FUNCTION:   --  This routine is the control routine for
//                 MAKEMV. It checks for double moves and
//                 sees that they are properly handled. It
//                 sets flags in the B register for double
//                 moves:
//                       En Passant -- Bit 0
//                       O-O        -- Bit 1
//                       O-O-O      -- Bit 2
//
// CALLED BY:   -- PLYRMV
//                 CPTRMV
//
// CALLS:       -- MAKEMV
//
// ARGUMENTS:   -- Flags set in the B register as described
//                 above.
//***********************************************************
void EXECMV()
{
    // Later - make this a proper return value (also from and to)
    uint8_t ret_double_flags=0;

    // Get move "from" and "to" positions
    uint8_t *p = BIN_TO_PTR(m.MLPTRJ);
    uint8_t from = *(p+MLFRP);
    uint8_t to   = *(p+MLTOP);

    // Make the move
    MAKEMV();

    // If double move
    uint8_t flags = *(p+MLFLG);
    if( flags & 0x40 )
    {

        // Point at second move
        p += 6;
        from = *(p+MLFRP);
        to   = *(p+MLTOP);

        // Check for en-passant
        if( to == from )
            ret_double_flags |= 0x01;

        // Check for O-O
        else if( to == 26 || to == 96 )
            ret_double_flags |= 0x02;

        // Else must be O-O-O
        else
            ret_double_flags |= 0x04;

        //  Make 2nd move on board
        MAKEMV();
    }

    // Return
    b = ret_double_flags;

    // Temporary
    c = from;
    e = to;
}
