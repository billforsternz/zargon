//***********************************************************       //;***********************************************************
//                                                                  //;
//               SARGON                                             //;               SARGON
//                                                                  //;
// Sargon is a computer chess playing program designed              //;       Sargon is a computer chess playing program designed
// and coded by Dan and Kathe Spracklen.  Copyright 1978. All       //
// rights reserved.  No part of this publication may be             //
// reproduced without the prior written permission.                 //
//***********************************************************       //
                                                                    //; and coded by Dan and Kathe Spracklen.  Copyright 1978. All
#include <stdio.h>                                                  //; rights reserved.  No part of this publication may be
#include <stdint.h>                                                 //; reproduced without the prior written permission.
#include <stddef.h>                                                 //;***********************************************************
#include "z80-macros.h"                                             //
                                                                    //
//***********************************************************       //;***********************************************************
// EQUATES                                                          //; EQUATES
//***********************************************************       //;***********************************************************
//                                                                  //;
#define PAWN    1                                                   //PAWN    EQU     1
#define KNIGHT  2                                                   //KNIGHT  EQU     2
#define BISHOP  3                                                   //BISHOP  EQU     3
#define ROOK    4                                                   //ROOK    EQU     4
#define QUEEN   5                                                   //QUEEN   EQU     5
#define KING    6                                                   //KING    EQU     6
#define WHITE   0                                                   //WHITE   EQU     0
#define BLACK   0x80                                                //BLACK   EQU     80H
#define BPAWN   BLACK+PAWN                                          //BPAWN   EQU     BLACK+PAWN
                                                                    //
//                                                                  //;***********************************************************
// 1) TABLES                                                        //
//                                                                  //
//***********************************************************       //
// TABLES SECTION                                                   //
//***********************************************************       //
//                                                                  //
//There are multiple tables used for fast table look ups            //
//that are declared relative to TBASE. In each case there           //
//is a table (say DIRECT) and one or more variables that            //
//index into the table (say INDX2). The table is declared           //
//as a relative offset from the TBASE like this;                    //
//                                                                  //
//DIRECT = .-TBASE  ;In this . is the current location              //
//                  ;($ rather than . is used in most assemblers)   //
//                                                                  //
//The index variable is declared as;                                //
//INDX2    .WORD TBASE                                              //
//                                                                  //
//TBASE itself is page aligned, for example TBASE = 100h            //
//Although 2 bytes are allocated for INDX2 the most significant     //
//never changes (so in our example it's 01h). If we want            //
//to index 5 bytes into DIRECT we set the low byte of INDX2         //
//to 5 (now INDX2 = 105h) and load IDX2 into an index               //
//register. The following sequence loads register C with            //
//the 5th byte of the DIRECT table (Z80 mnemonics)                  //
//        LD      A,5                                               //
//        LD      [INDX2],A                                         //
//        LD      IY,[INDX2]                                        //
//        LD      C,[IY+DIRECT]                                     //
//                                                                  //
//It's a bit like the little known C trick where array[5]           //
//can also be written as 5[array].                                  //
//                                                                  //
//The Z80 indexed addressing mode uses a signed 8 bit               //
//displacement offset (here DIRECT) in the range -128               //
//to 127. Sargon needs most of this range, which explains           //
//why DIRECT is allocated 80h bytes after start and 80h             //
//bytes *before* TBASE, this arrangement sets the DIRECT            //
//displacement to be -80h bytes (-128 bytes). After the 24          //
//byte DIRECT table comes the DPOINT table. So the DPOINT           //
//displacement is -128 + 24 = -104. The final tables have           //
//positive displacements.                                           //
//                                                                  //
//The negative displacements are not necessary in X86 where         //
//the equivalent mov reg,[di+offset] indexed addressing             //
//is not limited to 8 bit offsets, so in the X86 port we            //
//put the first table DIRECT at the same address as TBASE,          //
//a more natural arrangement I am sure you'll agree.                //
//                                                                  //
//In general it seems Sargon doesn't want memory allocated          //
//in the first page of memory, so we start TBASE at 100h not        //
//at 0h. One reason is that Sargon extensively uses a trick         //
//to test for a NULL pointer; it tests whether the hi byte of       //
//a pointer == 0 considers this as a equivalent to testing          //
//whether the whole pointer == 0 (works as long as pointers         //
//never point to page 0).                                           //
//                                                                  //
//Also there is an apparent bug in Sargon, such that MLPTRJ         //
//is left at 0 for the root node and the MLVAL for that root        //
//node is therefore written to memory at offset 5 from 0 (so        //
//in page 0). It's a bit wasteful to waste a whole 256 byte         //
//page for this, but it is compatible with the goal of making       //
//as few changes as possible to the inner heart of Sargon.          //
//In the X86 port we lock the uninitialised MLPTRJ bug down         //
//so MLPTRJ is always set to zero and rendering the bug             //
//harmless (search for MLPTRJ to find the relevant code).           //
//                                                                  //
                                                                    //
#define TBASE 0x100     // The following tables begin at this       //TBASE   EQU     START+100H
                        //  low but non-zero page boundary in       //;There are multiple tables used for fast table look ups
                        //  in our 64K emulated memory              //;that are declared relative to TBASE. In each case there
struct emulated_memory {                                            //;is a table (say DIRECT) and one or more variables that
                                                                    //;index into the table (say INDX2). The table is declared
uint8_t padding[TBASE];                                             //;as a relative offset from the TBASE like this;
//**********************************************************        //;
// DIRECT  --  Direction Table.  Used to determine the dir-         //; DIRECT  --  Direction Table.  Used to determine the dir-
//             ection of movement of each piece.                    //;             ection of movement of each piece.
//***********************************************************       //;***********************************************************
#define DIRECT (addr(direct)-TBASE)                                 //DIRECT  EQU     $-TBASE
int8_t direct[24] = {                                               //        DB      +09,+11,-11,-09
    +9,+11,-11,-9,                                                  //        DB      +10,-10,+01,-01
    +10,-10,+1,-1,                                                  //        DB      -21,-12,+08,+19
    -21,-12,+8,+19,                                                 //        DB      +21,+12,-08,-19
    +21,+12,-8,-19,                                                 //        DB      +10,+10,+11,+09
    +10,+10,+11,+9,                                                 //        DB      -10,-10,-11,-09
    -10,-10,-11,-9                                                  //;***********************************************************
};                                                                  //; DPOINT  --  Direction Table Pointer. Used to determine
//***********************************************************       //;             where to begin in the direction table for any
// DPOINT  --  Direction Table Pointer. Used to determine           //
//             where to begin in the direction table for any        //
//             given piece.                                         //;             given piece.
//***********************************************************       //;***********************************************************
#define DPOINT (addr(dpoint)-TBASE)                                 //DPOINT  EQU     $-TBASE
uint8_t dpoint[7] = {                                               //        DB      20,16,8,0,4,0,0
    20,16,8,0,4,0,0                                                 //
};                                                                  //;***********************************************************
                                                                    //; DCOUNT  --  Direction Table Counter. Used to determine
//***********************************************************       //;             the number of directions of movement for any
// DCOUNT  --  Direction Table Counter. Used to determine           //
//             the number of directions of movement for any         //
//             given piece.                                         //;             given piece.
//***********************************************************       //;***********************************************************
#define DCOUNT (addr(dcount)-TBASE)                                 //DCOUNT  EQU     $-TBASE
uint8_t dcount[7] = {                                               //        DB      4,4,8,4,4,8,8
    4,4,8,4,4,8,8                                                   //
};                                                                  //;***********************************************************
                                                                    //; PVALUE  --  Point Value. Gives the point value of each
                                                                    //;             piece, or the worth of each piece.
//***********************************************************       //;***********************************************************
// PVALUE  --  Point Value. Gives the point value of each           //
//             piece, or the worth of each piece.                   //
//***********************************************************       //
#define PVALUE (addr(pvalue)-TBASE-1)  //TODO what's this minus 1 about?//PVALUE  EQU     $-TBASE-1
uint8_t pvalue[6] = {                                               //        DB      1,3,3,5,9,10
    1,3,3,5,9,10                                                    //
};                                                                  //;***********************************************************
                                                                    //; PIECES  --  The initial arrangement of the first rank of
                                                                    //;             pieces on the board. Use to set up the board
//***********************************************************       //;             for the start of the game.
// PIECES  --  The initial arrangement of the first rank of         //
//             pieces on the board. Use to set up the board         //
//             for the start of the game.                           //
//***********************************************************       //;***********************************************************
#define PIECES  (addr(pieces)-TBASE)                                //PIECES  EQU     $-TBASE
uint8_t pieces[8] = {                                               //        DB      4,2,3,5,6,3,2,4
    4,2,3,5,6,3,2,4                                                 //
};                                                                  //;***********************************************************
                                                                    //; BOARD   --  Board Array.  Used to hold the current position
//***********************************************************       //;             of the board during play. The board itself
// BOARD   --  Board Array.  Used to hold the current position      //
//             of the board during play. The board itself           //
//             looks like:                                          //;             looks like:
//             FFFFFFFFFFFFFFFFFFFF                                 //;             FFFFFFFFFFFFFFFFFFFF
//             FFFFFFFFFFFFFFFFFFFF                                 //;             FFFFFFFFFFFFFFFFFFFF
//             FF0402030506030204FF                                 //;             FF0402030506030204FF
//             FF0101010101010101FF                                 //;             FF0101010101010101FF
//             FF0000000000000000FF                                 //;             FF0000000000000000FF
//             FF0000000000000000FF                                 //;             FF0000000000000000FF
//             FF0000000000000060FF                                 //;             FF0000000000000060FF
//             FF0000000000000000FF                                 //;             FF0000000000000000FF
//             FF8181818181818181FF                                 //;             FF8181818181818181FF
//             FF8482838586838284FF                                 //;             FF8482838586838284FF
//             FFFFFFFFFFFFFFFFFFFF                                 //;             FFFFFFFFFFFFFFFFFFFF
//             FFFFFFFFFFFFFFFFFFFF                                 //;             FFFFFFFFFFFFFFFFFFFF
//             The values of FF form the border of the              //;             The values of FF form the border of the
//             board, and are used to indicate when a piece         //;             board, and are used to indicate when a piece
//             moves off the board. The individual bits of          //;             moves off the board. The individual bits of
//             the other bytes in the board array are as            //;             the other bytes in the board array are as
//             follows:                                             //;             follows:
//             Bit 7 -- val(COLOR) of the piece                     //;             Bit 7 -- Color of the piece
//                     1 -- Black                                   //;                     1 -- Black
//                     0 -- White                                   //;                     0 -- White
//             Bit 6 -- Not used                                    //;             Bit 6 -- Not used
//             Bit 5 -- Not used                                    //;             Bit 5 -- Not used
//             Bit 4 --Castle flag for Kings only                   //;             Bit 4 --Castle flag for Kings only
//             Bit 3 -- Piece has moved flag                        //;             Bit 3 -- Piece has moved flag
//             Bits 2-0 Piece type                                  //;             Bits 2-0 Piece type
//                     1 -- Pawn                                    //;                     1 -- Pawn
//                     2 -- Knight                                  //;                     2 -- Knight
//                     3 -- Bishop                                  //;                     3 -- Bishop
//                     4 -- Rook                                    //;                     4 -- Rook
//                     5 -- Queen                                   //;                     5 -- Queen
//                     6 -- King                                    //;                     6 -- King
//                     7 -- Not used                                //;                     7 -- Not used
//                     0 -- Empty Square                            //;                     0 -- Empty Square
//***********************************************************       //;***********************************************************
#define BOARD addr(BOARDA)                                          //BOARD   EQU     $-TBASE
uint8_t BOARDA[120];                                                //BOARDA  DS      120
                                                                    //
                                                                    //;***********************************************************
//***********************************************************       //; ATKLIST -- Attack List. A two part array, the first
// ATKLIST -- Attack List. A two part array, the first              //
//            half for white and the second half for black.         //;            half for white and the second half for black.
//            It is used to hold the attackers of any given         //;            It is used to hold the attackers of any given
//            square in the order of their value.                   //;            square in the order of their value.
//                                                                  //;
// WACT   --  White Attack Count. This is the first                 //; WACT   --  White Attack Count. This is the first
//            byte of the array and tells how many pieces are       //;            byte of the array and tells how many pieces are
//            in the white portion of the attack list.              //;            in the white portion of the attack list.
//                                                                  //;
// BACT   --  Black Attack Count. This is the eighth byte of        //; BACT   --  Black Attack Count. This is the eighth byte of
//            the array and does the same for black.                //;            the array and does the same for black.
//***********************************************************       //;***********************************************************
#define WACT addr(ATKLST)                                           //WACT    EQU     ATKLST
#define BACT (addr(ATKLST)+7)                                       //BACT    EQU     ATKLST+7
union                                                               //ATKLST  DW      0,0,0,0,0,0,0
{                                                                   //
    uint16_t    ATKLST[7];                                          //;***********************************************************
        uint8_t     wact[7];                                        //; PLIST   --  Pinned Piece Array. This is a two part array.
/*    struct wact_bact                                              //;             PLISTA contains the pinned piece position.
    {                                                               //;             PLISTD contains the direction from the pinned
        uint8_t     wact[7];                                        //;             piece to the attacker.
        uint8_t     bact[7];                                        //;***********************************************************
    }; */                                                           //
};                                                                  //
                                                                    //
//***********************************************************       //
// PLIST   --  Pinned Piece Array. This is a two part array.        //
//             PLISTA contains the pinned piece position.           //
//             PLISTD contains the direction from the pinned        //
//             piece to the attacker.                               //
//***********************************************************       //
#define PLIST (addr(PLISTA)-TBASE-1)    ///TODO -1 why?             //PLIST   EQU     $-TBASE-1
#define PLISTD (PLIST+10)                                           //PLISTD  EQU     PLIST+10
uint8_t     PLISTA[10];     // pinned pieces                        //PLISTA  DW      0,0,0,0,0,0,0,0,0,0
uint8_t     plistd[10];     // corresponding directions             //
                                                                    //;***********************************************************
//***********************************************************       //; POSK    --  Position of Kings. A two byte area, the first
// POSK    --  Position of Kings. A two byte area, the first        //
//             byte of which hold the position of the white         //;             byte of which hold the position of the white
//             king and the second holding the position of          //;             king and the second holding the position of
//             the black king.                                      //;             the black king.
//                                                                  //;
// POSQ    --  Position of Queens. Like POSK,but for queens.        //; POSQ    --  Position of Queens. Like POSK,but for queens.
//***********************************************************       //;***********************************************************
uint8_t     POSK[2] = {                                             //POSK    DB      24,95
    24,95                                                           //POSQ    DB      14,94
};                                                                  //        DB      -1
uint8_t     POSQ[2] = {                                             //
    14,94                                                           //;***********************************************************
};                                                                  //; SCORE   --  Score Array. Used during Alpha-Beta pruning to
int8_t padding2 = -1;                                               //;             hold the scores at each ply. It includes two
                                                                    //;             "dummy" entries for ply -1 and ply 0.
//***********************************************************       //;***********************************************************
// SCORE   --  Score Array. Used during Alpha-Beta pruning to       //
//             hold the scores at each ply. It includes two         //
//             "dummy" entries for ply -1 and ply 0.                //
//***********************************************************       //
uint16_t    SCORE[6] = {                                            //SCORE   DW      0,0,0,0,0,0     ;Z80 max 6 ply
    0,0,0,0,0,0                         // Z80 max 6 ply            //
};                                                                  //;***********************************************************
                                                                    //; PLYIX   --  Ply Table. Contains pairs of pointers, a pair
                                                                    //;             for each ply. The first pointer points to the
//***********************************************************       //;             top of the list of possible moves at that ply.
// PLYIX   --  Ply Table. Contains pairs of pointers, a pair        //
//             for each ply. The first pointer points to the        //
//             top of the list of possible moves at that ply.       //
//             The second pointer points to which move in the       //;             The second pointer points to which move in the
//             list is the one currently being considered.          //;             list is the one currently being considered.
//***********************************************************       //;***********************************************************
uint16_t    PLYIX[20] = {                                           //PLYIX   DW      0,0,0,0,0,0,0,0,0,0
    0,0,0,0,0,0,0,0,0,0,                                            //        DW      0,0,0,0,0,0,0,0,0,0
    0,0,0,0,0,0,0,0,0,0                                             //
};                                                                  //;***********************************************************
                                                                    //; STACK   --  Contains the stack for the program.
//                                                                  //;***********************************************************
// 2) PTRS to TABLES                                                //
//                                                                  //
                                                                    //
//***********************************************************       //
// 2) TABLE INDICES SECTION                                         //
//                                                                  //
// M1-M4   --  Working indices used to index into                   //
//             the board array.                                     //
//                                                                  //
// T1-T3   --  Working indices used to index into Direction         //
//             Count, Direction Value, and Piece Value tables.      //
//                                                                  //
// INDX1   --  General working indices. Used for various            //
// INDX2       purposes.                                            //
//                                                                  //
// NPINS   --  Number of Pins. Count and pointer into the           //
//             pinned piece list.                                   //
//                                                                  //
// MLPTRI  --  Pointer into the ply table which tells               //
//             which pair of pointers are in current use.           //
//                                                                  //
// MLPTRJ  --  Pointer into the move list to the move that is       //
//             currently being processed.                           //
//                                                                  //
// SCRIX   --  Score Index. Pointer to the score table for          //
//             the ply being examined.                              //
//                                                                  //
// BESTM   --  Pointer into the move list for the move that         //
//             is currently considered the best by the              //
//             Alpha-Beta pruning process.                          //
//                                                                  //
// MLLST   --  Pointer to the previous move placed in the move      //
//             list. Used during generation of the move list.       //
//                                                                  //
// MLNXT   --  Pointer to the next available space in the move      //
//             list.                                                //
//                                                                  //
//***********************************************************       //
uint16_t M1      =      TBASE;                                      //M1      DW      TBASE
uint16_t M2      =      TBASE;                                      //M2      DW      TBASE
uint16_t M3      =      TBASE;                                      //M3      DW      TBASE
uint16_t M4      =      TBASE;                                      //M4      DW      TBASE
uint16_t T1      =      TBASE;                                      //T1      DW      TBASE
uint16_t T2      =      TBASE;                                      //T2      DW      TBASE
uint16_t T3      =      TBASE;                                      //T3      DW      TBASE
uint16_t INDX1   =      TBASE;                                      //INDX1   DW      TBASE
uint16_t INDX2   =      TBASE;                                      //INDX2   DW      TBASE
uint16_t NPINS   =      TBASE;                                      //NPINS   DW      TBASE
uint16_t MLPTRI  =      addr(PLYIX);                                //MLPTRI  DW      PLYIX
uint16_t MLPTRJ  =      0;                                          //MLPTRJ  DW      0
uint16_t SCRIX   =      0;                                          //SCRIX   DW      0
uint16_t BESTM   =      0;                                          //BESTM   DW      0
uint16_t MLLST   =      0;                                          //MLLST   DW      0
uint16_t MLNXT   =      addr(MLIST);                                //MLNXT   DW      MLIST
                                                                    //
//                                                                  //;***********************************************************
// 3) MISC VARIABLES                                                //
//                                                                  //
                                                                    //
//***********************************************************       //
// VARIABLES SECTION                                                //; VARIABLES SECTION
//                                                                  //;
// KOLOR   --  Indicates computer's val(COLOR). White is 0, and     //; KOLOR   --  Indicates computer's color. White is 0, and
//             Black is 80H.                                        //;             Black is 80H.
//                                                                  //;
// COLOR   --  Indicates val(COLOR) of the side with the move.      //; COLOR   --  Indicates color of the side with the move.
//                                                                  //;
// P1-P3   --  Working area to hold the contents of the board       //; P1-P3   --  Working area to hold the contents of the board
//             array for a given square.                            //;             array for a given square.
//                                                                  //;
// PMATE   --  The move number at which a checkmate is              //; PMATE   --  The move number at which a checkmate is
//             discovered during look ahead.                        //;             discovered during look ahead.
//                                                                  //;
// MOVENO  --  Current move number.                                 //; MOVENO  --  Current move number.
//                                                                  //;
// PLYMAX  --  Maximum depth of search using Alpha-Beta             //; PLYMAX  --  Maximum depth of search using Alpha-Beta
//             pruning.                                             //;             pruning.
//                                                                  //;
// NPLY    --  Current ply number during Alpha-Beta                 //; NPLY    --  Current ply number during Alpha-Beta
//             pruning.                                             //;             pruning.
//                                                                  //;
// CKFLG   --  A non-zero value indicates the king is in check.     //; CKFLG   --  A non-zero value indicates the king is in check.
//                                                                  //;
// MATEF   --  A zero value indicates no legal moves.               //; MATEF   --  A zero value indicates no legal moves.
//                                                                  //;
// VALM    --  The score of the current move being examined.        //; VALM    --  The score of the current move being examined.
//                                                                  //;
// BRDC    --  A measure of mobility equal to the total number      //; BRDC    --  A measure of mobility equal to the total number
//             of squares white can move to minus the number        //;             of squares white can move to minus the number
//             black can move to.                                   //;             black can move to.
//                                                                  //;
// PTSL    --  The maximum number of points which could be lost     //; PTSL    --  The maximum number of points which could be lost
//             through an exchange by the player not on the         //;             through an exchange by the player not on the
//             move.                                                //;             move.
//                                                                  //;
// PTSW1   --  The maximum number of points which could be won      //; PTSW1   --  The maximum number of points which could be won
//             through an exchange by the player not on the         //;             through an exchange by the player not on the
//             move.                                                //;             move.
//                                                                  //;
// PTSW2   --  The second highest number of points which could      //; PTSW2   --  The second highest number of points which could
//             be won through a different exchange by the player    //;             be won through a different exchange by the player
//             not on the move.                                     //;             not on the move.
//                                                                  //;
// MTRL    --  A measure of the difference in material              //; MTRL    --  A measure of the difference in material
//             currently on the board. It is the total value of     //;             currently on the board. It is the total value of
//             the white pieces minus the total value of the        //;             the white pieces minus the total value of the
//             black pieces.                                        //;             black pieces.
//                                                                  //;
// BC0     --  The value of board control(val(BRDC)) at ply 0.      //; BC0     --  The value of board control(BRDC) at ply 0.
//                                                                  //;
// MV0     --  The value of material(val(MTRL)) at ply 0.           //; MV0     --  The value of material(MTRL) at ply 0.
//                                                                  //;
// PTSCK   --  A non-zero value indicates that the piece has        //; PTSCK   --  A non-zero value indicates that the piece has
//             just moved itself into a losing exchange of          //;             just moved itself into a losing exchange of
//             material.                                            //;             material.
//                                                                  //;
// BMOVES  --  Our very tiny book of openings. Determines           //; BMOVES  --  Our very tiny book of openings. Determines
//             the first move for the computer.                     //;             the first move for the computer.
//                                                                  //;
//***********************************************************       //;***********************************************************
uint8_t KOLOR   =      0;               //                          //KOLOR   DB      0
uint8_t COLOR   =      0;               //                          //COLOR   DB      0
uint8_t P1      =      0;               //                          //P1      DB      0
uint8_t P2      =      0;               //                          //P2      DB      0
uint8_t P3      =      0;               //                          //P3      DB      0
uint8_t PMATE   =      0;               //                          //PMATE   DB      0
uint8_t MOVENO  =      0;               //                          //MOVENO  DB      0
uint8_t PLYMAX  =      2;               //                          //PLYMAX  DB      2
uint8_t NPLY    =      0;               //                          //NPLY    DB      0
uint8_t CKFLG   =      0;               //                          //CKFLG   DB      0
uint8_t MATEF   =      0;               //                          //MATEF   DB      0
uint8_t VALM    =      0;               //                          //VALM    DB      0
uint8_t BRDC    =      0;               //                          //BRDC    DB      0
uint8_t PTSL    =      0;               //                          //PTSL    DB      0
uint8_t PTSW1   =      0;               //                          //PTSW1   DB      0
uint8_t PTSW2   =      0;               //                          //PTSW2   DB      0
uint8_t MTRL    =      0;               //                          //MTRL    DB      0
uint8_t BC0     =      0;               //                          //BC0     DB      0
uint8_t MV0     =      0;               //                          //MV0     DB      0
uint8_t PTSCK   =      0;               //                          //PTSCK   DB      0
uint8_t BMOVES[12] = {                                              //BMOVES  DB      35,55,10H
    35,55,0x10,                                                     //        DB      34,54,10H
    34,54,0x10,                                                     //        DB      85,65,10H
    85,65,0x10,                                                     //        DB      84,64,10H
    84,64,0x10                                                      //
};                                                                  //;***********************************************************
                                                                    //; MOVE LIST SECTION
char MVEMSG[5] = {'a','1','-','a','1'};                             //;
char O_O[3]    = { '0', '-', '0' };                                 //; MLIST   --  A 2048 byte storage area for generated moves.
char O_O_O[5]  = { '0', '-', '0', '-', '0' };                       //;             This area must be large enough to hold all
uint8_t LINECT = 0;                                                 //;             the moves for a single leg of the move tree.
//                                                                  //;
// 4) MOVE ARRAY                                                    //
//                                                                  //
                                                                    //
//***********************************************************       //
// MOVE LIST SECTION                                                //
//                                                                  //
// MLIST   --  A 2048 byte storage area for generated moves.        //
//             This area must be large enough to hold all           //
//             the moves for a single leg of the move tree.         //
//                                                                  //
// MLEND   --  The address of the last available location           //
//             in the move list.                                    //
//                                                                  //
// MLPTR   --  The Move List is a linked list of individual         //
//             moves each of which is 6 bytes in length. The        //
//             move list pointer(MLPTR) is the link field           //
//             within a move.                                       //
//                                                                  //
// MLFRP   --  The field in the move entry which gives the          //
//             board position from which the piece is moving.       //
//                                                                  //
// MLTOP   --  The field in the move entry which gives the          //
//             board position to which the piece is moving.         //
//                                                                  //
// MLFLG   --  A field in the move entry which contains flag        //
//             information. The meaning of each bit is as           //
//             follows:                                             //
//             Bit 7  --  The val(COLOR) of any captured piece      //
//                        0 -- White                                //
//                        1 -- Black                                //
//             Bit 6  --  Double move flag (set for castling and    //
//                        en passant pawn captures)                 //
//             Bit 5  --  Pawn Promotion flag; set when pawn        //
//                        promotes.                                 //
//             Bit 4  --  When set, this flag indicates that        //
//                        this is the first move for the            //
//                        piece on the move.                        //
//             Bit 3  --  This flag is set is there is a piece      //
//                        captured, and that piece has moved at     //
//                        least once.                               //
//             Bits 2-0   Describe the captured piece.  A           //
//                        zero value indicates no capture.          //
//                                                                  //
// MLVAL   --  The field in the move entry which contains the       //
//             score assigned to the move.                          //
//                                                                  //
//***********************************************************       //
struct ML {                                                         //MLIST   DS      2048
    uint16_t    MLPTR_;                                             //
    uint8_t     MLFRP_;                                             //
    uint8_t     MLTOP_;                                             //
    uint8_t     MLFLG_;                                             //
    uint8_t     MLVAL_;                                             //
}  MLIST[340];                                                      //
uint8_t MLEND;                                                      //MLEND   EQU     MLIST+2040
};                                                                  //MLPTR   EQU     0
                                                                    //MLFRP   EQU     2
#define MLPTR 0                                                     //MLTOP   EQU     3
#define MLFRP 2                                                     //MLFLG   EQU     4
#define MLTOP 3                                                     //MLVAL   EQU     5
#define MLFLG 4                                                     //
#define MLVAL 5                                                     //;***********************************************************
                                                                    //
// Up to 64K of emulated memory                                     //;**********************************************************
emulated_memory mem;                                                //
                                                                    //
// Emulate Z80 machine                                              //
Z80_REGISTERS;                                                      //
                                                                    //
//***********************************************************       //
                                                                    //
//**********************************************************        //
// PROGRAM CODE SECTION                                             //
//**********************************************************        //
                                                                    //
//                                                                  //
// Miscellaneous stubs                                              //
//                                                                  //
void FCDMAT()  {}                                                   //
void TBCPMV()  {}                                                   //
void MAKEMV()  {}                                                   //
void PRTBLK( const char *name, int len ) {}                         //
void CARRET()  {}                                                   //
                                                                    //
//                                                                  //
// Forward references                                               //
//                                                                  //
uint8_t rand();                                                     //
void INITBD();                                                      //
void PATH();                                                        //
void MPIECE();                                                      //
void ENPSNT();                                                      //
void ADJPTR();                                                      //
void CASTLE();                                                      //
void ADMOVE();                                                      //
void GENMOV();                                                      //
void INCHK();                                                       //
void INCHK1();                                                      //
void ATTACK();                                                      //
void ATKSAV();                                                      //
void PNCK();                                                        //
void PINFND();                                                      //
void XCHNG();                                                       //
void NEXTAD();                                                      //
void POINTS();                                                      //
void LIMIT();                                                       //
void MOVE();                                                        //
void UNMOVE();                                                      //
void SORTM();                                                       //
void EVAL();                                                        //
void FNDMOV();                                                      //
void ASCEND();                                                      //
void BOOK();                                                        //
void CPTRMV();                                                      //
void BITASN();                                                      //
void ASNTBI();                                                      //
void VALMOV();                                                      //
void ROYALT();                                                      //
void DIVIDE();                                                      //
void MLTPLY();                                                      //
void EXECMV();                                                      //; PROGRAM CODE SECTION
                                                                    //;**********************************************************
//**********************************************************        //
// BOARD SETUP ROUTINE                                              //;**********************************************************
//***********************************************************       //; BOARD SETUP ROUTINE
// FUNCTION:   To initialize the board array, setting the           //;***********************************************************
//             pieces in their initial positions for the            //; FUNCTION:   To initialize the board array, setting the
//             start of the game.                                   //;             pieces in their initial positions for the
//                                                                  //;             start of the game.
// CALLED BY:  DRIVER                                               //;
//                                                                  //; CALLED BY:  DRIVER
// CALLS:      None                                                 //;
//                                                                  //; CALLS:      None
// ARGUMENTS:  None                                                 //;
//***********************************************************       //; ARGUMENTS:  None
void INITBD() {                                                     //;***********************************************************
        LD      (b,120);                //  Pre-fill board with -1's//INITBD: LD      b,120           ; Pre-fill board with -1's
        LD      (hl,addr(BOARDA));                                  //        LD      hl,BOARDA
back01: LD      (ptr(hl),-1);                                       //back01: LD      (hl),-1
        INC     (hl);                                               //        INC     hl
        DJNZ    (back01);                                           //        DJNZ    back01
        LD      (b,8);                                              //        LD      b,8
        LD      (ix,addr(BOARDA));                                  //        LD      ix,BOARDA
IB2:    LD      (a,ptr(ix-8));          //  Fill non-border squares //IB2:    LD      a,(ix-8)        ; Fill non-border squares
        LD      (ptr(ix+21),a);         //  White pieces            //        LD      (ix+21),a       ; White pieces
        SET     (7,a);                  //  Change to black         //        SET     7,a             ; Change to black
        LD      (ptr(ix+91),a);         //  Black pieces            //        LD      (ix+91),a       ; Black pieces
        LD      (ptr(ix+31),PAWN);      //  White Pawns             //        LD      (ix+31),PAWN    ; White Pawns
        LD      (ptr(ix+81),BPAWN);     //  Black Pawns             //        LD      (ix+81),BPAWN   ; Black Pawns
        LD      (ptr(ix+41),0);         //  Empty squares           //        LD      (ix+41),0       ; Empty squares
        LD      (ptr(ix+51),0);                                     //        LD      (ix+51),0
        LD      (ptr(ix+61),0);                                     //        LD      (ix+61),0
        LD      (ptr(ix+71),0);                                     //        LD      (ix+71),0
        INC     (ix);                                               //        INC     ix
        DJNZ    (IB2);                                              //        DJNZ    IB2
        LD      (ix,addr(POSK));        //  Init King/Queen position list//        LD      ix,POSK         ; Init King/Queen position list
        LD      (ptr(ix+0),25);                                     //        LD      (ix+0),25
        LD      (ptr(ix+1),95);                                     //        LD      (ix+1),95
        LD      (ix,addr(POSQ));                                    //        LD      (ix+2),24
        LD      (ptr(ix+0),24);                                     //        LD      (ix+3),94
        LD      (ptr(ix+1),94);                                     //        RET
        RETu;                                                       //
}                                                                   //;***********************************************************
                                                                    //; PATH ROUTINE
//***********************************************************       //;***********************************************************
// PATH ROUTINE                                                     //
//***********************************************************       //
// FUNCTION:   To generate a single possible move for a given       //
//             piece along its current path of motion including:    //; FUNCTION:   To generate a single possible move for a given
//                                                                  //;             piece along its current path of motion including:
//                Fetching the contents of the board at the new     //
//                position, and setting a flag describing the       //;                Fetching the contents of the board at the new
//                contents:                                         //;                position, and setting a flag describing the
//                          0  --  New position is empty            //;                contents:
//                          1  --  Encountered a piece of the       //;                          0  --  New position is empty
//                                 opposite val(COLOR)              //;                          1  --  Encountered a piece of the
//                          2  --  Encountered a piece of the       //;                                 opposite color
//                                 same val(COLOR)                  //;                          2  --  Encountered a piece of the
//                          3  --  New position is off the          //;                                 same color
//                                 board                            //;                          3  --  New position is off the
//                                                                  //;                                 board
// CALLED BY:  MPIECE                                               //;
//             ATTACK                                               //; CALLED BY:  MPIECE
//             PINFND                                               //;             ATTACK
//                                                                  //;             PINFND
// CALLS:      None                                                 //;
//                                                                  //; CALLS:      None
// ARGUMENTS:  Direction from the direction array giving the        //;
//             constant to be added for the new position.           //; ARGUMENTS:  Direction from the direction array giving the
//***********************************************************       //;             constant to be added for the new position.
void PATH() {                                                       //;***********************************************************
        LD      (hl,addr(M2));          //  Get previous position   //PATH:   LD      hl,M2           ; Get previous position
        LD      (a,ptr(hl));            //                          //        LD      a,(hl)
        ADD     (a,c);                  //  Add direction constant  //        ADD     a,c             ; Add direction constant
        LD      (ptr(hl),a);            //  Save new position       //        LD      (hl),a          ; Save new position
        LD      (ix,val16(M2));         //  Load board index        //        LD      ix,(M2)         ; Load board index
        LD      (a,ptr(ix+BOARD));      //  Get contents of board   //        LD      a,(ix+BOARD)    ; Get contents of board
        CP      (-1);                   //  In border area ?        //        CP      -1              ; In border area ?
        JR      (Z,PA2);                //  Yes - jump              //        JR      Z,PA2           ; Yes - jump
        LD      (val(P2),a);            //  Save piece              //        LD      (P2),a          ; Save piece
        AND     (7);                    //  Clear flags             //        AND     7               ; Clear flags
        LD      (val(T2),a);            //  Save piece type         //        LD      (T2),a          ; Save piece type
        RET     (Z);                    //  Return if empty         //        RET     Z               ; Return if empty
        LD      (a,val(P2));            //  Get piece encountered   //        LD      a,(P2)          ; Get piece encountered
        LD      (hl,addr(P1));          //  Get moving piece address//        LD      hl,P1           ; Get moving piece address
        XOR     (ptr(hl));              //  Compare                 //        XOR     (hl)            ; Compare
        BIT     (7,a);                  //  Do colors match ?       //        BIT     7,a             ; Do colors match ?
        JR      (Z,PA1);                //  Yes - jump              //        JR      Z,PA1           ; Yes - jump
        LD      (a,1);                  //  Set different val(COLOR) flag//        LD      a,1             ; Set different color flag
        RETu;                           //  Return                  //        RET                     ; Return
PA1:    LD      (a,2);                  //  Set same val(COLOR) flag//PA1:    LD      a,2             ; Set same color flag
        RETu;                           //  Return                  //        RET                     ; Return
PA2:    LD      (a,3);                  //  Set off board flag      //PA2:    LD      a,3             ; Set off board flag
        RETu;                           //  Return                  //        RET                     ; Return
}                                                                   //
                                                                    //;***********************************************************
                                                                    //; PIECE MOVER ROUTINE
                                                                    //;***********************************************************
//***********************************************************       //; FUNCTION:   To generate all the possible legal moves for a
// PIECE MOVER ROUTINE                                              //
//***********************************************************       //
// FUNCTION:   To generate all the possible legal moves for a       //
//             given piece.                                         //
//                                                                  //;             given piece.
// CALLED BY:  GENMOV                                               //;
//                                                                  //; CALLED BY:  GENMOV
// CALLS:      PATH                                                 //;
//             ADMOVE                                               //; CALLS:      PATH
//             CASTLE                                               //;             ADMOVE
//             ENPSNT                                               //;             CASTLE
//                                                                  //;             ENPSNT
// ARGUMENTS:  The piece to be moved.                               //;
//***********************************************************       //; ARGUMENTS:  The piece to be moved.
void MPIECE() {                                                     //;***********************************************************
        XOR     (ptr(hl));              //  Piece to move           //MPIECE: XOR     (hl)            ; Piece to move
        AND     (0x87);                 //  Clear flag bit          //        AND     87H             ; Clear flag bit
        CP      (BPAWN);                //  Is it a black Pawn ?    //        CP      BPAWN           ; Is it a black Pawn ?
        JR      (NZ,rel001);            //  No-Skip                 //        JR      NZ,rel001       ; No-Skip
        DEC     (a);                    //  Decrement for black Pawns//        DEC     a               ; Decrement for black Pawns
rel001: AND     (7);                    //  Get piece type          //rel001: AND     7               ; Get piece type
        LD      (val(T1),a);            //  Save piece type         //        LD      (T1),a          ; Save piece type
        LD      (iy,val16(T1));         //  Load index to DCOUNT/DPOINT//        LD      iy,(T1)         ; Load index to DCOUNT/DPOINT
        LD      (b,ptr(iy+DCOUNT));     //  Get direction count     //        LD      b,(iy+DCOUNT)   ; Get direction count
        LD      (a,ptr(iy+DPOINT));     //  Get direction pointer   //        LD      a,(iy+DPOINT)   ; Get direction pointer
        LD      (val(INDX2),a);         //  Save as index to direct //        LD      (INDX2),a       ; Save as index to direct
        LD      (iy,val16(INDX2));      //  Load index              //        LD      iy,(INDX2)      ; Load index
MP5:    LD      (c,ptr(iy+DIRECT));     //  Get move direction      //MP5:    LD      c,(iy+DIRECT)   ; Get move direction
        LD      (a,val(M1));            //  From position           //        LD      a,(M1)          ; From position
        LD      (val(M2),a);            //  Initialize to position  //        LD      (M2),a          ; Initialize to position
MP10:   CALLu   (PATH);                 //  Calculate next position //MP10:   CALL    PATH            ; Calculate next position
        CP      (2);                    //  Ready for new direction ?//        CP      2               ; Ready for new direction ?
        JR      (NC,MP15);              //  Yes - Jump              //        JR      NC,MP15         ; Yes - Jump
        AND     (a);                    //  Test for empty square   //        AND     a               ; Test for empty square
        Z80_EXAF;                       //  Save result             //        EX      af,af'          ; Save result
        LD      (a,val(T1));            //  Get piece moved         //        LD      a,(T1)          ; Get piece moved
        CP      (PAWN+1);               //  Is it a Pawn ?          //        CP      PAWN+1          ; Is it a Pawn ?
        JR      (C,MP20);               //  Yes - Jump              //        JR      C,MP20          ; Yes - Jump
        CALLu    (ADMOVE);              //  Add move to list        //        CALL    ADMOVE          ; Add move to list
        Z80_EXAF;                       //  Empty square ?          //        EX      af,af'          ; Empty square ?
        JR      (NZ,MP15);              //  No - Jump               //        JR      NZ,MP15         ; No - Jump
        LD      (a,val(T1));            //  Piece type              //        LD      a,(T1)          ; Piece type
        CP      (KING);                 //  King ?                  //        CP      KING            ; King ?
        JR      (Z,MP15);               //  Yes - Jump              //        JR      Z,MP15          ; Yes - Jump
        CP      (BISHOP);               //  Bishop, Rook, or Queen ?//        CP      BISHOP          ; Bishop, Rook, or Queen ?
        JR      (NC,MP10);              //  Yes - Jump              //        JR      NC,MP10         ; Yes - Jump
MP15:   INC     (iy);                   //  Increment direction index//MP15:   INC     iy              ; Increment direction index
        DJNZ    (MP5);                  //  Decr. count-jump if non-zerc//        DJNZ    MP5             ; Decr. count-jump if non-zero
        LD      (a,val(T1));            //  Piece type              //        LD      a,(T1)          ; Piece type
        CP      (KING);                 //  King ?                  //        CP      KING            ; King ?
        CALL    (Z,CASTLE);             //  Yes - Try Castling      //        CALL    Z,CASTLE        ; Yes - Try Castling
        RETu;                           //  Return                  //        RET                     ; Return
// ***** PAWN LOGIC *****                                           //; ***** PAWN LOGIC *****
MP20:   LD      (a,b);                  //  Counter for direction   //
        CP      (3);                    //  On diagonal moves ?     //
        JR      (C,MP35);               //  Yes - Jump              //
        JR      (Z,MP30);               //  -or-jump if on 2 square move//
        Z80_EXAF;                       //  Is forward square empty?//
        JR      (NZ,MP15);              //  No - jump               //
        LD      (a,val(M2));            //  Get "to" position       //
        CP      (91);                   //  Promote white Pawn ?    //
        JR      (NC,MP25);              //  Yes - Jump              //
        CP      (29);                   //  Promote black Pawn ?    //
        JR      (NC,MP26);              //  No - Jump               //
MP25:   LD      (hl,addr(P2));          //  Flag address            //
        SET     (5,ptr(hl));            //  Set promote flag        //
MP26:   CALLu   (ADMOVE);               //  Add to move list        //
        INC     (iy);                   //  Adjust to two square move//
        DEC     (b);                    //                          //
        LD      (hl,addr(P1));          //  Check Pawn moved flag   //
        BIT     (3,ptr(hl));            //  Has it moved before ?   //
        JR      (Z,MP10);               //  No - Jump               //
        JPu     (MP15);                 //  Jump                    //
MP30:   Z80_EXAF;                       //  Is forward square empty ?//
        JR      (NZ,MP15);              //  No - Jump               //
MP31:   CALLu   (ADMOVE);               //  Add to move list        //
        JPu     (MP15);                 //  Jump                    //
MP35:   Z80_EXAF;                       //  Is diagonal square empty ?//
        JR      (Z,MP36);               //  Yes - Jump              //
        LD      (a,val(M2));            //  Get "to" position       //
        CP      (91);                   //  Promote white Pawn ?    //
        JR      (NC,MP37);              //  Yes - Jump              //
        CP      (29);                   //  Black Pawn promotion ?  //
        JR      (NC,MP31);              //  No- Jump                //
MP37:   LD      (hl,addr(P2));          //  Get flag address        //
        SET     (5,ptr(hl));            //  Set promote flag        //
        JRu     (MP31);                 //  Jump                    //
MP36:   CALLu   (ENPSNT);               //  Try en passant capture  //
        JPu     (MP15);                 //  Jump                    //
}                                                                   //
                                                                    //
                                                                    //
//***********************************************************       //
// EN PASSANT ROUTINE                                               //;***********************************************************
//***********************************************************       //; EN PASSANT ROUTINE
// FUNCTION:   --  To test for en passant Pawn capture and          //;***********************************************************
//                 to add it to the move list if it is              //; FUNCTION:   --  To test for en passant Pawn capture and
//                 legal.                                           //;                 to add it to the move list if it is
//                                                                  //;                 legal.
// CALLED BY:  --  MPIECE                                           //;
//                                                                  //; CALLED BY:  --  MPIECE
// CALLS:      --  ADMOVE                                           //;
//                 ADJPTR                                           //; CALLS:      --  ADMOVE
//                                                                  //;                 ADJPTR
// ARGUMENTS:  --  None                                             //;
//***********************************************************       //; ARGUMENTS:  --  None
void ENPSNT() {                                                     //;***********************************************************
        LD      (a,val(M1));            //  Set position of Pawn    //ENPSNT: LD      a,(M1)          ; Set position of Pawn
        LD      (hl,addr(P1));          //  Check val(COLOR)        //        LD      hl,P1           ; Check color
        BIT     (7,ptr(hl));            //  Is it white ?           //        BIT     7,(hl)          ; Is it white ?
        JR      (Z,rel002);             //  Yes - skip              //        JR      Z,rel002        ; Yes - skip
        ADD     (a,10);                 //  Add 10 for black        //        ADD     a,10            ; Add 10 for black
rel002: CP      (61);                   //  On en passant capture rank ?//rel002: CP      61              ; On en passant capture rank ?
        RET     (C);                    //  No - return             //        RET     C               ; No - return
        CP      (69);                   //  On en passant capture rank ?//        CP      69              ; On en passant capture rank ?
        RET     (NC);                   //  No - return             //        RET     NC              ; No - return
        LD      (ix,val16(MLPTRJ));     //  Get pointer to previous move//        LD      ix,(MLPTRJ)     ; Get pointer to previous move
        BIT     (4,ptr(ix+MLFLG));      //  First move for that piece ?//        BIT     4,(ix+MLFLG)    ; First move for that piece ?
        RET     (Z);                    //  No - return             //        RET     Z               ; No - return
        LD      (a,ptr(ix+MLTOP));      //  Get "to" position       //        LD      a,(ix+MLTOP)    ; Get "to" position
        LD      (val(M4),a);            //  Store as index to board //        LD      (M4),a          ; Store as index to board
        LD      (ix,val16(M4));         //  Load board index        //        LD      ix,(M4)         ; Load board index
        LD      (a,ptr(ix+BOARD));      //  Get piece moved         //        LD      a,(ix+BOARD)    ; Get piece moved
        LD      (val(P3),a);            //  Save it                 //        LD      (P3),a          ; Save it
        AND     (7);                    //  Get piece type          //        AND     7               ; Get piece type
        CP      (PAWN);                 //  Is it a Pawn ?          //        CP      PAWN            ; Is it a Pawn ?
        RET     (NZ);                   //  No - return             //        RET     NZ              ; No - return
        LD      (a,val(M4));            //  Get "to" position       //        LD      a,(M4)          ; Get "to" position
        LD      (hl,addr(M2));          //  Get present "to" position//        LD      hl,M2           ; Get present "to" position
        SUB     (ptr(hl));              //  Find difference         //        SUB     (hl)            ; Find difference
        JP      (P,rel003);             //  Positive ? Yes - Jump   //        JP      P,rel003        ; Positive ? Yes - Jump
        NEG;                            //  Else take absolute value//        NEG                     ; Else take absolute value
rel003: CP      (10);                   //  Is difference 10 ?      //rel003: CP      10              ; Is difference 10 ?
        RET     (NZ);                   //  No - return             //        RET     NZ              ; No - return
        LD      (hl,addr(P2));          //  Address of flags        //        LD      hl,P2           ; Address of flags
        SET     (6,ptr(hl));            //  Set double move flag    //        SET     6,(hl)          ; Set double move flag
        CALLu   (ADMOVE);               //  Add Pawn move to move list//        CALL    ADMOVE          ; Add Pawn move to move list
        LD      (a,val(M1));            //  Save initial Pawn position//        LD      a,(M1)          ; Save initial Pawn position
        LD      (val(M3),a);            //                          //        LD      (M3),a
        LD      (a,val(M4));            //  Set "from" and "to" positions//        LD      a,(M4)          ; Set "from" and "to" positions
// for dummy move                                                   //                                ; for dummy move
        LD      (val(M1),a);            //                          //
        LD      (val(M2),a);            //                          //
        LD      (a,val(P3));            //  Save captured Pawn      //
        LD      (val(P2),a);            //                          //
        CALLu   (ADMOVE);               //  Add Pawn capture to move list//
        LD      (a,val(M3));            //  Restore "from" position //
        LD      (val(M1),a);            //                          //
}                                                                   //
                                                                    //
                                                                    //
//***********************************************************       //
// ADJUST MOVE LIST POINTER FOR DOUBLE MOVE                         //;***********************************************************
//***********************************************************       //; ADJUST MOVE LIST POINTER FOR DOUBLE MOVE
// FUNCTION:   --  To adjust move list pointer to link around       //;***********************************************************
//                 second move in double move.                      //; FUNCTION:   --  To adjust move list pointer to link around
//                                                                  //;                 second move in double move.
// CALLED BY:  --  ENPSNT                                           //;
//                 CASTLE                                           //; CALLED BY:  --  ENPSNT
//                 (This mini-routine is not really called,         //;                 CASTLE
//                 but is jumped to to save time.)                  //;                 (This mini-routine is not really called,
//                                                                  //;                 but is jumped to to save time.)
// CALLS:      --  None                                             //;
//                                                                  //; CALLS:      --  None
// ARGUMENTS:  --  None                                             //;
//***********************************************************       //; ARGUMENTS:  --  None
void ADJPTR() {                                                     //;***********************************************************
        LD      (hl,val16(MLLST));      //  Get list pointer        //ADJPTR: LD      hl,(MLLST)      ; Get list pointer
        LD      (de,-6);                //  Size of a move entry    //        LD      de,-6           ; Size of a move entry
        ADD16   (hl,de);                //  Back up list pointer    //        ADD     hl,de           ; Back up list pointer
        LD      (val16(MLLST),hl);      //  Save list pointer       //        LD      (MLLST),hl      ; Save list pointer
        LD      (ptr(hl),0);            //  Zero out link, first byte//        LD      (hl),0          ; Zero out link, first byte
        INC     (hl);                   //  Next byte               //        INC     hl              ; Next byte
        LD      (ptr(hl),0);            //  Zero out link, second byte//        LD      (hl),0          ; Zero out link, second byte
        RETu;                           //  Return                  //        RET                     ; Return
}                                                                   //
                                                                    //;***********************************************************
//***********************************************************       //; CASTLE ROUTINE
// CASTLE ROUTINE                                                   //
//***********************************************************       //
// FUNCTION:   --  To determine whether castling is legal           //;***********************************************************
//                 (Queen side, King side, or both) and add it      //; FUNCTION:   --  To determine whether castling is legal
//                 to the move list if it is.                       //;                 (Queen side, King side, or both) and add it
//                                                                  //;                 to the move list if it is.
// CALLED BY:  --  MPIECE                                           //;
//                                                                  //; CALLED BY:  --  MPIECE
// CALLS:      --  ATTACK                                           //;
//                 ADMOVE                                           //; CALLS:      --  ATTACK
//                 ADJPTR                                           //;                 ADMOVE
//                                                                  //;                 ADJPTR
// ARGUMENTS:  --  None                                             //;
//***********************************************************       //; ARGUMENTS:  --  None
void CASTLE() {                                                     //;***********************************************************
        LD      (a,val(P1));            //  Get King                //CASTLE: LD      a,(P1)          ; Get King
        BIT     (3,a);                  //  Has it moved ?          //        BIT     3,a             ; Has it moved ?
        RET     (NZ);                   //  Yes - return            //        RET     NZ              ; Yes - return
        LD      (a,val(CKFLG));         //  Fetch Check Flag        //        LD      a,(CKFLG)       ; Fetch Check Flag
        AND     (a);                    //  Is the King in check ?  //        AND     a               ; Is the King in check ?
        RET     (NZ);                   //  Yes - Return            //        RET     NZ              ; Yes - Return
        LD      (bc,0xFF03);            //  Initialize King-side values//        LD      bc,0FF03H       ; Initialize King-side values
CA5:    LD      (a,val(M1));            //  King position           //CA5:    LD      a,(M1)          ; King position
        ADD     (a,c);                  //  Rook position           //        ADD     a,c             ; Rook position
        LD      (c,a);                  //  Save                    //        LD      c,a             ; Save
        LD      (val(M3),a);            //  Store as board index    //        LD      (M3),a          ; Store as board index
        LD      (ix,val16(M3));         //  Load board index        //        LD      ix,(M3)         ; Load board index
        LD      (a,ptr(ix+BOARD));      //  Get contents of board   //        LD      a,(ix+BOARD)    ; Get contents of board
        AND     (0x7F);                 //  Clear val(COLOR) bit    //        AND     7FH             ; Clear color bit
        CP      (ROOK);                 //  Has Rook ever moved ?   //        CP      ROOK            ; Has Rook ever moved ?
        JR      (NZ,CA20);              //  Yes - Jump              //        JR      NZ,CA20         ; Yes - Jump
        LD      (a,c);                  //  Restore Rook position   //        LD      a,c             ; Restore Rook position
        JRu     (CA15);                 //  Jump                    //        JR      CA15            ; Jump
CA10:   LD      (ix,val16(M3));         //  Load board index        //CA10:   LD      ix,(M3)         ; Load board index
        LD      (a,ptr(ix+BOARD));      //  Get contents of board   //        LD      a,(ix+BOARD)    ; Get contents of board
        AND     (a);                    //  Empty ?                 //        AND     a               ; Empty ?
        JR      (NZ,CA20);              //  No - Jump               //        JR      NZ,CA20         ; No - Jump
        LD      (a,val(M3));            //  Current position        //        LD      a,(M3)          ; Current position
        CP      (22);                   //  White Queen Knight square ?//        CP      22              ; White Queen Knight square ?
        JR      (Z,CA15);               //  Yes - Jump              //        JR      Z,CA15          ; Yes - Jump
        CP      (92);                   //  Black Queen Knight square ?//        CP      92              ; Black Queen Knight square ?
        JR      (Z,CA15);               //  Yes - Jump              //        JR      Z,CA15          ; Yes - Jump
        CALLu   (ATTACK);               //  Look for attack on square//        CALL    ATTACK          ; Look for attack on square
        AND     (a);                    //  Any attackers ?         //        AND     a               ; Any attackers ?
        JR      (NZ,CA20);              //  Yes - Jump              //        JR      NZ,CA20         ; Yes - Jump
        LD      (a,val(M3));            //  Current position        //        LD      a,(M3)          ; Current position
CA15:   ADD     (a,b);                  //  Next position           //CA15:   ADD     a,b             ; Next position
        LD      (val(M3),a);            //  Save as board index     //        LD      (M3),a          ; Save as board index
        LD      (hl,addr(M1));          //  King position           //        LD      hl,M1           ; King position
        CP      (ptr(hl));              //  Reached King ?          //        CP      (hl)            ; Reached King ?
        JR      (NZ,CA10);              //  No - jump               //        JR      NZ,CA10         ; No - jump
        SUB     (b);                    //  Determine King's position//        SUB     b               ; Determine King's position
        SUB     (b);                                                //        SUB     b
        LD      (val(M2),a);            //  Save it                 //        LD      (M2),a          ; Save it
        LD      (hl,addr(P2));          //  Address of flags        //        LD      hl,P2           ; Address of flags
        LD      (ptr(hl),0x40);         //  Set double move flag    //        LD      (hl),40H        ; Set double move flag
        CALLu   (ADMOVE);               //  Put king move in list   //        CALL    ADMOVE          ; Put king move in list
        LD      (hl,addr(M1));          //  Addr of King "from" position//        LD      hl,M1           ; Addr of King "from" position
        LD      (a,ptr(hl));            //  Get King's "from" position//        LD      a,(hl)          ; Get King's "from" position
        LD      ((hl),c);               //  Store Rook "from" position//        LD      (hl),c          ; Store Rook "from" position
        SUB     (b);                    //  Get Rook "to" position  //        SUB     b               ; Get Rook "to" position
        LD      (val(M2),a);            //  Store Rook "to" position//        LD      (M2),a          ; Store Rook "to" position
        XOR     (a);                    //  Zero                    //        XOR     a               ; Zero
        LD      (val(P2),a);            //  Zero move flags         //        LD      (P2),a          ; Zero move flags
        CALLu   (ADMOVE);               //  Put Rook move in list   //        CALL    ADMOVE          ; Put Rook move in list
        CALLu   (ADJPTR);               //  Re-adjust move list pointer//        CALL    ADJPTR          ; Re-adjust move list pointer
        LD      (a,val(M3));            //  Restore King position   //        LD      a,(M3)          ; Restore King position
        LD      (val(M1),a);            //  Store                   //        LD      (M1),a          ; Store
CA20:   LD      (a,b);                  //  Scan Index              //CA20:   LD      a,b             ; Scan Index
        CP      (1);                    //  Done ?                  //        CP      1               ; Done ?
        RET     (Z);                    //  Yes - return            //        RET     Z               ; Yes - return
        LD      (bc,0x01FC);            //  Set Queen-side initial values//        LD      bc,01FCH        ; Set Queen-side initial values
        JPu     (CA5);                  //  Jump                    //        JP      CA5             ; Jump
}                                                                   //
                                                                    //;***********************************************************
//***********************************************************       //; ADMOVE ROUTINE
// ADMOVE ROUTINE                                                   //
//***********************************************************       //
// FUNCTION:   --  To add a move to the move list                   //;***********************************************************
//                                                                  //; FUNCTION:   --  To add a move to the move list
// CALLED BY:  --  MPIECE                                           //;
//                 ENPSNT                                           //; CALLED BY:  --  MPIECE
//                 CASTLE                                           //;                 ENPSNT
//                                                                  //;                 CASTLE
// CALLS:      --  None                                             //;
//                                                                  //; CALLS:      --  None
// ARGUMENT:  --  None                                              //;
//***********************************************************       //; ARGUMENT:  --  None
void ADMOVE() {                                                     //;***********************************************************
        LD      (de,val16(MLNXT));      //  Addr of next loc in move list//ADMOVE: LD      de,(MLNXT)      ; Addr of next loc in move list
        LD      (hl,addr(MLEND));       //  Address of list end     //        LD      hl,MLEND        ; Address of list end
        AND     (a);                    //  Clear carry flag        //        AND     a               ; Clear carry flag
        SBC     (hl,de);                //  Calculate difference    //        SBC     hl,de           ; Calculate difference
        JR      (C,AM10);               //  Jump if out of space    //        JR      C,AM10          ; Jump if out of space
        LD      (hl,val16(MLLST));      //  Addr of prev. list area //        LD      hl,(MLLST)      ; Addr of prev. list area
        LD      (val16(MLLST),de);      //  Save next as previous   //        LD      (MLLST),de      ; Save next as previous
        LD      (ptr(hl),e);            //  Store link address      //        LD      (hl),e          ; Store link address
        INC     (hl);                                               //        INC     hl
        LD      (ptr(hl),d);                                        //        LD      (hl),d
        LD      (hl,addr(P1));          //  Address of moved piece  //        LD      hl,P1           ; Address of moved piece
        BIT     (3,ptr(hl));            //  Has it moved before ?   //        BIT     3,(hl)          ; Has it moved before ?
        JR      (NZ,rel004);            //  Yes - jump              //        JR      NZ,rel004       ; Yes - jump
        LD      (hl,addr(P2));          //  Address of move flags   //        LD      hl,P2           ; Address of move flags
        SET     (4,ptr(hl));            //  Set first move flag     //        SET     4,(hl)          ; Set first move flag
rel004: EX      (de,hl);                //  Address of move area    //rel004: EX      de,hl           ; Address of move area
        LD      (ptr(hl),0);            //  Store zero in link address//        LD      (hl),0          ; Store zero in link address
        INC     (hl);                                               //        INC     hl
        LD      (ptr(hl),0);                                        //        LD      (hl),0
        INC     (hl);                                               //        INC     hl
        LD      (a,val(M1));            //  Store "from" move position//        LD      a,(M1)          ; Store "from" move position
        LD      (ptr(hl),a);                                        //        LD      (hl),a
        INC     (hl);                                               //        INC     hl
        LD      (a,val(M2));            //  Store "to" move position//        LD      a,(M2)          ; Store "to" move position
        LD      (ptr(hl),a);                                        //        LD      (hl),a
        INC     (hl);                                               //        INC     hl
        LD      (a,val(P2));            //  Store move flags/capt. piece//        LD      a,(P2)          ; Store move flags/capt. piece
        LD      (ptr(hl),a);                                        //        LD      (hl),a
        INC     (hl);                                               //        INC     hl
        LD      (ptr(hl),0);            //  Store initial move value//        LD      (hl),0          ; Store initial move value
        INC     (hl);                                               //        INC     hl
        LD      (val16(MLNXT),hl);      //  Save address for next move//        LD      (MLNXT),hl      ; Save address for next move
        RETu;                           //  Return                  //        RET                     ; Return
AM10:   LD      (ptr(hl),0);            //  Abort entry on table ovflow//AM10:   LD      (hl),0          ; Abort entry on table ovflow
        INC     (hl);                                               //        INC     hl
        LD      (ptr(hl),0);            //  TODO does this out of memory//        LD      (hl),0          ; TODO does this out of memory
        DEC     (hl);                   //       check actually work?//        DEC     hl              ;      check actually work?
        RETu;                                                       //        RET
}                                                                   //
                                                                    //;***********************************************************
//***********************************************************       //; GENERATE MOVE ROUTINE
// GENERATE MOVE ROUTINE                                            //
//***********************************************************       //
// FUNCTION:  --  To generate the move set for all of the           //;***********************************************************
//                pieces of a given val(COLOR).                     //; FUNCTION:  --  To generate the move set for all of the
//                                                                  //;                pieces of a given color.
// CALLED BY: --  FNDMOV                                            //;
//                                                                  //; CALLED BY: --  FNDMOV
// CALLS:     --  MPIECE                                            //;
//                INCHK                                             //; CALLS:     --  MPIECE
//                                                                  //;                INCHK
// ARGUMENTS: --  None                                              //;
//***********************************************************       //; ARGUMENTS: --  None
void GENMOV() {                                                     //;***********************************************************
        CALLu   (INCHK);                //  Test for King in check  //GENMOV: CALL    INCHK           ; Test for King in check
        LD      (val(CKFLG),a);         //  Save attack count as flag//        LD      (CKFLG),a       ; Save attack count as flag
        LD      (de,val16(MLNXT));      //  Addr of next avail list space//        LD      de,(MLNXT)      ; Addr of next avail list space
        LD      (hl,val16(MLPTRI));     //  Ply list pointer index  //        LD      hl,(MLPTRI)     ; Ply list pointer index
        INC     (hl);                   //  Increment to next ply   //        INC     hl              ; Increment to next ply
        INC     (hl);                                               //        INC     hl
        LD      (ptr(hl),e);            //  Save move list pointer  //        LD      (hl),e          ; Save move list pointer
        INC     (hl);                                               //        INC     hl
        LD      (ptr(hl),d);                                        //        LD      (hl),d
        INC     (hl);                                               //        INC     hl
        LD      (val16(MLPTRI),hl);     //  Save new index          //        LD      (MLPTRI),hl     ; Save new index
        LD      (val16(MLLST),hl);      //  Last pointer for chain init.//        LD      (MLLST),hl      ; Last pointer for chain init.
        LD      (a,21);                 //  First position on board //        LD      a,21            ; First position on board
GM5:    LD      (val(M1),a);            //  Save as index           //GM5:    LD      (M1),a          ; Save as index
        LD      (ix,val16(M1));         //  Load board index        //        LD      ix,(M1)         ; Load board index
        LD      (a,ptr(ix+BOARD));      //  Fetch board contents    //        LD      a,(ix+BOARD)    ; Fetch board contents
        AND     (a);                    //  Is it empty ?           //        AND     a               ; Is it empty ?
        JR      (Z,GM10);               //  Yes - Jump              //        JR      Z,GM10          ; Yes - Jump
        CP      (-1);                   //  Is it a border square ? //        CP      -1              ; Is it a border square ?
        JR      (Z,GM10);               //  Yes - Jump              //        JR      Z,GM10          ; Yes - Jump
        LD      (val(P1),a);            //  Save piece              //        LD      (P1),a          ; Save piece
        LD      (hl,val(COLOR));        //  Address of val(COLOR) of piece//        LD      hl,COLOR        ; Address of color of piece
        XOR     (ptr(hl));              //  Test val(COLOR) of piece//        XOR     (hl)            ; Test color of piece
        BIT     (7,a);                  //  Match ?                 //        BIT     7,a             ; Match ?
        CALL    (Z,MPIECE);             //  Yes - call Move Piece   //        CALL    Z,MPIECE        ; Yes - call Move Piece
GM10:   LD      (a,val(M1));            //  Fetch current board position//GM10:   LD      a,(M1)          ; Fetch current board position
        INC     (a);                    //  Incr to next board position//        INC     a               ; Incr to next board position
        CP      (99);                   //  End of board array ?    //        CP      99              ; End of board array ?
        JP      (NZ,GM5);               //  No - Jump               //        JP      NZ,GM5          ; No - Jump
        RETu;                           //  Return                  //        RET                     ; Return
}                                                                   //
                                                                    //;***********************************************************
//***********************************************************       //; CHECK ROUTINE
// CHECK ROUTINE                                                    //
//***********************************************************       //
// FUNCTION:   --  To determine whether or not the                  //;***********************************************************
//                 King is in check.                                //; FUNCTION:   --  To determine whether or not the
//                                                                  //;                 King is in check.
// CALLED BY:  --  GENMOV                                           //;
//                 FNDMOV                                           //; CALLED BY:  --  GENMOV
//                 EVAL                                             //;                 FNDMOV
//                                                                  //;                 EVAL
// CALLS:      --  ATTACK                                           //;
//                                                                  //; CALLS:      --  ATTACK
// ARGUMENTS:  --  val(COLOR) of King                               //;
//***********************************************************       //; ARGUMENTS:  --  Color of King
void INCHK() {                                                      //;***********************************************************
        LD      (a,val(COLOR));         //  Get val(COLOR)          //INCHK:  LD      a,(COLOR)       ; Get color
        CALLu   (INCHK1);                                           //
}                                                                   //
                                                                    //
void INCHK1() {                         // Like INCHK() but takes input in register A//
        LD      (hl,addr(POSK));        //  Addr of white King position//INCHK1: LD      hl,POSK         ; Addr of white King position
        AND     (a);                    //  White ?                 //        AND     a               ; White ?
        JR      (Z,rel005);             //  Yes - Skip              //        JR      Z,rel005        ; Yes - Skip
        INC     (hl);                   //  Addr of black King position//        INC     hl              ; Addr of black King position
rel005: LD      (a,ptr(hl));            //  Fetch King position     //rel005: LD      a,(hl)          ; Fetch King position
        LD      (val(M3),a);            //  Save                    //        LD      (M3),a          ; Save
        LD      (ix,val16(M3));         //  Load board index        //        LD      ix,(M3)         ; Load board index
        LD      (a,ptr(ix+BOARD));      //  Fetch board contents    //        LD      a,(ix+BOARD)    ; Fetch board contents
        LD      (val(P1),a);            //  Save                    //        LD      (P1),a          ; Save
        AND     (7);                    //  Get piece type          //        AND     7               ; Get piece type
        LD      (val(T1),a);            //  Save                    //        LD      (T1),a          ; Save
        CALLu   (ATTACK);               //  Look for attackers on King//        CALL    ATTACK          ; Look for attackers on King
        RETu;                           //  Return                  //        RET                     ; Return
}                                                                   //
                                                                    //;***********************************************************
//***********************************************************       //; ATTACK ROUTINE
// ATTACK ROUTINE                                                   //
//***********************************************************       //
// FUNCTION:   --  To find all attackers on a given square          //;***********************************************************
//                 by scanning outward from the square              //; FUNCTION:   --  To find all attackers on a given square
//                 until a piece is found that attacks              //;                 by scanning outward from the square
//                 that square, or a piece is found that            //;                 until a piece is found that attacks
//                 doesn't attack that square, or the edge          //;                 that square, or a piece is found that
//                 of the board is reached.                         //;                 doesn't attack that square, or the edge
//                                                                  //;                 of the board is reached.
//                 In determining which pieces attack               //;
//                 a square, this routine also takes into           //;                 In determining which pieces attack
//                 account the ability of certain pieces to         //;                 a square, this routine also takes into
//                 attack through another attacking piece. (For     //;                 account the ability of certain pieces to
//                 example a queen lined up behind a bishop         //;                 attack through another attacking piece. (For
//                 of her same val(COLOR) along a diagonal.) The    //;                 example a queen lined up behind a bishop
//                 bishop is then said to be transparent to the     //;                 of her same color along a diagonal.) The
//                 queen, since both participate in the             //;                 bishop is then said to be transparent to the
//                 attack.                                          //;                 queen, since both participate in the
//                                                                  //;                 attack.
//                 In the case where this routine is called         //;
//                 by CASTLE or INCHK, the routine is               //;                 In the case where this routine is called
//                 terminated as soon as an attacker of the         //;                 by CASTLE or INCHK, the routine is
//                 opposite val(COLOR) is encountered.              //;                 terminated as soon as an attacker of the
//                                                                  //;                 opposite color is encountered.
// CALLED BY:  --  POINTS                                           //;
//                 PINFND                                           //; CALLED BY:  --  POINTS
//                 CASTLE                                           //;                 PINFND
//                 INCHK                                            //;                 CASTLE
//                                                                  //;                 INCHK
// CALLS:      --  PATH                                             //;
//                 ATKSAV                                           //; CALLS:      --  PATH
//                                                                  //;                 ATKSAV
// ARGUMENTS:  --  None                                             //;
//***********************************************************       //; ARGUMENTS:  --  None
void ATTACK() {                                                     //;***********************************************************
        PUSH    (bc);                   //  Save Register B         //ATTACK: PUSH    bc              ; Save Register B
        XOR     (a);                    //  Clear                   //        XOR     a               ; Clear
        LD      (b,16);                 //  Initial direction count //        LD      b,16            ; Initial direction count
        LD      (val(INDX2),a);         //  Initial direction index //        LD      (INDX2),a       ; Initial direction index
        LD      (iy,val16(INDX2));      //  Load index              //        LD      iy,(INDX2)      ; Load index
AT5:    LD      (c,ptr(iy+DIRECT));     //  Get direction           //AT5:    LD      c,(iy+DIRECT)   ; Get direction
        LD      (d,0);                  //  Init. scan count/flags  //        LD      d,0             ; Init. scan count/flags
        LD      (a,val(M3));            //  Init. board start position//        LD      a,(M3)          ; Init. board start position
        LD      (val(M2),a);            //  Save                    //        LD      (M2),a          ; Save
AT10:   INC     (d);                    //  Increment scan count    //AT10:   INC     d               ; Increment scan count
        CALLu   (PATH);                 //  Next position           //        CALL    PATH            ; Next position
        CP      (1);                    //  Piece of a opposite val(COLOR) ?//        CP      1               ; Piece of a opposite color ?
        JR      (Z,AT14A);              //  Yes - jump              //        JR      Z,AT14A         ; Yes - jump
        CP      (2);                    //  Piece of same val(COLOR) ?//        CP      2               ; Piece of same color ?
        JR      (Z,AT14B);              //  Yes - jump              //        JR      Z,AT14B         ; Yes - jump
        AND     (a);                    //  Empty position ?        //        AND     a               ; Empty position ?
        JR      (NZ,AT12);              //  No - jump               //        JR      NZ,AT12         ; No - jump
        LD      (a,b);                  //  Fetch direction count   //        LD      a,b             ; Fetch direction count
        CP      (9);                    //  On knight scan ?        //        CP      9               ; On knight scan ?
        JR      (NC,AT10);              //  No - jump               //        JR      NC,AT10         ; No - jump
AT12:   INC     (iy);                   //  Increment direction index//AT12:   INC     iy              ; Increment direction index
        DJNZ    (AT5);                  //  Done ? No - jump        //        DJNZ    AT5             ; Done ? No - jump
        XOR     (a);                    //  No attackers            //        XOR     a               ; No attackers
AT13:   POP     (bc);                   //  Restore register B      //AT13:   POP     bc              ; Restore register B
        RETu;                           //  Return                  //        RET                     ; Return
AT14A:  BIT     (6,d);                  //  Same val(COLOR) found already ?//AT14A:  BIT     6,d             ; Same color found already ?
        JR      (NZ,AT12);              //  Yes - jump              //        JR      NZ,AT12         ; Yes - jump
        SET     (5,d);                  //  Set opposite val(COLOR) found flag//        SET     5,d             ; Set opposite color found flag
        JPu     (AT14);                 //  Jump                    //        JP      AT14            ; Jump
AT14B:  BIT     (5,d);                  //  Opposite val(COLOR) found already?//AT14B:  BIT     5,d             ; Opposite color found already?
        JR      (NZ,AT12);              //  Yes - jump              //        JR      NZ,AT12         ; Yes - jump
        SET     (6,d);                  //  Set same val(COLOR) found flag//        SET     6,d             ; Set same color found flag
                                                                    //
//                                                                  //;
// ***** DETERMINE IF PIECE ENCOUNTERED ATTACKS SQUARE *****        //
AT14:   LD      (a,val(T2));            //  Fetch piece type encountered//
        LD      (e,a);                  //  Save                    //
        LD      (a,b);                  //  Get direction-counter   //
        CP      (9);                    //  Look for Knights ?      //
        JR      (C,AT25);               //  Yes - jump              //
        LD      (a,e);                  //  Get piece type          //
        CP      (QUEEN);                //  Is is a Queen ?         //
        JR      (NZ,AT15);              //  No - Jump               //
        SET     (7,d);                  //  Set Queen found flag    //
        JRu     (AT30);                 //  Jump                    //
AT15:   LD      (a,d);                  //  Get flag/scan count     //
        AND     (0x0F);                 //  Isolate count           //
        CP      (1);                    //  On first position ?     //
        JR      (NZ,AT16);              //  No - jump               //
        LD      (a,e);                  //  Get encountered piece type//
        CP      (KING);                 //  Is it a King ?          //
        JR      (Z,AT30);               //  Yes - jump              //
AT16:   LD      (a,b);                  //  Get direction counter   //
        CP      (13);                   //  Scanning files or ranks ?//
        JR      (C,AT21);               //  Yes - jump              //
        LD      (a,e);                  //  Get piece type          //
        CP      (BISHOP);               //  Is it a Bishop ?        //
        JR      (Z,AT30);               //  Yes - jump              //
        LD      (a,d);                  //  Get flags/scan count    //
        AND     (0x0F);                 //  Isolate count           //
        CP      (1);                    //  On first position ?     //
        JR      (NZ,AT12);              //  No - jump               //
        CP      (e);                    //  Is it a Pawn ?          //
        JR      (NZ,AT12);              //  No - jump               //
        LD      (a,val(P2));            //  Fetch piece including val(COLOR)//
        BIT     (7,a);                  //  Is it white ?           //
        JR      (Z,AT20);               //  Yes - jump              //
        LD      (a,b);                  //  Get direction counter   //
        CP      (15);                   //  On a non-attacking diagonal ?//
        JR      (C,AT12);               //  Yes - jump              //
        JRu     (AT30);                 //  Jump                    //
AT20:   LD      (a,b);                  //  Get direction counter   //
        CP      (15);                   //  On a non-attacking diagonal ?//
        JR      (NC,AT12);              //  Yes - jump              //
        JRu     (AT30);                 //  Jump                    //
AT21:   LD      (a,e);                  //  Get piece type          //
        CP      (ROOK);                 //  Is is a Rook ?          //
        JR      (NZ,AT12);              //  No - jump               //
        JRu     (AT30);                 //  Jump                    //
AT25:   LD      (a,e);                  //  Get piece type          //
        CP      (KNIGHT);               //  Is it a Knight ?        //
        JR      (NZ,AT12);              //  No - jump               //
AT30:   LD      (a,val(T1));            //  Attacked piece type/flag//
        CP      (7);                    //  Call from POINTS ?      //
        JR      (Z,AT31);               //  Yes - jump              //
        BIT     (5,d);                  //  Is attacker opposite val(COLOR) ?//
        JR      (Z,AT32);               //  No - jump               //
        LD      (a,1);                  //  Set attacker found flag //
        JPu     (AT13);                 //  Jump                    //
AT31:   CALLu   (ATKSAV);               //  Save attacker in attack list//
AT32:   LD      (a,val(T2));            //  Attacking piece type    //
        CP      (KING);                 //  Is it a King,?          //
        JP      (Z,AT12);               //  Yes - jump              //
        CP      (KNIGHT);               //  Is it a Knight ?        //
        JP      (Z,AT12);               //  Yes - jump              //
        JPu     (AT10);                 //  Jump                    //
 }                                                                  //
                                                                    //
//***********************************************************       //
// ATTACK SAVE ROUTINE                                              //;***********************************************************
//***********************************************************       //; ATTACK SAVE ROUTINE
// FUNCTION:   --  To save an attacking piece value in the          //;***********************************************************
//                 attack list, and to increment the attack         //; FUNCTION:   --  To save an attacking piece value in the
//                 count for that val(COLOR) piece.                 //;                 attack list, and to increment the attack
//                                                                  //;                 count for that color piece.
//                 The pin piece list is checked for the            //;
//                 attacking piece, and if found there, the         //;                 The pin piece list is checked for the
//                 piece is not included in the attack list.        //;                 attacking piece, and if found there, the
//                                                                  //;                 piece is not included in the attack list.
// CALLED BY:  --  ATTACK                                           //;
//                                                                  //; CALLED BY:  --  ATTACK
// CALLS:      --  PNCK                                             //;
//                                                                  //; CALLS:      --  PNCK
// ARGUMENTS:  --  None                                             //;
//***********************************************************       //; ARGUMENTS:  --  None
void ATKSAV() {                                                     //;***********************************************************
        PUSH    (bc);                   //  Save Regs BC            //ATKSAV: PUSH    bc              ; Save Regs BC
        PUSH    (de);                   //  Save Regs DE            //        PUSH    de              ; Save Regs DE
        LD      (a,val(NPINS));         //  Number of pinned pieces //        LD      a,(NPINS)       ; Number of pinned pieces
        AND     (a);                    //  Any ?                   //        AND     a               ; Any ?
        CALL    (NZ,PNCK);              //  yes - check pin list    //        CALL    NZ,PNCK         ; yes - check pin list
        LD      (ix,val16(T2));         //  Init index to value table//        LD      ix,(T2)         ; Init index to value table
        LD      (hl,addr(ATKLST));      //  Init address of attack list//        LD      hl,ATKLST       ; Init address of attack list
        LD      (bc,0);                 //  Init increment for white//        LD      bc,0            ; Init increment for white
        LD      (a,val(P2));            //  Attacking piece         //        LD      a,(P2)          ; Attacking piece
        BIT     (7,a);                  //  Is it white ?           //        BIT     7,a             ; Is it white ?
        JR      (Z,rel006);             //  Yes - jump              //        JR      Z,rel006        ; Yes - jump
        LD      (c,7);                  //  Init increment for black//        LD      c,7             ; Init increment for black
rel006: AND     (7);                    //  Attacking piece type    //rel006: AND     7               ; Attacking piece type
        LD      (e,a);                  //  Init increment for type //        LD      e,a             ; Init increment for type
        BIT     (7,d);                  //  Queen found this scan ? //        BIT     7,d             ; Queen found this scan ?
        JR      (Z,rel007);             //  No - jump               //        JR      Z,rel007        ; No - jump
        LD      (e,QUEEN);              //  Use Queen slot in attack list//        LD      e,QUEEN         ; Use Queen slot in attack list
rel007: ADD16   (hl,bc);                //  Attack list address     //rel007: ADD     hl,bc           ; Attack list address
        INC     (ptr(hl));              //  Increment list count    //        INC     (hl)            ; Increment list count
        LD      (d,0);                                              //        LD      d,0
        ADD16   (hl,de);                //  Attack list slot address//        ADD     hl,de           ; Attack list slot address
        LD      (a,ptr(hl));            //  Get data already there  //        LD      a,(hl)          ; Get data already there
        AND     (0x0f);                 //  Is first slot empty ?   //        AND     0FH             ; Is first slot empty ?
        JR      (Z,AS20);               //  Yes - jump              //        JR      Z,AS20          ; Yes - jump
        LD      (a,ptr(hl));            //  Get data again          //        LD      a,(hl)          ; Get data again
        AND     (0xf0);                 //  Is second slot empty ?  //        AND     0F0H            ; Is second slot empty ?
        JR      (Z,AS19);               //  Yes - jump              //        JR      Z,AS19          ; Yes - jump
        INC     (hl);                   //  Increment to King slot  //        INC     hl              ; Increment to King slot
        JRu     (AS20);                 //  Jump                    //        JR      AS20            ; Jump
AS19:   RLD;                            //  Temp save lower in upper//AS19:   RLD                     ; Temp save lower in upper
        LD      (a,ptr(ix+PVALUE));     //  Get new value for attack list//        LD      a,(ix+PVALUE)   ; Get new value for attack list
        RRD;                            //  Put in 2nd attack list slot//        RRD                     ; Put in 2nd attack list slot
        JRu     (AS25);                 //  Jump                    //        JR      AS25            ; Jump
AS20:   LD      (a,ptr(ix+PVALUE));     //  Get new value for attack list//AS20:   LD      a,(ix+PVALUE)   ; Get new value for attack list
        RLD;                            //  Put in 1st attack list slot//        RLD                     ; Put in 1st attack list slot
AS25:   POP     (de);                   //  Restore DE regs         //AS25:   POP     de              ; Restore DE regs
        POP     (bc);                   //  Restore BC regs         //        POP     bc              ; Restore BC regs
        RETu;                           //  Return                  //        RET                     ; Return
 }                                                                  //
                                                                    //;***********************************************************
//***********************************************************       //; PIN CHECK ROUTINE
// PIN CHECK ROUTINE                                                //
//***********************************************************       //
// FUNCTION:   --  Checks to see if the attacker is in the          //;***********************************************************
//                 pinned piece list. If so he is not a valid       //; FUNCTION:   --  Checks to see if the attacker is in the
//                 attacker unless the direction in which he        //;                 pinned piece list. If so he is not a valid
//                 attacks is the same as the direction along       //;                 attacker unless the direction in which he
//                 which he is pinned. If the piece is              //;                 attacks is the same as the direction along
//                 found to be invalid as an attacker, the          //;                 which he is pinned. If the piece is
//                 return to the calling routine is aborted         //;                 found to be invalid as an attacker, the
//                 and this routine returns directly to ATTACK.     //;                 return to the calling routine is aborted
//                                                                  //;                 and this routine returns directly to ATTACK.
// CALLED BY:  --  ATKSAV                                           //;
//                                                                  //; CALLED BY:  --  ATKSAV
// CALLS:      --  None                                             //;
//                                                                  //; CALLS:      --  None
// ARGUMENTS:  --  The direction of the attack. The                 //;
//                 pinned piece counnt.                             //; ARGUMENTS:  --  The direction of the attack. The
//***********************************************************       //;                 pinned piece counnt.
void PNCK() {                                                       //;***********************************************************
        LD      (d,c);                  //  Save attack direction   //PNCK:   LD      d,c             ; Save attack direction
        LD      (e,0);                  //  Clear flag              //        LD      e,0             ; Clear flag
        LD      (c,a);                  //  Load pin count for search//        LD      c,a             ; Load pin count for search
        LD      (b,0);                                              //        LD      b,0
        LD      (a,val(M2));            //  Position of piece       //        LD      a,(M2)          ; Position of piece
        LD      (hl,addr(PLISTA));      //  Pin list address        //        LD      hl,PLISTA       ; Pin list address
PC1:    Z80_CPIR;                       //  Search list for position//PC1:    CPIR                    ; Search list for position
        RET     (NZ);                   //  Return if not found     //        RET     NZ              ; Return if not found
        Z80_EXAF;                       //  Save search parameters  //        EX      af,af'          ; Save search parameters
        BIT     (0,e);                  //  Is this the first find ?//        BIT     0,e             ; Is this the first find ?
        JR      (NZ,PC5);               //  No - jump               //        JR      NZ,PC5          ; No - jump
        SET     (0,e);                  //  Set first find flag     //        SET     0,e             ; Set first find flag
        PUSH    (hl);                   //  Get corresp index to dir list//        PUSH    hl              ; Get corresp index to dir list
        POP     (ix);                                               //        POP     ix
        LD      (a,ptr(ix+9));          //  Get direction           //        LD      a,(ix+9)        ; Get direction
        CP      (d);                    //  Same as attacking direction ?//        CP      d               ; Same as attacking direction ?
        JR      (Z,PC3);                //  Yes - jump              //        JR      Z,PC3           ; Yes - jump
        NEG;                            //  Opposite direction ?    //        NEG                     ; Opposite direction ?
        CP      (d);                    //  Same as attacking direction ?//        CP      d               ; Same as attacking direction ?
        JR      (NZ,PC5);               //  No - jump               //        JR      NZ,PC5          ; No - jump
PC3:    Z80_EXAF;                       //  Restore search parameters//PC3:    EX      af,af'          ; Restore search parameters
        JP      (PE,PC1);               //  Jump if search not complete//        JP      PE,PC1          ; Jump if search not complete
        RETu;                           //  Return                  //        RET                     ; Return
PC5:    POP     (af);                   //  Abnormal exit           //PC5:    POP     af              ; Abnormal exit
        POP     (de);                   //  Restore regs.           //        POP     de              ; Restore regs.
        POP     (bc);                                               //        POP     bc
        RETu;                           //  Return to ATTACK        //        RET                     ; Return to ATTACK
}                                                                   //
                                                                    //;***********************************************************
//***********************************************************       //; PIN FIND ROUTINE
// PIN FIND ROUTINE                                                 //
//***********************************************************       //
// FUNCTION:   --  To produce a list of all pieces pinned           //;***********************************************************
//                 against the King or Queen, for both white        //; FUNCTION:   --  To produce a list of all pieces pinned
//                 and black.                                       //;                 against the King or Queen, for both white
//                                                                  //;                 and black.
// CALLED BY:  --  FNDMOV                                           //;
//                 EVAL                                             //; CALLED BY:  --  FNDMOV
//                                                                  //;                 EVAL
// CALLS:      --  PATH                                             //;
//                 ATTACK                                           //; CALLS:      --  PATH
//                                                                  //;                 ATTACK
// ARGUMENTS:  --  None                                             //;
//***********************************************************       //; ARGUMENTS:  --  None
void PINFND() {                                                     //;***********************************************************
        XOR     (a);                    //  Zero pin count          //PINFND: XOR     a               ; Zero pin count
        LD      (val(NPINS),a);                                     //        LD      (NPINS),a
        LD      (de,addr(POSK));        //  Addr of King/Queen pos list//        LD      de,POSK         ; Addr of King/Queen pos list
PF1:    LD      (a,ptr(de));            //  Get position of royal piece//PF1:    LD      a,(de)          ; Get position of royal piece
        AND     (a);                    //  Is it on board ?        //        AND     a               ; Is it on board ?
        JP      (Z,PF26);               //  No- jump                //        JP      Z,PF26          ; No- jump
        CP      (-1);                   //  At end of list ?        //        CP      -1              ; At end of list ?
        RET     (Z);                    //  Yes return              //        RET     Z               ; Yes return
        LD      (val(M3),a);            //  Save position as board index//        LD      (M3),a          ; Save position as board index
        LD      (ix,val16(M3));         //  Load index to board     //        LD      ix,(M3)         ; Load index to board
        LD      (a,ptr(ix+BOARD));      //  Get contents of board   //        LD      a,(ix+BOARD)    ; Get contents of board
        LD      (val(P1),a);            //  Save                    //        LD      (P1),a          ; Save
        LD      (b,8);                  //  Init scan direction count//        LD      b,8             ; Init scan direction count
        XOR     (a);                                                //        XOR     a
        LD      (val(INDX2),a);         //  Init direction index    //        LD      (INDX2),a       ; Init direction index
        LD      (iy,val16(INDX2));                                  //        LD      iy,(INDX2)
PF2:    LD      (a,val(M3));            //  Get King/Queen position //PF2:    LD      a,(M3)          ; Get King/Queen position
        LD      (val(M2),a);            //  Save                    //        LD      (M2),a          ; Save
        XOR     (a);                                                //        XOR     a
        LD      (val(M4),a);            //  Clear pinned piece saved pos//        LD      (M4),a          ; Clear pinned piece saved pos
        LD      (c,ptr(iy+DIRECT));     //  Get direction of scan   //        LD      c,(iy+DIRECT)   ; Get direction of scan
PF5:    CALLu   (PATH);                 //  Compute next position   //PF5:    CALL    PATH            ; Compute next position
        AND     (a);                    //  Is it empty ?           //        AND     a               ; Is it empty ?
        JR      (Z,PF5);                //  Yes - jump              //        JR      Z,PF5           ; Yes - jump
        CP      (3);                    //  Off board ?             //        CP      3               ; Off board ?
        JP      (Z,PF25);               //  Yes - jump              //        JP      Z,PF25          ; Yes - jump
        CP      (2);                    //  Piece of same val(COLOR)//        CP      2               ; Piece of same color
        LD      (a,val(M4));            //  Load pinned piece position//        LD      a,(M4)          ; Load pinned piece position
        JR      (Z,PF15);               //  Yes - jump              //        JR      Z,PF15          ; Yes - jump
        AND     (a);                    //  Possible pin ?          //        AND     a               ; Possible pin ?
        JP      (Z,PF25);               //  No - jump               //        JP      Z,PF25          ; No - jump
        LD      (a,val(T2));            //  Piece type encountered  //        LD      a,(T2)          ; Piece type encountered
        CP      (QUEEN);                //  Queen ?                 //        CP      QUEEN           ; Queen ?
        JP      (Z,PF19);               //  Yes - jump              //        JP      Z,PF19          ; Yes - jump
        LD      (l,a);                  //  Save piece type         //        LD      l,a             ; Save piece type
        LD      (a,b);                  //  Direction counter       //        LD      a,b             ; Direction counter
        CP      (5);                    //  Non-diagonal direction ?//        CP      5               ; Non-diagonal direction ?
        JR      (C,PF10);               //  Yes - jump              //        JR      C,PF10          ; Yes - jump
        LD      (a,l);                  //  Piece type              //        LD      a,l             ; Piece type
        CP      (BISHOP);               //  Bishop ?                //        CP      BISHOP          ; Bishop ?
        JP      (NZ,PF25);              //  No - jump               //        JP      NZ,PF25         ; No - jump
        JPu     (PF20);                 //  Jump                    //        JP      PF20            ; Jump
PF10:   LD      (a,l);                  //  Piece type              //PF10:   LD      a,l             ; Piece type
        CP      (ROOK);                 //  Rook ?                  //        CP      ROOK            ; Rook ?
        JP      (NZ,PF25);              //  No - jump               //        JP      NZ,PF25         ; No - jump
        JPu     (PF20);                 //  Jump                    //        JP      PF20            ; Jump
PF15:   AND     (a);                    //  Possible pin ?          //PF15:   AND     a               ; Possible pin ?
        JP      (NZ,PF25);              //  No - jump               //        JP      NZ,PF25         ; No - jump
        LD      (a,val(M2));            //  Save possible pin position//        LD      a,(M2)          ; Save possible pin position
        LD      (val(M4),a);                                        //        LD      (M4),a
        JPu     (PF5);                  //  Jump                    //        JP      PF5             ; Jump
PF19:   LD      (a,val(P1));            //  Load King or Queen      //PF19:   LD      a,(P1)          ; Load King or Queen
        AND     (7);                    //  Clear flags             //        AND     7               ; Clear flags
        CP      (QUEEN);                //  Queen ?                 //        CP      QUEEN           ; Queen ?
        JR      (NZ,PF20);              //  No - jump               //        JR      NZ,PF20         ; No - jump
        PUSH    (bc);                   //  Save regs.              //        PUSH    bc              ; Save regs.
        PUSH    (de);                                               //        PUSH    de
        PUSH    (iy);                                               //        PUSH    iy
        XOR     (a);                    //  Zero out attack list    //        XOR     a               ; Zero out attack list
        LD      (b,14);                                             //        LD      b,14
        LD      (hl,addr(ATKLST));                                  //        LD      hl,ATKLST
back02: LD      (ptr(hl),a);                                        //back02: LD      (hl),a
        INC     (hl);                                               //        INC     hl
        DJNZ    (back02);                                           //        DJNZ    back02
        LD      (a,7);                  //  Set attack flag         //        LD      a,7             ; Set attack flag
        LD      (val(T1),a);                                        //        LD      (T1),a
        CALLu   (ATTACK);               //  Find attackers/defenders//        CALL    ATTACK          ; Find attackers/defenders
        LD      (hl,WACT);              //  White queen attackers   //        LD      hl,WACT         ; White queen attackers
        LD      (de,BACT);              //  Black queen attackers   //        LD      de,BACT         ; Black queen attackers
        LD      (a,val(P1));            //  Get queen               //        LD      a,(P1)          ; Get queen
        BIT     (7,a);                  //  Is she white ?          //        BIT     7,a             ; Is she white ?
        JR      (Z,rel008);             //  Yes - skip              //        JR      Z,rel008        ; Yes - skip
        EX      (de,hl);                //  Reverse for black       //        EX      de,hl           ; Reverse for black
rel008: LD      (a,ptr(hl));            //  Number of defenders     //rel008: LD      a,(hl)          ; Number of defenders
        EX      (de,hl);                //  Reverse for attackers   //        EX      de,hl           ; Reverse for attackers
        SUB     (ptr(hl));              //  Defenders minus attackers//        SUB     (hl)            ; Defenders minus attackers
        DEC     (a);                    //  Less 1                  //        DEC     a               ; Less 1
        POP     (iy);                   //  Restore regs.           //        POP     iy              ; Restore regs.
        POP     (de);                                               //        POP     de
        POP     (bc);                                               //        POP     bc
        JP      (P,PF25);               //  Jump if pin not valid   //        JP      P,PF25          ; Jump if pin not valid
PF20:   LD      (hl,addr(NPINS));       //  Address of pinned piece count//PF20:   LD      hl,NPINS        ; Address of pinned piece count
        INC     (ptr(hl));              //  Increment               //        INC     (hl)            ; Increment
        LD      (ix,val16(NPINS));      //  Load pin list index     //        LD      ix,(NPINS)      ; Load pin list index
        LD      (ptr(ix+PLISTD),c);     //  Save direction of pin   //        LD      (ix+PLISTD),c   ; Save direction of pin
        LD      (a,val(M4));            //  Position of pinned piece//        LD      a,(M4)          ; Position of pinned piece
        LD      (ptr(ix+PLIST),a);      //  Save in list            //        LD      (ix+PLIST),a    ; Save in list
PF25:   INC     (iy);                   //  Increment direction index//PF25:   INC     iy              ; Increment direction index
        DJNZ    (PF27);                 //  Done ? No - Jump        //        DJNZ    PF27            ; Done ? No - Jump
PF26:   INC     (de);                   //  Incr King/Queen pos index//PF26:   INC     de              ; Incr King/Queen pos index
        JPu     (PF1);                  //  Jump                    //        JP      PF1             ; Jump
PF27:   JPu     (PF2);                  //  Jump                    //PF27:   JP      PF2             ; Jump
}                                                                   //
                                                                    //;***********************************************************
//***********************************************************       //; EXCHANGE ROUTINE
// EXCHANGE ROUTINE                                                 //
//***********************************************************       //
// FUNCTION:   --  To determine the exchange value of a             //;***********************************************************
//                 piece on a given square by examining all         //; FUNCTION:   --  To determine the exchange value of a
//                 attackers and defenders of that piece.           //;                 piece on a given square by examining all
//                                                                  //;                 attackers and defenders of that piece.
// CALLED BY:  --  POINTS                                           //;
//                                                                  //; CALLED BY:  --  POINTS
// CALLS:      --  NEXTAD                                           //;
//                                                                  //; CALLS:      --  NEXTAD
// ARGUMENTS:  --  None.                                            //;
//***********************************************************       //; ARGUMENTS:  --  None.
void XCHNG() {                                                      //;***********************************************************
        EXX;                            //  Swap regs.              //XCHNG:  EXX                     ; Swap regs.
        LD      (a,(val(P1)));          //  Piece attacked          //        LD      a,(P1)          ; Piece attacked
        LD      (hl,WACT);        //  Addr of white attkrs/dfndrs   //        LD      hl,WACT         ; Addr of white attkrs/dfndrs
        LD      (de,BACT);        //  Addr of black attkrs/dfndrs   //        LD      de,BACT         ; Addr of black attkrs/dfndrs
        BIT     (7,a);                  //  Is piece white ?        //        BIT     7,a             ; Is piece white ?
        JR      (Z,rel009);             //  Yes - jump              //        JR      Z,rel009        ; Yes - jump
        EX      (de,hl);                //  Swap list pointers      //        EX      de,hl           ; Swap list pointers
rel009: LD      (b,ptr(hl));            //  Init list counts        //rel009: LD      b,(hl)          ; Init list counts
        EX      (de,hl);                                            //        EX      de,hl
        LD      (c,ptr(hl));                                        //        LD      c,(hl)
        EX      (de,hl);                                            //        EX      de,hl
        EXX;                            //  Restore regs.           //        EXX                     ; Restore regs.
        LD      (c,0);                  //  Init attacker/defender flag//        LD      c,0             ; Init attacker/defender flag
        LD      (e,0);                  //  Init points lost count  //        LD      e,0             ; Init points lost count
        LD      (ix,val16(T3));         //  Load piece value index  //        LD      ix,(T3)         ; Load piece value index
        LD      (d,ptr(ix+PVALUE));     //  Get attacked piece value//        LD      d,(ix+PVALUE)   ; Get attacked piece value
        SLA     (d);                    //  Double it               //        SLA     d               ; Double it
        LD      (b,d);                  //  Save                    //        LD      b,d             ; Save
        CALLu   (NEXTAD);               //  Retrieve first attacker //        CALL    NEXTAD          ; Retrieve first attacker
        RET     (Z);                    //  Return if none          //        RET     Z               ; Return if none
XC10:   LD      (l,a);                  //  Save attacker value     //XC10:   LD      l,a             ; Save attacker value
        CALLu   (NEXTAD);               //  Get next defender       //        CALL    NEXTAD          ; Get next defender
        JR      (Z,XC18);               //  Jump if none            //        JR      Z,XC18          ; Jump if none
        Z80_EXAF;                       //  Save defender value     //        EX      af,af'          ; Save defender value
        LD      (a,b);                  //  Get attacked value      //        LD      a,b             ; Get attacked value
        CP      (l);                    //  Attacked less than attacker ?//        CP      l               ; Attacked less than attacker ?
        JR      (NC,XC19);              //  No - jump               //        JR      NC,XC19         ; No - jump
        Z80_EXAF;                       //  -Restore defender       //        EX      af,af'          ; -Restore defender
XC15:   CP      (l);                    //  Defender less than attacker ?//XC15:   CP      l               ; Defender less than attacker ?
        RET     (C);                    //  Yes - return            //        RET     C               ; Yes - return
        CALLu   (NEXTAD);               //  Retrieve next attacker value//        CALL    NEXTAD          ; Retrieve next attacker value
        RET     (Z);                    //  Return if none          //        RET     Z               ; Return if none
        LD      (l,a);                  //  Save attacker value     //        LD      l,a             ; Save attacker value
        CALLu   (NEXTAD);               //  Retrieve next defender value//        CALL    NEXTAD          ; Retrieve next defender value
        JR      (NZ,XC15);              //  Jump if none            //        JR      NZ,XC15         ; Jump if none
XC18:   Z80_EXAF;                       //  Save Defender           //XC18:   EX      af,af'          ; Save Defender
        LD      (a,b);                  //  Get value of attacked piece//        LD      a,b             ; Get value of attacked piece
XC19:   BIT     (0,c);                  //  Attacker or defender ?  //XC19:   BIT     0,c             ; Attacker or defender ?
        JR      (Z,rel010);             //  Jump if defender        //        JR      Z,rel010        ; Jump if defender
        NEG;                     //  Negate value for attacker      //        NEG                     ; Negate value for attacker
rel010: ADD     (a,e);                  //  Total points lost       //rel010: ADD     a,e             ; Total points lost
        LD      (e,a);                  //  Save total              //        LD      e,a             ; Save total
        Z80_EXAF;                       //  Restore previous defender//        EX      af,af'          ; Restore previous defender
        RET     (Z);                    //  Return if none          //        RET     Z               ; Return if none
        LD      (b,l);                  //  Prev attckr becomes defender//        LD      b,l             ; Prev attckr becomes defender
        JPu     (XC10);                 //  Jump                    //        JP      XC10            ; Jump
}                                                                   //
                                                                    //;***********************************************************
//***********************************************************       //; NEXT ATTACKER/DEFENDER ROUTINE
// NEXT ATTACKER/DEFENDER ROUTINE                                   //
//***********************************************************       //
// FUNCTION:   --  To retrieve the next attacker or defender        //;***********************************************************
//                 piece value from the attack list, and delete     //; FUNCTION:   --  To retrieve the next attacker or defender
//                 that piece from the list.                        //;                 piece value from the attack list, and delete
//                                                                  //;                 that piece from the list.
// CALLED BY:  --  XCHNG                                            //;
//                                                                  //; CALLED BY:  --  XCHNG
// CALLS:      --  None                                             //;
//                                                                  //; CALLS:      --  None
// ARGUMENTS:  --  Attack list addresses.                           //;
//                 Side flag                                        //; ARGUMENTS:  --  Attack list addresses.
//                 Attack list counts                               //;                 Side flag
//***********************************************************       //;                 Attack list counts
void NEXTAD() {                                                     //;***********************************************************
        INC     (c);                    //  Increment side flag     //NEXTAD: INC     c               ; Increment side flag
        EXX;                            //  Swap registers          //        EXX                     ; Swap registers
        LD      (a,b);                  //  Swap list counts        //        LD      a,b             ; Swap list counts
        LD      (b,c);                  //                          //        LD      b,c
        LD      (c,a);                  //                          //        LD      c,a
        EX      (de,hl);                //  Swap list pointers      //        EX      de,hl           ; Swap list pointers
        XOR     (a);                    //                          //        XOR     a
        CP      (b);                    //  At end of list ?        //        CP      b               ; At end of list ?
        JR      (Z,NX6);                //  Yes - jump              //        JR      Z,NX6           ; Yes - jump
        DEC     (b);                    //  Decrement list count    //        DEC     b               ; Decrement list count
back03: INC     (hl);                   //  Increment list pointer  //back03: INC     hl              ; Increment list pointer
        CP      (ptr(hl));              //  Check next item in list //        CP      (hl)            ; Check next item in list
        JR      (Z,back03);             //  Jump if empty           //        JR      Z,back03        ; Jump if empty
        RRD;                            //  Get value from list     //        RRD                     ; Get value from list
        ADD     (a,a);                  //  Double it               //        ADD     a,a             ; Double it
        DEC     (hl);                   //  Decrement list pointer  //        DEC     hl              ; Decrement list pointer
NX6:    EXX;                            //  Restore regs.           //NX6:    EXX                     ; Restore regs.
        RETu;                           //  Return                  //        RET                     ; Return
}                                                                   //
                                                                    //;***********************************************************
//***********************************************************       //; POINT EVALUATION ROUTINE
// POINT EVALUATION ROUTINE                                         //
//***********************************************************       //
//FUNCTION:   --  To perform a static board evaluation and          //;***********************************************************
//                derive a score for a given board position         //;FUNCTION:   --  To perform a static board evaluation and
//                                                                  //;                derive a score for a given board position
// CALLED BY:  --  FNDMOV                                           //;
//                 EVAL                                             //; CALLED BY:  --  FNDMOV
//                                                                  //;                 EVAL
// CALLS:      --  ATTACK                                           //;
//                 XCHNG                                            //; CALLS:      --  ATTACK
//                 LIMIT                                            //;                 XCHNG
//                                                                  //;                 LIMIT
// ARGUMENTS:  --  None                                             //;
//***********************************************************       //; ARGUMENTS:  --  None
void POINTS() {                                                     //;***********************************************************
        XOR     (a);                    //  Zero out variables      //POINTS: XOR     a               ; Zero out variables
        LD      (val(MTRL),a);          //                          //        LD      (MTRL),a
        LD      (val(BRDC),a);          //                          //        LD      (BRDC),a
        LD      (val(PTSL),a);          //                          //        LD      (PTSL),a
        LD      (val(PTSW1),a);         //                          //        LD      (PTSW1),a
        LD      (val(PTSW2),a);         //                          //        LD      (PTSW2),a
        LD      (val(PTSCK),a);         //                          //        LD      (PTSCK),a
        LD      (hl,addr(T1));          //  Set attacker flag       //        LD      hl,T1           ; Set attacker flag
        LD      (ptr(hl),7);            //                          //        LD      (hl),7
        LD      (a,21);                 //  Init to first square on board//        LD      a,21            ; Init to first square on board
PT5:    LD      (val(M3),a);            //  Save as board index     //PT5:    LD      (M3),a          ; Save as board index
        LD      (ix,val16(M3));         //  Load board index        //        LD      ix,(M3)         ; Load board index
        LD      (a,ptr(ix+BOARD));      //  Get piece from board    //        LD      a,(ix+BOARD)    ; Get piece from board
        CP      (-1);                   //  Off board edge ?        //        CP      -1              ; Off board edge ?
        JP      (Z,PT25);               //  Yes - jump              //        JP      Z,PT25          ; Yes - jump
        LD      (hl,addr(P1));          //  Save piece, if any      //        LD      hl,P1           ; Save piece, if any
        LD      (ptr(hl),a);            //                          //        LD      (hl),a
        AND     (7);                    //  Save piece type, if any //        AND     7               ; Save piece type, if any
        LD      (val(T3),a);            //                          //        LD      (T3),a
        CP      (KNIGHT);               //  Less than a Knight (Pawn) ?//        CP      KNIGHT          ; Less than a Knight (Pawn) ?
        JR      (C,PT6X);               //  Yes - Jump              //        JR      C,PT6X          ; Yes - Jump
        CP      (ROOK);                 //  Rook, Queen or King ?   //        CP      ROOK            ; Rook, Queen or King ?
        JR      (C,PT6B);               //  No - jump               //        JR      C,PT6B          ; No - jump
        CP      (KING);                 //  Is it a King ?          //        CP      KING            ; Is it a King ?
        JR      (Z,PT6AA);              //  Yes - jump              //        JR      Z,PT6AA         ; Yes - jump
        LD      (a,val(MOVENO));        //  Get move number         //        LD      a,(MOVENO)      ; Get move number
        CP      (7);                    //  Less than 7 ?           //        CP      7               ; Less than 7 ?
        JR      (C,PT6A);               //  Yes - Jump              //        JR      C,PT6A          ; Yes - Jump
        JPu     (PT6X);                 //  Jump                    //        JP      PT6X            ; Jump
PT6AA:  BIT     (4,ptr(hl));            //  Castled yet ?           //PT6AA:  BIT     4,(hl)          ; Castled yet ?
        JR      (Z,PT6A);               //  No - jump               //        JR      Z,PT6A          ; No - jump
        LD      (a,+6);                 //  Bonus for castling      //        LD      a,+6            ; Bonus for castling
        BIT     (7,ptr(hl));            //  Check piece val(COLOR)  //        BIT     7,(hl)          ; Check piece color
        JR      (Z,PT6D);               //  Jump if white           //        JR      Z,PT6D          ; Jump if white
        LD      (a,-6);                 //  Bonus for black castling//        LD      a,-6            ; Bonus for black castling
        JPu     (PT6D);                 //  Jump                    //        JP      PT6D            ; Jump
PT6A:   BIT     (3,ptr(hl));            //  Has piece moved yet ?   //PT6A:   BIT     3,(hl)          ; Has piece moved yet ?
        JR      (Z,PT6X);               //  No - jump               //        JR      Z,PT6X          ; No - jump
        JPu     (PT6C);                 //  Jump                    //        JP      PT6C            ; Jump
PT6B:   BIT     (3,ptr(hl));            //  Has piece moved yet ?   //PT6B:   BIT     3,(hl)          ; Has piece moved yet ?
        JR      (NZ,PT6X);              //  Yes - jump              //        JR      NZ,PT6X         ; Yes - jump
PT6C:   LD      (a,-2);                 //  Two point penalty for white//PT6C:   LD      a,-2            ; Two point penalty for white
        BIT     (7,ptr(hl));            //  Check piece val(COLOR)  //        BIT     7,(hl)          ; Check piece color
        JR      (Z,PT6D);               //  Jump if white           //        JR      Z,PT6D          ; Jump if white
        LD      (a,+2);                 //  Two point penalty for black//        LD      a,+2            ; Two point penalty for black
PT6D:   LD      (hl,addr(BRDC));        //  Get address of board control//PT6D:   LD      hl,BRDC         ; Get address of board control
        ADD     (a,ptr(hl));            //  Add on penalty/bonus points//        ADD     a,(hl)          ; Add on penalty/bonus points
        LD      (ptr(hl),a);            //  Save                    //        LD      (hl),a          ; Save
PT6X:   XOR     (a);                    //  Zero out attack list    //PT6X:   XOR     a               ; Zero out attack list
        LD      (b,14);                 //                          //        LD      b,14
        LD      (hl,addr(ATKLST));      //                          //        LD      hl,ATKLST
back04: LD      (ptr(hl),a);            //                          //back04: LD      (hl),a
        INC     (hl);                   //                          //        INC     hl
        DJNZ    (back04);               //                          //        DJNZ    back04
        CALLu   (ATTACK);               //  Build attack list for square//        CALL    ATTACK          ; Build attack list for square
        LD      (hl,BACT);        //  Get black attacker count addr //        LD      hl,BACT         ; Get black attacker count addr
        LD      (a,val(wact));          //  Get white attacker count//        LD      a,(WACT)        ; Get white attacker count
        SUB     (ptr(hl));              //  Compute count difference//        SUB     (hl)            ; Compute count difference
        LD      (hl,addr(BRDC));        //  Address of board control//        LD      hl,BRDC         ; Address of board control
        ADD     (a,ptr(hl));            //  Accum board control score//        ADD     a,(hl)          ; Accum board control score
        LD      (ptr(hl),a);            //  Save                    //        LD      (hl),a          ; Save
        LD      (a,val(P1));            //  Get piece on current square//        LD      a,(P1)          ; Get piece on current square
        AND     (a);                    //  Is it empty ?           //        AND     a               ; Is it empty ?
        JP      (Z,PT25);               //  Yes - jump              //        JP      Z,PT25          ; Yes - jump
        CALLu   (XCHNG);                //  Evaluate exchange, if any//        CALL    XCHNG           ; Evaluate exchange, if any
        XOR     (a);                    //  Check for a loss        //        XOR     a               ; Check for a loss
        CP      (e);                    //  Points lost ?           //        CP      e               ; Points lost ?
        JR      (Z,PT23);               //  No - Jump               //        JR      Z,PT23          ; No - Jump
        DEC     (d);                    //  Deduct half a Pawn value//        DEC     d               ; Deduct half a Pawn value
        LD      (a,val(P1));            //  Get piece under attack  //        LD      a,(P1)          ; Get piece under attack
        LD      (hl,val(COLOR));        //  val(COLOR) of side just moved//        LD      hl,COLOR        ; Color of side just moved
        XOR     (ptr(hl));              //  Compare with piece      //        XOR     (hl)            ; Compare with piece
        BIT     (7,a);                  //  Do colors match ?       //        BIT     7,a             ; Do colors match ?
        LD      (a,e);                  //  Points lost             //        LD      a,e             ; Points lost
        JR      (NZ,PT20);              //  Jump if no match        //        JR      NZ,PT20         ; Jump if no match
        LD      (hl,val(PTSL));         //  Previous max points lost//        LD      hl,PTSL         ; Previous max points lost
        CP      (ptr(hl));              //  Compare to current value//        CP      (hl)            ; Compare to current value
        JR      (C,PT23);               //  Jump if greater than    //        JR      C,PT23          ; Jump if greater than
        LD      (ptr(hl),e);            //  Store new value as max lost//        LD      (hl),e          ; Store new value as max lost
        LD      (ix,val16(MLPTRJ));     //  Load pointer to this move//        LD      ix,(MLPTRJ)     ; Load pointer to this move
        LD      (a,val(M3));            //  Get position of lost piece//        LD      a,(M3)          ; Get position of lost piece
        CP      (ptr(ix+MLTOP));        //  Is it the one moving ?  //        CP      (ix+MLTOP)      ; Is it the one moving ?
        JR      (NZ,PT23);              //  No - jump               //        JR      NZ,PT23         ; No - jump
        LD      (val(PTSCK),a);         //  Save position as a flag //        LD      (PTSCK),a       ; Save position as a flag
        JPu     (PT23);                 //  Jump                    //        JP      PT23            ; Jump
PT20:   LD      (hl,addr(PTSW1));       //  Previous maximum points won//PT20:   LD      hl,PTSW1        ; Previous maximum points won
        CP      (ptr(hl));              //  Compare to current value//        CP      (hl)            ; Compare to current value
        JR      (C,rel011);             //  Jump if greater than    //        JR      C,rel011        ; Jump if greater than
        LD      (a,ptr(hl));            //  Load previous max value //        LD      a,(hl)          ; Load previous max value
        LD      (ptr(hl),e);            //  Store new value as max won//        LD      (hl),e          ; Store new value as max won
rel011: LD      (hl,addr(PTSW2));       //  Previous 2nd max points won//rel011: LD      hl,PTSW2        ; Previous 2nd max points won
        CP      (ptr(hl));              //  Compare to current value//        CP      (hl)            ; Compare to current value
        JR      (C,PT23);               //  Jump if greater than    //        JR      C,PT23          ; Jump if greater than
        LD      (ptr(hl),a);            //  Store as new 2nd max lost//        LD      (hl),a          ; Store as new 2nd max lost
PT23:   LD      (hl,addr(P1));          //  Get piece               //PT23:   LD      hl,P1           ; Get piece
        BIT     (7,ptr(hl));            //  Test val(COLOR)         //        BIT     7,(hl)          ; Test color
        LD      (a,d);                  //  Value of piece          //        LD      a,d             ; Value of piece
        JR      (Z,rel012);             //  Jump if white           //        JR      Z,rel012        ; Jump if white
        NEG;                            //  Negate for black        //        NEG                     ; Negate for black
rel012: LD      (hl,addr(MTRL));        //  Get addrs of material total//rel012: LD      hl,MTRL         ; Get addrs of material total
        ADD     (a,ptr(hl));            //  Add new value           //        ADD     a,(hl)          ; Add new value
        LD      (ptr(hl),a);            //  Store                   //        LD      (hl),a          ; Store
PT25:   LD      (a,val(M3));            //  Get current board position//PT25:   LD      a,(M3)          ; Get current board position
        INC     (a);                    //  Increment               //        INC     a               ; Increment
        CP      (99);                   //  At end of board ?       //        CP      99              ; At end of board ?
        JP      (NZ,PT5);               //  No - jump               //        JP      NZ,PT5          ; No - jump
        LD      (a,val(PTSCK));         //  Moving piece lost flag  //        LD      a,(PTSCK)       ; Moving piece lost flag
        AND     (a);                    //  Was it lost ?           //        AND     a               ; Was it lost ?
        JR      (Z,PT25A);              //  No - jump               //        JR      Z,PT25A         ; No - jump
        LD      (a,val(PTSW2));         //  2nd max points won      //        LD      a,(PTSW2)       ; 2nd max points won
        LD      (val(PTSW1),a);         //  Store as max points won //        LD      (PTSW1),a       ; Store as max points won
        XOR     (a);                    //  Zero out 2nd max points won//        XOR     a               ; Zero out 2nd max points won
        LD      (val(PTSW2),a);         //                          //        LD      (PTSW2),a
PT25A:  LD      (a,val(PTSL));          //  Get max points lost     //PT25A:  LD      a,(PTSL)        ; Get max points lost
        AND     (a);                    //  Is it zero ?            //        AND     a               ; Is it zero ?
        JR      (Z,rel013);             //  Yes - jump              //        JR      Z,rel013        ; Yes - jump
        DEC     (a);                    //  Decrement it            //        DEC     a               ; Decrement it
rel013: LD      (b,a);                  //  Save it                 //rel013: LD      b,a             ; Save it
        LD      (a,val(PTSW1));         //  Max,points won          //        LD      a,(PTSW1)       ; Max,points won
        AND     (a);                    //  Is it zero ?            //        AND     a               ; Is it zero ?
        JR      (Z,rel014);             //  Yes - jump              //        JR      Z,rel014        ; Yes - jump
        LD      (a,val(PTSW2));         //  2nd max points won      //        LD      a,(PTSW2)       ; 2nd max points won
        AND     (a);                    //  Is it zero ?            //        AND     a               ; Is it zero ?
        JR      (Z,rel014);             //  Yes - jump              //        JR      Z,rel014        ; Yes - jump
        DEC     (a);                    //  Decrement it            //        DEC     a               ; Decrement it
        SRL     (a);                    //  Divide it by 2          //        SRL     a               ; Divide it by 2
rel014: SUB     (b);                    //  Subtract points lost    //rel014: SUB     b               ; Subtract points lost
        LD      (hl,addr(COLOR));       //  val(COLOR) of side just moved ???//        LD      hl,COLOR        ; Color of side just moved ???
        BIT     (7,ptr(hl));            //  Is it white ?           //        BIT     7,(hl)          ; Is it white ?
        JR      (Z,rel015);             //  Yes - jump              //        JR      Z,rel015        ; Yes - jump
        NEG;                            //  Negate for black        //        NEG                     ; Negate for black
rel015: LD      (hl,addr(MTRL));        //  Net material on board   //rel015: LD      hl,MTRL         ; Net material on board
        ADD     (a,ptr(hl));            //  Add exchange adjustments//        ADD     a,(hl)          ; Add exchange adjustments
        LD      (hl,addr(MV0));         //  Material at ply 0       //        LD      hl,MV0          ; Material at ply 0
        SUB     (ptr(hl));              //  Subtract from current   //        SUB     (hl)            ; Subtract from current
        LD      (b,a);                  //  Save                    //        LD      b,a             ; Save
        LD      (a,30);                 //  Load material limit     //        LD      a,30            ; Load material limit
        CALLu   (LIMIT);                //  Limit to plus or minus value//        CALL    LIMIT           ; Limit to plus or minus value
        LD      (e,a);                  //  Save limited value      //        LD      e,a             ; Save limited value
        LD      (a,val(BRDC));          //  Get board control points//        LD      a,(BRDC)        ; Get board control points
        LD      (hl,addr(BC0));         //  Board control at ply zero//        LD      hl,BC0          ; Board control at ply zero
        SUB     (ptr(hl));              //  Get difference          //        SUB     (hl)            ; Get difference
        LD      (b,a);                  //  Save                    //        LD      b,a             ; Save
        LD      (a,val(PTSCK));         //  Moving piece lost flag  //        LD      a,(PTSCK)       ; Moving piece lost flag
        AND     (a);                    //  Is it zero ?            //        AND     a               ; Is it zero ?
        JR      (Z,rel026);             //  Yes - jump              //        JR      Z,rel026        ; Yes - jump
        LD      (b,0);                  //  Zero board control points//        LD      b,0             ; Zero board control points
rel026: LD      (a,6);                  //  Load board control limit//rel026: LD      a,6             ; Load board control limit
        CALLu   (LIMIT);                //  Limit to plus or minus value//        CALL    LIMIT           ; Limit to plus or minus value
        LD      (d,a);                  //  Save limited value      //        LD      d,a             ; Save limited value
        LD      (a,e);                  //  Get material points     //        LD      a,e             ; Get material points
        ADD     (a,a);                  //  Multiply by 4           //        ADD     a,a             ; Multiply by 4
        ADD     (a,a);                  //                          //        ADD     a,a
        ADD     (a,d);                  //  Add board control       //        ADD     a,d             ; Add board control
        LD      (hl,addr(COLOR));       //  val(COLOR) of side just moved//        LD      hl,COLOR        ; Color of side just moved
        BIT     (7,ptr(hl));            //  Is it white ?           //        BIT     7,(hl)          ; Is it white ?
        JR      (NZ,rel016);            //  No - jump               //        JR      NZ,rel016       ; No - jump
        NEG;                            //  Negate for white        //        NEG                     ; Negate for white
rel016: ADD     (a,0x80);               //  Rescale score (neutral = 80H)//rel016: ADD     a,80H           ; Rescale score (neutral = 80H)
        LD      (val(VALM),a);          //  Save score              //        LD      (VALM),a        ; Save score
        LD      (ix,val16(MLPTRJ));     //  Load move list pointer  //        LD      ix,(MLPTRJ)     ; Load move list pointer
        LD      (ptr(ix+MLVAL),a);      //  Save score in move list //        LD      (ix+MLVAL),a    ; Save score in move list
        RETu;                           //  Return                  //        RET                     ; Return
}                                                                   //
                                                                    //;***********************************************************
//***********************************************************       //; LIMIT ROUTINE
// LIMIT ROUTINE                                                    //
//***********************************************************       //
// FUNCTION:   --  To limit the magnitude of a given value          //;***********************************************************
//                 to another given value.                          //; FUNCTION:   --  To limit the magnitude of a given value
//                                                                  //;                 to another given value.
// CALLED BY:  --  POINTS                                           //;
//                                                                  //; CALLED BY:  --  POINTS
// CALLS:      --  None                                             //;
//                                                                  //; CALLS:      --  None
// ARGUMENTS:  --  Input  - Value, to be limited in the B           //;
//                          register.                               //; ARGUMENTS:  --  Input  - Value, to be limited in the B
//                        - Value to limit to in the A register     //;                          register.
//                 Output - Limited value in the A register.        //;                        - Value to limit to in the A register
//***********************************************************       //;                 Output - Limited value in the A register.
void LIMIT() {                                                      //;***********************************************************
        BIT     (7,b);                  //  Is value negative ?     //LIMIT:  BIT     7,b             ; Is value negative ?
        JP      (Z,LIM10);              //  No - jump               //        JP      Z,LIM10         ; No - jump
        NEG;                            //  Make positive           //        NEG                     ; Make positive
        CP      (b);                    //  Compare to limit        //        CP      b               ; Compare to limit
        RET     (NC);                   //  Return if outside limit //        RET     NC              ; Return if outside limit
        LD      (a,b);                  //  Output value as is      //        LD      a,b             ; Output value as is
        RETu;                           //  Return                  //        RET                     ; Return
LIM10:  CP      (b);                    //  Compare to limit        //LIM10:  CP      b               ; Compare to limit
        RET     (C);                    //  Return if outside limit //        RET     C               ; Return if outside limit
        LD      (a,b);                  //  Output value as is      //        LD      a,b             ; Output value as is
        RETu;                           //  Return                  //        RET                     ; Return
}                                                                   //
                                                                    //;***********************************************************
//***********************************************************       //; MOVE ROUTINE
// MOVE ROUTINE                                                     //
//***********************************************************       //
// FUNCTION:   --  To execute a move from the move list on the      //;***********************************************************
//                 board array.                                     //; FUNCTION:   --  To execute a move from the move list on the
//                                                                  //;                 board array.
// CALLED BY:  --  CPTRMV                                           //;
//                 PLYRMV                                           //; CALLED BY:  --  CPTRMV
//                 EVAL                                             //;                 PLYRMV
//                 FNDMOV                                           //;                 EVAL
//                 VALMOV                                           //;                 FNDMOV
//                                                                  //;                 VALMOV
// CALLS:      --  None                                             //;
//                                                                  //; CALLS:      --  None
// ARGUMENTS:  --  None                                             //;
//***********************************************************       //; ARGUMENTS:  --  None
void MOVE() {                                                       //;***********************************************************
        LD      (hl,val16(MLPTRJ));     //  Load move list pointer  //MOVE:   LD      hl,(MLPTRJ)     ; Load move list pointer
        INC     (hl);                   //  Increment past link bytes//        INC     hl              ; Increment past link bytes
        INC     (hl);                                               //        INC     hl
MV1:    LD      (a,ptr(hl));            //  "From" position         //MV1:    LD      a,(hl)          ; "From" position
        LD      (val(M1),a);            //  Save                    //        LD      (M1),a          ; Save
        INC     (hl);                   //  Increment pointer       //        INC     hl              ; Increment pointer
        LD      (a,ptr(hl));            //  "To" position           //        LD      a,(hl)          ; "To" position
        LD      (val(M2),a);            //  Save                    //        LD      (M2),a          ; Save
        INC     (hl);                   //  Increment pointer       //        INC     hl              ; Increment pointer
        LD      (d,ptr(hl));            //  Get captured piece/flags//        LD      d,(hl)          ; Get captured piece/flags
        LD      (ix,val16(M1));         //  Load "from" pos board index//        LD      ix,(M1)         ; Load "from" pos board index
        LD      (e,ptr(ix+BOARD));      //  Get piece moved         //        LD      e,(ix+BOARD)    ; Get piece moved
        BIT     (5,d);                  //  Test Pawn promotion flag//        BIT     5,d             ; Test Pawn promotion flag
        JR      (NZ,MV15);              //  Jump if set             //        JR      NZ,MV15         ; Jump if set
        LD      (a,e);                  //  Piece moved             //        LD      a,e             ; Piece moved
        AND     (7);                    //  Clear flag bits         //        AND     7               ; Clear flag bits
        CP      (QUEEN);                //  Is it a queen ?         //        CP      QUEEN           ; Is it a queen ?
        JR      (Z,MV20);               //  Yes - jump              //        JR      Z,MV20          ; Yes - jump
        CP      (KING);                 //  Is it a king ?          //        CP      KING            ; Is it a king ?
        JR      (Z,MV30);               //  Yes - jump              //        JR      Z,MV30          ; Yes - jump
MV5:    LD      (iy,val16(M2));         //  Load "to" pos board index//MV5:    LD      iy,(M2)         ; Load "to" pos board index
        SET     (3,e);                  //  Set piece moved flag    //        SET     3,e             ; Set piece moved flag
        LD      (ptr(iy+BOARD),e);      //  Insert piece at new position//        LD      (iy+BOARD),e    ; Insert piece at new position
        LD      (ptr(ix+BOARD),0);      //  Empty previous position //        LD      (ix+BOARD),0    ; Empty previous position
        BIT     (6,d);                  //  Double move ?           //        BIT     6,d             ; Double move ?
        JR      (NZ,MV40);              //  Yes - jump              //        JR      NZ,MV40         ; Yes - jump
        LD      (a,d);                  //  Get captured piece, if any//        LD      a,d             ; Get captured piece, if any
        AND     (7);                                                //        AND     7
        CP      (QUEEN);                //  Was it a queen ?        //        CP      QUEEN           ; Was it a queen ?
        RET     (NZ);                   //  No - return             //        RET     NZ              ; No - return
        LD      (hl,addr(POSQ));        //  Addr of saved Queen position//        LD      hl,POSQ         ; Addr of saved Queen position
        BIT     (7,d);                  //  Is Queen white ?        //        BIT     7,d             ; Is Queen white ?
        JR      (Z,MV10);               //  Yes - jump              //        JR      Z,MV10          ; Yes - jump
        INC     (hl);                   //  Increment to black Queen pos//        INC     hl              ; Increment to black Queen pos
MV10:   XOR     (a);                    //  Set saved position to zero//MV10:   XOR     a               ; Set saved position to zero
        LD      (ptr(hl),a);                                        //        LD      (hl),a
        RETu;                           //  Return                  //        RET                     ; Return
MV15:   SET     (2,e);                  //  Change Pawn to a Queen  //MV15:   SET     2,e             ; Change Pawn to a Queen
        JPu     (MV5);                  //  Jump                    //        JP      MV5             ; Jump
MV20:   LD      (hl,addr(POSQ));        //  Addr of saved Queen position//MV20:   LD      hl,POSQ         ; Addr of saved Queen position
MV21:   BIT     (7,e);                  //  Is Queen white ?        //MV21:   BIT     7,e             ; Is Queen white ?
        JR      (Z,MV22);               //  Yes - jump              //        JR      Z,MV22          ; Yes - jump
        INC     (hl);                   //  Increment to black Queen pos//        INC     hl              ; Increment to black Queen pos
MV22:   LD      (a,val(M2));            //  Get new Queen position  //MV22:   LD      a,(M2)          ; Get new Queen position
        LD      (ptr(hl),a);            //  Save                    //        LD      (hl),a          ; Save
        JPu     (MV5);                  //  Jump                    //        JP      MV5             ; Jump
MV30:   LD      (hl,addr(POSK));        //  Get saved King position //MV30:   LD      hl,POSK         ; Get saved King position
        BIT     (6,d);                  //  Castling ?              //        BIT     6,d             ; Castling ?
        JR      (Z,MV21);               //  No - jump               //        JR      Z,MV21          ; No - jump
        SET     (4,e);                  //  Set King castled flag   //        SET     4,e             ; Set King castled flag
        JPu     (MV21);                 //  Jump                    //        JP      MV21            ; Jump
MV40:   LD      (hl,val16(MLPTRJ));     //  Get move list pointer   //MV40:   LD      hl,(MLPTRJ)     ; Get move list pointer
        LD      (de,8);                 //  Increment to next move  //        LD      de,8            ; Increment to next move
        ADD     (hl,de);                //                          //        ADD     hl,de
        JPu     (MV1);                  //  Jump (2nd part of dbl move)//        JP      MV1             ; Jump (2nd part of dbl move)
}                                                                   //
                                                                    //;***********************************************************
//***********************************************************       //; UN-MOVE ROUTINE
// UN-MOVE ROUTINE                                                  //
//***********************************************************       //
// FUNCTION:   --  To reverse the process of the move routine,      //;***********************************************************
//                 thereby restoring the board array to its         //; FUNCTION:   --  To reverse the process of the move routine,
//                 previous position.                               //;                 thereby restoring the board array to its
//                                                                  //;                 previous position.
// CALLED BY:  --  VALMOV                                           //;
//                 EVAL                                             //; CALLED BY:  --  VALMOV
//                 FNDMOV                                           //;                 EVAL
//                 ASCEND                                           //;                 FNDMOV
//                                                                  //;                 ASCEND
// CALLS:      --  None                                             //;
//                                                                  //; CALLS:      --  None
// ARGUMENTS:  --  None                                             //;
//***********************************************************       //; ARGUMENTS:  --  None
void UNMOVE() {                                                     //;***********************************************************
        LD      (hl,val16(MLPTRJ));     //  Load move list pointer  //UNMOVE: LD      hl,(MLPTRJ)     ; Load move list pointer
        INC     (hl);                   //  Increment past link bytes//        INC     hl              ; Increment past link bytes
        INC     (hl);                                               //        INC     hl
UM1:    LD      (a,ptr(hl));            //  Get "from" position     //UM1:    LD      a,(hl)          ; Get "from" position
        LD      (val(M1),a);            //  Save                    //        LD      (M1),a          ; Save
        INC     (hl);                   //  Increment pointer       //        INC     hl              ; Increment pointer
        LD      (a,ptr(hl));            //  Get "to" position       //        LD      a,(hl)          ; Get "to" position
        LD      (val(M2),a);            //  Save                    //        LD      (M2),a          ; Save
        INC     (hl);                   //  Increment pointer       //        INC     hl              ; Increment pointer
        LD      (d,ptr(hl));            //  Get captured piece/flags//        LD      d,(hl)          ; Get captured piece/flags
        LD      (ix,val16(M2));         //  Load "to" pos board index//        LD      ix,(M2)         ; Load "to" pos board index
        LD      (e,ptr(ix+BOARD));      //  Get piece moved         //        LD      e,(ix+BOARD)    ; Get piece moved
        BIT     (5,d);                  //  Was it a Pawn promotion ?//        BIT     5,d             ; Was it a Pawn promotion ?
        JR      (NZ,UM15);              //  Yes - jump              //        JR      NZ,UM15         ; Yes - jump
        LD      (a,e);                  //  Get piece moved         //        LD      a,e             ; Get piece moved
        AND     (7);                    //  Clear flag bits         //        AND     7               ; Clear flag bits
        CP      (QUEEN);                //  Was it a Queen ?        //        CP      QUEEN           ; Was it a Queen ?
        JR      (Z,UM20);               //  Yes - jump              //        JR      Z,UM20          ; Yes - jump
        CP      (KING);                 //  Was it a King ?         //        CP      KING            ; Was it a King ?
        JR      (Z,UM30);               //  Yes - jump              //        JR      Z,UM30          ; Yes - jump
UM5:    BIT     (4,d);                  //  Is this 1st move for piece ?//UM5:    BIT     4,d             ; Is this 1st move for piece ?
        JR      (NZ,UM16);              //  Yes - jump              //        JR      NZ,UM16         ; Yes - jump
UM6:    LD      (iy,val16(M1));         //  Load "from" pos board index//UM6:    LD      iy,(M1)         ; Load "from" pos board index
        LD      (ptr(iy+BOARD),e);      //  Return to previous board pos//        LD      (iy+BOARD),e    ; Return to previous board pos
        LD      (a,d);                  //  Get captured piece, if any//        LD      a,d             ; Get captured piece, if any
        AND     (0x8f);                 //  Clear flags             //        AND     8FH             ; Clear flags
        LD      (ptr(ix+BOARD),a);      //  Return to board         //        LD      (ix+BOARD),a    ; Return to board
        BIT     (6,d);                  //  Was it a double move ?  //        BIT     6,d             ; Was it a double move ?
        JR      (NZ,UM40);              //  Yes - jump              //        JR      NZ,UM40         ; Yes - jump
        LD      (a,d);                  //  Get captured piece, if any//        LD      a,d             ; Get captured piece, if any
        AND     (7);                    //  Clear flag bits         //        AND     7               ; Clear flag bits
        CP      (QUEEN);                //  Was it a Queen ?        //        CP      QUEEN           ; Was it a Queen ?
        RET     (NZ);                   //  No - return             //        RET     NZ              ; No - return
        LD      (hl,addr(POSQ));        //  Address of saved Queen pos//        LD      hl,POSQ         ; Address of saved Queen pos
        BIT     (7,d);                  //  Is Queen white ?        //        BIT     7,d             ; Is Queen white ?
        JR      (Z,UM10);               //  Yes - jump              //        JR      Z,UM10          ; Yes - jump
        INC     (hl);                   //  Increment to black Queen pos//        INC     hl              ; Increment to black Queen pos
UM10:   LD      (a,val(M2));            //  Queen's previous position//UM10:   LD      a,(M2)          ; Queen's previous position
        LD      (ptr(hl),a);            //  Save                    //        LD      (hl),a          ; Save
        RETu;                           //  Return                  //        RET                     ; Return
UM15:   RES     (2,e);                  //  Restore Queen to Pawn   //UM15:   RES     2,e             ; Restore Queen to Pawn
        JPu     (UM5);                  //  Jump                    //        JP      UM5             ; Jump
UM16:   RES     (3,e);                  //  Clear piece moved flag  //UM16:   RES     3,e             ; Clear piece moved flag
        JPu     (UM6);                  //  Jump                    //        JP      UM6             ; Jump
UM20:   LD      (hl,addr(POSQ));        //  Addr of saved Queen position//UM20:   LD      hl,POSQ         ; Addr of saved Queen position
UM21:   BIT     (7,e);                  //  Is Queen white ?        //UM21:   BIT     7,e             ; Is Queen white ?
        JR      (Z,UM22);               //  Yes - jump              //        JR      Z,UM22          ; Yes - jump
        INC     (hl);                   //  Increment to black Queen pos//        INC     hl              ; Increment to black Queen pos
UM22:   LD      (a,val(M1));            //  Get previous position   //UM22:   LD      a,(M1)          ; Get previous position
        LD      (ptr(hl),a);            //  Save                    //        LD      (hl),a          ; Save
        JPu     (UM5);                  //  Jump                    //        JP      UM5             ; Jump
UM30:   LD      (hl,addr(POSK));        //  Address of saved King pos//UM30:   LD      hl,POSK         ; Address of saved King pos
        BIT     (6,d);                  //  Was it a castle ?       //        BIT     6,d             ; Was it a castle ?
        JR      (Z,UM21);               //  No - jump               //        JR      Z,UM21          ; No - jump
        RES     (4,e);                  //  Clear castled flag      //        RES     4,e             ; Clear castled flag
        JPu     (UM21);                 //  Jump                    //        JP      UM21            ; Jump
UM40:   LD      (hl,val16(MLPTRJ));     //  Load move list pointer  //UM40:   LD      hl,(MLPTRJ)     ; Load move list pointer
        LD      (de,8);                 //  Increment to next move  //        LD      de,8            ; Increment to next move
        ADD     (hl,de);                                            //        ADD     hl,de
        JPu     (UM1);                  //  Jump (2nd part of dbl move)//        JP      UM1             ; Jump (2nd part of dbl move)
}                                                                   //
                                                                    //;***********************************************************
//***********************************************************       //; SORT ROUTINE
// SORT ROUTINE                                                     //
//***********************************************************       //
// FUNCTION:   --  To sort the move list in order of                //;***********************************************************
//                 increasing move value scores.                    //; FUNCTION:   --  To sort the move list in order of
//                                                                  //;                 increasing move value scores.
// CALLED BY:  --  FNDMOV                                           //;
//                                                                  //; CALLED BY:  --  FNDMOV
// CALLS:      --  EVAL                                             //;
//                                                                  //; CALLS:      --  EVAL
// ARGUMENTS:  --  None                                             //;
//***********************************************************       //; ARGUMENTS:  --  None
void SORTM() {                                                      //;***********************************************************
        LD      (bc,val16(MLPTRI));     //  Move list begin pointer //SORTM:  LD      bc,(MLPTRI)     ; Move list begin pointer
        LD      (de,0);                 //  Initialize working pointers//        LD      de,0            ; Initialize working pointers
SR5:    LD      (h,b);                  //                          //SR5:    LD      h,b
        LD      (l,c);                  //                          //        LD      l,c
        LD      (c,ptr(hl));            //  Link to next move       //        LD      c,(hl)          ; Link to next move
        INC     (hl);                   //                          //        INC     hl
        LD      (b,ptr(hl));            //                          //        LD      b,(hl)
        LD      (ptr(hl),d);            //  Store to link in list   //        LD      (hl),d          ; Store to link in list
        DEC     (hl);                   //                          //        DEC     hl
        LD      (ptr(hl),e);            //                          //        LD      (hl),e
        XOR     (a);                    //  End of list ?           //        XOR     a               ; End of list ?
        CP      (b);                    //                          //        CP      b
        RET     (Z);                    //  Yes - return            //        RET     Z               ; Yes - return
SR10:   LD      (val16(MLPTRJ),bc);     //  Save list pointer       //SR10:   LD      (MLPTRJ),bc     ; Save list pointer
        CALLu   (EVAL);                 //  Evaluate move           //        CALL    EVAL            ; Evaluate move
        LD      (hl,val16(MLPTRI));     //  Begining of move list   //        LD      hl,(MLPTRI)     ; Begining of move list
        LD      (bc,val16(MLPTRJ));     //  Restore list pointer    //        LD      bc,(MLPTRJ)     ; Restore list pointer
SR15:   LD      (e,ptr(hl));            //  Next move for compare   //SR15:   LD      e,(hl)          ; Next move for compare
        INC     (hl);                   //                          //        INC     hl
        LD      (d,ptr(hl));            //                          //        LD      d,(hl)
        XOR     (a);                    //  At end of list ?        //        XOR     a               ; At end of list ?
        CP      (d);                    //                          //        CP      d
        JR      (Z,SR25);               //  Yes - jump              //        JR      Z,SR25          ; Yes - jump
        PUSH    (de);                   //  Transfer move pointer   //        PUSH    de              ; Transfer move pointer
        POP     (ix);                   //                          //        POP     ix
        LD      (a,val(VALM));          //  Get new move value      //        LD      a,(VALM)        ; Get new move value
        CP      (ptr(ix+MLVAL));        //  Less than list value ?  //        CP      (ix+MLVAL)      ; Less than list value ?
        JR      (NC,SR30);              //  No - jump               //        JR      NC,SR30         ; No - jump
SR25:   LD      (ptr(hl),b);            //  Link new move into list //SR25:   LD      (hl),b          ; Link new move into list
        DEC     (hl);                   //                          //        DEC     hl
        LD      (ptr(hl),c);            //                          //        LD      (hl),c
        JPu     (SR5);                  //  Jump                    //        JP      SR5             ; Jump
SR30:   EX      (de,hl);                //  Swap pointers           //SR30:   EX      de,hl           ; Swap pointers
        JPu     (SR15);                 //  Jump                    //        JP      SR15            ; Jump
}                                                                   //
                                                                    //;***********************************************************
//***********************************************************       //; EVALUATION ROUTINE
// EVALUATION ROUTINE                                               //
//***********************************************************       //
// FUNCTION:   --  To evaluate a given move in the move list.       //;***********************************************************
//                 It first makes the move on the board, then if    //; FUNCTION:   --  To evaluate a given move in the move list.
//                 the move is legal, it evaluates it, and then     //;                 It first makes the move on the board, then if
//                 restores the board position.                     //;                 the move is legal, it evaluates it, and then
//                                                                  //;                 restores the board position.
// CALLED BY:  --  SORT                                             //;
//                                                                  //; CALLED BY:  --  SORT
// CALLS:      --  MOVE                                             //;
//                 INCHK                                            //; CALLS:      --  MOVE
//                 PINFND                                           //;                 INCHK
//                 POINTS                                           //;                 PINFND
//                 UNMOVE                                           //;                 POINTS
//                                                                  //;                 UNMOVE
// ARGUMENTS:  --  None                                             //;
//***********************************************************       //; ARGUMENTS:  --  None
void EVAL() {                                                       //;***********************************************************
        CALLu   (MOVE);                 //  Make move on the board array//EVAL:   CALL    MOVE            ; Make move on the board array
        CALLu   (INCHK);                //  Determine if move is legal//        CALL    INCHK           ; Determine if move is legal
        AND     (a);                    //  Legal move ?            //        AND     a               ; Legal move ?
        JR      (Z,EV5);                //  Yes - jump              //        JR      Z,EV5           ; Yes - jump
        XOR     (a);                    //  Score of zero           //        XOR     a               ; Score of zero
        LD      (val(VALM),a);          //  For illegal move        //        LD      (VALM),a        ; For illegal move
        JPu     (EV10);                 //  Jump                    //        JP      EV10            ; Jump
EV5:    CALLu   (PINFND);               //  Compile pinned list     //EV5:    CALL    PINFND          ; Compile pinned list
        CALLu   (POINTS);               //  Assign points to move   //        CALL    POINTS          ; Assign points to move
EV10:   CALLu   (UNMOVE);               //  Restore board array     //EV10:   CALL    UNMOVE          ; Restore board array
        RETu;                           //  Return                  //        RET                     ; Return
}                                                                   //
                                                                    //;***********************************************************
//***********************************************************       //; FIND MOVE ROUTINE
// FIND MOVE ROUTINE                                                //
//***********************************************************       //
// FUNCTION:   --  To determine the computer's best move by         //;***********************************************************
//                 performing a depth first tree search using       //; FUNCTION:   --  To determine the computer's best move by
//                 the techniques of alpha-beta pruning.            //;                 performing a depth first tree search using
//                                                                  //;                 the techniques of alpha-beta pruning.
// CALLED BY:  --  CPTRMV                                           //;
//                                                                  //; CALLED BY:  --  CPTRMV
// CALLS:      --  PINFND                                           //;
//                 POINTS                                           //; CALLS:      --  PINFND
//                 GENMOV                                           //;                 POINTS
//                 SORTM                                            //;                 GENMOV
//                 ASCEND                                           //;                 SORTM
//                 UNMOVE                                           //;                 ASCEND
//                                                                  //;                 UNMOVE
// ARGUMENTS:  --  None                                             //;
//***********************************************************       //; ARGUMENTS:  --  None
void FNDMOV() {                                                     //;***********************************************************
        LD      (a,val(MOVENO));        //  Current move number     //FNDMOV: LD      a,(MOVENO)      ; Current move number
        CP      (1);                    //  First move ?            //        CP      1               ; First move ?
        CALL    (Z,BOOK);               //  Yes - execute book opening//        CALL    Z,BOOK          ; Yes - execute book opening
        XOR     (a);                    //  Initialize ply number to zero//        XOR     a               ; Initialize ply number to zero
        LD      (val(NPLY),a);                                      //        LD      (NPLY),a
        LD      (hl,0);                 //  Initialize best move to zero//        LD      hl,0            ; Initialize best move to zero
        LD      (val16(BESTM),hl);                                  //        LD      (BESTM),hl
        LD      (hl,addr(MLIST));       //  Initialize ply list pointers//        LD      hl,MLIST        ; Initialize ply list pointers
        LD      (val16(MLNXT),hl);                                  //        LD      (MLNXT),hl
        LD      (hl,addr(PLYIX)-2);                                 //        LD      hl,PLYIX-2
        LD      (val16(MLPTRI),hl);                                 //        LD      (MLPTRI),hl
        LD      (a,val(KOLOR));         //  Initialize val(COLOR)   //        LD      a,(KOLOR)       ; Initialize color
        LD      (val(COLOR),a);                                     //        LD      (COLOR),a
        LD      (hl,addr(SCORE));       //  Initialize score index  //        LD      hl,SCORE        ; Initialize score index
        LD      (val16(SCRIX),hl);                                  //        LD      (SCRIX),hl
        LD      (a,val(PLYMAX));        //  Get max ply number      //        LD      a,(PLYMAX)      ; Get max ply number
        ADD     (a,2);                  //  Add 2                   //        ADD     a,2             ; Add 2
        LD      (b,a);                  //  Save as counter         //        LD      b,a             ; Save as counter
        XOR     (a);                    //  Zero out score table    //        XOR     a               ; Zero out score table
back05: LD      (ptr(hl),a);                                        //back05: LD      (hl),a
        INC     (hl);                                               //        INC     hl
        DJNZ    (back05);                                           //        DJNZ    back05
        LD      (val(BC0),a);           //  Zero ply 0 board control//        LD      (BC0),a         ; Zero ply 0 board control
        LD      (val(MV0),a);           //  Zero ply 0 material     //        LD      (MV0),a         ; Zero ply 0 material
        CALLu   (PINFND);               //  Compile pin list        //        CALL    PINFND          ; Compile pin list
        CALLu   (POINTS);               //  Evaluate board at ply 0 //        CALL    POINTS          ; Evaluate board at ply 0
        LD      (a,val(BRDC));          //  Get board control points//        LD      a,(BRDC)        ; Get board control points
        LD      (val(BC0),a);           //  Save                    //        LD      (BC0),a         ; Save
        LD      (a,val(MTRL));          //  Get material count      //        LD      a,(MTRL)        ; Get material count
        LD      (val(MV0),a);           //  Save                    //        LD      (MV0),a         ; Save
FM5:    LD      (hl,addr(NPLY));        //  Address of ply counter  //FM5:    LD      hl,NPLY         ; Address of ply counter
        INC     (ptr(hl));              //  Increment ply count     //        INC     (hl)            ; Increment ply count
        XOR     (a);                    //  Initialize mate flag    //        XOR     a               ; Initialize mate flag
        LD      (val(MATEF),a);                                     //        LD      (MATEF),a
        CALLu   (GENMOV);               //  Generate list of moves  //        CALL    GENMOV          ; Generate list of moves
        LD      (a,val(NPLY));          //  Current ply counter     //        LD      a,(NPLY)        ; Current ply counter
        LD      (hl,addr(PLYMAX));      //  Address of maximum ply number//        LD      hl,PLYMAX       ; Address of maximum ply number
        CP      (ptr(hl));              //  At max ply ?            //        CP      (hl)            ; At max ply ?
        CALL    (C,SORTM);              //  No - call sort          //        CALL    C,SORTM         ; No - call sort
        LD      (hl,val16(MLPTRI));     //  Load ply index pointer  //        LD      hl,(MLPTRI)     ; Load ply index pointer
        LD      (val16(MLPTRJ),hl);     //  Save as last move pointer//        LD      (MLPTRJ),hl     ; Save as last move pointer
FM15:   LD      (hl,val16(MLPTRJ));     //  Load last move pointer  //FM15:   LD      hl,(MLPTRJ)     ; Load last move pointer
        LD      (e,ptr(hl));            //  Get next move pointer   //        LD      e,(hl)          ; Get next move pointer
        INC     (hl);                                               //        INC     hl
        LD      (d,ptr(hl));                                        //        LD      d,(hl)
        LD      (a,d);                                              //        LD      a,d
        AND     (a);                    //  End of move list ?      //        AND     a               ; End of move list ?
        JR      (Z,FM25);               //  Yes - jump              //        JR      Z,FM25          ; Yes - jump
        LD      (val16(MLPTRJ),de);     //  Save current move pointer//        LD      (MLPTRJ),de     ; Save current move pointer
        LD      (hl,val16(MLPTRI));     //  Save in ply pointer list//        LD      hl,(MLPTRI)     ; Save in ply pointer list
        LD      (ptr(hl),e);                                        //        LD      (hl),e
        INC     (hl);                                               //        INC     hl
        LD      (ptr(hl),d);                                        //        LD      (hl),d
        LD      (a,val(NPLY));          //  Current ply counter     //        LD      a,(NPLY)        ; Current ply counter
        LD      (hl,addr(PLYMAX));      //  Maximum ply number ?    //        LD      hl,PLYMAX       ; Maximum ply number ?
        CP      (ptr(hl));              //  Compare                 //        CP      (hl)            ; Compare
        JR      (C,FM18);               //  Jump if not max         //        JR      C,FM18          ; Jump if not max
        CALLu   (MOVE);                 //  Execute move on board array//        CALL    MOVE            ; Execute move on board array
        CALLu   (INCHK);                //  Check for legal move    //        CALL    INCHK           ; Check for legal move
        AND     (a);                    //  Is move legal           //        AND     a               ; Is move legal
        JR      (Z,rel017);             //  Yes - jump              //        JR      Z,rel017        ; Yes - jump
        CALLu   (UNMOVE);               //  Restore board position  //        CALL    UNMOVE          ; Restore board position
        JPu     (FM15);                 //  Jump                    //        JP      FM15            ; Jump
rel017: LD      (a,val(NPLY));          //  Get ply counter         //rel017: LD      a,(NPLY)        ; Get ply counter
        LD      (hl,addr(PLYMAX));      //  Max ply number          //        LD      hl,PLYMAX       ; Max ply number
        CP      (ptr(hl));              //  Beyond max ply ?        //        CP      (hl)            ; Beyond max ply ?
        JR      (NZ,FM35);              //  Yes - jump              //        JR      NZ,FM35         ; Yes - jump
        LD      (a,val(COLOR));         //  Get current COLOR       //        LD      a,(COLOR)       ; Get current color
        XOR     (0x80);                 //  Get opposite COLOR      //        XOR     80H             ; Get opposite color
        CALLu   (INCHK1);               //  Determine if King is in check//        CALL    INCHK1          ; Determine if King is in check
        AND     (a);                    //  In check ?              //        AND     a               ; In check ?
        JR      (Z,FM35);               //  No - jump               //        JR      Z,FM35          ; No - jump
        JPu     (FM19);                 //  Jump (One more ply for check)//        JP      FM19            ; Jump (One more ply for check)
FM18:   LD      (ix,val16(MLPTRJ));     //  Load move pointer       //FM18:   LD      ix,(MLPTRJ)     ; Load move pointer
        LD      (a,ptr(ix+MLVAL));      //  Get move score          //        LD      a,(ix+MLVAL)    ; Get move score
        AND     (a);                    //  Is it zero (illegal move) ?//        AND     a               ; Is it zero (illegal move) ?
        JR      (Z,FM15);               //  Yes - jump              //        JR      Z,FM15          ; Yes - jump
        CALLu   (MOVE);                 //  Execute move on board array//        CALL    MOVE            ; Execute move on board array
FM19:   LD      (hl,addr(COLOR));       //  Toggle val(COLOR)       //FM19:   LD      hl,COLOR        ; Toggle color
        LD      (a,0x80);                                           //        LD      a,80H
        XOR     (ptr(hl));                                          //        XOR     (hl)
        LD      (ptr(hl),a);            //  Save new val(COLOR)     //        LD      (hl),a          ; Save new color
        BIT     (7,a);                  //  Is it white ?           //        BIT     7,a             ; Is it white ?
        JR      (NZ,rel018);            //  No - jump               //        JR      NZ,rel018       ; No - jump
        LD      (hl,addr(MOVENO));      //  Increment move number   //        LD      hl,MOVENO       ; Increment move number
        INC     (ptr(hl));                                          //        INC     (hl)
rel018: LD      (hl,val16(SCRIX));      //  Load score table pointer//rel018: LD      hl,(SCRIX)      ; Load score table pointer
        LD      (a,ptr(hl));            //  Get score two plys above//        LD      a,(hl)          ; Get score two plys above
        INC     (hl);                   //  Increment to current ply//        INC     hl              ; Increment to current ply
        INC     (hl);                                               //        INC     hl
        LD      (ptr(hl),a);            //  Save score as initial value//        LD      (hl),a          ; Save score as initial value
        DEC     (hl);                   //  Decrement pointer       //        DEC     hl              ; Decrement pointer
        LD      (val16(SCRIX),hl);      //  Save it                 //        LD      (SCRIX),hl      ; Save it
        JPu     (FM5);                  //  Jump                    //        JP      FM5             ; Jump
FM25:   LD      (a,val(MATEF));         //  Get mate flag           //FM25:   LD      a,(MATEF)       ; Get mate flag
        AND     (a);                    //  Checkmate or stalemate ?//        AND     a               ; Checkmate or stalemate ?
        JR      (NZ,FM30);              //  No - jump               //        JR      NZ,FM30         ; No - jump
        LD      (a,val(CKFLG));         //  Get check flag          //        LD      a,(CKFLG)       ; Get check flag
        AND     (a);                    //  Was King in check ?     //        AND     a               ; Was King in check ?
        LD      (a,0x80);               //  Pre-set stalemate score //        LD      a,80H           ; Pre-set stalemate score
        JR      (Z,FM36);               //  No - jump (stalemate)   //        JR      Z,FM36          ; No - jump (stalemate)
        LD      (a,val(MOVENO));        //  Get move number         //        LD      a,(MOVENO)      ; Get move number
        LD      (val(PMATE),a);         //  Save                    //        LD      (PMATE),a       ; Save
        LD      (a,0xFF);               //  Pre-set checkmate score //        LD      a,0FFH          ; Pre-set checkmate score
        JPu     (FM36);                 //  Jump                    //        JP      FM36            ; Jump
FM30:   LD      (a,val(NPLY));          //  Get ply counter         //FM30:   LD      a,(NPLY)        ; Get ply counter
        CP      (1);                    //  At top of tree ?        //        CP      1               ; At top of tree ?
        RET     (Z);                    //  Yes - return            //        RET     Z               ; Yes - return
        CALLu   (ASCEND);               //  Ascend one ply in tree  //        CALL    ASCEND          ; Ascend one ply in tree
        LD      (hl,val16(SCRIX));      //  Load score table pointer//        LD      hl,(SCRIX)      ; Load score table pointer
        INC     (hl);                   //  Increment to current ply//        INC     hl              ; Increment to current ply
        INC     (hl);                   //                          //        INC     hl
        LD      (a,ptr(hl));            //  Get score               //        LD      a,(hl)          ; Get score
        DEC     (hl);                   //  Restore pointer         //        DEC     hl              ; Restore pointer
        DEC     (hl);                   //                          //        DEC     hl
        JPu     (FM37);                 //  Jump                    //        JP      FM37            ; Jump
FM35:   CALLu   (PINFND);               //  Compile pin list        //FM35:   CALL    PINFND          ; Compile pin list
        CALLu   (POINTS);               //  Evaluate move           //        CALL    POINTS          ; Evaluate move
        CALLu   (UNMOVE);               //  Restore board position  //        CALL    UNMOVE          ; Restore board position
        LD      (a,val(VALM));          //  Get value of move       //        LD      a,(VALM)        ; Get value of move
FM36:   LD      (hl,addr(MATEF));       //  Set mate flag           //FM36:   LD      hl,MATEF        ; Set mate flag
        SET     (0,ptr(hl));            //                          //        SET     0,(hl)
        LD      (hl,val16(SCRIX));      //  Load score table pointer//        LD      hl,(SCRIX)      ; Load score table pointer
FM37:                                                               //FM37:
        CP      (ptr(hl));              //  Compare to score 2 ply above//        CP      (hl)            ; Compare to score 2 ply above
        JR      (C,FM40);               //  Jump if less            //        JR      C,FM40          ; Jump if less
        JR      (Z,FM40);               //  Jump if equal           //        JR      Z,FM40          ; Jump if equal
        NEG;                            //  Negate score            //        NEG                     ; Negate score
        INC     (hl);                   //  Incr score table pointer//        INC     hl              ; Incr score table pointer
        CP      (ptr(hl));              //  Compare to score 1 ply above//        CP      (hl)            ; Compare to score 1 ply above
        JP      (C,FM15);               //  Jump if less than       //        JP      C,FM15          ; Jump if less than
        JP      (Z,FM15);               //  Jump if equal           //        JP      Z,FM15          ; Jump if equal
        LD      (ptr(hl),a);            //  Save as new score 1 ply above//        LD      (hl),a          ; Save as new score 1 ply above
        LD      (a,val(NPLY));          //  Get current ply counter //        LD      a,(NPLY)        ; Get current ply counter
        CP      (1);                    //  At top of tree ?        //        CP      1               ; At top of tree ?
        JP      (NZ,FM15);              //  No - jump               //        JP      NZ,FM15         ; No - jump
        LD      (hl,val16(MLPTRJ));     //  Load current move pointer//        LD      hl,(MLPTRJ)     ; Load current move pointer
        LD      (val16(BESTM),hl);      //  Save as best move pointer//        LD      (BESTM),hl      ; Save as best move pointer
        LD      (a,val(SCORE+1));      //  Get best move score      //        LD      a,(SCORE+1)     ; Get best move score
        CP      (0xff);                 //  Was it a checkmate ?    //        CP      0FFH            ; Was it a checkmate ?
        JP      (NZ,FM15);              //  No - jump               //        JP      NZ,FM15         ; No - jump
        LD      (hl,addr(PLYMAX));      //  Get maximum ply number  //        LD      hl,PLYMAX       ; Get maximum ply number
        DEC     (ptr(hl));              //  Subtract 2              //        DEC     (hl)            ; Subtract 2
        DEC     (ptr(hl));                                          //        DEC     (hl)
        LD      (a,val(KOLOR));         //  Get computer's val(COLOR)//        LD      a,(KOLOR)       ; Get computer's color
        BIT     (7,a);                  //  Is it white ?           //        BIT     7,a             ; Is it white ?
        RET     (Z);                    //  Yes - return            //        RET     Z               ; Yes - return
        LD      (hl,addr(PMATE));       //  Checkmate move number   //        LD      hl,PMATE        ; Checkmate move number
        DEC     (ptr(hl));              //  Decrement               //        DEC     (hl)            ; Decrement
        RETu;                           //  Return                  //        RET                     ; Return
FM40:   CALLu   (ASCEND);               //  Ascend one ply in tree  //FM40:   CALL    ASCEND          ; Ascend one ply in tree
        JPu     (FM15);                 //  Jump                    //        JP      FM15            ; Jump
}                                                                   //
                                                                    //;***********************************************************
//***********************************************************       //; ASCEND TREE ROUTINE
// ASCEND TREE ROUTINE                                              //
//***********************************************************       //
// FUNCTION:  --  To adjust all necessary parameters to             //;***********************************************************
//                ascend one ply in the tree.                       //; FUNCTION:  --  To adjust all necessary parameters to
//                                                                  //;                ascend one ply in the tree.
// CALLED BY: --  FNDMOV                                            //;
//                                                                  //; CALLED BY: --  FNDMOV
// CALLS:     --  UNMOVE                                            //;
//                                                                  //; CALLS:     --  UNMOVE
// ARGUMENTS: --  None                                              //;
//***********************************************************       //; ARGUMENTS: --  None
void ASCEND() {                                                     //;***********************************************************
        LD      (hl,addr(COLOR));       //  Toggle val(COLOR)       //ASCEND: LD      hl,COLOR        ; Toggle color
        LD      (a,0x80);               //                          //        LD      a,80H
        XOR     (ptr(hl));              //                          //        XOR     (hl)
        LD      (ptr(hl),a);            //  Save new val(COLOR)     //        LD      (hl),a          ; Save new color
        BIT     (7,a);                  //  Is it white ?           //        BIT     7,a             ; Is it white ?
        JR      (Z,rel019);             //  Yes - jump              //        JR      Z,rel019        ; Yes - jump
        LD      (hl,addr(MOVENO));      //  Decrement move number   //        LD      hl,MOVENO       ; Decrement move number
        DEC     (ptr(hl));              //                          //        DEC     (hl)
rel019: LD      (hl,val16(SCRIX));      //  Load score table index  //rel019: LD      hl,(SCRIX)      ; Load score table index
        DEC     (hl);                   //  Decrement               //        DEC     hl              ; Decrement
        LD      (val16(SCRIX),hl);      //  Save                    //        LD      (SCRIX),hl      ; Save
        LD      (hl,addr(NPLY));        //  Decrement ply counter   //        LD      hl,NPLY         ; Decrement ply counter
        DEC     (ptr(hl));              //                          //        DEC     (hl)
        LD      (hl,val16(MLPTRI));     //  Load ply list pointer   //        LD      hl,(MLPTRI)     ; Load ply list pointer
        DEC     (hl);                   //  Load pointer to move list top//        DEC     hl              ; Load pointer to move list top
        LD      (d,ptr(hl));            //                          //        LD      d,(hl)
        DEC     (hl);                   //                          //        DEC     hl
        LD      (e,ptr(hl));            //                          //        LD      e,(hl)
        LD      (val16(MLNXT),de);      //  Update move list avail ptr//        LD      (MLNXT),de      ; Update move list avail ptr
        DEC     (hl);                   //  Get ptr to next move to undo//        DEC     hl              ; Get ptr to next move to undo
        LD      (d,ptr(hl));            //                          //        LD      d,(hl)
        DEC     (hl);                   //                          //        DEC     hl
        LD      (e,ptr(hl));            //                          //        LD      e,(hl)
        LD      (val16(MLPTRI),hl);     //  Save new ply list pointer//        LD      (MLPTRI),hl     ; Save new ply list pointer
        LD      (val16(MLPTRJ),de);     //  Save next move pointer  //        LD      (MLPTRJ),de     ; Save next move pointer
        CALLu   (UNMOVE);               //  Restore board to previous ply//        CALL    UNMOVE          ; Restore board to previous ply
        RETu;                           //  Return                  //        RET                     ; Return
}                                                                   //
                                                                    //;***********************************************************
//***********************************************************       //; ONE MOVE BOOK OPENING
// ONE MOVE BOOK OPENING                                            //
// **********************************************************       //
// FUNCTION:   --  To provide an opening book of a single           //; **********************************************************
//                 move.                                            //; FUNCTION:   --  To provide an opening book of a single
//                                                                  //;                 move.
// CALLED BY:  --  FNDMOV                                           //;
//                                                                  //; CALLED BY:  --  FNDMOV
// CALLS:      --  None                                             //;
//                                                                  //; CALLS:      --  None
// ARGUMENTS:  --  None                                             //;
//***********************************************************       //; ARGUMENTS:  --  None
void BOOK() {                                                       //;***********************************************************
        POP     (af);                   //  Abort return to FNDMOV  //BOOK:   POP     af              ; Abort return to FNDMOV
        LD      (hl,addr(SCORE)+1);     //  Zero out score          //        LD      hl,SCORE+1      ; Zero out score
        LD      (ptr(hl),0);            //  Zero out score table    //        LD      (hl),0          ; Zero out score table
        LD      (hl,addr(BMOVES)-2);    //  Init best move ptr to book//        LD      hl,BMOVES-2     ; Init best move ptr to book
        LD      (val16(BESTM),hl);      //                          //        LD      (BESTM),hl
        LD      (hl,addr(BESTM));       //  Initialize address of pointer//        LD      hl,BESTM        ; Initialize address of pointer
        LD      (a,val(KOLOR));         //  Get computer's val(COLOR)//        LD      a,(KOLOR)       ; Get computer's color
        AND     (a);                    //  Is it white ?           //        AND     a               ; Is it white ?
        JR      (NZ,BM5);               //  No - jump               //        JR      NZ,BM5          ; No - jump
        LD      (a,rand());             //  Load refresh reg (random no)//        LD      a,r             ; Load refresh reg (random no)
        BIT     (0,a);                  //  Test random bit         //        BIT     0,a             ; Test random bit
        RET     (Z);                    //  Return if zero (P-K4)   //        RET     Z               ; Return if zero (P-K4)
        INC     (ptr(hl));              //  P-Q4                    //        INC     (hl)            ; P-Q4
        INC     (ptr(hl));              //                          //        INC     (hl)
        INC     (ptr(hl));              //                          //        INC     (hl)
        RETu;                           //  Return                  //        RET                     ; Return
BM5:    INC     (ptr(hl));              //  Increment to black moves//BM5:    INC     (hl)            ; Increment to black moves
        INC     (ptr(hl));              //                          //        INC     (hl)
        INC     (ptr(hl));              //                          //        INC     (hl)
        INC     (ptr(hl));              //                          //        INC     (hl)
        INC     (ptr(hl));              //                          //        INC     (hl)
        INC     (ptr(hl));              //                          //        INC     (hl)
        LD      (ix,val16(MLPTRJ));     //  Pointer to opponents 1st move//        LD      ix,(MLPTRJ)     ; Pointer to opponents 1st move
        LD      (a,ptr(ix+MLFRP));      //  Get "from" position     //        LD      a,(ix+MLFRP)    ; Get "from" position
        CP      (22);                   //  Is it a Queen Knight move ?//        CP      22              ; Is it a Queen Knight move ?
        JR      (Z,BM9);                //  Yes - Jump              //        JR      Z,BM9           ; Yes - Jump
        CP      (27);                   //  Is it a King Knight move ?//        CP      27              ; Is it a King Knight move ?
        JR      (Z,BM9);                //  Yes - jump              //        JR      Z,BM9           ; Yes - jump
        CP      (34);                   //  Is it a Queen Pawn ?    //        CP      34              ; Is it a Queen Pawn ?
        JR      (Z,BM9);                //  Yes - jump              //        JR      Z,BM9           ; Yes - jump
        RET     (C);                    //  If Queen side Pawn opening -//        RET     C               ; If Queen side Pawn opening -
// return (P-K4)                                                    //                                ; return (P-K4)
        CP      (35);                   //  Is it a King Pawn ?     //
        RET     (Z);                    //  Yes - return (P-K4)     //
BM9:    INC     (ptr(hl));              //  (P-Q4)                  //
        INC     (ptr(hl));              //                          //
        INC     (ptr(hl));              //                          //
        RETu;                           //  Return to CPTRMV        //
}                                                                   //
                                                                    //
//                                                                  //
//  Omit some Z80 User Interface code, eg                           //
//  user interface data including graphics data and text messages   //
//  Also macros                                                     //
//      CARRET, CLRSCR, PRTLIN, PRTBLK and EXIT                     //
//  and functions                                                   //
//      DRIVER and INTERR                                           //
//                                                                  //
                                                                    //
//***********************************************************       //
// COMPUTER MOVE ROUTINE                                            //;***********************************************************
//***********************************************************       //; COMPUTER MOVE ROUTINE
// FUNCTION:   --  To control the search for the computers move     //;***********************************************************
//                 and the display of that move on the board        //; FUNCTION:   --  To control the search for the computers move
//                 and in the move list.                            //;                 and the display of that move on the board
//                                                                  //;                 and in the move list.
// CALLED BY:  --  DRIVER                                           //;
//                                                                  //; CALLED BY:  --  DRIVER
// CALLS:      --  FNDMOV                                           //;
//                 FCDMAT                                           //; CALLS:      --  FNDMOV
//                 MOVE                                             //;                 FCDMAT
//                 EXECMV                                           //;                 MOVE
//                 BITASN                                           //;                 EXECMV
//                 INCHK                                            //;                 BITASN
//                                                                  //;                 INCHK
// MACRO CALLS:    PRTBLK                                           //;
//                 CARRET                                           //; MACRO CALLS:    PRTBLK
//                                                                  //;                 CARRET
// ARGUMENTS:  --  None                                             //;
//***********************************************************       //; ARGUMENTS:  --  None
void CPTRMV() {                                                     //;***********************************************************
        CALLu   (FNDMOV);               //  Select best move        //CPTRMV: CALL    FNDMOV          ; Select best move
        LD      (hl,val16(BESTM));      //  Move list pointer variable//        LD      hl,(BESTM)      ; Move list pointer variable
        LD      (val16(MLPTRJ),hl);     //  Pointer to move data    //        LD      (MLPTRJ),hl     ; Pointer to move data
        LD      (a,(val(SCORE+1)));     //  To check for mates      //        LD      a,(SCORE+1)     ; To check for mates
        CP      (1);                    //  Mate against computer ? //        CP      1               ; Mate against computer ?
        JR      (NZ,CP0C);              //  No - jump               //        JR      NZ,CP0C         ; No - jump
        LD      (c,1);                  //  Computer mate flag      //        LD      c,1             ; Computer mate flag
        CALLu   (FCDMAT);               //  Full checkmate ?        //        CALL    FCDMAT          ; Full checkmate ?
CP0C:   CALLu   (MOVE);                 //  Produce move on board array//CP0C:   CALL    MOVE            ; Produce move on board array
        CALLu   (EXECMV);               //  Make move on graphics board//        CALL    EXECMV          ; Make move on graphics board
// and return info about it                                         //                                ; and return info about it
        LD      (a,b);                  //  Special move flags      //
        AND     (a);                    //  Special ?               //
        JR      (NZ,CP10);              //  Yes - jump              //
        LD      (d,e);                  //  "To" position of the move//
        CALLu   (BITASN);               //  Convert to Ascii        //
        LD      (val16(MVEMSG+3),hl);   //  Put in move message     //
        LD      (d,c);                  //  "From" position of the move//
        CALLu   (BITASN);               //  Convert to Ascii        //
        LD      (val16(MVEMSG),hl);     //  Put in move message     //
        //PRTBLK  (MVEMSG,5);             //  Output text of move   //
        JRu     (CP1C);                 //  Jump                    //
CP10:   BIT     (1,b);                  //  King side castle ?      //
        JR      (Z,rel020);             //  No - jump               //
        //PRTBLK  (addr(O_O),5);                //  Output "O-O"    //
        JRu     (CP1C);                 //  Jump                    //
rel020: BIT     (2,b);                  //  Queen side castle ?     //
        JR      (Z,rel021);             //  No - jump               //
        //PRTBLK  (addr(O_O_O),5);              //  Output "O-O-O"  //
        JRu     (CP1C);                 //  Jump                    //
rel021: //PRTBLK  (P_PEP,5);              //  Output "PxPep" - En passant//
CP1C:   LD      (a,val(COLOR));         //  Should computer call check ?//
        LD      (b,a);                                              //
        XOR     (0x80);                 //  Toggle val(COLOR)       //
        LD      (val(COLOR),a);                                     //
        CALLu   (INCHK);                //  Check for check         //
        AND     (a);                    //  Is enemy in check ?     //
        LD      (a,b);                  //  Restore val(COLOR)      //
        LD      (val(COLOR),a);                                     //
        JR      (Z,CP24);               //  No - return             //
        CARRET();                       //  New line                //
        LD      (a,val(SCORE+1));       //  Check for player mated  //
        CP      (0xFF);                 //  Forced mate ?           //
        CALL    (NZ,TBCPMV);            //  No - Tab to computer column//
        //PRTBLK  (CKMSG,5);              //  Output "check"        //
        LD      (hl,addr(LINECT));      //  Address of screen line count//
        INC     (ptr(hl));              //  Increment for message   //
CP24:   LD      (a,val(SCORE+1));       //  Check again for mates   //
        CP      (0xFF);                 //  Player mated ?          //
        RET     (NZ);                   //  No - return             //
        LD      (c,0);                  //  Set player mate flag    //
        CALLu   (FCDMAT);               //  Full checkmate ?        //
        RETu;                           //  Return                  //
}                                                                   //
                                                                    //
//                                                                  //
//  Omit some more Z80 user interface stuff, functions              //
//  FCDMAT, TBPLCL, TBCPCL, TBPLMV and TBCPMV                       //
//                                                                  //
                                                                    //
//***********************************************************       //
// BOARD INDEX TO ASCII SQUARE NAME                                 //;***********************************************************
//***********************************************************       //; BOARD INDEX TO ASCII SQUARE NAME
// FUNCTION:   --  To translate a hexadecimal index in the          //;***********************************************************
//                 board array into an ascii description            //; FUNCTION:   --  To translate a hexadecimal index in the
//                 of the square in algebraic chess notation.       //;                 board array into an ascii description
//                                                                  //;                 of the square in algebraic chess notation.
// CALLED BY:  --  CPTRMV                                           //;
//                                                                  //; CALLED BY:  --  CPTRMV
// CALLS:      --  DIVIDE                                           //;
//                                                                  //; CALLS:      --  DIVIDE
// ARGUMENTS:  --  Board index input in register D and the          //;
//                 Ascii square name is output in register          //; ARGUMENTS:  --  Board index input in register D and the
//                 pair HL.                                         //;                 Ascii square name is output in register
//***********************************************************       //;                 pair HL.
void BITASN() {                                                     //;***********************************************************
        SUB     (a);                    //  Get ready for division  //BITASN: SUB     a               ; Get ready for division
        LD      (e,10);                 //                          //        LD      e,10
        CALLu   (DIVIDE);               //  Divide                  //        CALL    DIVIDE          ; Divide
        DEC     (d);                    //  Get rank on 1-8 basis   //        DEC     d               ; Get rank on 1-8 basis
        ADD     (a,0x60);               //  Convert file to Ascii (a-h)//        ADD     a,60H           ; Convert file to Ascii (a-h)
        LD      (l,a);                  //  Save                    //        LD      l,a             ; Save
        LD      (a,d);                  //  Rank                    //        LD      a,d             ; Rank
        ADD     (a,0x30);               //  Convert rank to Ascii (1-8)//        ADD     a,30H           ; Convert rank to Ascii (1-8)
        LD      (h,a);                  //  Save                    //        LD      h,a             ; Save
        RETu;                           //  Return                  //        RET                     ; Return
}                                                                   //
                                                                    //;***********************************************************
//                                                                  //; PLAYERS MOVE ANALYSIS
//  Omit some more Z80 user interface stuff, function               //
//  PLYRMV                                                          //
//                                                                  //
                                                                    //
//***********************************************************       //
// ASCII SQUARE NAME TO BOARD INDEX                                 //;***********************************************************
//***********************************************************       //; ASCII SQUARE NAME TO BOARD INDEX
// FUNCTION:   --  To convert an algebraic square name in           //;***********************************************************
//                 Ascii to a hexadecimal board index.              //; FUNCTION:   --  To convert an algebraic square name in
//                 This routine also checks the input for           //;                 Ascii to a hexadecimal board index.
//                 validity.                                        //;                 This routine also checks the input for
//                                                                  //;                 validity.
// CALLED BY:  --  PLYRMV                                           //;
//                                                                  //; CALLED BY:  --  PLYRMV
// CALLS:      --  MLTPLY                                           //;
//                                                                  //; CALLS:      --  MLTPLY
// ARGUMENTS:  --  Accepts the square name in register pair HL      //;
//                 and outputs the board index in register A.       //; ARGUMENTS:  --  Accepts the square name in register pair HL
//                 Register B = 0 if ok. Register B = Register      //;                 and outputs the board index in register A.
//                 A if invalid.                                    //;                 Register B = 0 if ok. Register B = Register
//***********************************************************       //;                 A if invalid.
void ASNTBI() {                                                     //;***********************************************************
        LD      (a,l);                  //  Ascii rank (1 - 8)      //ASNTBI: LD      a,l             ; Ascii rank (1 - 8)
        SUB     (0x30);                 //  Rank 1 - 8              //        SUB     30H             ; Rank 1 - 8
        CP      (1);                    //  Check lower bound       //        CP      1               ; Check lower bound
        JP      (M,AT04);               //  Jump if invalid         //        JP      M,AT04          ; Jump if invalid
        CP      (9);                    //  Check upper bound       //        CP      9               ; Check upper bound
        JR      (NC,AT04);              //  Jump if invalid         //        JR      NC,AT04         ; Jump if invalid
        INC     (a);                    //  Rank 2 - 9              //        INC     a               ; Rank 2 - 9
        LD      (d,a);                  //  Ready for multiplication//        LD      d,a             ; Ready for multiplication
        LD      (e,10);                                             //        LD      e,10
        CALLu   (MLTPLY);               //  Multiply                //        CALL    MLTPLY          ; Multiply
        LD      (a,h);                  //  Ascii file letter (a - h)//        LD      a,h             ; Ascii file letter (a - h)
        SUB     (0x40);                 //  File 1 - 8              //        SUB     40H             ; File 1 - 8
        CP      (1);                    //  Check lower bound       //        CP      1               ; Check lower bound
        JP      (M,AT04);               //  Jump if invalid         //        JP      M,AT04          ; Jump if invalid
        CP      (9);                    //  Check upper bound       //        CP      9               ; Check upper bound
        JR      (NC,AT04);              //  Jump if invalid         //        JR      NC,AT04         ; Jump if invalid
        ADD     (a,d);                  //  File+Rank(20-90)=Board index//        ADD     a,d             ; File+Rank(20-90)=Board index
        LD      (b,0);                  //  Ok flag                 //        LD      b,0             ; Ok flag
        RETu;                           //  Return                  //        RET                     ; Return
AT04:   LD      (b,a);                  //  Invalid flag            //AT04:   LD      b,a             ; Invalid flag
        RETu;                           //  Return                  //        RET                     ; Return
}                                                                   //
                                                                    //;***********************************************************
//***********************************************************       //; VALIDATE MOVE SUBROUTINE
// VALIDATE MOVE SUBROUTINE                                         //
//***********************************************************       //
// FUNCTION:   --  To check a players move for validity.            //;***********************************************************
//                                                                  //; FUNCTION:   --  To check a players move for validity.
// CALLED BY:  --  PLYRMV                                           //;
//                                                                  //; CALLED BY:  --  PLYRMV
// CALLS:      --  GENMOV                                           //;
//                 MOVE                                             //; CALLS:      --  GENMOV
//                 INCHK                                            //;                 MOVE
//                 UNMOVE                                           //;                 INCHK
//                                                                  //;                 UNMOVE
// ARGUMENTS:  --  Returns flag in register A, 0 for valid          //;
//                 and 1 for invalid move.                          //; ARGUMENTS:  --  Returns flag in register A, 0 for valid
//***********************************************************       //;                 and 1 for invalid move.
void VALMOV() {                                                     //;***********************************************************
        LD      (hl,val16(MLPTRJ));     //  Save last move pointer  //VALMOV: LD      hl,(MLPTRJ)     ; Save last move pointer
        PUSH    (hl);                   //  Save register           //        PUSH    hl              ; Save register
        LD      (a,val(KOLOR));         //  Computers val(COLOR)    //        LD      a,(KOLOR)       ; Computers color
        XOR     (0x80);                 //  Toggle val(COLOR)       //        XOR     80H             ; Toggle color
        LD      (val(COLOR),a);         //  Store                   //        LD      (COLOR),a       ; Store
        LD      (hl,addr(PLYIX)-2);     //  Load move list index    //        LD      hl,PLYIX-2      ; Load move list index
        LD      (val16(MLPTRI),hl);     //                          //        LD      (MLPTRI),hl
        LD      (hl,addr(MLIST)+1024);  //  Next available list pointer//        LD      hl,MLIST+1024   ; Next available list pointer
        LD      (val16(MLNXT),hl);      //                          //        LD      (MLNXT),hl
        CALLu   (GENMOV);               //  Generate opponents moves//        CALL    GENMOV          ; Generate opponents moves
        LD      (ix,addr(MLIST)+1024);  //  Index to start of moves //        LD      ix,MLIST+1024   ; Index to start of moves
VA5:    LD      (a,val(MVEMSG));        //  "From" position         //VA5:    LD      a,(MVEMSG)      ; "From" position
        CP      (ptr(ix+MLFRP));        //  Is it in list ?         //        CP      (ix+MLFRP)      ; Is it in list ?
        JR      (NZ,VA6);               //  No - jump               //        JR      NZ,VA6          ; No - jump
        LD      (a,val(MVEMSG+1));      //  "To" position           //        LD      a,(MVEMSG+1)    ; "To" position
        CP      (ptr(ix+MLTOP));        //  Is it in list ?         //        CP      (ix+MLTOP)      ; Is it in list ?
        JR      (Z,VA7);                //  Yes - jump              //        JR      Z,VA7           ; Yes - jump
VA6:    LD      (e,ptr(ix+MLPTR));      //  Pointer to next list move//VA6:    LD      e,(ix+MLPTR)    ; Pointer to next list move
        LD      (d,ptr(ix+MLPTR+1));    //                          //        LD      d,(ix+MLPTR+1)
        XOR     (a);                    //  At end of list ?        //        XOR     a               ; At end of list ?
        CP      (d);                    //                          //        CP      d
        JR      (Z,VA10);               //  Yes - jump              //        JR      Z,VA10          ; Yes - jump
        PUSH    (de);                   //  Move to X register      //        PUSH    de              ; Move to X register
        POP     (ix);                   //                          //        POP     ix
        JRu     (VA5);                  //  Jump                    //        JR      VA5             ; Jump
VA7:    LD      (val16(MLPTRJ),ix);     //  Save opponents move pointer//VA7:    LD      (MLPTRJ),ix     ; Save opponents move pointer
        CALLu   (MOVE);                 //  Make move on board array//        CALL    MOVE            ; Make move on board array
        CALLu   (INCHK);                //  Was it a legal move ?   //        CALL    INCHK           ; Was it a legal move ?
        AND     (a);                    //                          //        AND     a
        JR      (NZ,VA9);               //  No - jump               //        JR      NZ,VA9          ; No - jump
VA8:    POP     (hl);                   //  Restore saved register  //VA8:    POP     hl              ; Restore saved register
        RETu;                           //  Return                  //        RET                     ; Return
VA9:    CALLu   (UNMOVE);               //  Un-do move on board array//VA9:    CALL    UNMOVE          ; Un-do move on board array
VA10:   LD      (a,1);                  //  Set flag for invalid move//VA10:   LD      a,1             ; Set flag for invalid move
        POP     (hl);                   //  Restore saved register  //        POP     hl              ; Restore saved register
        LD      (val16(MLPTRJ),hl);     //  Save move pointer       //        LD      (MLPTRJ),hl     ; Save move pointer
        RETu;                           //  Return                  //        RET                     ; Return
}                                                                   //
                                                                    //;***********************************************************
//                                                                  //; ACCEPT INPUT CHARACTER
//  Omit some more Z80 user interface stuff, functions              //
//  CHARTR, PGIFND, MATED, ANALYS                                   //
//                                                                  //
                                                                    //
//***********************************************************       //
// UPDATE POSITIONS OF ROYALTY                                      //;***********************************************************
//***********************************************************       //; UPDATE POSITIONS OF ROYALTY
// FUNCTION:   --  To update the positions of the Kings             //;***********************************************************
//                 and Queen after a change of board position       //; FUNCTION:   --  To update the positions of the Kings
//                 in ANALYS.                                       //;                 and Queen after a change of board position
//                                                                  //;                 in ANALYS.
// CALLED BY:  --  ANALYS                                           //;
//                                                                  //; CALLED BY:  --  ANALYS
// CALLS:      --  None                                             //;
//                                                                  //; CALLS:      --  None
// ARGUMENTS:  --  None                                             //;
//***********************************************************       //; ARGUMENTS:  --  None
void ROYALT() {                                                     //;***********************************************************
        LD      (hl,addr(POSK));        //  Start of Royalty array  //ROYALT: LD      hl,POSK         ; Start of Royalty array
        LD      (b,4);                  //  Clear all four positions//        LD      b,4             ; Clear all four positions
back06: LD      (ptr(hl),0);            //                          //back06: LD      (hl),0
        INC     (hl);                   //                          //        INC     hl
        DJNZ    (back06);               //                          //        DJNZ    back06
        LD      (a,21);                 //  First board position    //        LD      a,21            ; First board position
RY04:   LD      (val(M1),a);            //  Set up board index      //RY04:   LD      (M1),a          ; Set up board index
        LD      (hl,addr(POSK));        //  Address of King position//        LD      hl,POSK         ; Address of King position
        LD      (ix,val16(M1));         //                          //        LD      ix,(M1)
        LD      (a,ptr(ix+BOARD));      //  Fetch board contents    //        LD      a,(ix+BOARD)    ; Fetch board contents
        BIT     (7,a);                  //  Test val(COLOR) bit     //        BIT     7,a             ; Test color bit
        JR      (Z,rel023);             //  Jump if white           //        JR      Z,rel023        ; Jump if white
        INC     (hl);                   //  Offset for black        //        INC     hl              ; Offset for black
rel023: AND     (7);                    //  Delete flags, leave piece//rel023: AND     7               ; Delete flags, leave piece
        CP      (KING);                 //  King ?                  //        CP      KING            ; King ?
        JR      (Z,RY08);               //  Yes - jump              //        JR      Z,RY08          ; Yes - jump
        CP      (QUEEN);                //  Queen ?                 //        CP      QUEEN           ; Queen ?
        JR      (NZ,RY0C);              //  No - jump               //        JR      NZ,RY0C         ; No - jump
        INC     (ptr(hl));              //  Queen position          //        INC     hl              ; Queen position
        INC     (ptr(hl));              //  Plus offset             //        INC     hl              ; Plus offset
RY08:   LD      (a,val(M1));            //  Index                   //RY08:   LD      a,(M1)          ; Index
        LD      (ptr(hl),a);            //  Save                    //        LD      (hl),a          ; Save
RY0C:   LD      (a,val(M1));            //  Current position        //RY0C:   LD      a,(M1)          ; Current position
        INC     (a);                    //  Next position           //        INC     a               ; Next position
        CP      (99);                   //  Done.?                  //        CP      99              ; Done.?
        JR      (NZ,RY04);              //  No - jump               //        JR      NZ,RY04         ; No - jump
        RETu;                           //  Return                  //        RET                     ; Return
}                                                                   //
                                                                    //;***********************************************************
//                                                                  //; SET UP EMPTY BOARD
//  Omit some more Z80 user interface stuff, functions              //
//  DSPBRD, BSETUP, INSPCE, CONVRT                                  //
//                                                                  //
                                                                    //
//***********************************************************       //
// POSITIVE INTEGER DIVISION                                        //;***********************************************************
//   inputs hi=A lo=D, divide by E                                  //; POSITIVE INTEGER DIVISION
//   output D, remainder in A                                       //;   inputs hi=A lo=D, divide by E
//***********************************************************       //;   output D, remainder in A
void DIVIDE() {                                                     //;***********************************************************
        PUSH    (bc);                   //                          //DIVIDE: PUSH    bc
        LD      (b,8);                  //                          //        LD      b,8
DD04:   SLA     (d);                    //                          //DD04:   SLA     d
        RLA     (a);                    //                          //        RLA
        SUB     (e);                    //                          //        SUB     e
        JP      (M,rel027);             //                          //        JP      M,rel027
        INC     (d);                    //                          //        INC     d
        JRu     (rel024);               //                          //        JR      rel024
rel027: ADD     (a,e);                  //                          //rel027: ADD     a,e
rel024: DJNZ    (DD04);                 //                          //rel024: DJNZ    DD04
        POP     (bc);                   //                          //        POP     bc
        RETu;                           //                          //        RET
}                                                                   //
                                                                    //;***********************************************************
//***********************************************************       //; POSITIVE INTEGER MULTIPLICATION
// POSITIVE INTEGER MULTIPLICATION                                  //
//   inputs D, E                                                    //
//   output hi=A lo=D                                               //;   inputs D, E
//***********************************************************       //;   output hi=A lo=D
void MLTPLY() {                                                     //;***********************************************************
        PUSH    (bc);                   //                          //MLTPLY: PUSH    bc
        SUB     (a);                    //                          //        SUB     a
        LD      (b,8);                  //                          //        LD      b,8
ML04:   BIT     (0,d);                  //                          //ML04:   BIT     0,d
        JR      (Z,rel025);             //                          //        JR      Z,rel025
        ADD     (a,e);                  //                          //        ADD     a,e
rel025: SRA     (a);                    //                          //rel025: SRA     a
        RR      (d);                    //                          //        RR      d
        DJNZ    (ML04);                 //                          //        DJNZ    ML04
        POP     (bc);                   //                          //        POP     bc
        RETu;                           //                          //        RET
}                                                                   //
                                                                    //;***********************************************************
//                                                                  //; SQUARE BLINKER
//  Omit some more Z80 user interface stuff, function               //
//  BLNKER                                                          //
//                                                                  //
                                                                    //
//***********************************************************       //
// EXECUTE MOVE SUBROUTINE                                          //;***********************************************************
//***********************************************************       //; EXECUTE MOVE SUBROUTINE
// FUNCTION:   --  This routine is the control routine for          //;***********************************************************
//                 MAKEMV. It checks for double moves and           //; FUNCTION:   --  This routine is the control routine for
//                 sees that they are properly handled. It          //;                 MAKEMV. It checks for double moves and
//                 sets flags in the B register for double          //;                 sees that they are properly handled. It
//                 moves:                                           //;                 sets flags in the B register for double
//                       En Passant -- Bit 0                        //;                 moves:
//                       O-O        -- Bit 1                        //;                       En Passant -- Bit 0
//                       O-O-O      -- Bit 2                        //;                       O-O        -- Bit 1
//                                                                  //;                       O-O-O      -- Bit 2
// CALLED BY:   -- PLYRMV                                           //;
//                 CPTRMV                                           //; CALLED BY:   -- PLYRMV
//                                                                  //;                 CPTRMV
// CALLS:       -- MAKEMV                                           //;
//                                                                  //; CALLS:       -- MAKEMV
// ARGUMENTS:   -- Flags set in the B register as described         //;
//                 above.                                           //; ARGUMENTS:   -- Flags set in the B register as described
//***********************************************************       //;                 above.
void EXECMV() {                                                     //;***********************************************************
        PUSH    (ix);                   //  Save registers          //EXECMV: PUSH    ix              ; Save registers
        PUSH    (af);                   //                          //        PUSH    af
        LD      (ix,val16(MLPTRJ));     //  Index into move list    //        LD      ix,(MLPTRJ)     ; Index into move list
        LD      (c,ptr(ix+MLFRP));      //  Move list "from" position//        LD      c,(ix+MLFRP)    ; Move list "from" position
        LD      (e,ptr(ix+MLTOP));      //  Move list "to" position //        LD      e,(ix+MLTOP)    ; Move list "to" position
        CALLu   (MAKEMV);               //  Produce move            //        CALL    MAKEMV          ; Produce move
        LD      (d,ptr(ix+MLFLG));      //  Move list flags         //        LD      d,(ix+MLFLG)    ; Move list flags
        LD      (b,0);                  //                          //        LD      b,0
        BIT     (6,d);                  //  Double move ?           //        BIT     6,d             ; Double move ?
        JR      (Z,EX14);               //  No - jump               //        JR      Z,EX14          ; No - jump
        LD      (de,6);                 //  Move list entry width   //        LD      de,6            ; Move list entry width
        ADD     (ix,de);                //  Increment MLPTRJ        //        ADD     ix,de           ; Increment MLPTRJ
        LD      (c,ptr(ix+MLFRP));      //  Second "from" position  //        LD      c,(ix+MLFRP)    ; Second "from" position
        LD      (e,ptr(ix+MLTOP));      //  Second "to" position    //        LD      e,(ix+MLTOP)    ; Second "to" position
        LD      (a,e);                  //  Get "to" position       //        LD      a,e             ; Get "to" position
        CP      (c);                    //  Same as "from" position ?//        CP      c               ; Same as "from" position ?
        JR      (NZ,EX04);              //  No - jump               //        JR      NZ,EX04         ; No - jump
        INC     (b);                    //  Set en passant flag     //        INC     b               ; Set en passant flag
        JRu     (EX10);                 //  Jump                    //        JR      EX10            ; Jump
EX04:   CP      (0x1A);                 //  White O-O ?             //EX04:   CP      1AH             ; White O-O ?
        JR      (NZ,EX08);              //  No - jump               //        JR      NZ,EX08         ; No - jump
        SET     (1,b);                  //  Set O-O flag            //        SET     1,b             ; Set O-O flag
        JRu     (EX10);                 //  Jump                    //        JR      EX10            ; Jump
EX08:   CP      (0x60);                 //  Black 0-0 ?             //EX08:   CP      60H             ; Black 0-0 ?
        JR      (NZ,EX0C);              //  No - jump               //        JR      NZ,EX0C         ; No - jump
        SET     (1,b);                  //  Set 0-0 flag            //        SET     1,b             ; Set 0-0 flag
        JRu     (EX10);                 //  Jump                    //        JR      EX10            ; Jump
EX0C:   SET     (2,b);                  //  Set 0-0-0 flag          //EX0C:   SET     2,b             ; Set 0-0-0 flag
EX10:   CALLu   (MAKEMV);               //  Make 2nd move on board  //EX10:   CALL    MAKEMV          ; Make 2nd move on board
EX14:   POP     (af);                   //  Restore registers       //EX14:   POP     af              ; Restore registers
        POP     (ix);                   //                          //        POP     ix
        RETu;                           //  Return                  //        RET                     ; Return
}                                                                   //
                                                                    //;***********************************************************
//                                                                  //; MAKE MOVE SUBROUTINE
