//***********************************************************              //0001: ;***********************************************************
//                                                                         //0002: ;
//               SARGON                                                    //0003: ;               SARGON
//                                                                         //0004: ;
// Sargon is a computer chess playing program designed                     //0005: ;       Sargon is a computer chess playing program designed
// and coded by Dan and Kathe Spracklen.  Copyright 1978. All
// rights reserved.  No part of this publication may be                                 (Numbered lines of original Z80 Sargon code on the right)
// reproduced without the prior written permission.
//***********************************************************
                                                                           //0006: ; and coded by Dan and Kathe Spracklen.  Copyright 1978. All
#include <stdio.h>                                                         //0007: ; rights reserved.  No part of this publication may be
#include <stdint.h>                                                        //0008: ; reproduced without the prior written permission.
#include <stddef.h>                                                        //0009: ;***********************************************************
#include "z80-macros.h"                                                    //0010: 
                                                                           //0011: 
//***********************************************************              //0012: ;***********************************************************
// EQUATES                                                                 //0013: ; EQUATES
//***********************************************************              //0014: ;***********************************************************
//                                                                         //0015: ;
#define PAWN    1                                                          //0016: PAWN    EQU     1
#define KNIGHT  2                                                          //0017: KNIGHT  EQU     2
#define BISHOP  3                                                          //0018: BISHOP  EQU     3
#define ROOK    4                                                          //0019: ROOK    EQU     4
#define QUEEN   5                                                          //0020: QUEEN   EQU     5
#define KING    6                                                          //0021: KING    EQU     6
#define WHITE   0                                                          //0022: WHITE   EQU     0
#define BLACK   0x80                                                       //0023: BLACK   EQU     80H
#define BPAWN   (BLACK+PAWN)                                               //0024: BPAWN   EQU     BLACK+PAWN
                                                                           //0025: 
//                                                                         //0026: ;***********************************************************
// 1) TABLES                                                               //0027: ; TABLES SECTION
//                                                                         //0028: ;***********************************************************
//***********************************************************              //0029: START:
// TABLES SECTION                                                          //0030:         ORG     START+80H
//***********************************************************
//
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

#define TBASE 0x100     // The following tables begin at this              //0031: TBASE   EQU     START+100H
                        //  low but non-zero page boundary in              //0032: ;There are multiple tables used for fast table look ups
                        //  in our 64K emulated memory                     //0033: ;that are declared relative to TBASE. In each case there
struct emulated_memory {                                                   //0034: ;is a table (say DIRECT) and one or more variables that
                                                                           //0035: ;index into the table (say INDX2). The table is declared
uint8_t padding[TBASE];                                                    //0036: ;as a relative offset from the TBASE like this;
//**********************************************************               //0037: ;
// DIRECT  --  Direction Table.  Used to determine the dir-                // (lines 38-93 omitted) 0094: ; DIRECT  --  Direction Table.  Used to determine the dir-
//             ection of movement of each piece.                           //0095: ;             ection of movement of each piece.
//***********************************************************              //0096: ;***********************************************************
#define DIRECT (addr(direct)-TBASE)                                        //0097: DIRECT  EQU     $-TBASE
int8_t direct[24] = {                                                      //0098:         DB      +09,+11,-11,-09
    +9,+11,-11,-9,                                                         //0099:         DB      +10,-10,+01,-01
    +10,-10,+1,-1,                                                         //0100:         DB      -21,-12,+08,+19
    -21,-12,+8,+19,                                                        //0101:         DB      +21,+12,-08,-19
    +21,+12,-8,-19,                                                        //0102:         DB      +10,+10,+11,+09
    +10,+10,+11,+9,                                                        //0103:         DB      -10,-10,-11,-09
    -10,-10,-11,-9                                                         //0104: ;***********************************************************
};                                                                         //0105: ; DPOINT  --  Direction Table Pointer. Used to determine
//***********************************************************              //0106: ;             where to begin in the direction table for any
// DPOINT  --  Direction Table Pointer. Used to determine
//             where to begin in the direction table for any
//             given piece.                                                //0107: ;             given piece.
//***********************************************************              //0108: ;***********************************************************
#define DPOINT (addr(dpoint)-TBASE)                                        //0109: DPOINT  EQU     $-TBASE
uint8_t dpoint[7] = {                                                      //0110:         DB      20,16,8,0,4,0,0
    20,16,8,0,4,0,0                                                        //0111: 
};                                                                         //0112: ;***********************************************************
                                                                           //0113: ; DCOUNT  --  Direction Table Counter. Used to determine
//***********************************************************              //0114: ;             the number of directions of movement for any
// DCOUNT  --  Direction Table Counter. Used to determine
//             the number of directions of movement for any
//             given piece.                                                //0115: ;             given piece.
//***********************************************************              //0116: ;***********************************************************
#define DCOUNT (addr(dcount)-TBASE)                                        //0117: DCOUNT  EQU     $-TBASE
uint8_t dcount[7] = {                                                      //0118:         DB      4,4,8,4,4,8,8
    4,4,8,4,4,8,8                                                          //0119: 
};                                                                         //0120: ;***********************************************************
                                                                           //0121: ; PVALUE  --  Point Value. Gives the point value of each
                                                                           //0122: ;             piece, or the worth of each piece.
//***********************************************************              //0123: ;***********************************************************
// PVALUE  --  Point Value. Gives the point value of each
//             piece, or the worth of each piece.
//***********************************************************
#define PVALUE (addr(pvalue)-TBASE-1)  //TODO what's this minus 1 about?   //0124: PVALUE  EQU     $-TBASE-1
uint8_t pvalue[6] = {                                                      //0125:         DB      1,3,3,5,9,10
    1,3,3,5,9,10                                                           //0126: 
};                                                                         //0127: ;***********************************************************
                                                                           //0128: ; PIECES  --  The initial arrangement of the first rank of
                                                                           //0129: ;             pieces on the board. Use to set up the board
//***********************************************************              //0130: ;             for the start of the game.
// PIECES  --  The initial arrangement of the first rank of
//             pieces on the board. Use to set up the board
//             for the start of the game.
//***********************************************************              //0131: ;***********************************************************
#define PIECES  (addr(pieces)-TBASE)                                       //0132: PIECES  EQU     $-TBASE
uint8_t pieces[8] = {                                                      //0133:         DB      4,2,3,5,6,3,2,4
    4,2,3,5,6,3,2,4                                                        //0134: 
};                                                                         //0135: ;***********************************************************
                                                                           //0136: ; BOARD   --  Board Array.  Used to hold the current position
//***********************************************************              //0137: ;             of the board during play. The board itself
// BOARD   --  Board Array.  Used to hold the current position
//             of the board during play. The board itself
//             looks like:                                                 //0138: ;             looks like:
//             FFFFFFFFFFFFFFFFFFFF                                        //0139: ;             FFFFFFFFFFFFFFFFFFFF
//             FFFFFFFFFFFFFFFFFFFF                                        //0140: ;             FFFFFFFFFFFFFFFFFFFF
//             FF0402030506030204FF                                        //0141: ;             FF0402030506030204FF
//             FF0101010101010101FF                                        //0142: ;             FF0101010101010101FF
//             FF0000000000000000FF                                        //0143: ;             FF0000000000000000FF
//             FF0000000000000000FF                                        //0144: ;             FF0000000000000000FF
//             FF0000000000000060FF                                        //0145: ;             FF0000000000000060FF
//             FF0000000000000000FF                                        //0146: ;             FF0000000000000000FF
//             FF8181818181818181FF                                        //0147: ;             FF8181818181818181FF
//             FF8482838586838284FF                                        //0148: ;             FF8482838586838284FF
//             FFFFFFFFFFFFFFFFFFFF                                        //0149: ;             FFFFFFFFFFFFFFFFFFFF
//             FFFFFFFFFFFFFFFFFFFF                                        //0150: ;             FFFFFFFFFFFFFFFFFFFF
//             The values of FF form the border of the                     //0151: ;             The values of FF form the border of the
//             board, and are used to indicate when a piece                //0152: ;             board, and are used to indicate when a piece
//             moves off the board. The individual bits of                 //0153: ;             moves off the board. The individual bits of
//             the other bytes in the board array are as                   //0154: ;             the other bytes in the board array are as
//             follows:                                                    //0155: ;             follows:
//             Bit 7 -- Color of the piece                                 //0156: ;             Bit 7 -- Color of the piece
//                     1 -- Black                                          //0157: ;                     1 -- Black
//                     0 -- White                                          //0158: ;                     0 -- White
//             Bit 6 -- Not used                                           //0159: ;             Bit 6 -- Not used
//             Bit 5 -- Not used                                           //0160: ;             Bit 5 -- Not used
//             Bit 4 --Castle flag for Kings only                          //0161: ;             Bit 4 --Castle flag for Kings only
//             Bit 3 -- Piece has moved flag                               //0162: ;             Bit 3 -- Piece has moved flag
//             Bits 2-0 Piece type                                         //0163: ;             Bits 2-0 Piece type
//                     1 -- Pawn                                           //0164: ;                     1 -- Pawn
//                     2 -- Knight                                         //0165: ;                     2 -- Knight
//                     3 -- Bishop                                         //0166: ;                     3 -- Bishop
//                     4 -- Rook                                           //0167: ;                     4 -- Rook
//                     5 -- Queen                                          //0168: ;                     5 -- Queen
//                     6 -- King                                           //0169: ;                     6 -- King
//                     7 -- Not used                                       //0170: ;                     7 -- Not used
//                     0 -- Empty Square                                   //0171: ;                     0 -- Empty Square
//***********************************************************              //0172: ;***********************************************************
#define BOARD (addr(BOARDA)-TBASE)                                         //0173: BOARD   EQU     $-TBASE
uint8_t BOARDA[120];                                                       //0174: BOARDA  DS      120
                                                                           //0175: 
                                                                           //0176: ;***********************************************************
//***********************************************************              //0177: ; ATKLIST -- Attack List. A two part array, the first
// ATKLIST -- Attack List. A two part array, the first
//            half for white and the second half for black.                //0178: ;            half for white and the second half for black.
//            It is used to hold the attackers of any given                //0179: ;            It is used to hold the attackers of any given
//            square in the order of their value.                          //0180: ;            square in the order of their value.
//                                                                         //0181: ;
// WACT   --  White Attack Count. This is the first                        //0182: ; WACT   --  White Attack Count. This is the first
//            byte of the array and tells how many pieces are              //0183: ;            byte of the array and tells how many pieces are
//            in the white portion of the attack list.                     //0184: ;            in the white portion of the attack list.
//                                                                         //0185: ;
// BACT   --  Black Attack Count. This is the eighth byte of               //0186: ; BACT   --  Black Attack Count. This is the eighth byte of
//            the array and does the same for black.                       //0187: ;            the array and does the same for black.
//***********************************************************              //0188: ;***********************************************************
#define WACT addr(ATKLST)                                                  //0189: WACT    EQU     ATKLST
#define BACT (addr(ATKLST)+7)                                              //0190: BACT    EQU     ATKLST+7
union                                                                      //0191: ATKLST  DW      0,0,0,0,0,0,0
{                                                                          //0192: 
    uint16_t    ATKLST[7];                                                 //0193: ;***********************************************************
        uint8_t     wact[7];                                               //0194: ; PLIST   --  Pinned Piece Array. This is a two part array.
/*    struct wact_bact                                                     //0195: ;             PLISTA contains the pinned piece position.
    {                                                                      //0196: ;             PLISTD contains the direction from the pinned
        uint8_t     wact[7];                                               //0197: ;             piece to the attacker.
        uint8_t     bact[7];                                               //0198: ;***********************************************************
    }; */
};

//***********************************************************
// PLIST   --  Pinned Piece Array. This is a two part array.
//             PLISTA contains the pinned piece position.
//             PLISTD contains the direction from the pinned
//             piece to the attacker.
//***********************************************************
#define PLIST (addr(PLISTA)-TBASE-1)    ///TODO -1 why?                    //0199: PLIST   EQU     $-TBASE-1
#define PLISTD (PLIST+10)                                                  //0200: PLISTD  EQU     PLIST+10
uint8_t     PLISTA[10];     // pinned pieces                               //0201: PLISTA  DW      0,0,0,0,0,0,0,0,0,0
uint8_t     plistd[10];     // corresponding directions                    //0202: 
                                                                           //0203: ;***********************************************************
//***********************************************************              //0204: ; POSK    --  Position of Kings. A two byte area, the first
// POSK    --  Position of Kings. A two byte area, the first
//             byte of which hold the position of the white                //0205: ;             byte of which hold the position of the white
//             king and the second holding the position of                 //0206: ;             king and the second holding the position of
//             the black king.                                             //0207: ;             the black king.
//                                                                         //0208: ;
// POSQ    --  Position of Queens. Like POSK,but for queens.               //0209: ; POSQ    --  Position of Queens. Like POSK,but for queens.
//***********************************************************              //0210: ;***********************************************************
uint8_t     POSK[2] = {                                                    //0211: POSK    DB      24,95
    24,95                                                                  //0212: POSQ    DB      14,94
};                                                                         //0213:         DB      -1
uint8_t     POSQ[2] = {                                                    //0214: 
    14,94                                                                  //0215: ;***********************************************************
};                                                                         //0216: ; SCORE   --  Score Array. Used during Alpha-Beta pruning to
int8_t padding2 = -1;                                                      //0217: ;             hold the scores at each ply. It includes two
                                                                           //0218: ;             "dummy" entries for ply -1 and ply 0.
//***********************************************************              //0219: ;***********************************************************
// SCORE   --  Score Array. Used during Alpha-Beta pruning to
//             hold the scores at each ply. It includes two
//             "dummy" entries for ply -1 and ply 0.
//***********************************************************
uint16_t    SCORE[20] = {                                                  //0220: SCORE   DW      0,0,0,0,0,0     ;Z80 max 6 ply
    0,0,0,0,0,0,0,0,0,0,                // Z80 max 6 ply                   //0221: 
    0,0,0,0,0,0,0,0,0,0                 // x86 max 20 ply                  //0222: ;***********************************************************
};                                                                         //0223: ; PLYIX   --  Ply Table. Contains pairs of pointers, a pair
                                                                           //0224: ;             for each ply. The first pointer points to the
//***********************************************************              //0225: ;             top of the list of possible moves at that ply.
// PLYIX   --  Ply Table. Contains pairs of pointers, a pair
//             for each ply. The first pointer points to the
//             top of the list of possible moves at that ply.
//             The second pointer points to which move in the              //0226: ;             The second pointer points to which move in the
//             list is the one currently being considered.                 //0227: ;             list is the one currently being considered.
//***********************************************************              //0228: ;***********************************************************
uint16_t    PLYIX[20] = {                                                  //0229: PLYIX   DW      0,0,0,0,0,0,0,0,0,0
    0,0,0,0,0,0,0,0,0,0,                                                   //0230:         DW      0,0,0,0,0,0,0,0,0,0
    0,0,0,0,0,0,0,0,0,0                                                    //0231: 
};                                                                         //0232: ;***********************************************************
                                                                           //0233: ; STACK   --  Contains the stack for the program.
//                                                                         //0234: ;***********************************************************
// 2) PTRS to TABLES                                                       //0235:         ORG     START+2FFH
//                                                                         //0236: STACK:
                                                                           //0237: 
//***********************************************************              //0238: ;***********************************************************
// 2) TABLE INDICES SECTION                                                //0239: ; TABLE INDICES SECTION
//                                                                         //0240: ;
// M1-M4   --  Working indices used to index into                          //0241: ; M1-M4   --  Working indices used to index into
//             the board array.                                            //0242: ;             the board array.
//                                                                         //0243: ;
// T1-T3   --  Working indices used to index into Direction                //0244: ; T1-T3   --  Working indices used to index into Direction
//             Count, Direction Value, and Piece Value tables.             //0245: ;             Count, Direction Value, and Piece Value tables.
//                                                                         //0246: ;
// INDX1   --  General working indices. Used for various                   //0247: ; INDX1   --  General working indices. Used for various
// INDX2       purposes.                                                   //0248: ; INDX2       purposes.
//                                                                         //0249: ;
// NPINS   --  Number of Pins. Count and pointer into the                  //0250: ; NPINS   --  Number of Pins. Count and pointer into the
//             pinned piece list.                                          //0251: ;             pinned piece list.
//                                                                         //0252: ;
// MLPTRI  --  Pointer into the ply table which tells                      //0253: ; MLPTRI  --  Pointer into the ply table which tells
//             which pair of pointers are in current use.                  //0254: ;             which pair of pointers are in current use.
//                                                                         //0255: ;
// MLPTRJ  --  Pointer into the move list to the move that is              //0256: ; MLPTRJ  --  Pointer into the move list to the move that is
//             currently being processed.                                  //0257: ;             currently being processed.
//                                                                         //0258: ;
// SCRIX   --  Score Index. Pointer to the score table for                 //0259: ; SCRIX   --  Score Index. Pointer to the score table for
//             the ply being examined.                                     //0260: ;             the ply being examined.
//                                                                         //0261: ;
// BESTM   --  Pointer into the move list for the move that                //0262: ; BESTM   --  Pointer into the move list for the move that
//             is currently considered the best by the                     //0263: ;             is currently considered the best by the
//             Alpha-Beta pruning process.                                 //0264: ;             Alpha-Beta pruning process.
//                                                                         //0265: ;
// MLLST   --  Pointer to the previous move placed in the move             //0266: ; MLLST   --  Pointer to the previous move placed in the move
//             list. Used during generation of the move list.              //0267: ;             list. Used during generation of the move list.
//                                                                         //0268: ;
// MLNXT   --  Pointer to the next available space in the move             //0269: ; MLNXT   --  Pointer to the next available space in the move
//             list.                                                       //0270: ;             list.
//                                                                         //0271: ;
//***********************************************************              //0272: ;***********************************************************
uint16_t M1      =      TBASE;                                             // (line 273 omitted) 0274: M1      DW      TBASE
uint16_t M2      =      TBASE;                                             //0275: M2      DW      TBASE
uint16_t M3      =      TBASE;                                             //0276: M3      DW      TBASE
uint16_t M4      =      TBASE;                                             //0277: M4      DW      TBASE
uint16_t T1      =      TBASE;                                             //0278: T1      DW      TBASE
uint16_t T2      =      TBASE;                                             //0279: T2      DW      TBASE
uint16_t T3      =      TBASE;                                             //0280: T3      DW      TBASE
uint16_t INDX1   =      TBASE;                                             //0281: INDX1   DW      TBASE
uint16_t INDX2   =      TBASE;                                             //0282: INDX2   DW      TBASE
uint16_t NPINS   =      TBASE;                                             //0283: NPINS   DW      TBASE
uint16_t MLPTRI  =      addr(PLYIX);                                       //0284: MLPTRI  DW      PLYIX
uint16_t MLPTRJ  =      0;                                                 //0285: MLPTRJ  DW      0
uint16_t SCRIX   =      0;                                                 //0286: SCRIX   DW      0
uint16_t BESTM   =      0;                                                 //0287: BESTM   DW      0
uint16_t MLLST   =      0;                                                 //0288: MLLST   DW      0
uint16_t MLNXT   =      addr(MLIST);                                       //0289: MLNXT   DW      MLIST
                                                                           //0290: 
//                                                                         //0291: ;***********************************************************
// 3) MISC VARIABLES
//

//***********************************************************
// VARIABLES SECTION                                                       //0292: ; VARIABLES SECTION
//                                                                         //0293: ;
// KOLOR   --  Indicates computer's color. White is 0, and                 //0294: ; KOLOR   --  Indicates computer's color. White is 0, and
//             Black is 80H.                                               //0295: ;             Black is 80H.
//                                                                         //0296: ;
// COLOR   --  Indicates color of the side with the move.                  //0297: ; COLOR   --  Indicates color of the side with the move.
//                                                                         //0298: ;
// P1-P3   --  Working area to hold the contents of the board              //0299: ; P1-P3   --  Working area to hold the contents of the board
//             array for a given square.                                   //0300: ;             array for a given square.
//                                                                         //0301: ;
// PMATE   --  The move number at which a checkmate is                     //0302: ; PMATE   --  The move number at which a checkmate is
//             discovered during look ahead.                               //0303: ;             discovered during look ahead.
//                                                                         //0304: ;
// MOVENO  --  Current move number.                                        //0305: ; MOVENO  --  Current move number.
//                                                                         //0306: ;
// PLYMAX  --  Maximum depth of search using Alpha-Beta                    //0307: ; PLYMAX  --  Maximum depth of search using Alpha-Beta
//             pruning.                                                    //0308: ;             pruning.
//                                                                         //0309: ;
// NPLY    --  Current ply number during Alpha-Beta                        //0310: ; NPLY    --  Current ply number during Alpha-Beta
//             pruning.                                                    //0311: ;             pruning.
//                                                                         //0312: ;
// CKFLG   --  A non-zero value indicates the king is in check.            //0313: ; CKFLG   --  A non-zero value indicates the king is in check.
//                                                                         //0314: ;
// MATEF   --  A zero value indicates no legal moves.                      //0315: ; MATEF   --  A zero value indicates no legal moves.
//                                                                         //0316: ;
// VALM    --  The score of the current move being examined.               //0317: ; VALM    --  The score of the current move being examined.
//                                                                         //0318: ;
// BRDC    --  A measure of mobility equal to the total number             //0319: ; BRDC    --  A measure of mobility equal to the total number
//             of squares white can move to minus the number               //0320: ;             of squares white can move to minus the number
//             black can move to.                                          //0321: ;             black can move to.
//                                                                         //0322: ;
// PTSL    --  The maximum number of points which could be lost            //0323: ; PTSL    --  The maximum number of points which could be lost
//             through an exchange by the player not on the                //0324: ;             through an exchange by the player not on the
//             move.                                                       //0325: ;             move.
//                                                                         //0326: ;
// PTSW1   --  The maximum number of points which could be won             //0327: ; PTSW1   --  The maximum number of points which could be won
//             through an exchange by the player not on the                //0328: ;             through an exchange by the player not on the
//             move.                                                       //0329: ;             move.
//                                                                         //0330: ;
// PTSW2   --  The second highest number of points which could             //0331: ; PTSW2   --  The second highest number of points which could
//             be won through a different exchange by the player           //0332: ;             be won through a different exchange by the player
//             not on the move.                                            //0333: ;             not on the move.
//                                                                         //0334: ;
// MTRL    --  A measure of the difference in material                     //0335: ; MTRL    --  A measure of the difference in material
//             currently on the board. It is the total value of            //0336: ;             currently on the board. It is the total value of
//             the white pieces minus the total value of the               //0337: ;             the white pieces minus the total value of the
//             black pieces.                                               //0338: ;             black pieces.
//                                                                         //0339: ;
// BC0     --  The value of board control(val(BRDC)) at ply 0.             //0340: ; BC0     --  The value of board control(BRDC) at ply 0.
//                                                                         //0341: ;
// MV0     --  The value of material(val(MTRL)) at ply 0.                  //0342: ; MV0     --  The value of material(MTRL) at ply 0.
//                                                                         //0343: ;
// PTSCK   --  A non-zero value indicates that the piece has               //0344: ; PTSCK   --  A non-zero value indicates that the piece has
//             just moved itself into a losing exchange of                 //0345: ;             just moved itself into a losing exchange of
//             material.                                                   //0346: ;             material.
//                                                                         //0347: ;
// BMOVES  --  Our very tiny book of openings. Determines                  //0348: ; BMOVES  --  Our very tiny book of openings. Determines
//             the first move for the computer.                            //0349: ;             the first move for the computer.
//                                                                         //0350: ;
//***********************************************************              //0351: ;***********************************************************
uint8_t KOLOR   =      0;               //                                 //0352: KOLOR   DB      0
uint8_t COLOR   =      0;               //                                 //0353: COLOR   DB      0
uint8_t P1      =      0;               //                                 //0354: P1      DB      0
uint8_t P2      =      0;               //                                 //0355: P2      DB      0
uint8_t P3      =      0;               //                                 //0356: P3      DB      0
uint8_t PMATE   =      0;               //                                 //0357: PMATE   DB      0
uint8_t MOVENO  =      0;               //                                 //0358: MOVENO  DB      0
uint8_t PLYMAX  =      2;               //                                 //0359: PLYMAX  DB      2
uint8_t NPLY    =      0;               //                                 //0360: NPLY    DB      0
uint8_t CKFLG   =      0;               //                                 //0361: CKFLG   DB      0
uint8_t MATEF   =      0;               //                                 //0362: MATEF   DB      0
uint8_t VALM    =      0;               //                                 //0363: VALM    DB      0
uint8_t BRDC    =      0;               //                                 //0364: BRDC    DB      0
uint8_t PTSL    =      0;               //                                 //0365: PTSL    DB      0
uint8_t PTSW1   =      0;               //                                 //0366: PTSW1   DB      0
uint8_t PTSW2   =      0;               //                                 //0367: PTSW2   DB      0
uint8_t MTRL    =      0;               //                                 //0368: MTRL    DB      0
uint8_t BC0     =      0;               //                                 //0369: BC0     DB      0
uint8_t MV0     =      0;               //                                 //0370: MV0     DB      0
uint8_t PTSCK   =      0;               //                                 //0371: PTSCK   DB      0
uint8_t BMOVES[12] = {                                                     //0372: BMOVES  DB      35,55,10H
    35,55,0x10,                                                            //0373:         DB      34,54,10H
    34,54,0x10,                                                            //0374:         DB      85,65,10H
    85,65,0x10,                                                            //0375:         DB      84,64,10H
    84,64,0x10                                                             //0376: 
};                                                                         //0377: ;***********************************************************
                                                                           //0378: ; MOVE LIST SECTION
char MVEMSG[5] = {'a','1','-','a','1'};                                    //0379: ;
char O_O[3]    = { '0', '-', '0' };                                        //0380: ; MLIST   --  A 2048 byte storage area for generated moves.
char O_O_O[5]  = { '0', '-', '0', '-', '0' };                              //0381: ;             This area must be large enough to hold all
uint8_t LINECT = 0;                                                        //0382: ;             the moves for a single leg of the move tree.
//                                                                         //0383: ;
// 4) MOVE ARRAY                                                           //0384: ; MLEND   --  The address of the last available location
//                                                                         //0385: ;             in the move list.
                                                                           //0386: ;
//***********************************************************              //0387: ; MLPTR   --  The Move List is a linked list of individual
// MOVE LIST SECTION                                                       //0388: ;             moves each of which is 6 bytes in length. The
//                                                                         //0389: ;             move list pointer(MLPTR) is the link field
// MLIST   --  A 2048 byte storage area for generated moves.               //0390: ;             within a move.
//             This area must be large enough to hold all                  //0391: ;
//             the moves for a single leg of the move tree.                //0392: ; MLFRP   --  The field in the move entry which gives the
//                                                                         //0393: ;             board position from which the piece is moving.
// MLEND   --  The address of the last available location                  //0394: ;
//             in the move list.                                           //0395: ; MLTOP   --  The field in the move entry which gives the
//                                                                         //0396: ;             board position to which the piece is moving.
// MLPTR   --  The Move List is a linked list of individual                //0397: ;
//             moves each of which is 6 bytes in length. The               //0398: ; MLFLG   --  A field in the move entry which contains flag
//             move list pointer(MLPTR) is the link field                  //0399: ;             information. The meaning of each bit is as
//             within a move.                                              //0400: ;             follows:
//                                                                         //0401: ;             Bit 7  --  The color of any captured piece
// MLFRP   --  The field in the move entry which gives the                 //0402: ;                        0 -- White
//             board position from which the piece is moving.              //0403: ;                        1 -- Black
//                                                                         //0404: ;             Bit 6  --  Double move flag (set for castling and
// MLTOP   --  The field in the move entry which gives the                 //0405: ;                        en passant pawn captures)
//             board position to which the piece is moving.                //0406: ;             Bit 5  --  Pawn Promotion flag; set when pawn
//                                                                         //0407: ;                        promotes.
// MLFLG   --  A field in the move entry which contains flag               //0408: ;             Bit 4  --  When set, this flag indicates that
//             information. The meaning of each bit is as                  //0409: ;                        this is the first move for the
//             follows:                                                    //0410: ;                        piece on the move.
//             Bit 7  --  The color of any captured piece                  //0411: ;             Bit 3  --  This flag is set is there is a piece
//                        0 -- White                                       //0412: ;                        captured, and that piece has moved at
//                        1 -- Black                                       //0413: ;                        least once.
//             Bit 6  --  Double move flag (set for castling and           //0414: ;             Bits 2-0   Describe the captured piece.  A
//                        en passant pawn captures)                        //0415: ;                        zero value indicates no capture.
//             Bit 5  --  Pawn Promotion flag; set when pawn               //0416: ;
//                        promotes.                                        //0417: ; MLVAL   --  The field in the move entry which contains the
//             Bit 4  --  When set, this flag indicates that               //0418: ;             score assigned to the move.
//                        this is the first move for the                   //0419: ;
//                        piece on the move.                               //0420: ;***********************************************************
//             Bit 3  --  This flag is set is there is a piece             //0421:         ORG     START+300H
//                        captured, and that piece has moved at
//                        least once.
//             Bits 2-0   Describe the captured piece.  A
//                        zero value indicates no capture.
//
// MLVAL   --  The field in the move entry which contains the
//             score assigned to the move.
//
//***********************************************************
struct ML {                                                                //0422: MLIST   DS      2048
    uint16_t    MLPTR_;
    uint8_t     MLFRP_;
    uint8_t     MLTOP_;
    uint8_t     MLFLG_;
    uint8_t     MLVAL_;
}  MLIST[340];
uint8_t MLEND;                                                             //0423: MLEND   EQU     MLIST+2040
};                                                                         //0424: MLPTR   EQU     0
                                                                           //0425: MLFRP   EQU     2
#define MLPTR 0                                                            //0426: MLTOP   EQU     3
#define MLFRP 2                                                            //0427: MLFLG   EQU     4
#define MLTOP 3                                                            //0428: MLVAL   EQU     5
#define MLFLG 4                                                            //0429: 
#define MLVAL 5                                                            //0430: ;***********************************************************
                                                                           //0431: 
// Up to 64K of emulated memory                                            //0432: ;**********************************************************
emulated_memory mem;

// Emulate Z80 machine
Z80_REGISTERS;

//***********************************************************

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

//
// Forward references
//
uint8_t rand();
void INITBD();
void PATH();
void MPIECE();
void ENPSNT();
void ADJPTR();
void CASTLE();
void ADMOVE();
void GENMOV();
void INCHK();
void INCHK1();
void ATTACK();
void ATKSAV();
void PNCK();
void PINFND();
void XCHNG();
void NEXTAD();
void POINTS();
void LIMIT();
void MOVE();
void UNMOVE();
void SORTM();
void EVAL();
void FNDMOV();
void ASCEND();
void BOOK();
void CPTRMV();
void BITASN();
void ASNTBI();
void VALMOV();
void ROYALT();
void DIVIDE();
void MLTPLY();
void EXECMV();                                                             //0433: ; PROGRAM CODE SECTION
                                                                           //0434: ;**********************************************************
//**********************************************************               //0435: 
// BOARD SETUP ROUTINE                                                     //0436: ;**********************************************************
//***********************************************************              //0437: ; BOARD SETUP ROUTINE
// FUNCTION:   To initialize the board array, setting the                  //0438: ;***********************************************************
//             pieces in their initial positions for the                   //0439: ; FUNCTION:   To initialize the board array, setting the
//             start of the game.                                          //0440: ;             pieces in their initial positions for the
//                                                                         //0441: ;             start of the game.
// CALLED BY:  DRIVER                                                      //0442: ;
//                                                                         //0443: ; CALLED BY:  DRIVER
// CALLS:      None                                                        //0444: ;
//                                                                         //0445: ; CALLS:      None
// ARGUMENTS:  None                                                        //0446: ;
//***********************************************************              //0447: ; ARGUMENTS:  None
void INITBD() {                                                            //0448: ;***********************************************************
        LD      (b,120);                //  Pre-fill board with -1's       //0449: INITBD: LD      b,120           ; Pre-fill board with -1's
        LD      (hl,addr(BOARDA));                                         //0450:         LD      hl,BOARDA
back01: LD      (ptr(hl),-1);                                              //0451: back01: LD      (hl),-1
        INC16   (hl);                                                      //0452:         INC     hl
        DJNZ    (back01);                                                  //0453:         DJNZ    back01
        LD      (b,8);                                                     //0454:         LD      b,8
        LD      (ix,addr(BOARDA));                                         //0455:         LD      ix,BOARDA
IB2:    LD      (a,ptr(ix-8));          //  Fill non-border squares        //0456: IB2:    LD      a,(ix-8)        ; Fill non-border squares
        LD      (ptr(ix+21),a);         //  White pieces                   //0457:         LD      (ix+21),a       ; White pieces
        SET     (7,a);                  //  Change to black                //0458:         SET     7,a             ; Change to black
        LD      (ptr(ix+91),a);         //  Black pieces                   //0459:         LD      (ix+91),a       ; Black pieces
        LD      (ptr(ix+31),PAWN);      //  White Pawns                    //0460:         LD      (ix+31),PAWN    ; White Pawns
        LD      (ptr(ix+81),BPAWN);     //  Black Pawns                    //0461:         LD      (ix+81),BPAWN   ; Black Pawns
        LD      (ptr(ix+41),0);         //  Empty squares                  //0462:         LD      (ix+41),0       ; Empty squares
        LD      (ptr(ix+51),0);                                            //0463:         LD      (ix+51),0
        LD      (ptr(ix+61),0);                                            //0464:         LD      (ix+61),0
        LD      (ptr(ix+71),0);                                            //0465:         LD      (ix+71),0
        INC16   (ix);                                                      //0466:         INC     ix
        DJNZ    (IB2);                                                     //0467:         DJNZ    IB2
        LD      (ix,addr(POSK));        //  Init King/Queen position list  //0468:         LD      ix,POSK         ; Init King/Queen position list
        LD      (ptr(ix+0),25);                                            //0469:         LD      (ix+0),25
        LD      (ptr(ix+1),95);                                            //0470:         LD      (ix+1),95
        LD      (ix,addr(POSQ));                                           //0471:         LD      (ix+2),24
        LD      (ptr(ix+0),24);                                            //0472:         LD      (ix+3),94
        LD      (ptr(ix+1),94);                                            //0473:         RET
        RETu;                                                              //0474: 
}                                                                          //0475: ;***********************************************************
                                                                           //0476: ; PATH ROUTINE
//***********************************************************              //0477: ;***********************************************************
// PATH ROUTINE
//***********************************************************
// FUNCTION:   To generate a single possible move for a given
//             piece along its current path of motion including:           //0478: ; FUNCTION:   To generate a single possible move for a given
//                                                                         //0479: ;             piece along its current path of motion including:
//                Fetching the contents of the board at the new            //0480: 
//                position, and setting a flag describing the              //0481: ;                Fetching the contents of the board at the new
//                contents:                                                //0482: ;                position, and setting a flag describing the
//                          0  --  New position is empty                   //0483: ;                contents:
//                          1  --  Encountered a piece of the              //0484: ;                          0  --  New position is empty
//                                 opposite color                          //0485: ;                          1  --  Encountered a piece of the
//                          2  --  Encountered a piece of the              //0486: ;                                 opposite color
//                                 same color                              //0487: ;                          2  --  Encountered a piece of the
//                          3  --  New position is off the                 //0488: ;                                 same color
//                                 board                                   //0489: ;                          3  --  New position is off the
//                                                                         //0490: ;                                 board
// CALLED BY:  MPIECE                                                      //0491: ;
//             ATTACK                                                      //0492: ; CALLED BY:  MPIECE
//             PINFND                                                      //0493: ;             ATTACK
//                                                                         //0494: ;             PINFND
// CALLS:      None                                                        //0495: ;
//                                                                         //0496: ; CALLS:      None
// ARGUMENTS:  Direction from the direction array giving the               //0497: ;
//             constant to be added for the new position.                  //0498: ; ARGUMENTS:  Direction from the direction array giving the
//***********************************************************              //0499: ;             constant to be added for the new position.
void PATH() {                                                              //0500: ;***********************************************************
        LD      (hl,addr(M2));          //  Get previous position          //0501: PATH:   LD      hl,M2           ; Get previous position
        LD      (a,ptr(hl));            //                                 //0502:         LD      a,(hl)
        ADD     (a,c);                  //  Add direction constant         //0503:         ADD     a,c             ; Add direction constant
        LD      (ptr(hl),a);            //  Save new position              //0504:         LD      (hl),a          ; Save new position
        LD      (ix,val16(M2));         //  Load board index               //0505:         LD      ix,(M2)         ; Load board index
        LD      (a,ptr(ix+BOARD));      //  Get contents of board          //0506:         LD      a,(ix+BOARD)    ; Get contents of board
        CP      (-1);                   //  In border area ?               //0507:         CP      -1              ; In border area ?
        JR      (Z,PA2);                //  Yes - jump                     //0508:         JR      Z,PA2           ; Yes - jump
        LD      (val(P2),a);            //  Save piece                     //0509:         LD      (P2),a          ; Save piece
        AND     (7);                    //  Clear flags                    //0510:         AND     7               ; Clear flags
        LD      (val(T2),a);            //  Save piece type                //0511:         LD      (T2),a          ; Save piece type
        RET     (Z);                    //  Return if empty                //0512:         RET     Z               ; Return if empty
        LD      (a,val(P2));            //  Get piece encountered          //0513:         LD      a,(P2)          ; Get piece encountered
        LD      (hl,addr(P1));          //  Get moving piece address       //0514:         LD      hl,P1           ; Get moving piece address
        XOR     (ptr(hl));              //  Compare                        //0515:         XOR     (hl)            ; Compare
        BIT     (7,a);                  //  Do colors match ?              //0516:         BIT     7,a             ; Do colors match ?
        JR      (Z,PA1);                //  Yes - jump                     //0517:         JR      Z,PA1           ; Yes - jump
        LD      (a,1);                  //  Set different color flag       //0518:         LD      a,1             ; Set different color flag
        RETu;                           //  Return                         //0519:         RET                     ; Return
PA1:    LD      (a,2);                  //  Set same color flag            //0520: PA1:    LD      a,2             ; Set same color flag
        RETu;                           //  Return                         //0521:         RET                     ; Return
PA2:    LD      (a,3);                  //  Set off board flag             //0522: PA2:    LD      a,3             ; Set off board flag
        RETu;                           //  Return                         //0523:         RET                     ; Return
}                                                                          //0524: 
                                                                           //0525: ;***********************************************************
                                                                           //0526: ; PIECE MOVER ROUTINE
                                                                           //0527: ;***********************************************************
//***********************************************************              //0528: ; FUNCTION:   To generate all the possible legal moves for a
// PIECE MOVER ROUTINE
//***********************************************************
// FUNCTION:   To generate all the possible legal moves for a
//             given piece.
//                                                                         //0529: ;             given piece.
// CALLED BY:  GENMOV                                                      //0530: ;
//                                                                         //0531: ; CALLED BY:  GENMOV
// CALLS:      PATH                                                        //0532: ;
//             ADMOVE                                                      //0533: ; CALLS:      PATH
//             CASTLE                                                      //0534: ;             ADMOVE
//             ENPSNT                                                      //0535: ;             CASTLE
//                                                                         //0536: ;             ENPSNT
// ARGUMENTS:  The piece to be moved.                                      //0537: ;
//***********************************************************              //0538: ; ARGUMENTS:  The piece to be moved.
void MPIECE() {                                                            //0539: ;***********************************************************
        XOR     (ptr(hl));              //  Piece to move                  //0540: MPIECE: XOR     (hl)            ; Piece to move
        AND     (0x87);                 //  Clear flag bit                 //0541:         AND     87H             ; Clear flag bit
        CP      (BPAWN);                //  Is it a black Pawn ?           //0542:         CP      BPAWN           ; Is it a black Pawn ?
        JR      (NZ,rel001);            //  No-Skip                        //0543:         JR      NZ,rel001       ; No-Skip
        DEC     (a);                    //  Decrement for black Pawns      //0544:         DEC     a               ; Decrement for black Pawns
rel001: AND     (7);                    //  Get piece type                 //0545: rel001: AND     7               ; Get piece type
        LD      (val(T1),a);            //  Save piece type                //0546:         LD      (T1),a          ; Save piece type
        LD      (iy,val16(T1));         //  Load index to DCOUNT/DPOINT    //0547:         LD      iy,(T1)         ; Load index to DCOUNT/DPOINT
        LD      (b,ptr(iy+DCOUNT));     //  Get direction count            //0548:         LD      b,(iy+DCOUNT)   ; Get direction count
        LD      (a,ptr(iy+DPOINT));     //  Get direction pointer          //0549:         LD      a,(iy+DPOINT)   ; Get direction pointer
        LD      (val(INDX2),a);         //  Save as index to direct        //0550:         LD      (INDX2),a       ; Save as index to direct
        LD      (iy,val16(INDX2));      //  Load index                     //0551:         LD      iy,(INDX2)      ; Load index
MP5:    LD      (c,ptr(iy+DIRECT));     //  Get move direction             //0552: MP5:    LD      c,(iy+DIRECT)   ; Get move direction
        LD      (a,val(M1));            //  From position                  //0553:         LD      a,(M1)          ; From position
        LD      (val(M2),a);            //  Initialize to position         //0554:         LD      (M2),a          ; Initialize to position
MP10:   CALLu   (PATH);                 //  Calculate next position        //0555: MP10:   CALL    PATH            ; Calculate next position
        CP      (2);                    //  Ready for new direction ?      //0556:         CP      2               ; Ready for new direction ?
        JR      (NC,MP15);              //  Yes - Jump                     //0557:         JR      NC,MP15         ; Yes - Jump
        AND     (a);                    //  Test for empty square          //0558:         AND     a               ; Test for empty square
        Z80_EXAF;                       //  Save result                    //0559:         EX      af,af'          ; Save result
        LD      (a,val(T1));            //  Get piece moved                //0560:         LD      a,(T1)          ; Get piece moved
        CP      (PAWN+1);               //  Is it a Pawn ?                 //0561:         CP      PAWN+1          ; Is it a Pawn ?
        JR      (C,MP20);               //  Yes - Jump                     //0562:         JR      C,MP20          ; Yes - Jump
        CALLu    (ADMOVE);              //  Add move to list               //0563:         CALL    ADMOVE          ; Add move to list
        Z80_EXAF;                       //  Empty square ?                 //0564:         EX      af,af'          ; Empty square ?
        JR      (NZ,MP15);              //  No - Jump                      //0565:         JR      NZ,MP15         ; No - Jump
        LD      (a,val(T1));            //  Piece type                     //0566:         LD      a,(T1)          ; Piece type
        CP      (KING);                 //  King ?                         //0567:         CP      KING            ; King ?
        JR      (Z,MP15);               //  Yes - Jump                     //0568:         JR      Z,MP15          ; Yes - Jump
        CP      (BISHOP);               //  Bishop, Rook, or Queen ?       //0569:         CP      BISHOP          ; Bishop, Rook, or Queen ?
        JR      (NC,MP10);              //  Yes - Jump                     //0570:         JR      NC,MP10         ; Yes - Jump
MP15:   INC16   (iy);                   //  Increment direction index      //0571: MP15:   INC     iy              ; Increment direction index
        DJNZ    (MP5);                  //  Decr. count-jump if non-zerc   //0572:         DJNZ    MP5             ; Decr. count-jump if non-zero
        LD      (a,val(T1));            //  Piece type                     //0573:         LD      a,(T1)          ; Piece type
        CP      (KING);                 //  King ?                         //0574:         CP      KING            ; King ?
        CALL    (Z,CASTLE);             //  Yes - Try Castling             //0575:         CALL    Z,CASTLE        ; Yes - Try Castling
        RETu;                           //  Return                         //0576:         RET                     ; Return
// ***** PAWN LOGIC *****                                                  //0577: ; ***** PAWN LOGIC *****
MP20:   LD      (a,b);                  //  Counter for direction          //0578: MP20:   LD      a,b             ; Counter for direction
        CP      (3);                    //  On diagonal moves ?            //0579:         CP      3               ; On diagonal moves ?
        JR      (C,MP35);               //  Yes - Jump                     //0580:         JR      C,MP35          ; Yes - Jump
        JR      (Z,MP30);               //  -or-jump if on 2 square move   //0581:         JR      Z,MP30          ; -or-jump if on 2 square move
        Z80_EXAF;                       //  Is forward square empty?       //0582:         EX      af,af'          ; Is forward square empty?
        JR      (NZ,MP15);              //  No - jump                      //0583:         JR      NZ,MP15         ; No - jump
        LD      (a,val(M2));            //  Get "to" position              //0584:         LD      a,(M2)          ; Get "to" position
        CP      (91);                   //  Promote white Pawn ?           //0585:         CP      91              ; Promote white Pawn ?
        JR      (NC,MP25);              //  Yes - Jump                     //0586:         JR      NC,MP25         ; Yes - Jump
        CP      (29);                   //  Promote black Pawn ?           //0587:         CP      29              ; Promote black Pawn ?
        JR      (NC,MP26);              //  No - Jump                      //0588:         JR      NC,MP26         ; No - Jump
MP25:   LD      (hl,addr(P2));          //  Flag address                   //0589: MP25:   LD      hl,P2           ; Flag address
        SET     (5,ptr(hl));            //  Set promote flag               //0590:         SET     5,(hl)          ; Set promote flag
MP26:   CALLu   (ADMOVE);               //  Add to move list               //0591: MP26:   CALL    ADMOVE          ; Add to move list
        INC16   (iy);                   //  Adjust to two square move      //0592:         INC     iy              ; Adjust to two square move
        DEC     (b);                    //                                 //0593:         DEC     b
        LD      (hl,addr(P1));          //  Check Pawn moved flag          //0594:         LD      hl,P1           ; Check Pawn moved flag
        BIT     (3,ptr(hl));            //  Has it moved before ?          //0595:         BIT     3,(hl)          ; Has it moved before ?
        JR      (Z,MP10);               //  No - Jump                      //0596:         JR      Z,MP10          ; No - Jump
        JPu     (MP15);                 //  Jump                           //0597:         JP      MP15            ; Jump
MP30:   Z80_EXAF;                       //  Is forward square empty ?      //0598: MP30:   EX      af,af'          ; Is forward square empty ?
        JR      (NZ,MP15);              //  No - Jump                      //0599:         JR      NZ,MP15         ; No - Jump
MP31:   CALLu   (ADMOVE);               //  Add to move list               //0600: MP31:   CALL    ADMOVE          ; Add to move list
        JPu     (MP15);                 //  Jump                           //0601:         JP      MP15            ; Jump
MP35:   Z80_EXAF;                       //  Is diagonal square empty ?     //0602: MP35:   EX      af,af'          ; Is diagonal square empty ?
        JR      (Z,MP36);               //  Yes - Jump                     //0603:         JR      Z,MP36          ; Yes - Jump
        LD      (a,val(M2));            //  Get "to" position              //0604:         LD      a,(M2)          ; Get "to" position
        CP      (91);                   //  Promote white Pawn ?           //0605:         CP      91              ; Promote white Pawn ?
        JR      (NC,MP37);              //  Yes - Jump                     //0606:         JR      NC,MP37         ; Yes - Jump
        CP      (29);                   //  Black Pawn promotion ?         //0607:         CP      29              ; Black Pawn promotion ?
        JR      (NC,MP31);              //  No- Jump                       //0608:         JR      NC,MP31         ; No- Jump
MP37:   LD      (hl,addr(P2));          //  Get flag address               //0609: MP37:   LD      hl,P2           ; Get flag address
        SET     (5,ptr(hl));            //  Set promote flag               //0610:         SET     5,(hl)          ; Set promote flag
        JRu     (MP31);                 //  Jump                           //0611:         JR      MP31            ; Jump
MP36:   CALLu   (ENPSNT);               //  Try en passant capture         //0612: MP36:   CALL    ENPSNT          ; Try en passant capture
        JPu     (MP15);                 //  Jump                           //0613:         JP      MP15            ; Jump
}


//***********************************************************              //0614: 
// EN PASSANT ROUTINE                                                      //0615: ;***********************************************************
//***********************************************************              //0616: ; EN PASSANT ROUTINE
// FUNCTION:   --  To test for en passant Pawn capture and                 //0617: ;***********************************************************
//                 to add it to the move list if it is                     //0618: ; FUNCTION:   --  To test for en passant Pawn capture and
//                 legal.                                                  //0619: ;                 to add it to the move list if it is
//                                                                         //0620: ;                 legal.
// CALLED BY:  --  MPIECE                                                  //0621: ;
//                                                                         //0622: ; CALLED BY:  --  MPIECE
// CALLS:      --  ADMOVE                                                  //0623: ;
//                 ADJPTR                                                  //0624: ; CALLS:      --  ADMOVE
//                                                                         //0625: ;                 ADJPTR
// ARGUMENTS:  --  None                                                    //0626: ;
//***********************************************************              //0627: ; ARGUMENTS:  --  None
void ENPSNT() {                                                            //0628: ;***********************************************************
        LD      (a,val(M1));            //  Set position of Pawn           //0629: ENPSNT: LD      a,(M1)          ; Set position of Pawn
        LD      (hl,addr(P1));          //  Check color                    //0630:         LD      hl,P1           ; Check color
        BIT     (7,ptr(hl));            //  Is it white ?                  //0631:         BIT     7,(hl)          ; Is it white ?
        JR      (Z,rel002);             //  Yes - skip                     //0632:         JR      Z,rel002        ; Yes - skip
        ADD     (a,10);                 //  Add 10 for black               //0633:         ADD     a,10            ; Add 10 for black
rel002: CP      (61);                   //  On en passant capture rank ?   //0634: rel002: CP      61              ; On en passant capture rank ?
        RET     (C);                    //  No - return                    //0635:         RET     C               ; No - return
        CP      (69);                   //  On en passant capture rank ?   //0636:         CP      69              ; On en passant capture rank ?
        RET     (NC);                   //  No - return                    //0637:         RET     NC              ; No - return
        LD      (ix,val16(MLPTRJ));     //  Get pointer to previous move   //0638:         LD      ix,(MLPTRJ)     ; Get pointer to previous move
        BIT     (4,ptr(ix+MLFLG));      //  First move for that piece ?    //0639:         BIT     4,(ix+MLFLG)    ; First move for that piece ?
        RET     (Z);                    //  No - return                    //0640:         RET     Z               ; No - return
        LD      (a,ptr(ix+MLTOP));      //  Get "to" position              //0641:         LD      a,(ix+MLTOP)    ; Get "to" position
        LD      (val(M4),a);            //  Store as index to board        //0642:         LD      (M4),a          ; Store as index to board
        LD      (ix,val16(M4));         //  Load board index               //0643:         LD      ix,(M4)         ; Load board index
        LD      (a,ptr(ix+BOARD));      //  Get piece moved                //0644:         LD      a,(ix+BOARD)    ; Get piece moved
        LD      (val(P3),a);            //  Save it                        //0645:         LD      (P3),a          ; Save it
        AND     (7);                    //  Get piece type                 //0646:         AND     7               ; Get piece type
        CP      (PAWN);                 //  Is it a Pawn ?                 //0647:         CP      PAWN            ; Is it a Pawn ?
        RET     (NZ);                   //  No - return                    //0648:         RET     NZ              ; No - return
        LD      (a,val(M4));            //  Get "to" position              //0649:         LD      a,(M4)          ; Get "to" position
        LD      (hl,addr(M2));          //  Get present "to" position      //0650:         LD      hl,M2           ; Get present "to" position
        SUB     (ptr(hl));              //  Find difference                //0651:         SUB     (hl)            ; Find difference
        JP      (P,rel003);             //  Positive ? Yes - Jump          //0652:         JP      P,rel003        ; Positive ? Yes - Jump
        NEG;                            //  Else take absolute value       //0653:         NEG                     ; Else take absolute value
rel003: CP      (10);                   //  Is difference 10 ?             //0654: rel003: CP      10              ; Is difference 10 ?
        RET     (NZ);                   //  No - return                    //0655:         RET     NZ              ; No - return
        LD      (hl,addr(P2));          //  Address of flags               //0656:         LD      hl,P2           ; Address of flags
        SET     (6,ptr(hl));            //  Set double move flag           //0657:         SET     6,(hl)          ; Set double move flag
        CALLu   (ADMOVE);               //  Add Pawn move to move list     //0658:         CALL    ADMOVE          ; Add Pawn move to move list
        LD      (a,val(M1));            //  Save initial Pawn position     //0659:         LD      a,(M1)          ; Save initial Pawn position
        LD      (val(M3),a);            //                                 //0660:         LD      (M3),a
        LD      (a,val(M4));            //  Set "from" and "to" positions  //0661:         LD      a,(M4)          ; Set "from" and "to" positions
// for dummy move                                                          //0662:                                 ; for dummy move
        LD      (val(M1),a);            //                                 //0663:         LD      (M1),a
        LD      (val(M2),a);            //                                 //0664:         LD      (M2),a
        LD      (a,val(P3));            //  Save captured Pawn             //0665:         LD      a,(P3)          ; Save captured Pawn
        LD      (val(P2),a);            //                                 //0666:         LD      (P2),a
        CALLu   (ADMOVE);               //  Add Pawn capture to move list  //0667:         CALL    ADMOVE          ; Add Pawn capture to move list
        LD      (a,val(M3));            //  Restore "from" position        //0668:         LD      a,(M3)          ; Restore "from" position
        LD      (val(M1),a);            //                                 //0669:         LD      (M1),a
}


//***********************************************************              //0670: 
// ADJUST MOVE LIST POINTER FOR DOUBLE MOVE                                //0671: ;***********************************************************
//***********************************************************              //0672: ; ADJUST MOVE LIST POINTER FOR DOUBLE MOVE
// FUNCTION:   --  To adjust move list pointer to link around              //0673: ;***********************************************************
//                 second move in double move.                             //0674: ; FUNCTION:   --  To adjust move list pointer to link around
//                                                                         //0675: ;                 second move in double move.
// CALLED BY:  --  ENPSNT                                                  //0676: ;
//                 CASTLE                                                  //0677: ; CALLED BY:  --  ENPSNT
//                 (This mini-routine is not really called,                //0678: ;                 CASTLE
//                 but is jumped to to save time.)                         //0679: ;                 (This mini-routine is not really called,
//                                                                         //0680: ;                 but is jumped to to save time.)
// CALLS:      --  None                                                    //0681: ;
//                                                                         //0682: ; CALLS:      --  None
// ARGUMENTS:  --  None                                                    //0683: ;
//***********************************************************              //0684: ; ARGUMENTS:  --  None
void ADJPTR() {                                                            //0685: ;***********************************************************
        LD      (hl,val16(MLLST));      //  Get list pointer               //0686: ADJPTR: LD      hl,(MLLST)      ; Get list pointer
        LD      (de,-6);                //  Size of a move entry           //0687:         LD      de,-6           ; Size of a move entry
        ADD16   (hl,de);                //  Back up list pointer           //0688:         ADD     hl,de           ; Back up list pointer
        LD      (val16(MLLST),hl);      //  Save list pointer              //0689:         LD      (MLLST),hl      ; Save list pointer
        LD      (ptr(hl),0);            //  Zero out link, first byte      //0690:         LD      (hl),0          ; Zero out link, first byte
        INC16   (hl);                   //  Next byte                      //0691:         INC     hl              ; Next byte
        LD      (ptr(hl),0);            //  Zero out link, second byte     //0692:         LD      (hl),0          ; Zero out link, second byte
        RETu;                           //  Return                         //0693:         RET                     ; Return
}                                                                          //0694: 
                                                                           //0695: ;***********************************************************
//***********************************************************              //0696: ; CASTLE ROUTINE
// CASTLE ROUTINE
//***********************************************************
// FUNCTION:   --  To determine whether castling is legal                  //0697: ;***********************************************************
//                 (Queen side, King side, or both) and add it             //0698: ; FUNCTION:   --  To determine whether castling is legal
//                 to the move list if it is.                              //0699: ;                 (Queen side, King side, or both) and add it
//                                                                         //0700: ;                 to the move list if it is.
// CALLED BY:  --  MPIECE                                                  //0701: ;
//                                                                         //0702: ; CALLED BY:  --  MPIECE
// CALLS:      --  ATTACK                                                  //0703: ;
//                 ADMOVE                                                  //0704: ; CALLS:      --  ATTACK
//                 ADJPTR                                                  //0705: ;                 ADMOVE
//                                                                         //0706: ;                 ADJPTR
// ARGUMENTS:  --  None                                                    //0707: ;
//***********************************************************              //0708: ; ARGUMENTS:  --  None
void CASTLE() {                                                            //0709: ;***********************************************************
        LD      (a,val(P1));            //  Get King                       //0710: CASTLE: LD      a,(P1)          ; Get King
        BIT     (3,a);                  //  Has it moved ?                 //0711:         BIT     3,a             ; Has it moved ?
        RET     (NZ);                   //  Yes - return                   //0712:         RET     NZ              ; Yes - return
        LD      (a,val(CKFLG));         //  Fetch Check Flag               //0713:         LD      a,(CKFLG)       ; Fetch Check Flag
        AND     (a);                    //  Is the King in check ?         //0714:         AND     a               ; Is the King in check ?
        RET     (NZ);                   //  Yes - Return                   //0715:         RET     NZ              ; Yes - Return
        LD      (bc,0xFF03);            //  Initialize King-side values    //0716:         LD      bc,0FF03H       ; Initialize King-side values
CA5:    LD      (a,val(M1));            //  King position                  //0717: CA5:    LD      a,(M1)          ; King position
        ADD     (a,c);                  //  Rook position                  //0718:         ADD     a,c             ; Rook position
        LD      (c,a);                  //  Save                           //0719:         LD      c,a             ; Save
        LD      (val(M3),a);            //  Store as board index           //0720:         LD      (M3),a          ; Store as board index
        LD      (ix,val16(M3));         //  Load board index               //0721:         LD      ix,(M3)         ; Load board index
        LD      (a,ptr(ix+BOARD));      //  Get contents of board          //0722:         LD      a,(ix+BOARD)    ; Get contents of board
        AND     (0x7F);                 //  Clear color bit                //0723:         AND     7FH             ; Clear color bit
        CP      (ROOK);                 //  Has Rook ever moved ?          //0724:         CP      ROOK            ; Has Rook ever moved ?
        JR      (NZ,CA20);              //  Yes - Jump                     //0725:         JR      NZ,CA20         ; Yes - Jump
        LD      (a,c);                  //  Restore Rook position          //0726:         LD      a,c             ; Restore Rook position
        JRu     (CA15);                 //  Jump                           //0727:         JR      CA15            ; Jump
CA10:   LD      (ix,val16(M3));         //  Load board index               //0728: CA10:   LD      ix,(M3)         ; Load board index
        LD      (a,ptr(ix+BOARD));      //  Get contents of board          //0729:         LD      a,(ix+BOARD)    ; Get contents of board
        AND     (a);                    //  Empty ?                        //0730:         AND     a               ; Empty ?
        JR      (NZ,CA20);              //  No - Jump                      //0731:         JR      NZ,CA20         ; No - Jump
        LD      (a,val(M3));            //  Current position               //0732:         LD      a,(M3)          ; Current position
        CP      (22);                   //  White Queen Knight square ?    //0733:         CP      22              ; White Queen Knight square ?
        JR      (Z,CA15);               //  Yes - Jump                     //0734:         JR      Z,CA15          ; Yes - Jump
        CP      (92);                   //  Black Queen Knight square ?    //0735:         CP      92              ; Black Queen Knight square ?
        JR      (Z,CA15);               //  Yes - Jump                     //0736:         JR      Z,CA15          ; Yes - Jump
        CALLu   (ATTACK);               //  Look for attack on square      //0737:         CALL    ATTACK          ; Look for attack on square
        AND     (a);                    //  Any attackers ?                //0738:         AND     a               ; Any attackers ?
        JR      (NZ,CA20);              //  Yes - Jump                     //0739:         JR      NZ,CA20         ; Yes - Jump
        LD      (a,val(M3));            //  Current position               //0740:         LD      a,(M3)          ; Current position
CA15:   ADD     (a,b);                  //  Next position                  //0741: CA15:   ADD     a,b             ; Next position
        LD      (val(M3),a);            //  Save as board index            //0742:         LD      (M3),a          ; Save as board index
        LD      (hl,addr(M1));          //  King position                  //0743:         LD      hl,M1           ; King position
        CP      (ptr(hl));              //  Reached King ?                 //0744:         CP      (hl)            ; Reached King ?
        JR      (NZ,CA10);              //  No - jump                      //0745:         JR      NZ,CA10         ; No - jump
        SUB     (b);                    //  Determine King's position      //0746:         SUB     b               ; Determine King's position
        SUB     (b);                                                       //0747:         SUB     b
        LD      (val(M2),a);            //  Save it                        //0748:         LD      (M2),a          ; Save it
        LD      (hl,addr(P2));          //  Address of flags               //0749:         LD      hl,P2           ; Address of flags
        LD      (ptr(hl),0x40);         //  Set double move flag           //0750:         LD      (hl),40H        ; Set double move flag
        CALLu   (ADMOVE);               //  Put king move in list          //0751:         CALL    ADMOVE          ; Put king move in list
        LD      (hl,addr(M1));          //  Addr of King "from" position   //0752:         LD      hl,M1           ; Addr of King "from" position
        LD      (a,ptr(hl));            //  Get King's "from" position     //0753:         LD      a,(hl)          ; Get King's "from" position
        LD      ((hl),c);               //  Store Rook "from" position     //0754:         LD      (hl),c          ; Store Rook "from" position
        SUB     (b);                    //  Get Rook "to" position         //0755:         SUB     b               ; Get Rook "to" position
        LD      (val(M2),a);            //  Store Rook "to" position       //0756:         LD      (M2),a          ; Store Rook "to" position
        XOR     (a);                    //  Zero                           //0757:         XOR     a               ; Zero
        LD      (val(P2),a);            //  Zero move flags                //0758:         LD      (P2),a          ; Zero move flags
        CALLu   (ADMOVE);               //  Put Rook move in list          //0759:         CALL    ADMOVE          ; Put Rook move in list
        CALLu   (ADJPTR);               //  Re-adjust move list pointer    //0760:         CALL    ADJPTR          ; Re-adjust move list pointer
        LD      (a,val(M3));            //  Restore King position          //0761:         LD      a,(M3)          ; Restore King position
        LD      (val(M1),a);            //  Store                          //0762:         LD      (M1),a          ; Store
CA20:   LD      (a,b);                  //  Scan Index                     //0763: CA20:   LD      a,b             ; Scan Index
        CP      (1);                    //  Done ?                         //0764:         CP      1               ; Done ?
        RET     (Z);                    //  Yes - return                   //0765:         RET     Z               ; Yes - return
        LD      (bc,0x01FC);            //  Set Queen-side initial values  //0766:         LD      bc,01FCH        ; Set Queen-side initial values
        JPu     (CA5);                  //  Jump                           //0767:         JP      CA5             ; Jump
}                                                                          //0768: 
                                                                           //0769: ;***********************************************************
//***********************************************************              //0770: ; ADMOVE ROUTINE
// ADMOVE ROUTINE
//***********************************************************
// FUNCTION:   --  To add a move to the move list                          //0771: ;***********************************************************
//                                                                         //0772: ; FUNCTION:   --  To add a move to the move list
// CALLED BY:  --  MPIECE                                                  //0773: ;
//                 ENPSNT                                                  //0774: ; CALLED BY:  --  MPIECE
//                 CASTLE                                                  //0775: ;                 ENPSNT
//                                                                         //0776: ;                 CASTLE
// CALLS:      --  None                                                    //0777: ;
//                                                                         //0778: ; CALLS:      --  None
// ARGUMENT:  --  None                                                     //0779: ;
//***********************************************************              //0780: ; ARGUMENT:  --  None
void ADMOVE() {                                                            //0781: ;***********************************************************
        LD      (de,val16(MLNXT));      //  Addr of next loc in move list  //0782: ADMOVE: LD      de,(MLNXT)      ; Addr of next loc in move list
        LD      (hl,addr(MLEND));       //  Address of list end            //0783:         LD      hl,MLEND        ; Address of list end
        AND     (a);                    //  Clear carry flag               //0784:         AND     a               ; Clear carry flag
        SBC16   (hl,de);                //  Calculate difference           //0785:         SBC     hl,de           ; Calculate difference
        JR      (C,AM10);               //  Jump if out of space           //0786:         JR      C,AM10          ; Jump if out of space
        LD      (hl,val16(MLLST));      //  Addr of prev. list area        //0787:         LD      hl,(MLLST)      ; Addr of prev. list area
        LD      (val16(MLLST),de);      //  Save next as previous          //0788:         LD      (MLLST),de      ; Save next as previous
        LD      (ptr(hl),e);            //  Store link address             //0789:         LD      (hl),e          ; Store link address
        INC16   (hl);                                                      //0790:         INC     hl
        LD      (ptr(hl),d);                                               //0791:         LD      (hl),d
        LD      (hl,addr(P1));          //  Address of moved piece         //0792:         LD      hl,P1           ; Address of moved piece
        BIT     (3,ptr(hl));            //  Has it moved before ?          //0793:         BIT     3,(hl)          ; Has it moved before ?
        JR      (NZ,rel004);            //  Yes - jump                     //0794:         JR      NZ,rel004       ; Yes - jump
        LD      (hl,addr(P2));          //  Address of move flags          //0795:         LD      hl,P2           ; Address of move flags
        SET     (4,ptr(hl));            //  Set first move flag            //0796:         SET     4,(hl)          ; Set first move flag
rel004: EX      (de,hl);                //  Address of move area           //0797: rel004: EX      de,hl           ; Address of move area
        LD      (ptr(hl),0);            //  Store zero in link address     //0798:         LD      (hl),0          ; Store zero in link address
        INC16   (hl);                                                      //0799:         INC     hl
        LD      (ptr(hl),0);                                               //0800:         LD      (hl),0
        INC16   (hl);                                                      //0801:         INC     hl
        LD      (a,val(M1));            //  Store "from" move position     //0802:         LD      a,(M1)          ; Store "from" move position
        LD      (ptr(hl),a);                                               //0803:         LD      (hl),a
        INC16   (hl);                                                      //0804:         INC     hl
        LD      (a,val(M2));            //  Store "to" move position       //0805:         LD      a,(M2)          ; Store "to" move position
        LD      (ptr(hl),a);                                               //0806:         LD      (hl),a
        INC16   (hl);                                                      //0807:         INC     hl
        LD      (a,val(P2));            //  Store move flags/capt. piece   //0808:         LD      a,(P2)          ; Store move flags/capt. piece
        LD      (ptr(hl),a);                                               //0809:         LD      (hl),a
        INC16   (hl);                                                      //0810:         INC     hl
        LD      (ptr(hl),0);            //  Store initial move value       //0811:         LD      (hl),0          ; Store initial move value
        INC16   (hl);                                                      //0812:         INC     hl
        LD      (val16(MLNXT),hl);      //  Save address for next move     //0813:         LD      (MLNXT),hl      ; Save address for next move
        RETu;                           //  Return                         //0814:         RET                     ; Return
AM10:   LD      (ptr(hl),0);            //  Abort entry on table ovflow    //0815: AM10:   LD      (hl),0          ; Abort entry on table ovflow
        INC16   (hl);                                                      //0816:         INC     hl
        LD      (ptr(hl),0);            //  TODO does this out of memory   //0817:         LD      (hl),0          ; TODO does this out of memory
        DEC16   (hl);                   //       check actually work?      //0818:         DEC     hl              ;      check actually work?
        RETu;                                                              //0819:         RET
}                                                                          //0820: 
                                                                           //0821: ;***********************************************************
//***********************************************************              //0822: ; GENERATE MOVE ROUTINE
// GENERATE MOVE ROUTINE
//***********************************************************
// FUNCTION:  --  To generate the move set for all of the                  //0823: ;***********************************************************
//                pieces of a given color.                                 //0824: ; FUNCTION:  --  To generate the move set for all of the
//                                                                         //0825: ;                pieces of a given color.
// CALLED BY: --  FNDMOV                                                   //0826: ;
//                                                                         //0827: ; CALLED BY: --  FNDMOV
// CALLS:     --  MPIECE                                                   //0828: ;
//                INCHK                                                    //0829: ; CALLS:     --  MPIECE
//                                                                         //0830: ;                INCHK
// ARGUMENTS: --  None                                                     //0831: ;
//***********************************************************              //0832: ; ARGUMENTS: --  None
void GENMOV() {                                                            //0833: ;***********************************************************
        CALLu   (INCHK);                //  Test for King in check         //0834: GENMOV: CALL    INCHK           ; Test for King in check
        LD      (val(CKFLG),a);         //  Save attack count as flag      //0835:         LD      (CKFLG),a       ; Save attack count as flag
        LD      (de,val16(MLNXT));      //  Addr of next avail list space  //0836:         LD      de,(MLNXT)      ; Addr of next avail list space
        LD      (hl,val16(MLPTRI));     //  Ply list pointer index         //0837:         LD      hl,(MLPTRI)     ; Ply list pointer index
        INC16   (hl);                   //  Increment to next ply          //0838:         INC     hl              ; Increment to next ply
        INC16   (hl);                                                      //0839:         INC     hl
        LD      (ptr(hl),e);            //  Save move list pointer         //0840:         LD      (hl),e          ; Save move list pointer
        INC16   (hl);                                                      //0841:         INC     hl
        LD      (ptr(hl),d);                                               //0842:         LD      (hl),d
        INC16   (hl);                                                      //0843:         INC     hl
        LD      (val16(MLPTRI),hl);     //  Save new index                 //0844:         LD      (MLPTRI),hl     ; Save new index
        LD      (val16(MLLST),hl);      //  Last pointer for chain init.   //0845:         LD      (MLLST),hl      ; Last pointer for chain init.
        LD      (a,21);                 //  First position on board        //0846:         LD      a,21            ; First position on board
GM5:    LD      (val(M1),a);            //  Save as index                  //0847: GM5:    LD      (M1),a          ; Save as index
        LD      (ix,val16(M1));         //  Load board index               //0848:         LD      ix,(M1)         ; Load board index
        LD      (a,ptr(ix+BOARD));      //  Fetch board contents           //0849:         LD      a,(ix+BOARD)    ; Fetch board contents
        AND     (a);                    //  Is it empty ?                  //0850:         AND     a               ; Is it empty ?
        JR      (Z,GM10);               //  Yes - Jump                     //0851:         JR      Z,GM10          ; Yes - Jump
        CP      (-1);                   //  Is it a border square ?        //0852:         CP      -1              ; Is it a border square ?
        JR      (Z,GM10);               //  Yes - Jump                     //0853:         JR      Z,GM10          ; Yes - Jump
        LD      (val(P1),a);            //  Save piece                     //0854:         LD      (P1),a          ; Save piece
        LD      (hl,val(COLOR));        //  Address of color of piece      //0855:         LD      hl,COLOR        ; Address of color of piece
        XOR     (ptr(hl));              //  Test color of piece            //0856:         XOR     (hl)            ; Test color of piece
        BIT     (7,a);                  //  Match ?                        //0857:         BIT     7,a             ; Match ?
        CALL    (Z,MPIECE);             //  Yes - call Move Piece          //0858:         CALL    Z,MPIECE        ; Yes - call Move Piece
GM10:   LD      (a,val(M1));            //  Fetch current board position   //0859: GM10:   LD      a,(M1)          ; Fetch current board position
        INC     (a);                    //  Incr to next board position    //0860:         INC     a               ; Incr to next board position
        CP      (99);                   //  End of board array ?           //0861:         CP      99              ; End of board array ?
        JP      (NZ,GM5);               //  No - Jump                      //0862:         JP      NZ,GM5          ; No - Jump
        RETu;                           //  Return                         //0863:         RET                     ; Return
}                                                                          //0864: 
                                                                           //0865: ;***********************************************************
//***********************************************************              //0866: ; CHECK ROUTINE
// CHECK ROUTINE
//***********************************************************
// FUNCTION:   --  To determine whether or not the                         //0867: ;***********************************************************
//                 King is in check.                                       //0868: ; FUNCTION:   --  To determine whether or not the
//                                                                         //0869: ;                 King is in check.
// CALLED BY:  --  GENMOV                                                  //0870: ;
//                 FNDMOV                                                  //0871: ; CALLED BY:  --  GENMOV
//                 EVAL                                                    //0872: ;                 FNDMOV
//                                                                         //0873: ;                 EVAL
// CALLS:      --  ATTACK                                                  //0874: ;
//                                                                         //0875: ; CALLS:      --  ATTACK
// ARGUMENTS:  --  Color of king                                           //0876: ;
//***********************************************************              //0877: ; ARGUMENTS:  --  Color of King
void INCHK() {                                                             //0878: ;***********************************************************
        LD      (a,val(COLOR));         //  Get color                      //0879: INCHK:  LD      a,(COLOR)       ; Get color
        CALLu   (INCHK1);
}

void INCHK1() {                         // Like INCHK() but takes input in register A
        LD      (hl,addr(POSK));        //  Addr of white King position    //0880: INCHK1: LD      hl,POSK         ; Addr of white King position
        AND     (a);                    //  White ?                        //0881:         AND     a               ; White ?
        JR      (Z,rel005);             //  Yes - Skip                     //0882:         JR      Z,rel005        ; Yes - Skip
        INC16   (hl);                   //  Addr of black King position    //0883:         INC     hl              ; Addr of black King position
rel005: LD      (a,ptr(hl));            //  Fetch King position            //0884: rel005: LD      a,(hl)          ; Fetch King position
        LD      (val(M3),a);            //  Save                           //0885:         LD      (M3),a          ; Save
        LD      (ix,val16(M3));         //  Load board index               //0886:         LD      ix,(M3)         ; Load board index
        LD      (a,ptr(ix+BOARD));      //  Fetch board contents           //0887:         LD      a,(ix+BOARD)    ; Fetch board contents
        LD      (val(P1),a);            //  Save                           //0888:         LD      (P1),a          ; Save
        AND     (7);                    //  Get piece type                 //0889:         AND     7               ; Get piece type
        LD      (val(T1),a);            //  Save                           //0890:         LD      (T1),a          ; Save
        CALLu   (ATTACK);               //  Look for attackers on King     //0891:         CALL    ATTACK          ; Look for attackers on King
        RETu;                           //  Return                         //0892:         RET                     ; Return
}                                                                          //0893: 
                                                                           //0894: ;***********************************************************
//***********************************************************              //0895: ; ATTACK ROUTINE
// ATTACK ROUTINE
//***********************************************************
// FUNCTION:   --  To find all attackers on a given square                 //0896: ;***********************************************************
//                 by scanning outward from the square                     //0897: ; FUNCTION:   --  To find all attackers on a given square
//                 until a piece is found that attacks                     //0898: ;                 by scanning outward from the square
//                 that square, or a piece is found that                   //0899: ;                 until a piece is found that attacks
//                 doesn't attack that square, or the edge                 //0900: ;                 that square, or a piece is found that
//                 of the board is reached.                                //0901: ;                 doesn't attack that square, or the edge
//                                                                         //0902: ;                 of the board is reached.
//                 In determining which pieces attack                      //0903: ;
//                 a square, this routine also takes into                  //0904: ;                 In determining which pieces attack
//                 account the ability of certain pieces to                //0905: ;                 a square, this routine also takes into
//                 attack through another attacking piece. (For            //0906: ;                 account the ability of certain pieces to
//                 example a queen lined up behind a bishop                //0907: ;                 attack through another attacking piece. (For
//                 of her same color along a diagonal.) The                //0908: ;                 example a queen lined up behind a bishop
//                 bishop is then said to be transparent to the            //0909: ;                 of her same color along a diagonal.) The
//                 queen, since both participate in the                    //0910: ;                 bishop is then said to be transparent to the
//                 attack.                                                 //0911: ;                 queen, since both participate in the
//                                                                         //0912: ;                 attack.
//                 In the case where this routine is called                //0913: ;
//                 by CASTLE or INCHK, the routine is                      //0914: ;                 In the case where this routine is called
//                 terminated as soon as an attacker of the                //0915: ;                 by CASTLE or INCHK, the routine is
//                 opposite color is encountered.                          //0916: ;                 terminated as soon as an attacker of the
//                                                                         //0917: ;                 opposite color is encountered.
// CALLED BY:  --  POINTS                                                  //0918: ;
//                 PINFND                                                  //0919: ; CALLED BY:  --  POINTS
//                 CASTLE                                                  //0920: ;                 PINFND
//                 INCHK                                                   //0921: ;                 CASTLE
//                                                                         //0922: ;                 INCHK
// CALLS:      --  PATH                                                    //0923: ;
//                 ATKSAV                                                  //0924: ; CALLS:      --  PATH
//                                                                         //0925: ;                 ATKSAV
// ARGUMENTS:  --  None                                                    //0926: ;
//***********************************************************              //0927: ; ARGUMENTS:  --  None
void ATTACK() {                                                            //0928: ;***********************************************************
        PUSH    (bc);                   //  Save Register B                //0929: ATTACK: PUSH    bc              ; Save Register B
        XOR     (a);                    //  Clear                          //0930:         XOR     a               ; Clear
        LD      (b,16);                 //  Initial direction count        //0931:         LD      b,16            ; Initial direction count
        LD      (val(INDX2),a);         //  Initial direction index        //0932:         LD      (INDX2),a       ; Initial direction index
        LD      (iy,val16(INDX2));      //  Load index                     //0933:         LD      iy,(INDX2)      ; Load index
AT5:    LD      (c,ptr(iy+DIRECT));     //  Get direction                  //0934: AT5:    LD      c,(iy+DIRECT)   ; Get direction
        LD      (d,0);                  //  Init. scan count/flags         //0935:         LD      d,0             ; Init. scan count/flags
        LD      (a,val(M3));            //  Init. board start position     //0936:         LD      a,(M3)          ; Init. board start position
        LD      (val(M2),a);            //  Save                           //0937:         LD      (M2),a          ; Save
AT10:   INC     (d);                    //  Increment scan count           //0938: AT10:   INC     d               ; Increment scan count
        CALLu   (PATH);                 //  Next position                  //0939:         CALL    PATH            ; Next position
        CP      (1);                    //  Piece of a opposite color ?    //0940:         CP      1               ; Piece of a opposite color ?
        JR      (Z,AT14A);              //  Yes - jump                     //0941:         JR      Z,AT14A         ; Yes - jump
        CP      (2);                    //  Piece of same color ?          //0942:         CP      2               ; Piece of same color ?
        JR      (Z,AT14B);              //  Yes - jump                     //0943:         JR      Z,AT14B         ; Yes - jump
        AND     (a);                    //  Empty position ?               //0944:         AND     a               ; Empty position ?
        JR      (NZ,AT12);              //  No - jump                      //0945:         JR      NZ,AT12         ; No - jump
        LD      (a,b);                  //  Fetch direction count          //0946:         LD      a,b             ; Fetch direction count
        CP      (9);                    //  On knight scan ?               //0947:         CP      9               ; On knight scan ?
        JR      (NC,AT10);              //  No - jump                      //0948:         JR      NC,AT10         ; No - jump
AT12:   INC16   (iy);                   //  Increment direction index      //0949: AT12:   INC     iy              ; Increment direction index
        DJNZ    (AT5);                  //  Done ? No - jump               //0950:         DJNZ    AT5             ; Done ? No - jump
        XOR     (a);                    //  No attackers                   //0951:         XOR     a               ; No attackers
AT13:   POP     (bc);                   //  Restore register B             //0952: AT13:   POP     bc              ; Restore register B
        RETu;                           //  Return                         //0953:         RET                     ; Return
AT14A:  BIT     (6,d);                  //  Same color found already ?     //0954: AT14A:  BIT     6,d             ; Same color found already ?
        JR      (NZ,AT12);              //  Yes - jump                     //0955:         JR      NZ,AT12         ; Yes - jump
        SET     (5,d);                  //  Set opposite color found flag  //0956:         SET     5,d             ; Set opposite color found flag
        JPu     (AT14);                 //  Jump                           //0957:         JP      AT14            ; Jump
AT14B:  BIT     (5,d);                  //  Opposite color found already?  //0958: AT14B:  BIT     5,d             ; Opposite color found already?
        JR      (NZ,AT12);              //  Yes - jump                     //0959:         JR      NZ,AT12         ; Yes - jump
        SET     (6,d);                  //  Set same color found flag      //0960:         SET     6,d             ; Set same color found flag
                                                                           //0961: 
//                                                                         //0962: ;
// ***** DETERMINE IF PIECE ENCOUNTERED ATTACKS SQUARE *****               //0963: ; ***** DETERMINE IF PIECE ENCOUNTERED ATTACKS SQUARE *****
AT14:   LD      (a,val(T2));            //  Fetch piece type encountered   //0964: AT14:   LD      a,(T2)          ; Fetch piece type encountered
        LD      (e,a);                  //  Save                           //0965:         LD      e,a             ; Save
        LD      (a,b);                  //  Get direction-counter          //0966:         LD      a,b             ; Get direction-counter
        CP      (9);                    //  Look for Knights ?             //0967:         CP      9               ; Look for Knights ?
        JR      (C,AT25);               //  Yes - jump                     //0968:         JR      C,AT25          ; Yes - jump
        LD      (a,e);                  //  Get piece type                 //0969:         LD      a,e             ; Get piece type
        CP      (QUEEN);                //  Is is a Queen ?                //0970:         CP      QUEEN           ; Is is a Queen ?
        JR      (NZ,AT15);              //  No - Jump                      //0971:         JR      NZ,AT15         ; No - Jump
        SET     (7,d);                  //  Set Queen found flag           //0972:         SET     7,d             ; Set Queen found flag
        JRu     (AT30);                 //  Jump                           //0973:         JR      AT30            ; Jump
AT15:   LD      (a,d);                  //  Get flag/scan count            //0974: AT15:   LD      a,d             ; Get flag/scan count
        AND     (0x0F);                 //  Isolate count                  //0975:         AND     0FH             ; Isolate count
        CP      (1);                    //  On first position ?            //0976:         CP      1               ; On first position ?
        JR      (NZ,AT16);              //  No - jump                      //0977:         JR      NZ,AT16         ; No - jump
        LD      (a,e);                  //  Get encountered piece type     //0978:         LD      a,e             ; Get encountered piece type
        CP      (KING);                 //  Is it a King ?                 //0979:         CP      KING            ; Is it a King ?
        JR      (Z,AT30);               //  Yes - jump                     //0980:         JR      Z,AT30          ; Yes - jump
AT16:   LD      (a,b);                  //  Get direction counter          //0981: AT16:   LD      a,b             ; Get direction counter
        CP      (13);                   //  Scanning files or ranks ?      //0982:         CP      13              ; Scanning files or ranks ?
        JR      (C,AT21);               //  Yes - jump                     //0983:         JR      C,AT21          ; Yes - jump
        LD      (a,e);                  //  Get piece type                 //0984:         LD      a,e             ; Get piece type
        CP      (BISHOP);               //  Is it a Bishop ?               //0985:         CP      BISHOP          ; Is it a Bishop ?
        JR      (Z,AT30);               //  Yes - jump                     //0986:         JR      Z,AT30          ; Yes - jump
        LD      (a,d);                  //  Get flags/scan count           //0987:         LD      a,d             ; Get flags/scan count
        AND     (0x0F);                 //  Isolate count                  //0988:         AND     0FH             ; Isolate count
        CP      (1);                    //  On first position ?            //0989:         CP      1               ; On first position ?
        JR      (NZ,AT12);              //  No - jump                      //0990:         JR      NZ,AT12         ; No - jump
        CP      (e);                    //  Is it a Pawn ?                 //0991:         CP      e               ; Is it a Pawn ?
        JR      (NZ,AT12);              //  No - jump                      //0992:         JR      NZ,AT12         ; No - jump
        LD      (a,val(P2));            //  Fetch piece including color    //0993:         LD      a,(P2)          ; Fetch piece including color
        BIT     (7,a);                  //  Is it white ?                  //0994:         BIT     7,a             ; Is it white ?
        JR      (Z,AT20);               //  Yes - jump                     //0995:         JR      Z,AT20          ; Yes - jump
        LD      (a,b);                  //  Get direction counter          //0996:         LD      a,b             ; Get direction counter
        CP      (15);                   //  On a non-attacking diagonal ?  //0997:         CP      15              ; On a non-attacking diagonal ?
        JR      (C,AT12);               //  Yes - jump                     //0998:         JR      C,AT12          ; Yes - jump
        JRu     (AT30);                 //  Jump                           //0999:         JR      AT30            ; Jump
AT20:   LD      (a,b);                  //  Get direction counter          //1000: AT20:   LD      a,b             ; Get direction counter
        CP      (15);                   //  On a non-attacking diagonal ?  //1001:         CP      15              ; On a non-attacking diagonal ?
        JR      (NC,AT12);              //  Yes - jump                     //1002:         JR      NC,AT12         ; Yes - jump
        JRu     (AT30);                 //  Jump                           //1003:         JR      AT30            ; Jump
AT21:   LD      (a,e);                  //  Get piece type                 //1004: AT21:   LD      a,e             ; Get piece type
        CP      (ROOK);                 //  Is is a Rook ?                 //1005:         CP      ROOK            ; Is is a Rook ?
        JR      (NZ,AT12);              //  No - jump                      //1006:         JR      NZ,AT12         ; No - jump
        JRu     (AT30);                 //  Jump                           //1007:         JR      AT30            ; Jump
AT25:   LD      (a,e);                  //  Get piece type                 //1008: AT25:   LD      a,e             ; Get piece type
        CP      (KNIGHT);               //  Is it a Knight ?               //1009:         CP      KNIGHT          ; Is it a Knight ?
        JR      (NZ,AT12);              //  No - jump                      //1010:         JR      NZ,AT12         ; No - jump
AT30:   LD      (a,val(T1));            //  Attacked piece type/flag       //1011: AT30:   LD      a,(T1)          ; Attacked piece type/flag
        CP      (7);                    //  Call from POINTS ?             //1012:         CP      7               ; Call from POINTS ?
        JR      (Z,AT31);               //  Yes - jump                     //1013:         JR      Z,AT31          ; Yes - jump
        BIT     (5,d);                  //  Is attacker opposite color ?   //1014:         BIT     5,d             ; Is attacker opposite color ?
        JR      (Z,AT32);               //  No - jump                      //1015:         JR      Z,AT32          ; No - jump
        LD      (a,1);                  //  Set attacker found flag        //1016:         LD      a,1             ; Set attacker found flag
        JPu     (AT13);                 //  Jump                           //1017:         JP      AT13            ; Jump
AT31:   CALLu   (ATKSAV);               //  Save attacker in attack list   //1018: AT31:   CALL    ATKSAV          ; Save attacker in attack list
AT32:   LD      (a,val(T2));            //  Attacking piece type           //1019: AT32:   LD      a,(T2)          ; Attacking piece type
        CP      (KING);                 //  Is it a King,?                 //1020:         CP      KING            ; Is it a King,?
        JP      (Z,AT12);               //  Yes - jump                     //1021:         JP      Z,AT12          ; Yes - jump
        CP      (KNIGHT);               //  Is it a Knight ?               //1022:         CP      KNIGHT          ; Is it a Knight ?
        JP      (Z,AT12);               //  Yes - jump                     //1023:         JP      Z,AT12          ; Yes - jump
        JPu     (AT10);                 //  Jump                           //1024:         JP      AT10            ; Jump
 }

//***********************************************************              //1025: 
// ATTACK SAVE ROUTINE                                                     //1026: ;***********************************************************
//***********************************************************              //1027: ; ATTACK SAVE ROUTINE
// FUNCTION:   --  To save an attacking piece value in the                 //1028: ;***********************************************************
//                 attack list, and to increment the attack                //1029: ; FUNCTION:   --  To save an attacking piece value in the
//                 count for that color piece.                             //1030: ;                 attack list, and to increment the attack
//                                                                         //1031: ;                 count for that color piece.
//                 The pin piece list is checked for the                   //1032: ;
//                 attacking piece, and if found there, the                //1033: ;                 The pin piece list is checked for the
//                 piece is not included in the attack list.               //1034: ;                 attacking piece, and if found there, the
//                                                                         //1035: ;                 piece is not included in the attack list.
// CALLED BY:  --  ATTACK                                                  //1036: ;
//                                                                         //1037: ; CALLED BY:  --  ATTACK
// CALLS:      --  PNCK                                                    //1038: ;
//                                                                         //1039: ; CALLS:      --  PNCK
// ARGUMENTS:  --  None                                                    //1040: ;
//***********************************************************              //1041: ; ARGUMENTS:  --  None
void ATKSAV() {                                                            //1042: ;***********************************************************
        PUSH    (bc);                   //  Save Regs BC                   //1043: ATKSAV: PUSH    bc              ; Save Regs BC
        PUSH    (de);                   //  Save Regs DE                   //1044:         PUSH    de              ; Save Regs DE
        LD      (a,val(NPINS));         //  Number of pinned pieces        //1045:         LD      a,(NPINS)       ; Number of pinned pieces
        AND     (a);                    //  Any ?                          //1046:         AND     a               ; Any ?
        CALL    (NZ,PNCK);              //  yes - check pin list           //1047:         CALL    NZ,PNCK         ; yes - check pin list
        LD      (ix,val16(T2));         //  Init index to value table      //1048:         LD      ix,(T2)         ; Init index to value table
        LD      (hl,addr(ATKLST));      //  Init address of attack list    //1049:         LD      hl,ATKLST       ; Init address of attack list
        LD      (bc,0);                 //  Init increment for white       //1050:         LD      bc,0            ; Init increment for white
        LD      (a,val(P2));            //  Attacking piece                //1051:         LD      a,(P2)          ; Attacking piece
        BIT     (7,a);                  //  Is it white ?                  //1052:         BIT     7,a             ; Is it white ?
        JR      (Z,rel006);             //  Yes - jump                     //1053:         JR      Z,rel006        ; Yes - jump
        LD      (c,7);                  //  Init increment for black       //1054:         LD      c,7             ; Init increment for black
rel006: AND     (7);                    //  Attacking piece type           //1055: rel006: AND     7               ; Attacking piece type
        LD      (e,a);                  //  Init increment for type        //1056:         LD      e,a             ; Init increment for type
        BIT     (7,d);                  //  Queen found this scan ?        //1057:         BIT     7,d             ; Queen found this scan ?
        JR      (Z,rel007);             //  No - jump                      //1058:         JR      Z,rel007        ; No - jump
        LD      (e,QUEEN);              //  Use Queen slot in attack list  //1059:         LD      e,QUEEN         ; Use Queen slot in attack list
rel007: ADD16   (hl,bc);                //  Attack list address            //1060: rel007: ADD     hl,bc           ; Attack list address
        INC     (ptr(hl));              //  Increment list count           //1061:         INC     (hl)            ; Increment list count
        LD      (d,0);                                                     //1062:         LD      d,0
        ADD16   (hl,de);                //  Attack list slot address       //1063:         ADD     hl,de           ; Attack list slot address
        LD      (a,ptr(hl));            //  Get data already there         //1064:         LD      a,(hl)          ; Get data already there
        AND     (0x0f);                 //  Is first slot empty ?          //1065:         AND     0FH             ; Is first slot empty ?
        JR      (Z,AS20);               //  Yes - jump                     //1066:         JR      Z,AS20          ; Yes - jump
        LD      (a,ptr(hl));            //  Get data again                 //1067:         LD      a,(hl)          ; Get data again
        AND     (0xf0);                 //  Is second slot empty ?         //1068:         AND     0F0H            ; Is second slot empty ?
        JR      (Z,AS19);               //  Yes - jump                     //1069:         JR      Z,AS19          ; Yes - jump
        INC16   (hl);                   //  Increment to King slot         //1070:         INC     hl              ; Increment to King slot
        JRu     (AS20);                 //  Jump                           //1071:         JR      AS20            ; Jump
AS19:   RLD;                            //  Temp save lower in upper       //1072: AS19:   RLD                     ; Temp save lower in upper
        LD      (a,ptr(ix+PVALUE));     //  Get new value for attack list  //1073:         LD      a,(ix+PVALUE)   ; Get new value for attack list
        RRD;                            //  Put in 2nd attack list slot    //1074:         RRD                     ; Put in 2nd attack list slot
        JRu     (AS25);                 //  Jump                           //1075:         JR      AS25            ; Jump
AS20:   LD      (a,ptr(ix+PVALUE));     //  Get new value for attack list  //1076: AS20:   LD      a,(ix+PVALUE)   ; Get new value for attack list
        RLD;                            //  Put in 1st attack list slot    //1077:         RLD                     ; Put in 1st attack list slot
AS25:   POP     (de);                   //  Restore DE regs                //1078: AS25:   POP     de              ; Restore DE regs
        POP     (bc);                   //  Restore BC regs                //1079:         POP     bc              ; Restore BC regs
        RETu;                           //  Return                         //1080:         RET                     ; Return
 }                                                                         //1081: 
                                                                           //1082: ;***********************************************************
//***********************************************************              //1083: ; PIN CHECK ROUTINE
// PIN CHECK ROUTINE
//***********************************************************
// FUNCTION:   --  Checks to see if the attacker is in the                 //1084: ;***********************************************************
//                 pinned piece list. If so he is not a valid              //1085: ; FUNCTION:   --  Checks to see if the attacker is in the
//                 attacker unless the direction in which he               //1086: ;                 pinned piece list. If so he is not a valid
//                 attacks is the same as the direction along              //1087: ;                 attacker unless the direction in which he
//                 which he is pinned. If the piece is                     //1088: ;                 attacks is the same as the direction along
//                 found to be invalid as an attacker, the                 //1089: ;                 which he is pinned. If the piece is
//                 return to the calling routine is aborted                //1090: ;                 found to be invalid as an attacker, the
//                 and this routine returns directly to ATTACK.            //1091: ;                 return to the calling routine is aborted
//                                                                         //1092: ;                 and this routine returns directly to ATTACK.
// CALLED BY:  --  ATKSAV                                                  //1093: ;
//                                                                         //1094: ; CALLED BY:  --  ATKSAV
// CALLS:      --  None                                                    //1095: ;
//                                                                         //1096: ; CALLS:      --  None
// ARGUMENTS:  --  The direction of the attack. The                        //1097: ;
//                 pinned piece counnt.                                    //1098: ; ARGUMENTS:  --  The direction of the attack. The
//***********************************************************              //1099: ;                 pinned piece counnt.
void PNCK() {                                                              //1100: ;***********************************************************
        LD      (d,c);                  //  Save attack direction          //1101: PNCK:   LD      d,c             ; Save attack direction
        LD      (e,0);                  //  Clear flag                     //1102:         LD      e,0             ; Clear flag
        LD      (c,a);                  //  Load pin count for search      //1103:         LD      c,a             ; Load pin count for search
        LD      (b,0);                                                     //1104:         LD      b,0
        LD      (a,val(M2));            //  Position of piece              //1105:         LD      a,(M2)          ; Position of piece
        LD      (hl,addr(PLISTA));      //  Pin list address               //1106:         LD      hl,PLISTA       ; Pin list address
PC1:    Z80_CPIR;                       //  Search list for position       //1107: PC1:    CPIR                    ; Search list for position
        RET     (NZ);                   //  Return if not found            //1108:         RET     NZ              ; Return if not found
        Z80_EXAF;                       //  Save search parameters         //1109:         EX      af,af'          ; Save search parameters
        BIT     (0,e);                  //  Is this the first find ?       //1110:         BIT     0,e             ; Is this the first find ?
        JR      (NZ,PC5);               //  No - jump                      //1111:         JR      NZ,PC5          ; No - jump
        SET     (0,e);                  //  Set first find flag            //1112:         SET     0,e             ; Set first find flag
        PUSH    (hl);                   //  Get corresp index to dir list  //1113:         PUSH    hl              ; Get corresp index to dir list
        POP     (ix);                                                      //1114:         POP     ix
        LD      (a,ptr(ix+9));          //  Get direction                  //1115:         LD      a,(ix+9)        ; Get direction
        CP      (d);                    //  Same as attacking direction ?  //1116:         CP      d               ; Same as attacking direction ?
        JR      (Z,PC3);                //  Yes - jump                     //1117:         JR      Z,PC3           ; Yes - jump
        NEG;                            //  Opposite direction ?           //1118:         NEG                     ; Opposite direction ?
        CP      (d);                    //  Same as attacking direction ?  //1119:         CP      d               ; Same as attacking direction ?
        JR      (NZ,PC5);               //  No - jump                      //1120:         JR      NZ,PC5          ; No - jump
PC3:    Z80_EXAF;                       //  Restore search parameters      //1121: PC3:    EX      af,af'          ; Restore search parameters
        JP      (PE,PC1);               //  Jump if search not complete    //1122:         JP      PE,PC1          ; Jump if search not complete
        RETu;                           //  Return                         //1123:         RET                     ; Return
PC5:    POPf    (af);                   //  Abnormal exit                  //1124: PC5:    POP     af              ; Abnormal exit
        POP     (de);                   //  Restore regs.                  //1125:         POP     de              ; Restore regs.
        POP     (bc);                                                      //1126:         POP     bc
        RETu;                           //  Return to ATTACK               //1127:         RET                     ; Return to ATTACK
}                                                                          //1128: 
                                                                           //1129: ;***********************************************************
//***********************************************************              //1130: ; PIN FIND ROUTINE
// PIN FIND ROUTINE
//***********************************************************
// FUNCTION:   --  To produce a list of all pieces pinned                  //1131: ;***********************************************************
//                 against the King or Queen, for both white               //1132: ; FUNCTION:   --  To produce a list of all pieces pinned
//                 and black.                                              //1133: ;                 against the King or Queen, for both white
//                                                                         //1134: ;                 and black.
// CALLED BY:  --  FNDMOV                                                  //1135: ;
//                 EVAL                                                    //1136: ; CALLED BY:  --  FNDMOV
//                                                                         //1137: ;                 EVAL
// CALLS:      --  PATH                                                    //1138: ;
//                 ATTACK                                                  //1139: ; CALLS:      --  PATH
//                                                                         //1140: ;                 ATTACK
// ARGUMENTS:  --  None                                                    //1141: ;
//***********************************************************              //1142: ; ARGUMENTS:  --  None
void PINFND() {                                                            //1143: ;***********************************************************
        XOR     (a);                    //  Zero pin count                 //1144: PINFND: XOR     a               ; Zero pin count
        LD      (val(NPINS),a);                                            //1145:         LD      (NPINS),a
        LD      (de,addr(POSK));        //  Addr of King/Queen pos list    //1146:         LD      de,POSK         ; Addr of King/Queen pos list
PF1:    LD      (a,ptr(de));            //  Get position of royal piece    //1147: PF1:    LD      a,(de)          ; Get position of royal piece
        AND     (a);                    //  Is it on board ?               //1148:         AND     a               ; Is it on board ?
        JP      (Z,PF26);               //  No- jump                       //1149:         JP      Z,PF26          ; No- jump
        CP      (-1);                   //  At end of list ?               //1150:         CP      -1              ; At end of list ?
        RET     (Z);                    //  Yes return                     //1151:         RET     Z               ; Yes return
        LD      (val(M3),a);            //  Save position as board index   //1152:         LD      (M3),a          ; Save position as board index
        LD      (ix,val16(M3));         //  Load index to board            //1153:         LD      ix,(M3)         ; Load index to board
        LD      (a,ptr(ix+BOARD));      //  Get contents of board          //1154:         LD      a,(ix+BOARD)    ; Get contents of board
        LD      (val(P1),a);            //  Save                           //1155:         LD      (P1),a          ; Save
        LD      (b,8);                  //  Init scan direction count      //1156:         LD      b,8             ; Init scan direction count
        XOR     (a);                                                       //1157:         XOR     a
        LD      (val(INDX2),a);         //  Init direction index           //1158:         LD      (INDX2),a       ; Init direction index
        LD      (iy,val16(INDX2));                                         //1159:         LD      iy,(INDX2)
PF2:    LD      (a,val(M3));            //  Get King/Queen position        //1160: PF2:    LD      a,(M3)          ; Get King/Queen position
        LD      (val(M2),a);            //  Save                           //1161:         LD      (M2),a          ; Save
        XOR     (a);                                                       //1162:         XOR     a
        LD      (val(M4),a);            //  Clear pinned piece saved pos   //1163:         LD      (M4),a          ; Clear pinned piece saved pos
        LD      (c,ptr(iy+DIRECT));     //  Get direction of scan          //1164:         LD      c,(iy+DIRECT)   ; Get direction of scan
PF5:    CALLu   (PATH);                 //  Compute next position          //1165: PF5:    CALL    PATH            ; Compute next position
        AND     (a);                    //  Is it empty ?                  //1166:         AND     a               ; Is it empty ?
        JR      (Z,PF5);                //  Yes - jump                     //1167:         JR      Z,PF5           ; Yes - jump
        CP      (3);                    //  Off board ?                    //1168:         CP      3               ; Off board ?
        JP      (Z,PF25);               //  Yes - jump                     //1169:         JP      Z,PF25          ; Yes - jump
        CP      (2);                    //  Piece of same color            //1170:         CP      2               ; Piece of same color
        LD      (a,val(M4));            //  Load pinned piece position     //1171:         LD      a,(M4)          ; Load pinned piece position
        JR      (Z,PF15);               //  Yes - jump                     //1172:         JR      Z,PF15          ; Yes - jump
        AND     (a);                    //  Possible pin ?                 //1173:         AND     a               ; Possible pin ?
        JP      (Z,PF25);               //  No - jump                      //1174:         JP      Z,PF25          ; No - jump
        LD      (a,val(T2));            //  Piece type encountered         //1175:         LD      a,(T2)          ; Piece type encountered
        CP      (QUEEN);                //  Queen ?                        //1176:         CP      QUEEN           ; Queen ?
        JP      (Z,PF19);               //  Yes - jump                     //1177:         JP      Z,PF19          ; Yes - jump
        LD      (l,a);                  //  Save piece type                //1178:         LD      l,a             ; Save piece type
        LD      (a,b);                  //  Direction counter              //1179:         LD      a,b             ; Direction counter
        CP      (5);                    //  Non-diagonal direction ?       //1180:         CP      5               ; Non-diagonal direction ?
        JR      (C,PF10);               //  Yes - jump                     //1181:         JR      C,PF10          ; Yes - jump
        LD      (a,l);                  //  Piece type                     //1182:         LD      a,l             ; Piece type
        CP      (BISHOP);               //  Bishop ?                       //1183:         CP      BISHOP          ; Bishop ?
        JP      (NZ,PF25);              //  No - jump                      //1184:         JP      NZ,PF25         ; No - jump
        JPu     (PF20);                 //  Jump                           //1185:         JP      PF20            ; Jump
PF10:   LD      (a,l);                  //  Piece type                     //1186: PF10:   LD      a,l             ; Piece type
        CP      (ROOK);                 //  Rook ?                         //1187:         CP      ROOK            ; Rook ?
        JP      (NZ,PF25);              //  No - jump                      //1188:         JP      NZ,PF25         ; No - jump
        JPu     (PF20);                 //  Jump                           //1189:         JP      PF20            ; Jump
PF15:   AND     (a);                    //  Possible pin ?                 //1190: PF15:   AND     a               ; Possible pin ?
        JP      (NZ,PF25);              //  No - jump                      //1191:         JP      NZ,PF25         ; No - jump
        LD      (a,val(M2));            //  Save possible pin position     //1192:         LD      a,(M2)          ; Save possible pin position
        LD      (val(M4),a);                                               //1193:         LD      (M4),a
        JPu     (PF5);                  //  Jump                           //1194:         JP      PF5             ; Jump
PF19:   LD      (a,val(P1));            //  Load King or Queen             //1195: PF19:   LD      a,(P1)          ; Load King or Queen
        AND     (7);                    //  Clear flags                    //1196:         AND     7               ; Clear flags
        CP      (QUEEN);                //  Queen ?                        //1197:         CP      QUEEN           ; Queen ?
        JR      (NZ,PF20);              //  No - jump                      //1198:         JR      NZ,PF20         ; No - jump
        PUSH    (bc);                   //  Save regs.                     //1199:         PUSH    bc              ; Save regs.
        PUSH    (de);                                                      //1200:         PUSH    de
        PUSH    (iy);                                                      //1201:         PUSH    iy
        XOR     (a);                    //  Zero out attack list           //1202:         XOR     a               ; Zero out attack list
        LD      (b,14);                                                    //1203:         LD      b,14
        LD      (hl,addr(ATKLST));                                         //1204:         LD      hl,ATKLST
back02: LD      (ptr(hl),a);                                               //1205: back02: LD      (hl),a
        INC16   (hl);                                                      //1206:         INC     hl
        DJNZ    (back02);                                                  //1207:         DJNZ    back02
        LD      (a,7);                  //  Set attack flag                //1208:         LD      a,7             ; Set attack flag
        LD      (val(T1),a);                                               //1209:         LD      (T1),a
        CALLu   (ATTACK);               //  Find attackers/defenders       //1210:         CALL    ATTACK          ; Find attackers/defenders
        LD      (hl,WACT);              //  White queen attackers          //1211:         LD      hl,WACT         ; White queen attackers
        LD      (de,BACT);              //  Black queen attackers          //1212:         LD      de,BACT         ; Black queen attackers
        LD      (a,val(P1));            //  Get queen                      //1213:         LD      a,(P1)          ; Get queen
        BIT     (7,a);                  //  Is she white ?                 //1214:         BIT     7,a             ; Is she white ?
        JR      (Z,rel008);             //  Yes - skip                     //1215:         JR      Z,rel008        ; Yes - skip
        EX      (de,hl);                //  Reverse for black              //1216:         EX      de,hl           ; Reverse for black
rel008: LD      (a,ptr(hl));            //  Number of defenders            //1217: rel008: LD      a,(hl)          ; Number of defenders
        EX      (de,hl);                //  Reverse for attackers          //1218:         EX      de,hl           ; Reverse for attackers
        SUB     (ptr(hl));              //  Defenders minus attackers      //1219:         SUB     (hl)            ; Defenders minus attackers
        DEC     (a);                    //  Less 1                         //1220:         DEC     a               ; Less 1
        POP     (iy);                   //  Restore regs.                  //1221:         POP     iy              ; Restore regs.
        POP     (de);                                                      //1222:         POP     de
        POP     (bc);                                                      //1223:         POP     bc
        JP      (P,PF25);               //  Jump if pin not valid          //1224:         JP      P,PF25          ; Jump if pin not valid
PF20:   LD      (hl,addr(NPINS));       //  Address of pinned piece count  //1225: PF20:   LD      hl,NPINS        ; Address of pinned piece count
        INC     (ptr(hl));              //  Increment                      //1226:         INC     (hl)            ; Increment
        LD      (ix,val16(NPINS));      //  Load pin list index            //1227:         LD      ix,(NPINS)      ; Load pin list index
        LD      (ptr(ix+PLISTD),c);     //  Save direction of pin          //1228:         LD      (ix+PLISTD),c   ; Save direction of pin
        LD      (a,val(M4));            //  Position of pinned piece       //1229:         LD      a,(M4)          ; Position of pinned piece
        LD      (ptr(ix+PLIST),a);      //  Save in list                   //1230:         LD      (ix+PLIST),a    ; Save in list
PF25:   INC16   (iy);                   //  Increment direction index      //1231: PF25:   INC     iy              ; Increment direction index
        DJNZ    (PF27);                 //  Done ? No - Jump               //1232:         DJNZ    PF27            ; Done ? No - Jump
PF26:   INC16   (de);                   //  Incr King/Queen pos index      //1233: PF26:   INC     de              ; Incr King/Queen pos index
        JPu     (PF1);                  //  Jump                           //1234:         JP      PF1             ; Jump
PF27:   JPu     (PF2);                  //  Jump                           //1235: PF27:   JP      PF2             ; Jump
}                                                                          //1236: 
                                                                           //1237: ;***********************************************************
//***********************************************************              //1238: ; EXCHANGE ROUTINE
// EXCHANGE ROUTINE
//***********************************************************
// FUNCTION:   --  To determine the exchange value of a                    //1239: ;***********************************************************
//                 piece on a given square by examining all                //1240: ; FUNCTION:   --  To determine the exchange value of a
//                 attackers and defenders of that piece.                  //1241: ;                 piece on a given square by examining all
//                                                                         //1242: ;                 attackers and defenders of that piece.
// CALLED BY:  --  POINTS                                                  //1243: ;
//                                                                         //1244: ; CALLED BY:  --  POINTS
// CALLS:      --  NEXTAD                                                  //1245: ;
//                                                                         //1246: ; CALLS:      --  NEXTAD
// ARGUMENTS:  --  None.                                                   //1247: ;
//***********************************************************              //1248: ; ARGUMENTS:  --  None.
void XCHNG() {                                                             //1249: ;***********************************************************
        EXX;                            //  Swap regs.                     //1250: XCHNG:  EXX                     ; Swap regs.
        LD      (a,(val(P1)));          //  Piece attacked                 //1251:         LD      a,(P1)          ; Piece attacked
        LD      (hl,WACT);        //  Addr of white attkrs/dfndrs          //1252:         LD      hl,WACT         ; Addr of white attkrs/dfndrs
        LD      (de,BACT);        //  Addr of black attkrs/dfndrs          //1253:         LD      de,BACT         ; Addr of black attkrs/dfndrs
        BIT     (7,a);                  //  Is piece white ?               //1254:         BIT     7,a             ; Is piece white ?
        JR      (Z,rel009);             //  Yes - jump                     //1255:         JR      Z,rel009        ; Yes - jump
        EX      (de,hl);                //  Swap list pointers             //1256:         EX      de,hl           ; Swap list pointers
rel009: LD      (b,ptr(hl));            //  Init list counts               //1257: rel009: LD      b,(hl)          ; Init list counts
        EX      (de,hl);                                                   //1258:         EX      de,hl
        LD      (c,ptr(hl));                                               //1259:         LD      c,(hl)
        EX      (de,hl);                                                   //1260:         EX      de,hl
        EXX;                            //  Restore regs.                  //1261:         EXX                     ; Restore regs.
        LD      (c,0);                  //  Init attacker/defender flag    //1262:         LD      c,0             ; Init attacker/defender flag
        LD      (e,0);                  //  Init points lost count         //1263:         LD      e,0             ; Init points lost count
        LD      (ix,val16(T3));         //  Load piece value index         //1264:         LD      ix,(T3)         ; Load piece value index
        LD      (d,ptr(ix+PVALUE));     //  Get attacked piece value       //1265:         LD      d,(ix+PVALUE)   ; Get attacked piece value
        SLA     (d);                    //  Double it                      //1266:         SLA     d               ; Double it
        LD      (b,d);                  //  Save                           //1267:         LD      b,d             ; Save
        CALLu   (NEXTAD);               //  Retrieve first attacker        //1268:         CALL    NEXTAD          ; Retrieve first attacker
        RET     (Z);                    //  Return if none                 //1269:         RET     Z               ; Return if none
XC10:   LD      (l,a);                  //  Save attacker value            //1270: XC10:   LD      l,a             ; Save attacker value
        CALLu   (NEXTAD);               //  Get next defender              //1271:         CALL    NEXTAD          ; Get next defender
        JR      (Z,XC18);               //  Jump if none                   //1272:         JR      Z,XC18          ; Jump if none
        Z80_EXAF;                       //  Save defender value            //1273:         EX      af,af'          ; Save defender value
        LD      (a,b);                  //  Get attacked value             //1274:         LD      a,b             ; Get attacked value
        CP      (l);                    //  Attacked less than attacker ?  //1275:         CP      l               ; Attacked less than attacker ?
        JR      (NC,XC19);              //  No - jump                      //1276:         JR      NC,XC19         ; No - jump
        Z80_EXAF;                       //  -Restore defender              //1277:         EX      af,af'          ; -Restore defender
XC15:   CP      (l);                    //  Defender less than attacker ?  //1278: XC15:   CP      l               ; Defender less than attacker ?
        RET     (C);                    //  Yes - return                   //1279:         RET     C               ; Yes - return
        CALLu   (NEXTAD);               //  Retrieve next attacker value   //1280:         CALL    NEXTAD          ; Retrieve next attacker value
        RET     (Z);                    //  Return if none                 //1281:         RET     Z               ; Return if none
        LD      (l,a);                  //  Save attacker value            //1282:         LD      l,a             ; Save attacker value
        CALLu   (NEXTAD);               //  Retrieve next defender value   //1283:         CALL    NEXTAD          ; Retrieve next defender value
        JR      (NZ,XC15);              //  Jump if none                   //1284:         JR      NZ,XC15         ; Jump if none
XC18:   Z80_EXAF;                       //  Save Defender                  //1285: XC18:   EX      af,af'          ; Save Defender
        LD      (a,b);                  //  Get value of attacked piece    //1286:         LD      a,b             ; Get value of attacked piece
XC19:   BIT     (0,c);                  //  Attacker or defender ?         //1287: XC19:   BIT     0,c             ; Attacker or defender ?
        JR      (Z,rel010);             //  Jump if defender               //1288:         JR      Z,rel010        ; Jump if defender
        NEG;                     //  Negate value for attacker             //1289:         NEG                     ; Negate value for attacker
rel010: ADD     (a,e);                  //  Total points lost              //1290: rel010: ADD     a,e             ; Total points lost
        LD      (e,a);                  //  Save total                     //1291:         LD      e,a             ; Save total
        Z80_EXAF;                       //  Restore previous defender      //1292:         EX      af,af'          ; Restore previous defender
        RET     (Z);                    //  Return if none                 //1293:         RET     Z               ; Return if none
        LD      (b,l);                  //  Prev attckr becomes defender   //1294:         LD      b,l             ; Prev attckr becomes defender
        JPu     (XC10);                 //  Jump                           //1295:         JP      XC10            ; Jump
}                                                                          //1296: 
                                                                           //1297: ;***********************************************************
//***********************************************************              //1298: ; NEXT ATTACKER/DEFENDER ROUTINE
// NEXT ATTACKER/DEFENDER ROUTINE
//***********************************************************
// FUNCTION:   --  To retrieve the next attacker or defender               //1299: ;***********************************************************
//                 piece value from the attack list, and delete            //1300: ; FUNCTION:   --  To retrieve the next attacker or defender
//                 that piece from the list.                               //1301: ;                 piece value from the attack list, and delete
//                                                                         //1302: ;                 that piece from the list.
// CALLED BY:  --  XCHNG                                                   //1303: ;
//                                                                         //1304: ; CALLED BY:  --  XCHNG
// CALLS:      --  None                                                    //1305: ;
//                                                                         //1306: ; CALLS:      --  None
// ARGUMENTS:  --  Attack list addresses.                                  //1307: ;
//                 Side flag                                               //1308: ; ARGUMENTS:  --  Attack list addresses.
//                 Attack list counts                                      //1309: ;                 Side flag
//***********************************************************              //1310: ;                 Attack list counts
void NEXTAD() {                                                            //1311: ;***********************************************************
        INC     (c);                    //  Increment side flag            //1312: NEXTAD: INC     c               ; Increment side flag
        EXX;                            //  Swap registers                 //1313:         EXX                     ; Swap registers
        LD      (a,b);                  //  Swap list counts               //1314:         LD      a,b             ; Swap list counts
        LD      (b,c);                  //                                 //1315:         LD      b,c
        LD      (c,a);                  //                                 //1316:         LD      c,a
        EX      (de,hl);                //  Swap list pointers             //1317:         EX      de,hl           ; Swap list pointers
        XOR     (a);                    //                                 //1318:         XOR     a
        CP      (b);                    //  At end of list ?               //1319:         CP      b               ; At end of list ?
        JR      (Z,NX6);                //  Yes - jump                     //1320:         JR      Z,NX6           ; Yes - jump
        DEC     (b);                    //  Decrement list count           //1321:         DEC     b               ; Decrement list count
back03: INC16   (hl);                   //  Increment list pointer         //1322: back03: INC     hl              ; Increment list pointer
        CP      (ptr(hl));              //  Check next item in list        //1323:         CP      (hl)            ; Check next item in list
        JR      (Z,back03);             //  Jump if empty                  //1324:         JR      Z,back03        ; Jump if empty
        RRD;                            //  Get value from list            //1325:         RRD                     ; Get value from list
        ADD     (a,a);                  //  Double it                      //1326:         ADD     a,a             ; Double it
        DEC16   (hl);                   //  Decrement list pointer         //1327:         DEC     hl              ; Decrement list pointer
NX6:    EXX;                            //  Restore regs.                  //1328: NX6:    EXX                     ; Restore regs.
        RETu;                           //  Return                         //1329:         RET                     ; Return
}                                                                          //1330: 
                                                                           //1331: ;***********************************************************
//***********************************************************              //1332: ; POINT EVALUATION ROUTINE
// POINT EVALUATION ROUTINE
//***********************************************************
//FUNCTION:   --  To perform a static board evaluation and                 //1333: ;***********************************************************
//                derive a score for a given board position                //1334: ;FUNCTION:   --  To perform a static board evaluation and
//                                                                         //1335: ;                derive a score for a given board position
// CALLED BY:  --  FNDMOV                                                  //1336: ;
//                 EVAL                                                    //1337: ; CALLED BY:  --  FNDMOV
//                                                                         //1338: ;                 EVAL
// CALLS:      --  ATTACK                                                  //1339: ;
//                 XCHNG                                                   //1340: ; CALLS:      --  ATTACK
//                 LIMIT                                                   //1341: ;                 XCHNG
//                                                                         //1342: ;                 LIMIT
// ARGUMENTS:  --  None                                                    //1343: ;
//***********************************************************              //1344: ; ARGUMENTS:  --  None
void POINTS() {                                                            //1345: ;***********************************************************
        XOR     (a);                    //  Zero out variables             //1346: POINTS: XOR     a               ; Zero out variables
        LD      (val(MTRL),a);          //                                 //1347:         LD      (MTRL),a
        LD      (val(BRDC),a);          //                                 //1348:         LD      (BRDC),a
        LD      (val(PTSL),a);          //                                 //1349:         LD      (PTSL),a
        LD      (val(PTSW1),a);         //                                 //1350:         LD      (PTSW1),a
        LD      (val(PTSW2),a);         //                                 //1351:         LD      (PTSW2),a
        LD      (val(PTSCK),a);         //                                 //1352:         LD      (PTSCK),a
        LD      (hl,addr(T1));          //  Set attacker flag              //1353:         LD      hl,T1           ; Set attacker flag
        LD      (ptr(hl),7);            //                                 //1354:         LD      (hl),7
        LD      (a,21);                 //  Init to first square on board  //1355:         LD      a,21            ; Init to first square on board
PT5:    LD      (val(M3),a);            //  Save as board index            //1356: PT5:    LD      (M3),a          ; Save as board index
        LD      (ix,val16(M3));         //  Load board index               //1357:         LD      ix,(M3)         ; Load board index
        LD      (a,ptr(ix+BOARD));      //  Get piece from board           //1358:         LD      a,(ix+BOARD)    ; Get piece from board
        CP      (-1);                   //  Off board edge ?               //1359:         CP      -1              ; Off board edge ?
        JP      (Z,PT25);               //  Yes - jump                     //1360:         JP      Z,PT25          ; Yes - jump
        LD      (hl,addr(P1));          //  Save piece, if any             //1361:         LD      hl,P1           ; Save piece, if any
        LD      (ptr(hl),a);            //                                 //1362:         LD      (hl),a
        AND     (7);                    //  Save piece type, if any        //1363:         AND     7               ; Save piece type, if any
        LD      (val(T3),a);            //                                 //1364:         LD      (T3),a
        CP      (KNIGHT);               //  Less than a Knight (Pawn) ?    //1365:         CP      KNIGHT          ; Less than a Knight (Pawn) ?
        JR      (C,PT6X);               //  Yes - Jump                     //1366:         JR      C,PT6X          ; Yes - Jump
        CP      (ROOK);                 //  Rook, Queen or King ?          //1367:         CP      ROOK            ; Rook, Queen or King ?
        JR      (C,PT6B);               //  No - jump                      //1368:         JR      C,PT6B          ; No - jump
        CP      (KING);                 //  Is it a King ?                 //1369:         CP      KING            ; Is it a King ?
        JR      (Z,PT6AA);              //  Yes - jump                     //1370:         JR      Z,PT6AA         ; Yes - jump
        LD      (a,val(MOVENO));        //  Get move number                //1371:         LD      a,(MOVENO)      ; Get move number
        CP      (7);                    //  Less than 7 ?                  //1372:         CP      7               ; Less than 7 ?
        JR      (C,PT6A);               //  Yes - Jump                     //1373:         JR      C,PT6A          ; Yes - Jump
        JPu     (PT6X);                 //  Jump                           //1374:         JP      PT6X            ; Jump
PT6AA:  BIT     (4,ptr(hl));            //  Castled yet ?                  //1375: PT6AA:  BIT     4,(hl)          ; Castled yet ?
        JR      (Z,PT6A);               //  No - jump                      //1376:         JR      Z,PT6A          ; No - jump
        LD      (a,+6);                 //  Bonus for castling             //1377:         LD      a,+6            ; Bonus for castling
        BIT     (7,ptr(hl));            //  Check piece color              //1378:         BIT     7,(hl)          ; Check piece color
        JR      (Z,PT6D);               //  Jump if white                  //1379:         JR      Z,PT6D          ; Jump if white
        LD      (a,-6);                 //  Bonus for black castling       //1380:         LD      a,-6            ; Bonus for black castling
        JPu     (PT6D);                 //  Jump                           //1381:         JP      PT6D            ; Jump
PT6A:   BIT     (3,ptr(hl));            //  Has piece moved yet ?          //1382: PT6A:   BIT     3,(hl)          ; Has piece moved yet ?
        JR      (Z,PT6X);               //  No - jump                      //1383:         JR      Z,PT6X          ; No - jump
        JPu     (PT6C);                 //  Jump                           //1384:         JP      PT6C            ; Jump
PT6B:   BIT     (3,ptr(hl));            //  Has piece moved yet ?          //1385: PT6B:   BIT     3,(hl)          ; Has piece moved yet ?
        JR      (NZ,PT6X);              //  Yes - jump                     //1386:         JR      NZ,PT6X         ; Yes - jump
PT6C:   LD      (a,-2);                 //  Two point penalty for white    //1387: PT6C:   LD      a,-2            ; Two point penalty for white
        BIT     (7,ptr(hl));            //  Check piece color              //1388:         BIT     7,(hl)          ; Check piece color
        JR      (Z,PT6D);               //  Jump if white                  //1389:         JR      Z,PT6D          ; Jump if white
        LD      (a,+2);                 //  Two point penalty for black    //1390:         LD      a,+2            ; Two point penalty for black
PT6D:   LD      (hl,addr(BRDC));        //  Get address of board control   //1391: PT6D:   LD      hl,BRDC         ; Get address of board control
        ADD     (a,ptr(hl));            //  Add on penalty/bonus points    //1392:         ADD     a,(hl)          ; Add on penalty/bonus points
        LD      (ptr(hl),a);            //  Save                           //1393:         LD      (hl),a          ; Save
PT6X:   XOR     (a);                    //  Zero out attack list           //1394: PT6X:   XOR     a               ; Zero out attack list
        LD      (b,14);                 //                                 //1395:         LD      b,14
        LD      (hl,addr(ATKLST));      //                                 //1396:         LD      hl,ATKLST
back04: LD      (ptr(hl),a);            //                                 //1397: back04: LD      (hl),a
        INC16   (hl);                   //                                 //1398:         INC     hl
        DJNZ    (back04);               //                                 //1399:         DJNZ    back04
        CALLu   (ATTACK);               //  Build attack list for square   //1400:         CALL    ATTACK          ; Build attack list for square
        LD      (hl,BACT);        //  Get black attacker count addr        //1401:         LD      hl,BACT         ; Get black attacker count addr
        LD      (a,val(wact));          //  Get white attacker count       //1402:         LD      a,(WACT)        ; Get white attacker count
        SUB     (ptr(hl));              //  Compute count difference       //1403:         SUB     (hl)            ; Compute count difference
        LD      (hl,addr(BRDC));        //  Address of board control       //1404:         LD      hl,BRDC         ; Address of board control
        ADD     (a,ptr(hl));            //  Accum board control score      //1405:         ADD     a,(hl)          ; Accum board control score
        LD      (ptr(hl),a);            //  Save                           //1406:         LD      (hl),a          ; Save
        LD      (a,val(P1));            //  Get piece on current square    //1407:         LD      a,(P1)          ; Get piece on current square
        AND     (a);                    //  Is it empty ?                  //1408:         AND     a               ; Is it empty ?
        JP      (Z,PT25);               //  Yes - jump                     //1409:         JP      Z,PT25          ; Yes - jump
        CALLu   (XCHNG);                //  Evaluate exchange, if any      //1410:         CALL    XCHNG           ; Evaluate exchange, if any
        XOR     (a);                    //  Check for a loss               //1411:         XOR     a               ; Check for a loss
        CP      (e);                    //  Points lost ?                  //1412:         CP      e               ; Points lost ?
        JR      (Z,PT23);               //  No - Jump                      //1413:         JR      Z,PT23          ; No - Jump
        DEC     (d);                    //  Deduct half a Pawn value       //1414:         DEC     d               ; Deduct half a Pawn value
        LD      (a,val(P1));            //  Get piece under attack         //1415:         LD      a,(P1)          ; Get piece under attack
        LD      (hl,val(COLOR));        //  Color of side just moved       //1416:         LD      hl,COLOR        ; Color of side just moved
        XOR     (ptr(hl));              //  Compare with piece             //1417:         XOR     (hl)            ; Compare with piece
        BIT     (7,a);                  //  Do colors match ?              //1418:         BIT     7,a             ; Do colors match ?
        LD      (a,e);                  //  Points lost                    //1419:         LD      a,e             ; Points lost
        JR      (NZ,PT20);              //  Jump if no match               //1420:         JR      NZ,PT20         ; Jump if no match
        LD      (hl,val(PTSL));         //  Previous max points lost       //1421:         LD      hl,PTSL         ; Previous max points lost
        CP      (ptr(hl));              //  Compare to current value       //1422:         CP      (hl)            ; Compare to current value
        JR      (C,PT23);               //  Jump if greater than           //1423:         JR      C,PT23          ; Jump if greater than
        LD      (ptr(hl),e);            //  Store new value as max lost    //1424:         LD      (hl),e          ; Store new value as max lost
        LD      (ix,val16(MLPTRJ));     //  Load pointer to this move      //1425:         LD      ix,(MLPTRJ)     ; Load pointer to this move
        LD      (a,val(M3));            //  Get position of lost piece     //1426:         LD      a,(M3)          ; Get position of lost piece
        CP      (ptr(ix+MLTOP));        //  Is it the one moving ?         //1427:         CP      (ix+MLTOP)      ; Is it the one moving ?
        JR      (NZ,PT23);              //  No - jump                      //1428:         JR      NZ,PT23         ; No - jump
        LD      (val(PTSCK),a);         //  Save position as a flag        //1429:         LD      (PTSCK),a       ; Save position as a flag
        JPu     (PT23);                 //  Jump                           //1430:         JP      PT23            ; Jump
PT20:   LD      (hl,addr(PTSW1));       //  Previous maximum points won    //1431: PT20:   LD      hl,PTSW1        ; Previous maximum points won
        CP      (ptr(hl));              //  Compare to current value       //1432:         CP      (hl)            ; Compare to current value
        JR      (C,rel011);             //  Jump if greater than           //1433:         JR      C,rel011        ; Jump if greater than
        LD      (a,ptr(hl));            //  Load previous max value        //1434:         LD      a,(hl)          ; Load previous max value
        LD      (ptr(hl),e);            //  Store new value as max won     //1435:         LD      (hl),e          ; Store new value as max won
rel011: LD      (hl,addr(PTSW2));       //  Previous 2nd max points won    //1436: rel011: LD      hl,PTSW2        ; Previous 2nd max points won
        CP      (ptr(hl));              //  Compare to current value       //1437:         CP      (hl)            ; Compare to current value
        JR      (C,PT23);               //  Jump if greater than           //1438:         JR      C,PT23          ; Jump if greater than
        LD      (ptr(hl),a);            //  Store as new 2nd max lost      //1439:         LD      (hl),a          ; Store as new 2nd max lost
PT23:   LD      (hl,addr(P1));          //  Get piece                      //1440: PT23:   LD      hl,P1           ; Get piece
        BIT     (7,ptr(hl));            //  Test color                     //1441:         BIT     7,(hl)          ; Test color
        LD      (a,d);                  //  Value of piece                 //1442:         LD      a,d             ; Value of piece
        JR      (Z,rel012);             //  Jump if white                  //1443:         JR      Z,rel012        ; Jump if white
        NEG;                            //  Negate for black               //1444:         NEG                     ; Negate for black
rel012: LD      (hl,addr(MTRL));        //  Get addrs of material total    //1445: rel012: LD      hl,MTRL         ; Get addrs of material total
        ADD     (a,ptr(hl));            //  Add new value                  //1446:         ADD     a,(hl)          ; Add new value
        LD      (ptr(hl),a);            //  Store                          //1447:         LD      (hl),a          ; Store
PT25:   LD      (a,val(M3));            //  Get current board position     //1448: PT25:   LD      a,(M3)          ; Get current board position
        INC     (a);                    //  Increment                      //1449:         INC     a               ; Increment
        CP      (99);                   //  At end of board ?              //1450:         CP      99              ; At end of board ?
        JP      (NZ,PT5);               //  No - jump                      //1451:         JP      NZ,PT5          ; No - jump
        LD      (a,val(PTSCK));         //  Moving piece lost flag         //1452:         LD      a,(PTSCK)       ; Moving piece lost flag
        AND     (a);                    //  Was it lost ?                  //1453:         AND     a               ; Was it lost ?
        JR      (Z,PT25A);              //  No - jump                      //1454:         JR      Z,PT25A         ; No - jump
        LD      (a,val(PTSW2));         //  2nd max points won             //1455:         LD      a,(PTSW2)       ; 2nd max points won
        LD      (val(PTSW1),a);         //  Store as max points won        //1456:         LD      (PTSW1),a       ; Store as max points won
        XOR     (a);                    //  Zero out 2nd max points won    //1457:         XOR     a               ; Zero out 2nd max points won
        LD      (val(PTSW2),a);         //                                 //1458:         LD      (PTSW2),a
PT25A:  LD      (a,val(PTSL));          //  Get max points lost            //1459: PT25A:  LD      a,(PTSL)        ; Get max points lost
        AND     (a);                    //  Is it zero ?                   //1460:         AND     a               ; Is it zero ?
        JR      (Z,rel013);             //  Yes - jump                     //1461:         JR      Z,rel013        ; Yes - jump
        DEC     (a);                    //  Decrement it                   //1462:         DEC     a               ; Decrement it
rel013: LD      (b,a);                  //  Save it                        //1463: rel013: LD      b,a             ; Save it
        LD      (a,val(PTSW1));         //  Max,points won                 //1464:         LD      a,(PTSW1)       ; Max,points won
        AND     (a);                    //  Is it zero ?                   //1465:         AND     a               ; Is it zero ?
        JR      (Z,rel014);             //  Yes - jump                     //1466:         JR      Z,rel014        ; Yes - jump
        LD      (a,val(PTSW2));         //  2nd max points won             //1467:         LD      a,(PTSW2)       ; 2nd max points won
        AND     (a);                    //  Is it zero ?                   //1468:         AND     a               ; Is it zero ?
        JR      (Z,rel014);             //  Yes - jump                     //1469:         JR      Z,rel014        ; Yes - jump
        DEC     (a);                    //  Decrement it                   //1470:         DEC     a               ; Decrement it
        SRL     (a);                    //  Divide it by 2                 //1471:         SRL     a               ; Divide it by 2
rel014: SUB     (b);                    //  Subtract points lost           //1472: rel014: SUB     b               ; Subtract points lost
        LD      (hl,addr(COLOR));       //  Color of side just moved ???   //1473:         LD      hl,COLOR        ; Color of side just moved ???
        BIT     (7,ptr(hl));            //  Is it white ?                  //1474:         BIT     7,(hl)          ; Is it white ?
        JR      (Z,rel015);             //  Yes - jump                     //1475:         JR      Z,rel015        ; Yes - jump
        NEG;                            //  Negate for black               //1476:         NEG                     ; Negate for black
rel015: LD      (hl,addr(MTRL));        //  Net material on board          //1477: rel015: LD      hl,MTRL         ; Net material on board
        ADD     (a,ptr(hl));            //  Add exchange adjustments       //1478:         ADD     a,(hl)          ; Add exchange adjustments
        LD      (hl,addr(MV0));         //  Material at ply 0              //1479:         LD      hl,MV0          ; Material at ply 0
        SUB     (ptr(hl));              //  Subtract from current          //1480:         SUB     (hl)            ; Subtract from current
        LD      (b,a);                  //  Save                           //1481:         LD      b,a             ; Save
        LD      (a,30);                 //  Load material limit            //1482:         LD      a,30            ; Load material limit
        CALLu   (LIMIT);                //  Limit to plus or minus value   //1483:         CALL    LIMIT           ; Limit to plus or minus value
        LD      (e,a);                  //  Save limited value             //1484:         LD      e,a             ; Save limited value
        LD      (a,val(BRDC));          //  Get board control points       //1485:         LD      a,(BRDC)        ; Get board control points
        LD      (hl,addr(BC0));         //  Board control at ply zero      //1486:         LD      hl,BC0          ; Board control at ply zero
        SUB     (ptr(hl));              //  Get difference                 //1487:         SUB     (hl)            ; Get difference
        LD      (b,a);                  //  Save                           //1488:         LD      b,a             ; Save
        LD      (a,val(PTSCK));         //  Moving piece lost flag         //1489:         LD      a,(PTSCK)       ; Moving piece lost flag
        AND     (a);                    //  Is it zero ?                   //1490:         AND     a               ; Is it zero ?
        JR      (Z,rel026);             //  Yes - jump                     //1491:         JR      Z,rel026        ; Yes - jump
        LD      (b,0);                  //  Zero board control points      //1492:         LD      b,0             ; Zero board control points
rel026: LD      (a,6);                  //  Load board control limit       //1493: rel026: LD      a,6             ; Load board control limit
        CALLu   (LIMIT);                //  Limit to plus or minus value   //1494:         CALL    LIMIT           ; Limit to plus or minus value
        LD      (d,a);                  //  Save limited value             //1495:         LD      d,a             ; Save limited value
        LD      (a,e);                  //  Get material points            //1496:         LD      a,e             ; Get material points
        ADD     (a,a);                  //  Multiply by 4                  //1497:         ADD     a,a             ; Multiply by 4
        ADD     (a,a);                  //                                 //1498:         ADD     a,a
        ADD     (a,d);                  //  Add board control              //1499:         ADD     a,d             ; Add board control
        LD      (hl,addr(COLOR));       //  Color of side just moved       //1500:         LD      hl,COLOR        ; Color of side just moved
        BIT     (7,ptr(hl));            //  Is it white ?                  //1501:         BIT     7,(hl)          ; Is it white ?
        JR      (NZ,rel016);            //  No - jump                      //1502:         JR      NZ,rel016       ; No - jump
        NEG;                            //  Negate for white               //1503:         NEG                     ; Negate for white
rel016: ADD     (a,0x80);               //  Rescale score (neutral = 80H)  //1504: rel016: ADD     a,80H           ; Rescale score (neutral = 80H)
        LD      (val(VALM),a);          //  Save score                     //1505:         LD      (VALM),a        ; Save score
        LD      (ix,val16(MLPTRJ));     //  Load move list pointer         //1506:         LD      ix,(MLPTRJ)     ; Load move list pointer
        LD      (ptr(ix+MLVAL),a);      //  Save score in move list        //1507:         LD      (ix+MLVAL),a    ; Save score in move list
        RETu;                           //  Return                         //1508:         RET                     ; Return
}                                                                          //1509: 
                                                                           //1510: ;***********************************************************
//***********************************************************              //1511: ; LIMIT ROUTINE
// LIMIT ROUTINE
//***********************************************************
// FUNCTION:   --  To limit the magnitude of a given value                 //1512: ;***********************************************************
//                 to another given value.                                 //1513: ; FUNCTION:   --  To limit the magnitude of a given value
//                                                                         //1514: ;                 to another given value.
// CALLED BY:  --  POINTS                                                  //1515: ;
//                                                                         //1516: ; CALLED BY:  --  POINTS
// CALLS:      --  None                                                    //1517: ;
//                                                                         //1518: ; CALLS:      --  None
// ARGUMENTS:  --  Input  - Value, to be limited in the B                  //1519: ;
//                          register.                                      //1520: ; ARGUMENTS:  --  Input  - Value, to be limited in the B
//                        - Value to limit to in the A register            //1521: ;                          register.
//                 Output - Limited value in the A register.               //1522: ;                        - Value to limit to in the A register
//***********************************************************              //1523: ;                 Output - Limited value in the A register.
void LIMIT() {                                                             //1524: ;***********************************************************
        BIT     (7,b);                  //  Is value negative ?            //1525: LIMIT:  BIT     7,b             ; Is value negative ?
        JP      (Z,LIM10);              //  No - jump                      //1526:         JP      Z,LIM10         ; No - jump
        NEG;                            //  Make positive                  //1527:         NEG                     ; Make positive
        CP      (b);                    //  Compare to limit               //1528:         CP      b               ; Compare to limit
        RET     (NC);                   //  Return if outside limit        //1529:         RET     NC              ; Return if outside limit
        LD      (a,b);                  //  Output value as is             //1530:         LD      a,b             ; Output value as is
        RETu;                           //  Return                         //1531:         RET                     ; Return
LIM10:  CP      (b);                    //  Compare to limit               //1532: LIM10:  CP      b               ; Compare to limit
        RET     (C);                    //  Return if outside limit        //1533:         RET     C               ; Return if outside limit
        LD      (a,b);                  //  Output value as is             //1534:         LD      a,b             ; Output value as is
        RETu;                           //  Return                         //1535:         RET                     ; Return
}                                                                          //1536: 
                                                                           //1537: ;***********************************************************
//***********************************************************              //1538: ; MOVE ROUTINE
// MOVE ROUTINE
//***********************************************************
// FUNCTION:   --  To execute a move from the move list on the             //1539: ;***********************************************************
//                 board array.                                            //1540: ; FUNCTION:   --  To execute a move from the move list on the
//                                                                         //1541: ;                 board array.
// CALLED BY:  --  CPTRMV                                                  //1542: ;
//                 PLYRMV                                                  //1543: ; CALLED BY:  --  CPTRMV
//                 EVAL                                                    //1544: ;                 PLYRMV
//                 FNDMOV                                                  //1545: ;                 EVAL
//                 VALMOV                                                  //1546: ;                 FNDMOV
//                                                                         //1547: ;                 VALMOV
// CALLS:      --  None                                                    //1548: ;
//                                                                         //1549: ; CALLS:      --  None
// ARGUMENTS:  --  None                                                    //1550: ;
//***********************************************************              //1551: ; ARGUMENTS:  --  None
void MOVE() {                                                              //1552: ;***********************************************************
        LD      (hl,val16(MLPTRJ));     //  Load move list pointer         //1553: MOVE:   LD      hl,(MLPTRJ)     ; Load move list pointer
        INC16   (hl);                   //  Increment past link bytes      //1554:         INC     hl              ; Increment past link bytes
        INC16   (hl);                                                      //1555:         INC     hl
MV1:    LD      (a,ptr(hl));            //  "From" position                //1556: MV1:    LD      a,(hl)          ; "From" position
        LD      (val(M1),a);            //  Save                           //1557:         LD      (M1),a          ; Save
        INC16   (hl);                   //  Increment pointer              //1558:         INC     hl              ; Increment pointer
        LD      (a,ptr(hl));            //  "To" position                  //1559:         LD      a,(hl)          ; "To" position
        LD      (val(M2),a);            //  Save                           //1560:         LD      (M2),a          ; Save
        INC16   (hl);                   //  Increment pointer              //1561:         INC     hl              ; Increment pointer
        LD      (d,ptr(hl));            //  Get captured piece/flags       //1562:         LD      d,(hl)          ; Get captured piece/flags
        LD      (ix,val16(M1));         //  Load "from" pos board index    //1563:         LD      ix,(M1)         ; Load "from" pos board index
        LD      (e,ptr(ix+BOARD));      //  Get piece moved                //1564:         LD      e,(ix+BOARD)    ; Get piece moved
        BIT     (5,d);                  //  Test Pawn promotion flag       //1565:         BIT     5,d             ; Test Pawn promotion flag
        JR      (NZ,MV15);              //  Jump if set                    //1566:         JR      NZ,MV15         ; Jump if set
        LD      (a,e);                  //  Piece moved                    //1567:         LD      a,e             ; Piece moved
        AND     (7);                    //  Clear flag bits                //1568:         AND     7               ; Clear flag bits
        CP      (QUEEN);                //  Is it a queen ?                //1569:         CP      QUEEN           ; Is it a queen ?
        JR      (Z,MV20);               //  Yes - jump                     //1570:         JR      Z,MV20          ; Yes - jump
        CP      (KING);                 //  Is it a king ?                 //1571:         CP      KING            ; Is it a king ?
        JR      (Z,MV30);               //  Yes - jump                     //1572:         JR      Z,MV30          ; Yes - jump
MV5:    LD      (iy,val16(M2));         //  Load "to" pos board index      //1573: MV5:    LD      iy,(M2)         ; Load "to" pos board index
        SET     (3,e);                  //  Set piece moved flag           //1574:         SET     3,e             ; Set piece moved flag
        LD      (ptr(iy+BOARD),e);      //  Insert piece at new position   //1575:         LD      (iy+BOARD),e    ; Insert piece at new position
        LD      (ptr(ix+BOARD),0);      //  Empty previous position        //1576:         LD      (ix+BOARD),0    ; Empty previous position
        BIT     (6,d);                  //  Double move ?                  //1577:         BIT     6,d             ; Double move ?
        JR      (NZ,MV40);              //  Yes - jump                     //1578:         JR      NZ,MV40         ; Yes - jump
        LD      (a,d);                  //  Get captured piece, if any     //1579:         LD      a,d             ; Get captured piece, if any
        AND     (7);                                                       //1580:         AND     7
        CP      (QUEEN);                //  Was it a queen ?               //1581:         CP      QUEEN           ; Was it a queen ?
        RET     (NZ);                   //  No - return                    //1582:         RET     NZ              ; No - return
        LD      (hl,addr(POSQ));        //  Addr of saved Queen position   //1583:         LD      hl,POSQ         ; Addr of saved Queen position
        BIT     (7,d);                  //  Is Queen white ?               //1584:         BIT     7,d             ; Is Queen white ?
        JR      (Z,MV10);               //  Yes - jump                     //1585:         JR      Z,MV10          ; Yes - jump
        INC16   (hl);                   //  Increment to black Queen pos   //1586:         INC     hl              ; Increment to black Queen pos
MV10:   XOR     (a);                    //  Set saved position to zero     //1587: MV10:   XOR     a               ; Set saved position to zero
        LD      (ptr(hl),a);                                               //1588:         LD      (hl),a
        RETu;                           //  Return                         //1589:         RET                     ; Return
MV15:   SET     (2,e);                  //  Change Pawn to a Queen         //1590: MV15:   SET     2,e             ; Change Pawn to a Queen
        JPu     (MV5);                  //  Jump                           //1591:         JP      MV5             ; Jump
MV20:   LD      (hl,addr(POSQ));        //  Addr of saved Queen position   //1592: MV20:   LD      hl,POSQ         ; Addr of saved Queen position
MV21:   BIT     (7,e);                  //  Is Queen white ?               //1593: MV21:   BIT     7,e             ; Is Queen white ?
        JR      (Z,MV22);               //  Yes - jump                     //1594:         JR      Z,MV22          ; Yes - jump
        INC16   (hl);                   //  Increment to black Queen pos   //1595:         INC     hl              ; Increment to black Queen pos
MV22:   LD      (a,val(M2));            //  Get new Queen position         //1596: MV22:   LD      a,(M2)          ; Get new Queen position
        LD      (ptr(hl),a);            //  Save                           //1597:         LD      (hl),a          ; Save
        JPu     (MV5);                  //  Jump                           //1598:         JP      MV5             ; Jump
MV30:   LD      (hl,addr(POSK));        //  Get saved King position        //1599: MV30:   LD      hl,POSK         ; Get saved King position
        BIT     (6,d);                  //  Castling ?                     //1600:         BIT     6,d             ; Castling ?
        JR      (Z,MV21);               //  No - jump                      //1601:         JR      Z,MV21          ; No - jump
        SET     (4,e);                  //  Set King castled flag          //1602:         SET     4,e             ; Set King castled flag
        JPu     (MV21);                 //  Jump                           //1603:         JP      MV21            ; Jump
MV40:   LD      (hl,val16(MLPTRJ));     //  Get move list pointer          //1604: MV40:   LD      hl,(MLPTRJ)     ; Get move list pointer
        LD      (de,8);                 //  Increment to next move         //1605:         LD      de,8            ; Increment to next move
        ADD16   (hl,de);                //                                 //1606:         ADD     hl,de
        JPu     (MV1);                  //  Jump (2nd part of dbl move)    //1607:         JP      MV1             ; Jump (2nd part of dbl move)
}                                                                          //1608: 
                                                                           //1609: ;***********************************************************
//***********************************************************              //1610: ; UN-MOVE ROUTINE
// UN-MOVE ROUTINE
//***********************************************************
// FUNCTION:   --  To reverse the process of the move routine,             //1611: ;***********************************************************
//                 thereby restoring the board array to its                //1612: ; FUNCTION:   --  To reverse the process of the move routine,
//                 previous position.                                      //1613: ;                 thereby restoring the board array to its
//                                                                         //1614: ;                 previous position.
// CALLED BY:  --  VALMOV                                                  //1615: ;
//                 EVAL                                                    //1616: ; CALLED BY:  --  VALMOV
//                 FNDMOV                                                  //1617: ;                 EVAL
//                 ASCEND                                                  //1618: ;                 FNDMOV
//                                                                         //1619: ;                 ASCEND
// CALLS:      --  None                                                    //1620: ;
//                                                                         //1621: ; CALLS:      --  None
// ARGUMENTS:  --  None                                                    //1622: ;
//***********************************************************              //1623: ; ARGUMENTS:  --  None
void UNMOVE() {                                                            //1624: ;***********************************************************
        LD      (hl,val16(MLPTRJ));     //  Load move list pointer         //1625: UNMOVE: LD      hl,(MLPTRJ)     ; Load move list pointer
        INC16   (hl);                   //  Increment past link bytes      //1626:         INC     hl              ; Increment past link bytes
        INC16   (hl);                                                      //1627:         INC     hl
UM1:    LD      (a,ptr(hl));            //  Get "from" position            //1628: UM1:    LD      a,(hl)          ; Get "from" position
        LD      (val(M1),a);            //  Save                           //1629:         LD      (M1),a          ; Save
        INC16   (hl);                   //  Increment pointer              //1630:         INC     hl              ; Increment pointer
        LD      (a,ptr(hl));            //  Get "to" position              //1631:         LD      a,(hl)          ; Get "to" position
        LD      (val(M2),a);            //  Save                           //1632:         LD      (M2),a          ; Save
        INC16   (hl);                   //  Increment pointer              //1633:         INC     hl              ; Increment pointer
        LD      (d,ptr(hl));            //  Get captured piece/flags       //1634:         LD      d,(hl)          ; Get captured piece/flags
        LD      (ix,val16(M2));         //  Load "to" pos board index      //1635:         LD      ix,(M2)         ; Load "to" pos board index
        LD      (e,ptr(ix+BOARD));      //  Get piece moved                //1636:         LD      e,(ix+BOARD)    ; Get piece moved
        BIT     (5,d);                  //  Was it a Pawn promotion ?      //1637:         BIT     5,d             ; Was it a Pawn promotion ?
        JR      (NZ,UM15);              //  Yes - jump                     //1638:         JR      NZ,UM15         ; Yes - jump
        LD      (a,e);                  //  Get piece moved                //1639:         LD      a,e             ; Get piece moved
        AND     (7);                    //  Clear flag bits                //1640:         AND     7               ; Clear flag bits
        CP      (QUEEN);                //  Was it a Queen ?               //1641:         CP      QUEEN           ; Was it a Queen ?
        JR      (Z,UM20);               //  Yes - jump                     //1642:         JR      Z,UM20          ; Yes - jump
        CP      (KING);                 //  Was it a King ?                //1643:         CP      KING            ; Was it a King ?
        JR      (Z,UM30);               //  Yes - jump                     //1644:         JR      Z,UM30          ; Yes - jump
UM5:    BIT     (4,d);                  //  Is this 1st move for piece ?   //1645: UM5:    BIT     4,d             ; Is this 1st move for piece ?
        JR      (NZ,UM16);              //  Yes - jump                     //1646:         JR      NZ,UM16         ; Yes - jump
UM6:    LD      (iy,val16(M1));         //  Load "from" pos board index    //1647: UM6:    LD      iy,(M1)         ; Load "from" pos board index
        LD      (ptr(iy+BOARD),e);      //  Return to previous board pos   //1648:         LD      (iy+BOARD),e    ; Return to previous board pos
        LD      (a,d);                  //  Get captured piece, if any     //1649:         LD      a,d             ; Get captured piece, if any
        AND     (0x8f);                 //  Clear flags                    //1650:         AND     8FH             ; Clear flags
        LD      (ptr(ix+BOARD),a);      //  Return to board                //1651:         LD      (ix+BOARD),a    ; Return to board
        BIT     (6,d);                  //  Was it a double move ?         //1652:         BIT     6,d             ; Was it a double move ?
        JR      (NZ,UM40);              //  Yes - jump                     //1653:         JR      NZ,UM40         ; Yes - jump
        LD      (a,d);                  //  Get captured piece, if any     //1654:         LD      a,d             ; Get captured piece, if any
        AND     (7);                    //  Clear flag bits                //1655:         AND     7               ; Clear flag bits
        CP      (QUEEN);                //  Was it a Queen ?               //1656:         CP      QUEEN           ; Was it a Queen ?
        RET     (NZ);                   //  No - return                    //1657:         RET     NZ              ; No - return
        LD      (hl,addr(POSQ));        //  Address of saved Queen pos     //1658:         LD      hl,POSQ         ; Address of saved Queen pos
        BIT     (7,d);                  //  Is Queen white ?               //1659:         BIT     7,d             ; Is Queen white ?
        JR      (Z,UM10);               //  Yes - jump                     //1660:         JR      Z,UM10          ; Yes - jump
        INC16   (hl);                   //  Increment to black Queen pos   //1661:         INC     hl              ; Increment to black Queen pos
UM10:   LD      (a,val(M2));            //  Queen's previous position      //1662: UM10:   LD      a,(M2)          ; Queen's previous position
        LD      (ptr(hl),a);            //  Save                           //1663:         LD      (hl),a          ; Save
        RETu;                           //  Return                         //1664:         RET                     ; Return
UM15:   RES     (2,e);                  //  Restore Queen to Pawn          //1665: UM15:   RES     2,e             ; Restore Queen to Pawn
        JPu     (UM5);                  //  Jump                           //1666:         JP      UM5             ; Jump
UM16:   RES     (3,e);                  //  Clear piece moved flag         //1667: UM16:   RES     3,e             ; Clear piece moved flag
        JPu     (UM6);                  //  Jump                           //1668:         JP      UM6             ; Jump
UM20:   LD      (hl,addr(POSQ));        //  Addr of saved Queen position   //1669: UM20:   LD      hl,POSQ         ; Addr of saved Queen position
UM21:   BIT     (7,e);                  //  Is Queen white ?               //1670: UM21:   BIT     7,e             ; Is Queen white ?
        JR      (Z,UM22);               //  Yes - jump                     //1671:         JR      Z,UM22          ; Yes - jump
        INC16   (hl);                   //  Increment to black Queen pos   //1672:         INC     hl              ; Increment to black Queen pos
UM22:   LD      (a,val(M1));            //  Get previous position          //1673: UM22:   LD      a,(M1)          ; Get previous position
        LD      (ptr(hl),a);            //  Save                           //1674:         LD      (hl),a          ; Save
        JPu     (UM5);                  //  Jump                           //1675:         JP      UM5             ; Jump
UM30:   LD      (hl,addr(POSK));        //  Address of saved King pos      //1676: UM30:   LD      hl,POSK         ; Address of saved King pos
        BIT     (6,d);                  //  Was it a castle ?              //1677:         BIT     6,d             ; Was it a castle ?
        JR      (Z,UM21);               //  No - jump                      //1678:         JR      Z,UM21          ; No - jump
        RES     (4,e);                  //  Clear castled flag             //1679:         RES     4,e             ; Clear castled flag
        JPu     (UM21);                 //  Jump                           //1680:         JP      UM21            ; Jump
UM40:   LD      (hl,val16(MLPTRJ));     //  Load move list pointer         //1681: UM40:   LD      hl,(MLPTRJ)     ; Load move list pointer
        LD      (de,8);                 //  Increment to next move         //1682:         LD      de,8            ; Increment to next move
        ADD16   (hl,de);                                                   //1683:         ADD     hl,de
        JPu     (UM1);                  //  Jump (2nd part of dbl move)    //1684:         JP      UM1             ; Jump (2nd part of dbl move)
}                                                                          //1685: 
                                                                           //1686: ;***********************************************************
//***********************************************************              //1687: ; SORT ROUTINE
// SORT ROUTINE
//***********************************************************
// FUNCTION:   --  To sort the move list in order of                       //1688: ;***********************************************************
//                 increasing move value scores.                           //1689: ; FUNCTION:   --  To sort the move list in order of
//                                                                         //1690: ;                 increasing move value scores.
// CALLED BY:  --  FNDMOV                                                  //1691: ;
//                                                                         //1692: ; CALLED BY:  --  FNDMOV
// CALLS:      --  EVAL                                                    //1693: ;
//                                                                         //1694: ; CALLS:      --  EVAL
// ARGUMENTS:  --  None                                                    //1695: ;
//***********************************************************              //1696: ; ARGUMENTS:  --  None
void SORTM() {                                                             //1697: ;***********************************************************
        LD      (bc,val16(MLPTRI));     //  Move list begin pointer        //1698: SORTM:  LD      bc,(MLPTRI)     ; Move list begin pointer
        LD      (de,0);                 //  Initialize working pointers    //1699:         LD      de,0            ; Initialize working pointers
SR5:    LD      (h,b);                  //                                 //1700: SR5:    LD      h,b
        LD      (l,c);                  //                                 //1701:         LD      l,c
        LD      (c,ptr(hl));            //  Link to next move              //1702:         LD      c,(hl)          ; Link to next move
        INC16   (hl);                   //                                 //1703:         INC     hl
        LD      (b,ptr(hl));            //                                 //1704:         LD      b,(hl)
        LD      (ptr(hl),d);            //  Store to link in list          //1705:         LD      (hl),d          ; Store to link in list
        DEC16   (hl);                   //                                 //1706:         DEC     hl
        LD      (ptr(hl),e);            //                                 //1707:         LD      (hl),e
        XOR     (a);                    //  End of list ?                  //1708:         XOR     a               ; End of list ?
        CP      (b);                    //                                 //1709:         CP      b
        RET     (Z);                    //  Yes - return                   //1710:         RET     Z               ; Yes - return
SR10:   LD      (val16(MLPTRJ),bc);     //  Save list pointer              //1711: SR10:   LD      (MLPTRJ),bc     ; Save list pointer
        CALLu   (EVAL);                 //  Evaluate move                  //1712:         CALL    EVAL            ; Evaluate move
        LD      (hl,val16(MLPTRI));     //  Begining of move list          //1713:         LD      hl,(MLPTRI)     ; Begining of move list
        LD      (bc,val16(MLPTRJ));     //  Restore list pointer           //1714:         LD      bc,(MLPTRJ)     ; Restore list pointer
SR15:   LD      (e,ptr(hl));            //  Next move for compare          //1715: SR15:   LD      e,(hl)          ; Next move for compare
        INC16   (hl);                   //                                 //1716:         INC     hl
        LD      (d,ptr(hl));            //                                 //1717:         LD      d,(hl)
        XOR     (a);                    //  At end of list ?               //1718:         XOR     a               ; At end of list ?
        CP      (d);                    //                                 //1719:         CP      d
        JR      (Z,SR25);               //  Yes - jump                     //1720:         JR      Z,SR25          ; Yes - jump
        PUSH    (de);                   //  Transfer move pointer          //1721:         PUSH    de              ; Transfer move pointer
        POP     (ix);                   //                                 //1722:         POP     ix
        LD      (a,val(VALM));          //  Get new move value             //1723:         LD      a,(VALM)        ; Get new move value
        CP      (ptr(ix+MLVAL));        //  Less than list value ?         //1724:         CP      (ix+MLVAL)      ; Less than list value ?
        JR      (NC,SR30);              //  No - jump                      //1725:         JR      NC,SR30         ; No - jump
SR25:   LD      (ptr(hl),b);            //  Link new move into list        //1726: SR25:   LD      (hl),b          ; Link new move into list
        DEC16   (hl);                   //                                 //1727:         DEC     hl
        LD      (ptr(hl),c);            //                                 //1728:         LD      (hl),c
        JPu     (SR5);                  //  Jump                           //1729:         JP      SR5             ; Jump
SR30:   EX      (de,hl);                //  Swap pointers                  //1730: SR30:   EX      de,hl           ; Swap pointers
        JPu     (SR15);                 //  Jump                           //1731:         JP      SR15            ; Jump
}                                                                          //1732: 
                                                                           //1733: ;***********************************************************
//***********************************************************              //1734: ; EVALUATION ROUTINE
// EVALUATION ROUTINE
//***********************************************************
// FUNCTION:   --  To evaluate a given move in the move list.              //1735: ;***********************************************************
//                 It first makes the move on the board, then if           //1736: ; FUNCTION:   --  To evaluate a given move in the move list.
//                 the move is legal, it evaluates it, and then            //1737: ;                 It first makes the move on the board, then if
//                 restores the board position.                            //1738: ;                 the move is legal, it evaluates it, and then
//                                                                         //1739: ;                 restores the board position.
// CALLED BY:  --  SORT                                                    //1740: ;
//                                                                         //1741: ; CALLED BY:  --  SORT
// CALLS:      --  MOVE                                                    //1742: ;
//                 INCHK                                                   //1743: ; CALLS:      --  MOVE
//                 PINFND                                                  //1744: ;                 INCHK
//                 POINTS                                                  //1745: ;                 PINFND
//                 UNMOVE                                                  //1746: ;                 POINTS
//                                                                         //1747: ;                 UNMOVE
// ARGUMENTS:  --  None                                                    //1748: ;
//***********************************************************              //1749: ; ARGUMENTS:  --  None
void EVAL() {                                                              //1750: ;***********************************************************
        CALLu   (MOVE);                 //  Make move on the board array   //1751: EVAL:   CALL    MOVE            ; Make move on the board array
        CALLu   (INCHK);                //  Determine if move is legal     //1752:         CALL    INCHK           ; Determine if move is legal
        AND     (a);                    //  Legal move ?                   //1753:         AND     a               ; Legal move ?
        JR      (Z,EV5);                //  Yes - jump                     //1754:         JR      Z,EV5           ; Yes - jump
        XOR     (a);                    //  Score of zero                  //1755:         XOR     a               ; Score of zero
        LD      (val(VALM),a);          //  For illegal move               //1756:         LD      (VALM),a        ; For illegal move
        JPu     (EV10);                 //  Jump                           //1757:         JP      EV10            ; Jump
EV5:    CALLu   (PINFND);               //  Compile pinned list            //1758: EV5:    CALL    PINFND          ; Compile pinned list
        CALLu   (POINTS);               //  Assign points to move          //1759:         CALL    POINTS          ; Assign points to move
EV10:   CALLu   (UNMOVE);               //  Restore board array            //1760: EV10:   CALL    UNMOVE          ; Restore board array
        RETu;                           //  Return                         //1761:         RET                     ; Return
}                                                                          //1762: 
                                                                           //1763: ;***********************************************************
//***********************************************************              //1764: ; FIND MOVE ROUTINE
// FIND MOVE ROUTINE
//***********************************************************
// FUNCTION:   --  To determine the computer's best move by                //1765: ;***********************************************************
//                 performing a depth first tree search using              //1766: ; FUNCTION:   --  To determine the computer's best move by
//                 the techniques of alpha-beta pruning.                   //1767: ;                 performing a depth first tree search using
//                                                                         //1768: ;                 the techniques of alpha-beta pruning.
// CALLED BY:  --  CPTRMV                                                  //1769: ;
//                                                                         //1770: ; CALLED BY:  --  CPTRMV
// CALLS:      --  PINFND                                                  //1771: ;
//                 POINTS                                                  //1772: ; CALLS:      --  PINFND
//                 GENMOV                                                  //1773: ;                 POINTS
//                 SORTM                                                   //1774: ;                 GENMOV
//                 ASCEND                                                  //1775: ;                 SORTM
//                 UNMOVE                                                  //1776: ;                 ASCEND
//                                                                         //1777: ;                 UNMOVE
// ARGUMENTS:  --  None                                                    //1778: ;
//***********************************************************              //1779: ; ARGUMENTS:  --  None
void FNDMOV() {                                                            //1780: ;***********************************************************
        LD      (a,val(MOVENO));        //  Current move number            //1781: FNDMOV: LD      a,(MOVENO)      ; Current move number
        CP      (1);                    //  First move ?                   //1782:         CP      1               ; First move ?
        CALL    (Z,BOOK);               //  Yes - execute book opening     //1783:         CALL    Z,BOOK          ; Yes - execute book opening
        XOR     (a);                    //  Initialize ply number to zero  //1784:         XOR     a               ; Initialize ply number to zero
        LD      (val(NPLY),a);                                             //1785:         LD      (NPLY),a
        LD      (hl,0);                 //  Initialize best move to zero   //1786:         LD      hl,0            ; Initialize best move to zero
        LD      (val16(BESTM),hl);                                         //1787:         LD      (BESTM),hl
        LD      (hl,addr(MLIST));       //  Initialize ply list pointers   //1788:         LD      hl,MLIST        ; Initialize ply list pointers
        LD      (val16(MLNXT),hl);                                         //1789:         LD      (MLNXT),hl
        LD      (hl,addr(PLYIX)-2);                                        //1790:         LD      hl,PLYIX-2
        LD      (val16(MLPTRI),hl);                                        //1791:         LD      (MLPTRI),hl
        LD      (a,val(KOLOR));         //  Initialize color               //1792:         LD      a,(KOLOR)       ; Initialize color
        LD      (val(COLOR),a);                                            //1793:         LD      (COLOR),a
        LD      (hl,addr(SCORE));       //  Initialize score index         //1794:         LD      hl,SCORE        ; Initialize score index
        LD      (val16(SCRIX),hl);                                         //1795:         LD      (SCRIX),hl
        LD      (a,val(PLYMAX));        //  Get max ply number             //1796:         LD      a,(PLYMAX)      ; Get max ply number
        ADD     (a,2);                  //  Add 2                          //1797:         ADD     a,2             ; Add 2
        LD      (b,a);                  //  Save as counter                //1798:         LD      b,a             ; Save as counter
        XOR     (a);                    //  Zero out score table           //1799:         XOR     a               ; Zero out score table
back05: LD      (ptr(hl),a);                                               //1800: back05: LD      (hl),a
        INC16   (hl);                                                      //1801:         INC     hl
        DJNZ    (back05);                                                  //1802:         DJNZ    back05
        LD      (val(BC0),a);           //  Zero ply 0 board control       //1803:         LD      (BC0),a         ; Zero ply 0 board control
        LD      (val(MV0),a);           //  Zero ply 0 material            //1804:         LD      (MV0),a         ; Zero ply 0 material
        CALLu   (PINFND);               //  Compile pin list               //1805:         CALL    PINFND          ; Compile pin list
        CALLu   (POINTS);               //  Evaluate board at ply 0        //1806:         CALL    POINTS          ; Evaluate board at ply 0
        LD      (a,val(BRDC));          //  Get board control points       //1807:         LD      a,(BRDC)        ; Get board control points
        LD      (val(BC0),a);           //  Save                           //1808:         LD      (BC0),a         ; Save
        LD      (a,val(MTRL));          //  Get material count             //1809:         LD      a,(MTRL)        ; Get material count
        LD      (val(MV0),a);           //  Save                           //1810:         LD      (MV0),a         ; Save
FM5:    LD      (hl,addr(NPLY));        //  Address of ply counter         //1811: FM5:    LD      hl,NPLY         ; Address of ply counter
        INC     (ptr(hl));              //  Increment ply count            //1812:         INC     (hl)            ; Increment ply count
        XOR     (a);                    //  Initialize mate flag           //1813:         XOR     a               ; Initialize mate flag
        LD      (val(MATEF),a);                                            //1814:         LD      (MATEF),a
        CALLu   (GENMOV);               //  Generate list of moves         //1815:         CALL    GENMOV          ; Generate list of moves
        LD      (a,val(NPLY));          //  Current ply counter            //1816:         LD      a,(NPLY)        ; Current ply counter
        LD      (hl,addr(PLYMAX));      //  Address of maximum ply number  //1817:         LD      hl,PLYMAX       ; Address of maximum ply number
        CP      (ptr(hl));              //  At max ply ?                   //1818:         CP      (hl)            ; At max ply ?
        CALL    (C,SORTM);              //  No - call sort                 //1819:         CALL    C,SORTM         ; No - call sort
        LD      (hl,val16(MLPTRI));     //  Load ply index pointer         //1820:         LD      hl,(MLPTRI)     ; Load ply index pointer
        LD      (val16(MLPTRJ),hl);     //  Save as last move pointer      //1821:         LD      (MLPTRJ),hl     ; Save as last move pointer
FM15:   LD      (hl,val16(MLPTRJ));     //  Load last move pointer         //1822: FM15:   LD      hl,(MLPTRJ)     ; Load last move pointer
        LD      (e,ptr(hl));            //  Get next move pointer          //1823:         LD      e,(hl)          ; Get next move pointer
        INC16   (hl);                                                      //1824:         INC     hl
        LD      (d,ptr(hl));                                               //1825:         LD      d,(hl)
        LD      (a,d);                                                     //1826:         LD      a,d
        AND     (a);                    //  End of move list ?             //1827:         AND     a               ; End of move list ?
        JR      (Z,FM25);               //  Yes - jump                     //1828:         JR      Z,FM25          ; Yes - jump
        LD      (val16(MLPTRJ),de);     //  Save current move pointer      //1829:         LD      (MLPTRJ),de     ; Save current move pointer
        LD      (hl,val16(MLPTRI));     //  Save in ply pointer list       //1830:         LD      hl,(MLPTRI)     ; Save in ply pointer list
        LD      (ptr(hl),e);                                               //1831:         LD      (hl),e
        INC16   (hl);                                                      //1832:         INC     hl
        LD      (ptr(hl),d);                                               //1833:         LD      (hl),d
        LD      (a,val(NPLY));          //  Current ply counter            //1834:         LD      a,(NPLY)        ; Current ply counter
        LD      (hl,addr(PLYMAX));      //  Maximum ply number ?           //1835:         LD      hl,PLYMAX       ; Maximum ply number ?
        CP      (ptr(hl));              //  Compare                        //1836:         CP      (hl)            ; Compare
        JR      (C,FM18);               //  Jump if not max                //1837:         JR      C,FM18          ; Jump if not max
        CALLu   (MOVE);                 //  Execute move on board array    //1838:         CALL    MOVE            ; Execute move on board array
        CALLu   (INCHK);                //  Check for legal move           //1839:         CALL    INCHK           ; Check for legal move
        AND     (a);                    //  Is move legal                  //1840:         AND     a               ; Is move legal
        JR      (Z,rel017);             //  Yes - jump                     //1841:         JR      Z,rel017        ; Yes - jump
        CALLu   (UNMOVE);               //  Restore board position         //1842:         CALL    UNMOVE          ; Restore board position
        JPu     (FM15);                 //  Jump                           //1843:         JP      FM15            ; Jump
rel017: LD      (a,val(NPLY));          //  Get ply counter                //1844: rel017: LD      a,(NPLY)        ; Get ply counter
        LD      (hl,addr(PLYMAX));      //  Max ply number                 //1845:         LD      hl,PLYMAX       ; Max ply number
        CP      (ptr(hl));              //  Beyond max ply ?               //1846:         CP      (hl)            ; Beyond max ply ?
        JR      (NZ,FM35);              //  Yes - jump                     //1847:         JR      NZ,FM35         ; Yes - jump
        LD      (a,val(COLOR));         //  Get current COLOR              //1848:         LD      a,(COLOR)       ; Get current color
        XOR     (0x80);                 //  Get opposite COLOR             //1849:         XOR     80H             ; Get opposite color
        CALLu   (INCHK1);               //  Determine if King is in check  //1850:         CALL    INCHK1          ; Determine if King is in check
        AND     (a);                    //  In check ?                     //1851:         AND     a               ; In check ?
        JR      (Z,FM35);               //  No - jump                      //1852:         JR      Z,FM35          ; No - jump
        JPu     (FM19);                 //  Jump (One more ply for check)  //1853:         JP      FM19            ; Jump (One more ply for check)
FM18:   LD      (ix,val16(MLPTRJ));     //  Load move pointer              //1854: FM18:   LD      ix,(MLPTRJ)     ; Load move pointer
        LD      (a,ptr(ix+MLVAL));      //  Get move score                 //1855:         LD      a,(ix+MLVAL)    ; Get move score
        AND     (a);                    //  Is it zero (illegal move) ?    //1856:         AND     a               ; Is it zero (illegal move) ?
        JR      (Z,FM15);               //  Yes - jump                     //1857:         JR      Z,FM15          ; Yes - jump
        CALLu   (MOVE);                 //  Execute move on board array    //1858:         CALL    MOVE            ; Execute move on board array
FM19:   LD      (hl,addr(COLOR));       //  Toggle color                   //1859: FM19:   LD      hl,COLOR        ; Toggle color
        LD      (a,0x80);                                                  //1860:         LD      a,80H
        XOR     (ptr(hl));                                                 //1861:         XOR     (hl)
        LD      (ptr(hl),a);            //  Save new color                 //1862:         LD      (hl),a          ; Save new color
        BIT     (7,a);                  //  Is it white ?                  //1863:         BIT     7,a             ; Is it white ?
        JR      (NZ,rel018);            //  No - jump                      //1864:         JR      NZ,rel018       ; No - jump
        LD      (hl,addr(MOVENO));      //  Increment move number          //1865:         LD      hl,MOVENO       ; Increment move number
        INC     (ptr(hl));                                                 //1866:         INC     (hl)
rel018: LD      (hl,val16(SCRIX));      //  Load score table pointer       //1867: rel018: LD      hl,(SCRIX)      ; Load score table pointer
        LD      (a,ptr(hl));            //  Get score two plys above       //1868:         LD      a,(hl)          ; Get score two plys above
        INC16   (hl);                   //  Increment to current ply       //1869:         INC     hl              ; Increment to current ply
        INC16   (hl);                                                      //1870:         INC     hl
        LD      (ptr(hl),a);            //  Save score as initial value    //1871:         LD      (hl),a          ; Save score as initial value
        DEC16   (hl);                   //  Decrement pointer              //1872:         DEC     hl              ; Decrement pointer
        LD      (val16(SCRIX),hl);      //  Save it                        //1873:         LD      (SCRIX),hl      ; Save it
        JPu     (FM5);                  //  Jump                           //1874:         JP      FM5             ; Jump
FM25:   LD      (a,val(MATEF));         //  Get mate flag                  //1875: FM25:   LD      a,(MATEF)       ; Get mate flag
        AND     (a);                    //  Checkmate or stalemate ?       //1876:         AND     a               ; Checkmate or stalemate ?
        JR      (NZ,FM30);              //  No - jump                      //1877:         JR      NZ,FM30         ; No - jump
        LD      (a,val(CKFLG));         //  Get check flag                 //1878:         LD      a,(CKFLG)       ; Get check flag
        AND     (a);                    //  Was King in check ?            //1879:         AND     a               ; Was King in check ?
        LD      (a,0x80);               //  Pre-set stalemate score        //1880:         LD      a,80H           ; Pre-set stalemate score
        JR      (Z,FM36);               //  No - jump (stalemate)          //1881:         JR      Z,FM36          ; No - jump (stalemate)
        LD      (a,val(MOVENO));        //  Get move number                //1882:         LD      a,(MOVENO)      ; Get move number
        LD      (val(PMATE),a);         //  Save                           //1883:         LD      (PMATE),a       ; Save
        LD      (a,0xFF);               //  Pre-set checkmate score        //1884:         LD      a,0FFH          ; Pre-set checkmate score
        JPu     (FM36);                 //  Jump                           //1885:         JP      FM36            ; Jump
FM30:   LD      (a,val(NPLY));          //  Get ply counter                //1886: FM30:   LD      a,(NPLY)        ; Get ply counter
        CP      (1);                    //  At top of tree ?               //1887:         CP      1               ; At top of tree ?
        RET     (Z);                    //  Yes - return                   //1888:         RET     Z               ; Yes - return
        CALLu   (ASCEND);               //  Ascend one ply in tree         //1889:         CALL    ASCEND          ; Ascend one ply in tree
        LD      (hl,val16(SCRIX));      //  Load score table pointer       //1890:         LD      hl,(SCRIX)      ; Load score table pointer
        INC16   (hl);                   //  Increment to current ply       //1891:         INC     hl              ; Increment to current ply
        INC16   (hl);                   //                                 //1892:         INC     hl
        LD      (a,ptr(hl));            //  Get score                      //1893:         LD      a,(hl)          ; Get score
        DEC16   (hl);                   //  Restore pointer                //1894:         DEC     hl              ; Restore pointer
        DEC16   (hl);                   //                                 //1895:         DEC     hl
        JPu     (FM37);                 //  Jump                           //1896:         JP      FM37            ; Jump
FM35:   CALLu   (PINFND);               //  Compile pin list               //1897: FM35:   CALL    PINFND          ; Compile pin list
        CALLu   (POINTS);               //  Evaluate move                  //1898:         CALL    POINTS          ; Evaluate move
        CALLu   (UNMOVE);               //  Restore board position         //1899:         CALL    UNMOVE          ; Restore board position
        LD      (a,val(VALM));          //  Get value of move              //1900:         LD      a,(VALM)        ; Get value of move
FM36:   LD      (hl,addr(MATEF));       //  Set mate flag                  //1901: FM36:   LD      hl,MATEF        ; Set mate flag
        SET     (0,ptr(hl));            //                                 //1902:         SET     0,(hl)
        LD      (hl,val16(SCRIX));      //  Load score table pointer       //1903:         LD      hl,(SCRIX)      ; Load score table pointer
FM37:                                                                      //1904: FM37:
        CP      (ptr(hl));              //  Compare to score 2 ply above   //1905:         CP      (hl)            ; Compare to score 2 ply above
        JR      (C,FM40);               //  Jump if less                   //1906:         JR      C,FM40          ; Jump if less
        JR      (Z,FM40);               //  Jump if equal                  //1907:         JR      Z,FM40          ; Jump if equal
        NEG;                            //  Negate score                   //1908:         NEG                     ; Negate score
        INC16   (hl);                   //  Incr score table pointer       //1909:         INC     hl              ; Incr score table pointer
        CP      (ptr(hl));              //  Compare to score 1 ply above   //1910:         CP      (hl)            ; Compare to score 1 ply above
        JP      (C,FM15);               //  Jump if less than              //1911:         JP      C,FM15          ; Jump if less than
        JP      (Z,FM15);               //  Jump if equal                  //1912:         JP      Z,FM15          ; Jump if equal
        LD      (ptr(hl),a);            //  Save as new score 1 ply above  //1913:         LD      (hl),a          ; Save as new score 1 ply above
        LD      (a,val(NPLY));          //  Get current ply counter        //1914:         LD      a,(NPLY)        ; Get current ply counter
        CP      (1);                    //  At top of tree ?               //1915:         CP      1               ; At top of tree ?
        JP      (NZ,FM15);              //  No - jump                      //1916:         JP      NZ,FM15         ; No - jump
        LD      (hl,val16(MLPTRJ));     //  Load current move pointer      //1917:         LD      hl,(MLPTRJ)     ; Load current move pointer
        LD      (val16(BESTM),hl);      //  Save as best move pointer      //1918:         LD      (BESTM),hl      ; Save as best move pointer
        LD      (a,val(SCORE+1));      //  Get best move score             //1919:         LD      a,(SCORE+1)     ; Get best move score
        CP      (0xff);                 //  Was it a checkmate ?           //1920:         CP      0FFH            ; Was it a checkmate ?
        JP      (NZ,FM15);              //  No - jump                      //1921:         JP      NZ,FM15         ; No - jump
        LD      (hl,addr(PLYMAX));      //  Get maximum ply number         //1922:         LD      hl,PLYMAX       ; Get maximum ply number
        DEC     (ptr(hl));              //  Subtract 2                     //1923:         DEC     (hl)            ; Subtract 2
        DEC     (ptr(hl));                                                 //1924:         DEC     (hl)
        LD      (a,val(KOLOR));         //  Get computer's color           //1925:         LD      a,(KOLOR)       ; Get computer's color
        BIT     (7,a);                  //  Is it white ?                  //1926:         BIT     7,a             ; Is it white ?
        RET     (Z);                    //  Yes - return                   //1927:         RET     Z               ; Yes - return
        LD      (hl,addr(PMATE));       //  Checkmate move number          //1928:         LD      hl,PMATE        ; Checkmate move number
        DEC     (ptr(hl));              //  Decrement                      //1929:         DEC     (hl)            ; Decrement
        RETu;                           //  Return                         //1930:         RET                     ; Return
FM40:   CALLu   (ASCEND);               //  Ascend one ply in tree         //1931: FM40:   CALL    ASCEND          ; Ascend one ply in tree
        JPu     (FM15);                 //  Jump                           //1932:         JP      FM15            ; Jump
}                                                                          //1933: 
                                                                           //1934: ;***********************************************************
//***********************************************************              //1935: ; ASCEND TREE ROUTINE
// ASCEND TREE ROUTINE
//***********************************************************
// FUNCTION:  --  To adjust all necessary parameters to                    //1936: ;***********************************************************
//                ascend one ply in the tree.                              //1937: ; FUNCTION:  --  To adjust all necessary parameters to
//                                                                         //1938: ;                ascend one ply in the tree.
// CALLED BY: --  FNDMOV                                                   //1939: ;
//                                                                         //1940: ; CALLED BY: --  FNDMOV
// CALLS:     --  UNMOVE                                                   //1941: ;
//                                                                         //1942: ; CALLS:     --  UNMOVE
// ARGUMENTS: --  None                                                     //1943: ;
//***********************************************************              //1944: ; ARGUMENTS: --  None
void ASCEND() {                                                            //1945: ;***********************************************************
        LD      (hl,addr(COLOR));       //  Toggle color                   //1946: ASCEND: LD      hl,COLOR        ; Toggle color
        LD      (a,0x80);               //                                 //1947:         LD      a,80H
        XOR     (ptr(hl));              //                                 //1948:         XOR     (hl)
        LD      (ptr(hl),a);            //  Save new color                 //1949:         LD      (hl),a          ; Save new color
        BIT     (7,a);                  //  Is it white ?                  //1950:         BIT     7,a             ; Is it white ?
        JR      (Z,rel019);             //  Yes - jump                     //1951:         JR      Z,rel019        ; Yes - jump
        LD      (hl,addr(MOVENO));      //  Decrement move number          //1952:         LD      hl,MOVENO       ; Decrement move number
        DEC     (ptr(hl));              //                                 //1953:         DEC     (hl)
rel019: LD      (hl,val16(SCRIX));      //  Load score table index         //1954: rel019: LD      hl,(SCRIX)      ; Load score table index
        DEC16   (hl);                   //  Decrement                      //1955:         DEC     hl              ; Decrement
        LD      (val16(SCRIX),hl);      //  Save                           //1956:         LD      (SCRIX),hl      ; Save
        LD      (hl,addr(NPLY));        //  Decrement ply counter          //1957:         LD      hl,NPLY         ; Decrement ply counter
        DEC     (ptr(hl));              //                                 //1958:         DEC     (hl)
        LD      (hl,val16(MLPTRI));     //  Load ply list pointer          //1959:         LD      hl,(MLPTRI)     ; Load ply list pointer
        DEC16   (hl);                   //  Load pointer to move list top  //1960:         DEC     hl              ; Load pointer to move list top
        LD      (d,ptr(hl));            //                                 //1961:         LD      d,(hl)
        DEC16   (hl);                   //                                 //1962:         DEC     hl
        LD      (e,ptr(hl));            //                                 //1963:         LD      e,(hl)
        LD      (val16(MLNXT),de);      //  Update move list avail ptr     //1964:         LD      (MLNXT),de      ; Update move list avail ptr
        DEC16   (hl);                   //  Get ptr to next move to undo   //1965:         DEC     hl              ; Get ptr to next move to undo
        LD      (d,ptr(hl));            //                                 //1966:         LD      d,(hl)
        DEC16   (hl);                   //                                 //1967:         DEC     hl
        LD      (e,ptr(hl));            //                                 //1968:         LD      e,(hl)
        LD      (val16(MLPTRI),hl);     //  Save new ply list pointer      //1969:         LD      (MLPTRI),hl     ; Save new ply list pointer
        LD      (val16(MLPTRJ),de);     //  Save next move pointer         //1970:         LD      (MLPTRJ),de     ; Save next move pointer
        CALLu   (UNMOVE);               //  Restore board to previous ply  //1971:         CALL    UNMOVE          ; Restore board to previous ply
        RETu;                           //  Return                         //1972:         RET                     ; Return
}                                                                          //1973: 
                                                                           //1974: ;***********************************************************
//***********************************************************              //1975: ; ONE MOVE BOOK OPENING
// ONE MOVE BOOK OPENING
// **********************************************************
// FUNCTION:   --  To provide an opening book of a single                  //1976: ; **********************************************************
//                 move.                                                   //1977: ; FUNCTION:   --  To provide an opening book of a single
//                                                                         //1978: ;                 move.
// CALLED BY:  --  FNDMOV                                                  //1979: ;
//                                                                         //1980: ; CALLED BY:  --  FNDMOV
// CALLS:      --  None                                                    //1981: ;
//                                                                         //1982: ; CALLS:      --  None
// ARGUMENTS:  --  None                                                    //1983: ;
//***********************************************************              //1984: ; ARGUMENTS:  --  None
void BOOK() {                                                              //1985: ;***********************************************************
        POPf    (af);                   //  Abort return to FNDMOV         //1986: BOOK:   POP     af              ; Abort return to FNDMOV
        LD      (hl,addr(SCORE)+1);     //  Zero out score                 //1987:         LD      hl,SCORE+1      ; Zero out score
        LD      (ptr(hl),0);            //  Zero out score table           //1988:         LD      (hl),0          ; Zero out score table
        LD      (hl,addr(BMOVES)-2);    //  Init best move ptr to book     //1989:         LD      hl,BMOVES-2     ; Init best move ptr to book
        LD      (val16(BESTM),hl);      //                                 //1990:         LD      (BESTM),hl
        LD      (hl,addr(BESTM));       //  Initialize address of pointer  //1991:         LD      hl,BESTM        ; Initialize address of pointer
        LD      (a,val(KOLOR));         //  Get computer's color           //1992:         LD      a,(KOLOR)       ; Get computer's color
        AND     (a);                    //  Is it white ?                  //1993:         AND     a               ; Is it white ?
        JR      (NZ,BM5);               //  No - jump                      //1994:         JR      NZ,BM5          ; No - jump
        LD      (a,rand());             //  Load refresh reg (random no)   //1995:         LD      a,r             ; Load refresh reg (random no)
        BIT     (0,a);                  //  Test random bit                //1996:         BIT     0,a             ; Test random bit
        RET     (Z);                    //  Return if zero (P-K4)          //1997:         RET     Z               ; Return if zero (P-K4)
        INC     (ptr(hl));              //  P-Q4                           //1998:         INC     (hl)            ; P-Q4
        INC     (ptr(hl));              //                                 //1999:         INC     (hl)
        INC     (ptr(hl));              //                                 //2000:         INC     (hl)
        RETu;                           //  Return                         //2001:         RET                     ; Return
BM5:    INC     (ptr(hl));              //  Increment to black moves       //2002: BM5:    INC     (hl)            ; Increment to black moves
        INC     (ptr(hl));              //                                 //2003:         INC     (hl)
        INC     (ptr(hl));              //                                 //2004:         INC     (hl)
        INC     (ptr(hl));              //                                 //2005:         INC     (hl)
        INC     (ptr(hl));              //                                 //2006:         INC     (hl)
        INC     (ptr(hl));              //                                 //2007:         INC     (hl)
        LD      (ix,val16(MLPTRJ));     //  Pointer to opponents 1st move  //2008:         LD      ix,(MLPTRJ)     ; Pointer to opponents 1st move
        LD      (a,ptr(ix+MLFRP));      //  Get "from" position            //2009:         LD      a,(ix+MLFRP)    ; Get "from" position
        CP      (22);                   //  Is it a Queen Knight move ?    //2010:         CP      22              ; Is it a Queen Knight move ?
        JR      (Z,BM9);                //  Yes - Jump                     //2011:         JR      Z,BM9           ; Yes - Jump
        CP      (27);                   //  Is it a King Knight move ?     //2012:         CP      27              ; Is it a King Knight move ?
        JR      (Z,BM9);                //  Yes - jump                     //2013:         JR      Z,BM9           ; Yes - jump
        CP      (34);                   //  Is it a Queen Pawn ?           //2014:         CP      34              ; Is it a Queen Pawn ?
        JR      (Z,BM9);                //  Yes - jump                     //2015:         JR      Z,BM9           ; Yes - jump
        RET     (C);                    //  If Queen side Pawn opening -   //2016:         RET     C               ; If Queen side Pawn opening -
// return (P-K4)                                                           //2017:                                 ; return (P-K4)
        CP      (35);                   //  Is it a King Pawn ?            //2018:         CP      35              ; Is it a King Pawn ?
        RET     (Z);                    //  Yes - return (P-K4)            //2019:         RET     Z               ; Yes - return (P-K4)
BM9:    INC     (ptr(hl));              //  (P-Q4)                         //2020: BM9:    INC     (hl)            ; (P-Q4)
        INC     (ptr(hl));              //                                 //2021:         INC     (hl)
        INC     (ptr(hl));              //                                 //2022:         INC     (hl)
        RETu;                           //  Return to CPTRMV               //2023:         RET                     ; Return to CPTRMV
}                                                                          //2024: 
                                                                           //2025: ;*******************************************************
//                                                                         //2026: ; GRAPHICS DATA BASE
//  Omit some Z80 User Interface code, eg                                  //2027: ;*******************************************************
//  user interface data including graphics data and text messages          //2028: ; DESCRIPTION:  The Graphics Data Base contains the
//  Also macros                                                            //2029: ;               necessary stored data to produce the piece
//      CARRET, CLRSCR, PRTLIN, PRTBLK and EXIT                            //2030: ;               on the board. Only the center 4 x 4 blocks are
//  and functions                                                          //2031: ;               stored and only for a Black Piece on a White
//      DRIVER and INTERR                                                  //2032: ;               square. A White piece on a black square is
//                                                                         //2033: ;               produced by complementing each block, and a
                                                                           //2034: ;               piece on its own color square is produced
//***********************************************************              // (lines 2035-2316 omitted) 2317: 
// COMPUTER MOVE ROUTINE                                                   //2318: ;***********************************************************
//***********************************************************              //2319: ; COMPUTER MOVE ROUTINE
// FUNCTION:   --  To control the search for the computers move            //2320: ;***********************************************************
//                 and the display of that move on the board               //2321: ; FUNCTION:   --  To control the search for the computers move
//                 and in the move list.                                   //2322: ;                 and the display of that move on the board
//                                                                         //2323: ;                 and in the move list.
// CALLED BY:  --  DRIVER                                                  //2324: ;
//                                                                         //2325: ; CALLED BY:  --  DRIVER
// CALLS:      --  FNDMOV                                                  //2326: ;
//                 FCDMAT                                                  //2327: ; CALLS:      --  FNDMOV
//                 MOVE                                                    //2328: ;                 FCDMAT
//                 EXECMV                                                  //2329: ;                 MOVE
//                 BITASN                                                  //2330: ;                 EXECMV
//                 INCHK                                                   //2331: ;                 BITASN
//                                                                         //2332: ;                 INCHK
// MACRO CALLS:    PRTBLK                                                  //2333: ;
//                 CARRET                                                  //2334: ; MACRO CALLS:    PRTBLK
//                                                                         //2335: ;                 CARRET
// ARGUMENTS:  --  None                                                    //2336: ;
//***********************************************************              //2337: ; ARGUMENTS:  --  None
void CPTRMV() {                                                            //2338: ;***********************************************************
        CALLu   (FNDMOV);               //  Select best move               //2339: CPTRMV: CALL    FNDMOV          ; Select best move
        LD      (hl,val16(BESTM));      //  Move list pointer variable     //2340:         LD      hl,(BESTM)      ; Move list pointer variable
        LD      (val16(MLPTRJ),hl);     //  Pointer to move data           //2341:         LD      (MLPTRJ),hl     ; Pointer to move data
        LD      (a,(val(SCORE+1)));     //  To check for mates             //2342:         LD      a,(SCORE+1)     ; To check for mates
        CP      (1);                    //  Mate against computer ?        //2343:         CP      1               ; Mate against computer ?
        JR      (NZ,CP0C);              //  No - jump                      //2344:         JR      NZ,CP0C         ; No - jump
        LD      (c,1);                  //  Computer mate flag             //2345:         LD      c,1             ; Computer mate flag
        CALLu   (FCDMAT);               //  Full checkmate ?               //2346:         CALL    FCDMAT          ; Full checkmate ?
CP0C:   CALLu   (MOVE);                 //  Produce move on board array    //2347: CP0C:   CALL    MOVE            ; Produce move on board array
        CALLu   (EXECMV);               //  Make move on graphics board    //2348:         CALL    EXECMV          ; Make move on graphics board
// and return info about it                                                //2349:                                 ; and return info about it
        LD      (a,b);                  //  Special move flags             //2350:         LD      a,b             ; Special move flags
        AND     (a);                    //  Special ?                      //2351:         AND     a               ; Special ?
        JR      (NZ,CP10);              //  Yes - jump                     //2352:         JR      NZ,CP10         ; Yes - jump
        LD      (d,e);                  //  "To" position of the move      //2353:         LD      d,e             ; "To" position of the move
        CALLu   (BITASN);               //  Convert to Ascii               //2354:         CALL    BITASN          ; Convert to Ascii
        LD      (val16(MVEMSG+3),hl);   //  Put in move message            //2355:         LD      (MVEMSG+3),hl   ; Put in move message
        LD      (d,c);                  //  "From" position of the move    //2356:         LD      d,c             ; "From" position of the move
        CALLu   (BITASN);               //  Convert to Ascii               //2357:         CALL    BITASN          ; Convert to Ascii
        LD      (val16(MVEMSG),hl);     //  Put in move message            //2358:         LD      (MVEMSG),hl     ; Put in move message
        //PRTBLK  (MVEMSG,5);           //  Output text of move            //2359:         PRTBLK  MVEMSG,5        ; Output text of move
        JRu     (CP1C);                 //  Jump                           //2360:         JR      CP1C            ; Jump
CP10:   BIT     (1,b);                  //  King side castle ?             //2361: CP10:   BIT     1,b             ; King side castle ?
        JR      (Z,rel020);             //  No - jump                      //2362:         JR      Z,rel020        ; No - jump
        //PRTBLK  (addr(O_O),5);        //  Output "O-O"                   //2363:         PRTBLK  O_O,5           ; Output "O-O"
        JRu     (CP1C);                 //  Jump                           //2364:         JR      CP1C            ; Jump
rel020: BIT     (2,b);                  //  Queen side castle ?            //2365: rel020: BIT     2,b             ; Queen side castle ?
        JR      (Z,rel021);             //  No - jump                      //2366:         JR      Z,rel021        ; No - jump
        //PRTBLK  (addr(O_O_O),5);      //  Output "O-O-O"                 //2367:         PRTBLK  O_O_O,5         ; Output "O-O-O"
        JRu     (CP1C);                 //  Jump                           //2368:         JR      CP1C            ; Jump
rel021: //PRTBLK  (P_PEP,5);            //  Output "PxPep" - En passant    //2369: rel021: PRTBLK  P_PEP,5         ; Output "PxPep" - En passant
CP1C:   LD      (a,val(COLOR));         //  Should computer call check ?   //2370: CP1C:   LD      a,(COLOR)       ; Should computer call check ?
        LD      (b,a);                                                     //2371:         LD      b,a
        XOR     (0x80);                 //  Toggle color                   //2372:         XOR     80H             ; Toggle color
        LD      (val(COLOR),a);                                            //2373:         LD      (COLOR),a
        CALLu   (INCHK);                //  Check for check                //2374:         CALL    INCHK           ; Check for check
        AND     (a);                    //  Is enemy in check ?            //2375:         AND     a               ; Is enemy in check ?
        LD      (a,b);                  //  Restore color                  //2376:         LD      a,b             ; Restore color
        LD      (val(COLOR),a);                                            //2377:         LD      (COLOR),a
        JR      (Z,CP24);               //  No - return                    //2378:         JR      Z,CP24          ; No - return
        CARRET();                       //  New line                       //2379:         CARRET                  ; New line
        LD      (a,val(SCORE+1));       //  Check for player mated         //2380:         LD      a,(SCORE+1)     ; Check for player mated
        CP      (0xFF);                 //  Forced mate ?                  //2381:         CP      0FFH            ; Forced mate ?
        CALL    (NZ,TBCPMV);            //  No - Tab to computer column    //2382:         CALL    NZ,TBCPMV       ; No - Tab to computer column
        //PRTBLK  (CKMSG,5);            //  Output "check"                 //2383:         PRTBLK  CKMSG,5         ; Output "check"
        LD      (hl,addr(LINECT));      //  Address of screen line count   //2384:         LD      hl,LINECT       ; Address of screen line count
        INC     (ptr(hl));              //  Increment for message          //2385:         INC     (hl)            ; Increment for message
CP24:   LD      (a,val(SCORE+1));       //  Check again for mates          //2386: CP24:   LD      a,(SCORE+1)     ; Check again for mates
        CP      (0xFF);                 //  Player mated ?                 //2387:         CP      0FFH            ; Player mated ?
        RET     (NZ);                   //  No - return                    //2388:         RET     NZ              ; No - return
        LD      (c,0);                  //  Set player mate flag           //2389:         LD      c,0             ; Set player mate flag
        CALLu   (FCDMAT);               //  Full checkmate ?               //2390:         CALL    FCDMAT          ; Full checkmate ?
        RETu;                           //  Return                         //2391:         RET                     ; Return
}                                                                          //2392: 
                                                                           //2393: ;***********************************************************
//                                                                         //2394: ; FORCED MATE HANDLING
//  Omit some more Z80 user interface stuff, functions                     //2395: ;***********************************************************
//  FCDMAT, TBPLCL, TBCPCL, TBPLMV and TBCPMV                              //2396: ; FUNCTION:   --  To examine situations where there exits
//                                                                         //2397: ;                 a forced mate and determine whether or
                                                                           //2398: ;                 not the current move is checkmate. If it is,
//***********************************************************              // (lines 2399-2518 omitted) 2519: 
// BOARD INDEX TO ASCII SQUARE NAME                                        //2520: ;***********************************************************
//***********************************************************              //2521: ; BOARD INDEX TO ASCII SQUARE NAME
// FUNCTION:   --  To translate a hexadecimal index in the                 //2522: ;***********************************************************
//                 board array into an ascii description                   //2523: ; FUNCTION:   --  To translate a hexadecimal index in the
//                 of the square in algebraic chess notation.              //2524: ;                 board array into an ascii description
//                                                                         //2525: ;                 of the square in algebraic chess notation.
// CALLED BY:  --  CPTRMV                                                  //2526: ;
//                                                                         //2527: ; CALLED BY:  --  CPTRMV
// CALLS:      --  DIVIDE                                                  //2528: ;
//                                                                         //2529: ; CALLS:      --  DIVIDE
// ARGUMENTS:  --  Board index input in register D and the                 //2530: ;
//                 Ascii square name is output in register                 //2531: ; ARGUMENTS:  --  Board index input in register D and the
//                 pair HL.                                                //2532: ;                 Ascii square name is output in register
//***********************************************************              //2533: ;                 pair HL.
void BITASN() {                                                            //2534: ;***********************************************************
        SUB     (a);                    //  Get ready for division         //2535: BITASN: SUB     a               ; Get ready for division
        LD      (e,10);                 //                                 //2536:         LD      e,10
        CALLu   (DIVIDE);               //  Divide                         //2537:         CALL    DIVIDE          ; Divide
        DEC     (d);                    //  Get rank on 1-8 basis          //2538:         DEC     d               ; Get rank on 1-8 basis
        ADD     (a,0x60);               //  Convert file to Ascii (a-h)    //2539:         ADD     a,60H           ; Convert file to Ascii (a-h)
        LD      (l,a);                  //  Save                           //2540:         LD      l,a             ; Save
        LD      (a,d);                  //  Rank                           //2541:         LD      a,d             ; Rank
        ADD     (a,0x30);               //  Convert rank to Ascii (1-8)    //2542:         ADD     a,30H           ; Convert rank to Ascii (1-8)
        LD      (h,a);                  //  Save                           //2543:         LD      h,a             ; Save
        RETu;                           //  Return                         //2544:         RET                     ; Return
}                                                                          //2545: 
                                                                           //2546: ;***********************************************************
//                                                                         //2547: ; PLAYERS MOVE ANALYSIS
//  Omit some more Z80 user interface stuff, function                      //2548: ;***********************************************************
//  PLYRMV                                                                 //2549: ; FUNCTION:   --  To accept and validate the players move
//                                                                         //2550: ;                 and produce it on the graphics board. Also
                                                                           //2551: ;                 allows player to resign the game by
//***********************************************************              // (lines 2552-2597 omitted) 2598: 
// ASCII SQUARE NAME TO BOARD INDEX                                        //2599: ;***********************************************************
//***********************************************************              //2600: ; ASCII SQUARE NAME TO BOARD INDEX
// FUNCTION:   --  To convert an algebraic square name in                  //2601: ;***********************************************************
//                 Ascii to a hexadecimal board index.                     //2602: ; FUNCTION:   --  To convert an algebraic square name in
//                 This routine also checks the input for                  //2603: ;                 Ascii to a hexadecimal board index.
//                 validity.                                               //2604: ;                 This routine also checks the input for
//                                                                         //2605: ;                 validity.
// CALLED BY:  --  PLYRMV                                                  //2606: ;
//                                                                         //2607: ; CALLED BY:  --  PLYRMV
// CALLS:      --  MLTPLY                                                  //2608: ;
//                                                                         //2609: ; CALLS:      --  MLTPLY
// ARGUMENTS:  --  Accepts the square name in register pair HL             //2610: ;
//                 and outputs the board index in register A.              //2611: ; ARGUMENTS:  --  Accepts the square name in register pair HL
//                 Register B = 0 if ok. Register B = Register             //2612: ;                 and outputs the board index in register A.
//                 A if invalid.                                           //2613: ;                 Register B = 0 if ok. Register B = Register
//***********************************************************              //2614: ;                 A if invalid.
void ASNTBI() {                                                            //2615: ;***********************************************************
        LD      (a,l);                  //  Ascii rank (1 - 8)             //2616: ASNTBI: LD      a,l             ; Ascii rank (1 - 8)
        SUB     (0x30);                 //  Rank 1 - 8                     //2617:         SUB     30H             ; Rank 1 - 8
        CP      (1);                    //  Check lower bound              //2618:         CP      1               ; Check lower bound
        JP      (M,AT04);               //  Jump if invalid                //2619:         JP      M,AT04          ; Jump if invalid
        CP      (9);                    //  Check upper bound              //2620:         CP      9               ; Check upper bound
        JR      (NC,AT04);              //  Jump if invalid                //2621:         JR      NC,AT04         ; Jump if invalid
        INC     (a);                    //  Rank 2 - 9                     //2622:         INC     a               ; Rank 2 - 9
        LD      (d,a);                  //  Ready for multiplication       //2623:         LD      d,a             ; Ready for multiplication
        LD      (e,10);                                                    //2624:         LD      e,10
        CALLu   (MLTPLY);               //  Multiply                       //2625:         CALL    MLTPLY          ; Multiply
        LD      (a,h);                  //  Ascii file letter (a - h)      //2626:         LD      a,h             ; Ascii file letter (a - h)
        SUB     (0x40);                 //  File 1 - 8                     //2627:         SUB     40H             ; File 1 - 8
        CP      (1);                    //  Check lower bound              //2628:         CP      1               ; Check lower bound
        JP      (M,AT04);               //  Jump if invalid                //2629:         JP      M,AT04          ; Jump if invalid
        CP      (9);                    //  Check upper bound              //2630:         CP      9               ; Check upper bound
        JR      (NC,AT04);              //  Jump if invalid                //2631:         JR      NC,AT04         ; Jump if invalid
        ADD     (a,d);                  //  File+Rank(20-90)=Board index   //2632:         ADD     a,d             ; File+Rank(20-90)=Board index
        LD      (b,0);                  //  Ok flag                        //2633:         LD      b,0             ; Ok flag
        RETu;                           //  Return                         //2634:         RET                     ; Return
AT04:   LD      (b,a);                  //  Invalid flag                   //2635: AT04:   LD      b,a             ; Invalid flag
        RETu;                           //  Return                         //2636:         RET                     ; Return
}                                                                          //2637: 
                                                                           //2638: ;***********************************************************
//***********************************************************              //2639: ; VALIDATE MOVE SUBROUTINE
// VALIDATE MOVE SUBROUTINE
//***********************************************************
// FUNCTION:   --  To check a players move for validity.                   //2640: ;***********************************************************
//                                                                         //2641: ; FUNCTION:   --  To check a players move for validity.
// CALLED BY:  --  PLYRMV                                                  //2642: ;
//                                                                         //2643: ; CALLED BY:  --  PLYRMV
// CALLS:      --  GENMOV                                                  //2644: ;
//                 MOVE                                                    //2645: ; CALLS:      --  GENMOV
//                 INCHK                                                   //2646: ;                 MOVE
//                 UNMOVE                                                  //2647: ;                 INCHK
//                                                                         //2648: ;                 UNMOVE
// ARGUMENTS:  --  Returns flag in register A, 0 for valid                 //2649: ;
//                 and 1 for invalid move.                                 //2650: ; ARGUMENTS:  --  Returns flag in register A, 0 for valid
//***********************************************************              //2651: ;                 and 1 for invalid move.
void VALMOV() {                                                            //2652: ;***********************************************************
        LD      (hl,val16(MLPTRJ));     //  Save last move pointer         //2653: VALMOV: LD      hl,(MLPTRJ)     ; Save last move pointer
        PUSH    (hl);                   //  Save register                  //2654:         PUSH    hl              ; Save register
        LD      (a,val(KOLOR));         //  Computers color                //2655:         LD      a,(KOLOR)       ; Computers color
        XOR     (0x80);                 //  Toggle color                   //2656:         XOR     80H             ; Toggle color
        LD      (val(COLOR),a);         //  Store                          //2657:         LD      (COLOR),a       ; Store
        LD      (hl,addr(PLYIX)-2);     //  Load move list index           //2658:         LD      hl,PLYIX-2      ; Load move list index
        LD      (val16(MLPTRI),hl);     //                                 //2659:         LD      (MLPTRI),hl
        LD      (hl,addr(MLIST)+1024);  //  Next available list pointer    //2660:         LD      hl,MLIST+1024   ; Next available list pointer
        LD      (val16(MLNXT),hl);      //                                 //2661:         LD      (MLNXT),hl
        CALLu   (GENMOV);               //  Generate opponents moves       //2662:         CALL    GENMOV          ; Generate opponents moves
        LD      (ix,addr(MLIST)+1024);  //  Index to start of moves        //2663:         LD      ix,MLIST+1024   ; Index to start of moves
VA5:    LD      (a,val(MVEMSG));        //  "From" position                //2664: VA5:    LD      a,(MVEMSG)      ; "From" position
        CP      (ptr(ix+MLFRP));        //  Is it in list ?                //2665:         CP      (ix+MLFRP)      ; Is it in list ?
        JR      (NZ,VA6);               //  No - jump                      //2666:         JR      NZ,VA6          ; No - jump
        LD      (a,val(MVEMSG+1));      //  "To" position                  //2667:         LD      a,(MVEMSG+1)    ; "To" position
        CP      (ptr(ix+MLTOP));        //  Is it in list ?                //2668:         CP      (ix+MLTOP)      ; Is it in list ?
        JR      (Z,VA7);                //  Yes - jump                     //2669:         JR      Z,VA7           ; Yes - jump
VA6:    LD      (e,ptr(ix+MLPTR));      //  Pointer to next list move      //2670: VA6:    LD      e,(ix+MLPTR)    ; Pointer to next list move
        LD      (d,ptr(ix+MLPTR+1));    //                                 //2671:         LD      d,(ix+MLPTR+1)
        XOR     (a);                    //  At end of list ?               //2672:         XOR     a               ; At end of list ?
        CP      (d);                    //                                 //2673:         CP      d
        JR      (Z,VA10);               //  Yes - jump                     //2674:         JR      Z,VA10          ; Yes - jump
        PUSH    (de);                   //  Move to X register             //2675:         PUSH    de              ; Move to X register
        POP     (ix);                   //                                 //2676:         POP     ix
        JRu     (VA5);                  //  Jump                           //2677:         JR      VA5             ; Jump
VA7:    LD      (val16(MLPTRJ),ix);     //  Save opponents move pointer    //2678: VA7:    LD      (MLPTRJ),ix     ; Save opponents move pointer
        CALLu   (MOVE);                 //  Make move on board array       //2679:         CALL    MOVE            ; Make move on board array
        CALLu   (INCHK);                //  Was it a legal move ?          //2680:         CALL    INCHK           ; Was it a legal move ?
        AND     (a);                    //                                 //2681:         AND     a
        JR      (NZ,VA9);               //  No - jump                      //2682:         JR      NZ,VA9          ; No - jump
VA8:    POP     (hl);                   //  Restore saved register         //2683: VA8:    POP     hl              ; Restore saved register
        RETu;                           //  Return                         //2684:         RET                     ; Return
VA9:    CALLu   (UNMOVE);               //  Un-do move on board array      //2685: VA9:    CALL    UNMOVE          ; Un-do move on board array
VA10:   LD      (a,1);                  //  Set flag for invalid move      //2686: VA10:   LD      a,1             ; Set flag for invalid move
        POP     (hl);                   //  Restore saved register         //2687:         POP     hl              ; Restore saved register
        LD      (val16(MLPTRJ),hl);     //  Save move pointer              //2688:         LD      (MLPTRJ),hl     ; Save move pointer
        RETu;                           //  Return                         //2689:         RET                     ; Return
}                                                                          //2690: 
                                                                           //2691: ;***********************************************************
//                                                                         //2692: ; ACCEPT INPUT CHARACTER
//  Omit some more Z80 user interface stuff, functions                     //2693: ;***********************************************************
//  CHARTR, PGIFND, MATED, ANALYS                                          //2694: ; FUNCTION:   --  Accepts a single character input from the
//                                                                         //2695: ;                 console keyboard and places it in the A
                                                                           //2696: ;                 register. The character is also echoed on
//***********************************************************              // (lines 2697-2929 omitted) 2930: 
// UPDATE POSITIONS OF ROYALTY                                             //2931: ;***********************************************************
//***********************************************************              //2932: ; UPDATE POSITIONS OF ROYALTY
// FUNCTION:   --  To update the positions of the Kings                    //2933: ;***********************************************************
//                 and Queen after a change of board position              //2934: ; FUNCTION:   --  To update the positions of the Kings
//                 in ANALYS.                                              //2935: ;                 and Queen after a change of board position
//                                                                         //2936: ;                 in ANALYS.
// CALLED BY:  --  ANALYS                                                  //2937: ;
//                                                                         //2938: ; CALLED BY:  --  ANALYS
// CALLS:      --  None                                                    //2939: ;
//                                                                         //2940: ; CALLS:      --  None
// ARGUMENTS:  --  None                                                    //2941: ;
//***********************************************************              //2942: ; ARGUMENTS:  --  None
void ROYALT() {                                                            //2943: ;***********************************************************
        LD      (hl,addr(POSK));        //  Start of Royalty array         //2944: ROYALT: LD      hl,POSK         ; Start of Royalty array
        LD      (b,4);                  //  Clear all four positions       //2945:         LD      b,4             ; Clear all four positions
back06: LD      (ptr(hl),0);            //                                 //2946: back06: LD      (hl),0
        INC16   (hl);                   //                                 //2947:         INC     hl
        DJNZ    (back06);               //                                 //2948:         DJNZ    back06
        LD      (a,21);                 //  First board position           //2949:         LD      a,21            ; First board position
RY04:   LD      (val(M1),a);            //  Set up board index             //2950: RY04:   LD      (M1),a          ; Set up board index
        LD      (hl,addr(POSK));        //  Address of King position       //2951:         LD      hl,POSK         ; Address of King position
        LD      (ix,val16(M1));         //                                 //2952:         LD      ix,(M1)
        LD      (a,ptr(ix+BOARD));      //  Fetch board contents           //2953:         LD      a,(ix+BOARD)    ; Fetch board contents
        BIT     (7,a);                  //  Test color bit                 //2954:         BIT     7,a             ; Test color bit
        JR      (Z,rel023);             //  Jump if white                  //2955:         JR      Z,rel023        ; Jump if white
        INC16   (hl);                   //  Offset for black               //2956:         INC     hl              ; Offset for black
rel023: AND     (7);                    //  Delete flags, leave piece      //2957: rel023: AND     7               ; Delete flags, leave piece
        CP      (KING);                 //  King ?                         //2958:         CP      KING            ; King ?
        JR      (Z,RY08);               //  Yes - jump                     //2959:         JR      Z,RY08          ; Yes - jump
        CP      (QUEEN);                //  Queen ?                        //2960:         CP      QUEEN           ; Queen ?
        JR      (NZ,RY0C);              //  No - jump                      //2961:         JR      NZ,RY0C         ; No - jump
        INC     (ptr(hl));              //  Queen position                 //2962:         INC     hl              ; Queen position
        INC     (ptr(hl));              //  Plus offset                    //2963:         INC     hl              ; Plus offset
RY08:   LD      (a,val(M1));            //  Index                          //2964: RY08:   LD      a,(M1)          ; Index
        LD      (ptr(hl),a);            //  Save                           //2965:         LD      (hl),a          ; Save
RY0C:   LD      (a,val(M1));            //  Current position               //2966: RY0C:   LD      a,(M1)          ; Current position
        INC     (a);                    //  Next position                  //2967:         INC     a               ; Next position
        CP      (99);                   //  Done.?                         //2968:         CP      99              ; Done.?
        JR      (NZ,RY04);              //  No - jump                      //2969:         JR      NZ,RY04         ; No - jump
        RETu;                           //  Return                         //2970:         RET                     ; Return
}                                                                          //2971: 
                                                                           //2972: ;***********************************************************
//                                                                         //2973: ; SET UP EMPTY BOARD
//  Omit some more Z80 user interface stuff, functions                     //2974: ;***********************************************************
//  DSPBRD, BSETUP, INSPCE, CONVRT                                         //2975: ; FUNCTION:   --  Display graphics board and pieces.
//                                                                         //2976: ;
                                                                           //2977: ; CALLED BY:  --  DRIVER
//***********************************************************              // (lines 2978-3187 omitted) 3188: 
// POSITIVE INTEGER DIVISION                                               //3189: ;***********************************************************
//   inputs hi=A lo=D, divide by E                                         //3190: ; POSITIVE INTEGER DIVISION
//   output D, remainder in A                                              //3191: ;   inputs hi=A lo=D, divide by E
//***********************************************************              //3192: ;   output D, remainder in A
void DIVIDE() {                                                            //3193: ;***********************************************************
        PUSH    (bc);                   //                                 //3194: DIVIDE: PUSH    bc
        LD      (b,8);                  //                                 //3195:         LD      b,8
DD04:   SLA     (d);                    //                                 //3196: DD04:   SLA     d
        RLA;                            //                                 //3197:         RLA
        SUB     (e);                    //                                 //3198:         SUB     e
        JP      (M,rel027);             //                                 //3199:         JP      M,rel027
        INC     (d);                    //                                 //3200:         INC     d
        JRu     (rel024);               //                                 //3201:         JR      rel024
rel027: ADD     (a,e);                  //                                 //3202: rel027: ADD     a,e
rel024: DJNZ    (DD04);                 //                                 //3203: rel024: DJNZ    DD04
        POP     (bc);                   //                                 //3204:         POP     bc
        RETu;                           //                                 //3205:         RET
}                                                                          //3206: 
                                                                           //3207: ;***********************************************************
//***********************************************************              //3208: ; POSITIVE INTEGER MULTIPLICATION
// POSITIVE INTEGER MULTIPLICATION
//   inputs D, E
//   output hi=A lo=D                                                      //3209: ;   inputs D, E
//***********************************************************              //3210: ;   output hi=A lo=D
void MLTPLY() {                                                            //3211: ;***********************************************************
        PUSH    (bc);                   //                                 //3212: MLTPLY: PUSH    bc
        SUB     (a);                    //                                 //3213:         SUB     a
        LD      (b,8);                  //                                 //3214:         LD      b,8
ML04:   BIT     (0,d);                  //                                 //3215: ML04:   BIT     0,d
        JR      (Z,rel025);             //                                 //3216:         JR      Z,rel025
        ADD     (a,e);                  //                                 //3217:         ADD     a,e
rel025: SRA     (a);                    //                                 //3218: rel025: SRA     a
        RR      (d);                    //                                 //3219:         RR      d
        DJNZ    (ML04);                 //                                 //3220:         DJNZ    ML04
        POP     (bc);                   //                                 //3221:         POP     bc
        RETu;                           //                                 //3222:         RET
}                                                                          //3223: 
                                                                           //3224: ;***********************************************************
//                                                                         //3225: ; SQUARE BLINKER
//  Omit some more Z80 user interface stuff, function                      //3226: ;***********************************************************
//  BLNKER                                                                 //3227: ;
//                                                                         //3228: ; FUNCTION:   --  To blink the graphics board square to signal
                                                                           //3229: ;                 a piece's intention to move, or to high-
//***********************************************************              // (lines 3230-3278 omitted) 3279: 
// EXECUTE MOVE SUBROUTINE                                                 //3280: ;***********************************************************
//***********************************************************              //3281: ; EXECUTE MOVE SUBROUTINE
// FUNCTION:   --  This routine is the control routine for                 //3282: ;***********************************************************
//                 MAKEMV. It checks for double moves and                  //3283: ; FUNCTION:   --  This routine is the control routine for
//                 sees that they are properly handled. It                 //3284: ;                 MAKEMV. It checks for double moves and
//                 sets flags in the B register for double                 //3285: ;                 sees that they are properly handled. It
//                 moves:                                                  //3286: ;                 sets flags in the B register for double
//                       En Passant -- Bit 0                               //3287: ;                 moves:
//                       O-O        -- Bit 1                               //3288: ;                       En Passant -- Bit 0
//                       O-O-O      -- Bit 2                               //3289: ;                       O-O        -- Bit 1
//                                                                         //3290: ;                       O-O-O      -- Bit 2
// CALLED BY:   -- PLYRMV                                                  //3291: ;
//                 CPTRMV                                                  //3292: ; CALLED BY:   -- PLYRMV
//                                                                         //3293: ;                 CPTRMV
// CALLS:       -- MAKEMV                                                  //3294: ;
//                                                                         //3295: ; CALLS:       -- MAKEMV
// ARGUMENTS:   -- Flags set in the B register as described                //3296: ;
//                 above.                                                  //3297: ; ARGUMENTS:   -- Flags set in the B register as described
//***********************************************************              //3298: ;                 above.
void EXECMV() {                                                            //3299: ;***********************************************************
        PUSH    (ix);                   //  Save registers                 //3300: EXECMV: PUSH    ix              ; Save registers
        PUSHf   (af);                   //                                 //3301:         PUSH    af
        LD      (ix,val16(MLPTRJ));     //  Index into move list           //3302:         LD      ix,(MLPTRJ)     ; Index into move list
        LD      (c,ptr(ix+MLFRP));      //  Move list "from" position      //3303:         LD      c,(ix+MLFRP)    ; Move list "from" position
        LD      (e,ptr(ix+MLTOP));      //  Move list "to" position        //3304:         LD      e,(ix+MLTOP)    ; Move list "to" position
        CALLu   (MAKEMV);               //  Produce move                   //3305:         CALL    MAKEMV          ; Produce move
        LD      (d,ptr(ix+MLFLG));      //  Move list flags                //3306:         LD      d,(ix+MLFLG)    ; Move list flags
        LD      (b,0);                  //                                 //3307:         LD      b,0
        BIT     (6,d);                  //  Double move ?                  //3308:         BIT     6,d             ; Double move ?
        JR      (Z,EX14);               //  No - jump                      //3309:         JR      Z,EX14          ; No - jump
        LD      (de,6);                 //  Move list entry width          //3310:         LD      de,6            ; Move list entry width
        ADD     (ix,de);                //  Increment MLPTRJ               //3311:         ADD     ix,de           ; Increment MLPTRJ
        LD      (c,ptr(ix+MLFRP));      //  Second "from" position         //3312:         LD      c,(ix+MLFRP)    ; Second "from" position
        LD      (e,ptr(ix+MLTOP));      //  Second "to" position           //3313:         LD      e,(ix+MLTOP)    ; Second "to" position
        LD      (a,e);                  //  Get "to" position              //3314:         LD      a,e             ; Get "to" position
        CP      (c);                    //  Same as "from" position ?      //3315:         CP      c               ; Same as "from" position ?
        JR      (NZ,EX04);              //  No - jump                      //3316:         JR      NZ,EX04         ; No - jump
        INC     (b);                    //  Set en passant flag            //3317:         INC     b               ; Set en passant flag
        JRu     (EX10);                 //  Jump                           //3318:         JR      EX10            ; Jump
EX04:   CP      (0x1A);                 //  White O-O ?                    //3319: EX04:   CP      1AH             ; White O-O ?
        JR      (NZ,EX08);              //  No - jump                      //3320:         JR      NZ,EX08         ; No - jump
        SET     (1,b);                  //  Set O-O flag                   //3321:         SET     1,b             ; Set O-O flag
        JRu     (EX10);                 //  Jump                           //3322:         JR      EX10            ; Jump
EX08:   CP      (0x60);                 //  Black 0-0 ?                    //3323: EX08:   CP      60H             ; Black 0-0 ?
        JR      (NZ,EX0C);              //  No - jump                      //3324:         JR      NZ,EX0C         ; No - jump
        SET     (1,b);                  //  Set 0-0 flag                   //3325:         SET     1,b             ; Set 0-0 flag
        JRu     (EX10);                 //  Jump                           //3326:         JR      EX10            ; Jump
EX0C:   SET     (2,b);                  //  Set 0-0-0 flag                 //3327: EX0C:   SET     2,b             ; Set 0-0-0 flag
EX10:   CALLu   (MAKEMV);               //  Make 2nd move on board         //3328: EX10:   CALL    MAKEMV          ; Make 2nd move on board
EX14:   POPf    (af);                   //  Restore registers              //3329: EX14:   POP     af              ; Restore registers
        POP     (ix);                   //                                 //3330:         POP     ix
        RETu;                           //  Return                         //3331:         RET                     ; Return
}                                                                          //3332: 
                                                                           //3333: ;***********************************************************
//                                                                         //3334: ; MAKE MOVE SUBROUTINE
