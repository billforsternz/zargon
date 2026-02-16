//
//  Declarations for zargon.cpp, including data layout
//
#ifndef ZARGON_H_INCLUDED
#define ZARGON_H_INCLUDED
#include <stdio.h>
#include "sargon-asm-interface.h"   // for zargon_data_defs_check_and_regen

// Provide ptr to the 64K emulated memory
struct emulated_memory;
emulated_memory *zargon_get_ptr_emulated_memory();

// Now export emulated memory as a global, transitional I think
extern emulated_memory gbl_emulated_memory;

// Transitional pointer manipulation macros
//  Note: a "bin" for our purposes is the binary representation of
//        a Sargon pointer, it is a uint16_t offset from the start
//        of Sargon's memory. Later we renamed "bin" to "mig" as
//        we "migrated" to native pointers. Now we have native
//        pointers but we still have migs at least until we have
//        transitioned to specific pointer types with fewer casts
#define MIG_PTR
typedef uint8_t *byte_ptr;
#ifdef MIG_PTR
typedef byte_ptr mig_t;
#else
typedef uint64_t mig_t;
#endif

#ifdef MIG_PTR
#define MIG_TO_PTR(mig)     ((byte_ptr)(mig))
#define PTR_TO_MIG(p)       ((mig_t)(p))
inline mig_t    RD_MIG( const uint8_t *p) { return *((mig_t*)(p)); }
inline void     WR_MIG( uint8_t *p, mig_t mig ) { *((mig_t*)(p)) = mig; }
#else
#define MIG_TO_PTR(mig)     ((uint8_t*) ((mig) + ((uint8_t*)(&m))))
#define PTR_TO_MIG(p)       (mig_t)(((uint8_t*)(p)) - (uint8_t*)(&m))
inline mig_t    RD_MIG( const uint8_t *p) { mig_t temp=*(p+1); return (temp<<8)|*p; }
#define HI(bin)             ((bin>>8)&0xff)
#define LO(bin)             (bin&0xff)
inline void     WR_MIG( uint8_t *p, mig_t mig ) { *p = (uint8_t)LO(mig); *(p+1) = (uint8_t)HI(mig); }
#endif

// Linked move
struct ML
{
    mig_t       link_ptr;
    uint8_t     from;
    uint8_t     to;
    uint8_t     flags;
    uint8_t     val;
};

#define MLPTR 0
#define MLFRP offsetof(ML,from)
#define MLTOP offsetof(ML,to)
#define MLFLG offsetof(ML,flags)
#define MLVAL offsetof(ML,val)

// Flag definitions
// These definitions apply to pieces on the board array and to move flags
// Always bit 7 is color (1=black) and bits 2-0 indicate piece type
// Other bits
//  6 - move flags only - double move flag (set for castling and en passant)
//  5 - move flags only - pawn promotion flag
//  4 - board array     - has castled flag for kings only
//      move flags      - first move flag for the piece on the move
//  3   board array     - piece has moved flag
//      move flags      - captured piece (in bits 2-0) has moved flag
inline bool IS_WHITE( uint8_t piece ) { return (piece&0x80) == 0;  }
inline bool IS_BLACK( uint8_t piece ) { return (piece&0x80) != 0;  }
inline void TOGGLE  ( uint8_t &color) { color = (color==0?0x80:0); }
inline bool IS_SAME_COLOR( uint8_t piece1, uint8_t piece2 )      { return (piece1&0x80) == (piece2&0x80); }
inline bool IS_DIFFERENT_COLOR( uint8_t piece1, uint8_t piece2 ) { return ((piece1&0x80) ^ (piece2&0x80)) != 0; }
inline bool HAS_MOVED( uint8_t piece ) { return (piece&0x08) != 0;  }
inline bool HAS_NOT_MOVED( uint8_t piece ) { return (piece&0x08) == 0;  }
inline void SET_MOVED( uint8_t &piece ) { piece|=0x08;  }
inline void CLR_MOVED( uint8_t &piece ) { piece&=0xf7;  }
inline bool IS_DOUBLE_MOVE( uint8_t piece ) { return (piece&0x40) != 0;  }
inline void SET_DOUBLE_MOVE( uint8_t &piece ) { piece|=0x40;  }
inline bool IS_PROMOTION( uint8_t piece ) { return (piece&0x20) != 0;  }
inline void SET_PROMOTION( uint8_t &piece ) { piece|=0x20;  }
inline bool IS_FIRST_MOVE( uint8_t piece ) { return (piece&0x10) != 0;  }
inline void SET_FIRST_MOVE( uint8_t &piece ) { piece|=0x10;  }
inline bool IS_CASTLED( uint8_t piece ) { return (piece&0x10) != 0;  }
inline void SET_CASTLED( uint8_t &piece ) { piece|=0x10;  }
inline void CLR_CASTLED( uint8_t &piece ) { piece&=0xef;  }

// Sargon squares
#define SQ_a1 21
#define SQ_b1 22
#define SQ_d1 24
#define SQ_e1 25
#define SQ_h1 28
#define SQ_a2 31
#define SQ_b2 32
#define SQ_c2 33
#define SQ_e2 35
#define SQ_a3 41
#define SQ_a4 51
#define SQ_a5 61
#define SQ_h5 68
#define SQ_a6 71
#define SQ_a7 81
#define SQ_a8 91
#define SQ_b8 92
#define SQ_d8 94
#define SQ_e8 95
#define SQ_h8 98
#define SQ_f3 46
#define SQ_e5 65
#define SQ_f1 26
#define SQ_g1 27

// Initially at least we were emulating 64K of Z80 memory
//  at some point we will probably rename this to simply
//  zargon_data or similar
struct emulated_memory {

//***********************************************************
// TABLES SECTION
//***********************************************************
//**********************************************************
// DIRECT  --  Direction Table.  Used to determine the dir-
//             ection of movement of each piece.
//***********************************************************
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
uint8_t dpoint[7] = {
    20,16,8,0,4,0,0
};

//***********************************************************
// DCOUNT  --  Direction Table Counter. Used to determine
//             the number of directions of movement for any
//             given piece.
//***********************************************************
uint8_t dcount[7] = {
    4,4,8,4,4,8,8
};

//***********************************************************
// PVALUE  --  Point Value. Gives the point value of each
//             piece, or the worth of each piece.
//***********************************************************
uint8_t pvalue[6] = {
    1,3,3,5,9,10
};

//***********************************************************
// PIECES  --  The initial arrangement of the first rank of
//             pieces on the board. Use to set up the board
//             for the start of the game.
//***********************************************************
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
uint8_t     wact[7];
uint8_t     bact[7];

//***********************************************************
// PLIST   --  Pinned Piece Array. This is a two part array.
//             PLISTA contains the pinned piece position.
//             PLISTD contains the direction from the pinned
//             piece to the attacker.
//***********************************************************
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

//***********************************************************
// SCORE   --  Score Array. Used during Alpha-Beta pruning to
//             hold the scores at each ply. It includes two
//             "dummy" entries for ply -1 and ply 0.
//***********************************************************
uint8_t padding[8];
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
uint64_t padding2[2];
mig_t    PLYIX[20] = {
    0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0
};

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
uint8_t  M1      =      0;
uint8_t  M2      =      0;
uint8_t  M3      =      0;
uint8_t  M4      =      0;
uint8_t  T1      =      0;
uint8_t  T2      =      0;
uint8_t  T3      =      0;
uint8_t  INDX1   =      0;
uint8_t  INDX2   =      0;
uint8_t  NPINS   =      0;
mig_t    MLPTRI  =
#ifdef MIG_PTR
                        (byte_ptr)&PLYIX;
#else
                        offsetof(emulated_memory,PLYIX);
#endif
mig_t    MLPTRJ  =
#ifdef MIG_PTR
                        (byte_ptr)&MLIST;
#else
                        offsetof(emulated_memory,MLIST);
#endif
byte_ptr SCRIX   =      0;
mig_t    BESTM   =      0;
mig_t    MLLST   =      0;
mig_t    MLNXT   =
#ifdef MIG_PTR
                        (byte_ptr)&MLIST;
#else
                        offsetof(emulated_memory,MLIST);
#endif

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
    35,55,0x10,        // e2-e4 double move
    34,54,0x10,        // d2-d4 double move
    85,65,0x10,        // e7-e5 double move
    84,64,0x10         // d7-d5 double move
};
uint8_t LINECT = 0;
char MVEMSG[5]; // = {'a','1','-','a','1'};

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
ML      MLIST[3750];
uint8_t MLEND;
};   //  end struct emulated_memory

#endif // ZARGON_H_INCLUDED

