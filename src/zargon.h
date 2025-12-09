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

// Initially at least we were emulating 64K of Z80 memory
//  at some point we will probably rename this to simply
//  zargon_data or similar
struct emulated_memory {

//***********************************************************              //0026: ;***********************************************************
// TABLES SECTION                                                          //0027: ; TABLES SECTION
//***********************************************************              //0028: ;***********************************************************
//                                                                         //0029: START:
//                                                                         //0030:         ORG     START+80H
// Note that as much as possible #defines have been commented out
//  to avoid polluting the global namespace, they are collected            //         (Numbered lines of original Z80 Sargon code on the right)
//  together in zargon.cpp instead
#define TBASE 0x100     // The following tables begin at this              //0031: TBASE   EQU     START+100H
                        //  low but non-zero page boundary in
                        //  in our 64K emulated memory
uint8_t padding[TBASE];

//There are multiple tables used for fast table look ups                   //0032: ;There are multiple tables used for fast table look ups
//that are declared relative to TBASE. In each case there                  //0033: ;that are declared relative to TBASE. In each case there
//is a table (say DIRECT) and one or more variables that                   //0034: ;is a table (say DIRECT) and one or more variables that
//index into the table (say INDX2). The table is declared                  //0035: ;index into the table (say INDX2). The table is declared
//as a relative offset from the TBASE like this;                           //0036: ;as a relative offset from the TBASE like this;
//                                                                         //0037: ;
//DIRECT = .-TBASE  ;In this . is the current location                     //0038: ;DIRECT = .-TBASE  ;In this . is the current location
//                  ;($ rather than . is used in most assemblers)          //0039: ;                  ;($ rather than . is used in most assemblers)
//                                                                         //0040: ;
//The index variable is declared as;                                       //0041: ;The index variable is declared as;
//INDX2    .WORD TBASE                                                     //0042: ;INDX2    .WORD TBASE
//                                                                         //0043: ;
//TBASE itself is page aligned, for example TBASE = 100h                   //0044: ;TBASE itself is page aligned, for example TBASE = 100h
//Although 2 bytes are allocated for INDX2 the most significant            //0045: ;Although 2 bytes are allocated for INDX2 the most significant
//never changes (so in our example it's 01h). If we want                   //0046: ;never changes (so in our example it's 01h). If we want
//to index 5 bytes into DIRECT we set the low byte of INDX2                //0047: ;to index 5 bytes into DIRECT we set the low byte of INDX2
//to 5 (now INDX2 = 105h) and load IDX2 into an index                      //0048: ;to 5 (now INDX2 = 105h) and load IDX2 into an index
//register. The following sequence loads register C with                   //0049: ;register. The following sequence loads register C with
//the 5th byte of the DIRECT table (Z80 mnemonics)                         //0050: ;the 5th byte of the DIRECT table (Z80 mnemonics)
//        LD      A,5                                                      //0051: ;        LD      A,5
//        LD      [INDX2],A                                                //0052: ;        LD      [INDX2],A
//        LD      IY,[INDX2]                                               //0053: ;        LD      IY,[INDX2]
//        LD      C,[IY+DIRECT]                                            //0054: ;        LD      C,[IY+DIRECT]
//                                                                         //0055: ;
//It's a bit like the little known C trick where array[5]                  //0056: ;It's a bit like the little known C trick where array[5]
//can also be written as 5[array].                                         //0057: ;can also be written as 5[array].
//                                                                         //0058: ;
//The Z80 indexed addressing mode uses a signed 8 bit                      //0059: ;The Z80 indexed addressing mode uses a signed 8 bit
//displacement offset (here DIRECT) in the range -128                      //0060: ;displacement offset (here DIRECT) in the range -128
//to 127. Sargon needs most of this range, which explains                  //0061: ;to 127. Sargon needs most of this range, which explains
//why DIRECT is allocated 80h bytes after start and 80h                    //0062: ;why DIRECT is allocated 80h bytes after start and 80h
//bytes *before* TBASE, this arrangement sets the DIRECT                   //0063: ;bytes *before* TBASE, this arrangement sets the DIRECT
//displacement to be -80h bytes (-128 bytes). After the 24                 //0064: ;displacement to be -80h bytes (-128 bytes). After the 24
//byte DIRECT table comes the DPOINT table. So the DPOINT                  //0065: ;byte DIRECT table comes the DPOINT table. So the DPOINT
//displacement is -128 + 24 = -104. The final tables have                  //0066: ;displacement is -128 + 24 = -104. The final tables have
//positive displacements.                                                  //0067: ;positive displacements.
//                                                                         //0068: ;
//The negative displacements are not necessary in X86 where                //0069: ;The negative displacements are not necessary in X86 where
//the equivalent mov reg,[di+offset] indexed addressing                    //0070: ;the equivalent mov reg,[di+offset] indexed addressing
//is not limited to 8 bit offsets, so in the X86 port we                   //0071: ;is not limited to 8 bit offsets, so in the X86 port we
//put the first table DIRECT at the same address as TBASE,                 //0072: ;put the first table DIRECT at the same address as TBASE,
//a more natural arrangement I am sure you'll agree.                       //0073: ;a more natural arrangement I am sure you'll agree.
//                                                                         //0074: ;
//In general it seems Sargon doesn't want memory allocated                 //0075: In general it seems Sargon doesn't want memory allocated
//in the first page of memory, so we start TBASE at 100h not               //0076: ;in the first page of memory, so we start TBASE at 100h not
//at 0h. One reason is that Sargon extensively uses a trick                //0077: ;at 0h. One reason is that Sargon extensively uses a trick
//to test for a NULL pointer; it tests whether the hi byte of              //0078: ;to test for a NULL pointer; it tests whether the hi byte of
//a pointer == 0 considers this as a equivalent to testing                 //0079: ;a pointer == 0 considers this as a equivalent to testing
//whether the whole pointer == 0 (works as long as pointers                //0080: ;whether the whole pointer == 0 (works as long as pointers
//never point to page 0).                                                  //0081: ;never point to page 0).
//                                                                         //0082: ;
//Also there is an apparent bug in Sargon, such that MLPTRJ                //0083: ;Also there is an apparent bug in Sargon, such that MLPTRJ
//is left at 0 for the root node and the MLVAL for that root               //0084: ;is left at 0 for the root node and the MLVAL for that root
//node is therefore written to memory at offset 5 from 0 (so               //0085: ;node is therefore written to memory at offset 5 from 0 (so
//in page 0). It's a bit wasteful to waste a whole 256 byte                //0086: ;in page 0). It's a bit wasteful to waste a whole 256 byte
//page for this, but it is compatible with the goal of making              //0087: ;page for this, but it is compatible with the goal of making
//as few changes as possible to the inner heart of Sargon.                 //0088: ;as few changes as possible to the inner heart of Sargon.
//In the X86 port we lock the uninitialised MLPTRJ bug down                //0089: ;In the X86 port we lock the uninitialised MLPTRJ bug down
//so MLPTRJ is always set to zero and rendering the bug                    //0090: ;so MLPTRJ is always set to zero and rendering the bug
//harmless (search for MLPTRJ to find the relevant code).                  //0091: ;harmless (search for MLPTRJ to find the relevant code).
//                                                                         //0092:
//**********************************************************               //0093: ;**********************************************************
// DIRECT  --  Direction Table.  Used to determine the dir-                //0094: ; DIRECT  --  Direction Table.  Used to determine the dir-
//             ection of movement of each piece.                           //0095: ;             ection of movement of each piece.
//***********************************************************              //0096: ;***********************************************************
// #define DIRECT (addr(direct)-TBASE)                                     //0097: DIRECT  EQU     $-TBASE
int8_t direct[24] = {                                                      //0098:         DB      +09,+11,-11,-09
    +9,+11,-11,-9,                                                         //0099:         DB      +10,-10,+01,-01
    +10,-10,+1,-1,                                                         //0100:         DB      -21,-12,+08,+19
    -21,-12,+8,+19,                                                        //0101:         DB      +21,+12,-08,-19
    +21,+12,-8,-19,                                                        //0102:         DB      +10,+10,+11,+09
    +10,+10,+11,+9,                                                        //0103:         DB      -10,-10,-11,-09
    -10,-10,-11,-9
};
//***********************************************************              //0104: ;***********************************************************
// DPOINT  --  Direction Table Pointer. Used to determine                  //0105: ; DPOINT  --  Direction Table Pointer. Used to determine
//             where to begin in the direction table for any               //0106: ;             where to begin in the direction table for any
//             given piece.                                                //0107: ;             given piece.
//***********************************************************              //0108: ;***********************************************************
// #define DPOINT (addr(dpoint)-TBASE)                                     //0109: DPOINT  EQU     $-TBASE
uint8_t dpoint[7] = {                                                      //0110:         DB      20,16,8,0,4,0,0
    20,16,8,0,4,0,0                                                        //0111:
};

//***********************************************************              //0112: ;***********************************************************
// DCOUNT  --  Direction Table Counter. Used to determine                  //0113: ; DCOUNT  --  Direction Table Counter. Used to determine
//             the number of directions of movement for any                //0114: ;             the number of directions of movement for any
//             given piece.                                                //0115: ;             given piece.
//***********************************************************              //0116: ;***********************************************************
// #define DCOUNT (addr(dcount)-TBASE)                                     //0117: DCOUNT  EQU     $-TBASE
uint8_t dcount[7] = {                                                      //0118:         DB      4,4,8,4,4,8,8
    4,4,8,4,4,8,8                                                          //0119:
};

//***********************************************************              //0120: ;***********************************************************
// PVALUE  --  Point Value. Gives the point value of each                  //0121: ; PVALUE  --  Point Value. Gives the point value of each
//             piece, or the worth of each piece.                          //0122: ;             piece, or the worth of each piece.
//***********************************************************              //0123: ;***********************************************************
// #define PVALUE (addr(pvalue)-TBASE-1)  //-1 because PAWN is 1 not 0     //0124: PVALUE  EQU     $-TBASE-1
uint8_t pvalue[6] = {                                                      //0125:         DB      1,3,3,5,9,10
    1,3,3,5,9,10                                                           //0126:
};

//***********************************************************              //0127: ;***********************************************************
// PIECES  --  The initial arrangement of the first rank of                //0128: ; PIECES  --  The initial arrangement of the first rank of
//             pieces on the board. Use to set up the board                //0129: ;             pieces on the board. Use to set up the board
//             for the start of the game.                                  //0130: ;             for the start of the game.
//***********************************************************              //0131: ;***********************************************************
// #define PIECES  (addr(pieces)-TBASE)                                    //0132: PIECES  EQU     $-TBASE
uint8_t pieces[8] = {                                                      //0133:         DB      4,2,3,5,6,3,2,4
    4,2,3,5,6,3,2,4                                                        //0134:
};

//***********************************************************              //0135: ;***********************************************************
// BOARD   --  Board Array.  Used to hold the current position             //0136: ; BOARD   --  Board Array.  Used to hold the current position
//             of the board during play. The board itself                  //0137: ;             of the board during play. The board itself
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
// #define BOARD (addr(BOARDA)-TBASE)                                      //0173: BOARD   EQU     $-TBASE
uint8_t BOARDA[120];                                                       //0174: BOARDA  DS      120
                                                                           //0175:
//***********************************************************              //0176: ;***********************************************************
// ATKLIST -- Attack List. A two part array, the first                     //0177: ; ATKLIST -- Attack List. A two part array, the first
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
// #define WACT addr(ATKLST)                                               //0189: WACT    EQU     ATKLST
// #define BACT (addr(ATKLST)+7)                                           //0190: BACT    EQU     ATKLST+7
union                                                                      //0191: ATKLST  DW      0,0,0,0,0,0,0
{                                                                          //0192:
    uint16_t    ATKLST[7];
        uint8_t     wact[7];
/*    struct wact_bact
    {
        uint8_t     wact[7];
        uint8_t     bact[7];
    }; */
};

//***********************************************************              //0193: ;***********************************************************
// PLIST   --  Pinned Piece Array. This is a two part array.               //0194: ; PLIST   --  Pinned Piece Array. This is a two part array.
//             PLISTA contains the pinned piece position.                  //0195: ;             PLISTA contains the pinned piece position.
//             PLISTD contains the direction from the pinned               //0196: ;             PLISTD contains the direction from the pinned
//             piece to the attacker.                                      //0197: ;             piece to the attacker.
//***********************************************************              //0198: ;***********************************************************
// #define PLIST (addr(PLISTA)-TBASE-1)    ///TODO -1 why?                 //0199: PLIST   EQU     $-TBASE-1
// #define PLISTD (PLIST+10)                                               //0200: PLISTD  EQU     PLIST+10
uint8_t     PLISTA[10];     // pinned pieces                               //0201: PLISTA  DW      0,0,0,0,0,0,0,0,0,0
uint8_t     plistd[10];     // corresponding directions                    //0202:

//***********************************************************              //0203: ;***********************************************************
// POSK    --  Position of Kings. A two byte area, the first               //0204: ; POSK    --  Position of Kings. A two byte area, the first
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
    14,94
};
int8_t padding2 = -1;

//***********************************************************              //0215: ;***********************************************************
// SCORE   --  Score Array. Used during Alpha-Beta pruning to              //0216: ; SCORE   --  Score Array. Used during Alpha-Beta pruning to
//             hold the scores at each ply. It includes two                //0217: ;             hold the scores at each ply. It includes two
//             "dummy" entries for ply -1 and ply 0.                       //0218: ;             "dummy" entries for ply -1 and ply 0.
//***********************************************************              //0219: ;***********************************************************
uint8_t padding3[44];
uint16_t    SCORE[20] = {                                                  //0220: SCORE   DW      0,0,0,0,0,0     ;Z80 max 6 ply
    0,0,0,0,0,0,0,0,0,0,                // Z80 max 6 ply                   //0221:
    0,0,0,0,0,0,0,0,0,0                 // x86 max 20 ply
};

//***********************************************************              //0222: ;***********************************************************
// PLYIX   --  Ply Table. Contains pairs of pointers, a pair               //0223: ; PLYIX   --  Ply Table. Contains pairs of pointers, a pair
//             for each ply. The first pointer points to the               //0224: ;             for each ply. The first pointer points to the
//             top of the list of possible moves at that ply.              //0225: ;             top of the list of possible moves at that ply.
//             The second pointer points to which move in the              //0226: ;             The second pointer points to which move in the
//             list is the one currently being considered.                 //0227: ;             list is the one currently being considered.
//***********************************************************              //0228: ;***********************************************************
uint8_t padding4[2];
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
uint8_t padding5[174];                                                     //0273:         ORG     START+0
uint8_t  M1      =      0;              uint8_t tbase_hi0 = 1;             //0274: M1      DW      TBASE
uint8_t  M2      =      0;              uint8_t tbase_hi1 = 1;             //0275: M2      DW      TBASE
uint8_t  M3      =      0;              uint8_t tbase_hi2 = 1;             //0276: M3      DW      TBASE
uint8_t  M4      =      0;              uint8_t tbase_hi3 = 1;             //0277: M4      DW      TBASE
uint8_t  T1      =      0;              uint8_t tbase_hi4 = 1;             //0278: T1      DW      TBASE
uint8_t  T2      =      0;              uint8_t tbase_hi5 = 1;             //0279: T2      DW      TBASE
uint8_t  T3      =      0;              uint8_t tbase_hi6 = 1;             //0280: T3      DW      TBASE
uint8_t  INDX1   =      0;              uint8_t tbase_hi7 = 1;             //0281: INDX1   DW      TBASE
uint8_t  INDX2   =      0;              uint8_t tbase_hi8 = 1;             //0282: INDX2   DW      TBASE
uint8_t  NPINS   =      0;              uint8_t tbase_hi9 = 1;             //0283: NPINS   DW      TBASE
uint16_t MLPTRI  =      offsetof(emulated_memory,PLYIX);                   //0284: MLPTRI  DW      PLYIX
uint16_t MLPTRJ  =      0;                                                 //0285: MLPTRJ  DW      0
uint16_t SCRIX   =      0;                                                 //0286: SCRIX   DW      0
uint16_t BESTM   =      0;                                                 //0287: BESTM   DW      0
uint16_t MLLST   =      0;                                                 //0288: MLLST   DW      0
uint16_t MLNXT   =      offsetof(emulated_memory,MLIST);                   //0289: MLNXT   DW      MLIST
                                                                           //0290:
//
// 3) MISC VARIABLES
//

//***********************************************************              //0291: ;***********************************************************
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
// BC0     --  The value of board control(BRDC) at ply 0.                  //0340: ; BC0     --  The value of board control(BRDC) at ply 0.
//                                                                         //0341: ;
// MV0     --  The value of material(MTRL) at ply 0.                       //0342: ; MV0     --  The value of material(MTRL) at ply 0.
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
};
uint8_t LINECT = 0;
char MVEMSG[5]; // = {'a','1','-','a','1'};
char O_O[3];    //    = { '0', '-', '0' };
char O_O_O[5];  //  = { '0', '-', '0', '-', '0' };

//
// 4) MOVE ARRAY
//

//***********************************************************              //0377: ;***********************************************************
// MOVE LIST SECTION                                                       //0378: ; MOVE LIST SECTION
//                                                                         //0379: ;
// MLIST   --  A 2048 byte storage area for generated moves.               //0380: ; MLIST   --  A 2048 byte storage area for generated moves.
//             This area must be large enough to hold all                  //0381: ;             This area must be large enough to hold all
//             the moves for a single leg of the move tree.                //0382: ;             the moves for a single leg of the move tree.
//                                                                         //0383: ;
// MLEND   --  The address of the last available location                  //0384: ; MLEND   --  The address of the last available location
//             in the move list.                                           //0385: ;             in the move list.
//                                                                         //0386: ;
// MLPTR   --  The Move List is a linked list of individual                //0387: ; MLPTR   --  The Move List is a linked list of individual
//             moves each of which is 6 bytes in length. The               //0388: ;             moves each of which is 6 bytes in length. The
//             move list pointer(MLPTR) is the link field                  //0389: ;             move list pointer(MLPTR) is the link field
//             within a move.                                              //0390: ;             within a move.
//                                                                         //0391: ;
// MLFRP   --  The field in the move entry which gives the                 //0392: ; MLFRP   --  The field in the move entry which gives the
//             board position from which the piece is moving.              //0393: ;             board position from which the piece is moving.
//                                                                         //0394: ;
// MLTOP   --  The field in the move entry which gives the                 //0395: ; MLTOP   --  The field in the move entry which gives the
//             board position to which the piece is moving.                //0396: ;             board position to which the piece is moving.
//                                                                         //0397: ;
// MLFLG   --  A field in the move entry which contains flag               //0398: ; MLFLG   --  A field in the move entry which contains flag
//             information. The meaning of each bit is as                  //0399: ;             information. The meaning of each bit is as
//             follows:                                                    //0400: ;             follows:
//             Bit 7  --  The color of any captured piece                  //0401: ;             Bit 7  --  The color of any captured piece
//                        0 -- White                                       //0402: ;                        0 -- White
//                        1 -- Black                                       //0403: ;                        1 -- Black
//             Bit 6  --  Double move flag (set for castling and           //0404: ;             Bit 6  --  Double move flag (set for castling and
//                        en passant pawn captures)                        //0405: ;                        en passant pawn captures)
//             Bit 5  --  Pawn Promotion flag; set when pawn               //0406: ;             Bit 5  --  Pawn Promotion flag; set when pawn
//                        promotes.                                        //0407: ;                        promotes.
//             Bit 4  --  When set, this flag indicates that               //0408: ;             Bit 4  --  When set, this flag indicates that
//                        this is the first move for the                   //0409: ;                        this is the first move for the
//                        piece on the move.                               //0410: ;                        piece on the move.
//             Bit 3  --  This flag is set is there is a piece             //0411: ;             Bit 3  --  This flag is set is there is a piece
//                        captured, and that piece has moved at            //0412: ;                        captured, and that piece has moved at
//                        least once.                                      //0413: ;                        least once.
//             Bits 2-0   Describe the captured piece.  A                  //0414: ;             Bits 2-0   Describe the captured piece.  A
//                        zero value indicates no capture.                 //0415: ;                        zero value indicates no capture.
//                                                                         //0416: ;
// MLVAL   --  The field in the move entry which contains the              //0417: ; MLVAL   --  The field in the move entry which contains the
//             score assigned to the move.                                 //0418: ;             score assigned to the move.
//                                                                         //0419: ;
//***********************************************************              //0420: ;***********************************************************
uint8_t padding6[178];                                                     //0421:         ORG     START+300H
struct ML {                                                                //0422: MLIST   DS      2048
    uint16_t    MLPTR_;
    uint8_t     MLFRP_;
    uint8_t     MLTOP_;
    uint8_t     MLFLG_;
    uint8_t     MLVAL_;
}  MLIST[10000];
uint8_t MLEND;                                                             //0423: MLEND   EQU     MLIST+2040
};   //  end struct emulated_memory

// Instantiate this class to check sargon-asm-interface.h definitions
//  and regenerate them if required
class zargon_data_defs_check_and_regen
{
    public:
    zargon_data_defs_check_and_regen()
    {
        bool match =
               (BOARDA == offsetof(emulated_memory,BOARDA) )
            && (ATKLST == offsetof(emulated_memory,ATKLST) )
            && (PLISTA == offsetof(emulated_memory,PLISTA) )
            && (POSK   == offsetof(emulated_memory,POSK) )
            && (POSQ   == offsetof(emulated_memory,POSQ) )
            && (SCORE  == offsetof(emulated_memory,SCORE) )
            && (PLYIX  == offsetof(emulated_memory,PLYIX) )
            && (M1     == offsetof(emulated_memory,M1) )
            && (M2     == offsetof(emulated_memory,M2) )
            && (M3     == offsetof(emulated_memory,M3) )
            && (M4     == offsetof(emulated_memory,M4) )
            && (T1     == offsetof(emulated_memory,T1) )
            && (T2     == offsetof(emulated_memory,T2) )
            && (T3     == offsetof(emulated_memory,T3) )
            && (INDX1  == offsetof(emulated_memory,INDX1) )
            && (INDX2  == offsetof(emulated_memory,INDX2) )
            && (NPINS  == offsetof(emulated_memory,NPINS) )
            && (MLPTRI == offsetof(emulated_memory,MLPTRI) )
            && (MLPTRJ == offsetof(emulated_memory,MLPTRJ) )
            && (SCRIX  == offsetof(emulated_memory,SCRIX) )
            && (BESTM  == offsetof(emulated_memory,BESTM) )
            && (MLLST  == offsetof(emulated_memory,MLLST) )
            && (MLNXT  == offsetof(emulated_memory,MLNXT) )
            && (KOLOR  == offsetof(emulated_memory,KOLOR) )
            && (COLOR  == offsetof(emulated_memory,COLOR) )
            && (P1     == offsetof(emulated_memory,P1) )
            && (P2     == offsetof(emulated_memory,P2) )
            && (P3     == offsetof(emulated_memory,P3) )
            && (PMATE  == offsetof(emulated_memory,PMATE) )
            && (MOVENO == offsetof(emulated_memory,MOVENO) )
            && (PLYMAX == offsetof(emulated_memory,PLYMAX) )
            && (NPLY   == offsetof(emulated_memory,NPLY) )
            && (CKFLG  == offsetof(emulated_memory,CKFLG) )
            && (MATEF  == offsetof(emulated_memory,MATEF) )
            && (VALM   == offsetof(emulated_memory,VALM) )
            && (BRDC   == offsetof(emulated_memory,BRDC) )
            && (PTSL   == offsetof(emulated_memory,PTSL) )
            && (PTSW1  == offsetof(emulated_memory,PTSW1) )
            && (PTSW2  == offsetof(emulated_memory,PTSW2) )
            && (MTRL   == offsetof(emulated_memory,MTRL) )
            && (BC0    == offsetof(emulated_memory,BC0) )
            && (MV0    == offsetof(emulated_memory,MV0) )
            && (PTSCK  == offsetof(emulated_memory,PTSCK) )
            && (BMOVES == offsetof(emulated_memory,BMOVES) )
            && (LINECT == offsetof(emulated_memory,LINECT) )
            && (MVEMSG == offsetof(emulated_memory,MVEMSG) )
            && (MLIST  == offsetof(emulated_memory,MLIST) )
            && (MLEND  == offsetof(emulated_memory,MLEND) );
        if( !match )
        {
            printf( "*\n" );
            printf( "* The data offsets in sargon-asm-interface.h do not match the\n" );
            printf( "* emulated_memory data structure defined in zargon.h. Please copy\n" );
            printf( "* and paste these updated definitions into sargon-asm-interface.h\n" );
            printf( "* and rebuild\n" );
            printf( "*\n" );
            printf( "const int BOARDA = 0x%04zx; // (0x0134 in sargon_x86.asm)\n", offsetof(emulated_memory,BOARDA) );
            printf( "const int ATKLST = 0x%04zx; // (0x01ac in sargon_x86.asm)\n", offsetof(emulated_memory,ATKLST) );
            printf( "const int PLISTA = 0x%04zx; // (0x01ba in sargon_x86.asm)\n", offsetof(emulated_memory,PLISTA) );
            printf( "const int POSK   = 0x%04zx; // (0x01ce in sargon_x86.asm)\n", offsetof(emulated_memory,POSK) );
            printf( "const int POSQ   = 0x%04zx; // (0x01d0 in sargon_x86.asm)\n", offsetof(emulated_memory,POSQ) );
            printf( "const int SCORE  = 0x%04zx; // (0x0200 in sargon_x86.asm)\n", offsetof(emulated_memory,SCORE) );
            printf( "const int PLYIX  = 0x%04zx; // (0x022a in sargon_x86.asm)\n", offsetof(emulated_memory,PLYIX) );
            printf( "const int M1     = 0x%04zx; // (0x0300 in sargon_x86.asm)\n", offsetof(emulated_memory,M1) );
            printf( "const int M2     = 0x%04zx; // (0x0302 in sargon_x86.asm)\n", offsetof(emulated_memory,M2) );
            printf( "const int M3     = 0x%04zx; // (0x0304 in sargon_x86.asm)\n", offsetof(emulated_memory,M3) );
            printf( "const int M4     = 0x%04zx; // (0x0306 in sargon_x86.asm)\n", offsetof(emulated_memory,M4) );
            printf( "const int T1     = 0x%04zx; // (0x0308 in sargon_x86.asm)\n", offsetof(emulated_memory,T1) );
            printf( "const int T2     = 0x%04zx; // (0x030a in sargon_x86.asm)\n", offsetof(emulated_memory,T2) );
            printf( "const int T3     = 0x%04zx; // (0x030c in sargon_x86.asm)\n", offsetof(emulated_memory,T3) );
            printf( "const int INDX1  = 0x%04zx; // (0x030e in sargon_x86.asm)\n", offsetof(emulated_memory,INDX1) );
            printf( "const int INDX2  = 0x%04zx; // (0x0310 in sargon_x86.asm)\n", offsetof(emulated_memory,INDX2) );
            printf( "const int NPINS  = 0x%04zx; // (0x0312 in sargon_x86.asm)\n", offsetof(emulated_memory,NPINS) );
            printf( "const int MLPTRI = 0x%04zx; // (0x0314 in sargon_x86.asm)\n", offsetof(emulated_memory,MLPTRI) );
            printf( "const int MLPTRJ = 0x%04zx; // (0x0316 in sargon_x86.asm)\n", offsetof(emulated_memory,MLPTRJ) );
            printf( "const int SCRIX  = 0x%04zx; // (0x0318 in sargon_x86.asm)\n", offsetof(emulated_memory,SCRIX) );
            printf( "const int BESTM  = 0x%04zx; // (0x031a in sargon_x86.asm)\n", offsetof(emulated_memory,BESTM) );
            printf( "const int MLLST  = 0x%04zx; // (0x031c in sargon_x86.asm)\n", offsetof(emulated_memory,MLLST) );
            printf( "const int MLNXT  = 0x%04zx; // (0x031e in sargon_x86.asm)\n", offsetof(emulated_memory,MLNXT) );
            printf( "const int KOLOR  = 0x%04zx; // (0x0320 in sargon_x86.asm)\n", offsetof(emulated_memory,KOLOR) );
            printf( "const int COLOR  = 0x%04zx; // (0x0321 in sargon_x86.asm)\n", offsetof(emulated_memory,COLOR) );
            printf( "const int P1     = 0x%04zx; // (0x0322 in sargon_x86.asm)\n", offsetof(emulated_memory,P1) );
            printf( "const int P2     = 0x%04zx; // (0x0323 in sargon_x86.asm)\n", offsetof(emulated_memory,P2) );
            printf( "const int P3     = 0x%04zx; // (0x0324 in sargon_x86.asm)\n", offsetof(emulated_memory,P3) );
            printf( "const int PMATE  = 0x%04zx; // (0x0325 in sargon_x86.asm)\n", offsetof(emulated_memory,PMATE) );
            printf( "const int MOVENO = 0x%04zx; // (0x0326 in sargon_x86.asm)\n", offsetof(emulated_memory,MOVENO) );
            printf( "const int PLYMAX = 0x%04zx; // (0x0327 in sargon_x86.asm)\n", offsetof(emulated_memory,PLYMAX) );
            printf( "const int NPLY   = 0x%04zx; // (0x0328 in sargon_x86.asm)\n", offsetof(emulated_memory,NPLY) );
            printf( "const int CKFLG  = 0x%04zx; // (0x0329 in sargon_x86.asm)\n", offsetof(emulated_memory,CKFLG) );
            printf( "const int MATEF  = 0x%04zx; // (0x032a in sargon_x86.asm)\n", offsetof(emulated_memory,MATEF) );
            printf( "const int VALM   = 0x%04zx; // (0x032b in sargon_x86.asm)\n", offsetof(emulated_memory,VALM) );
            printf( "const int BRDC   = 0x%04zx; // (0x032c in sargon_x86.asm)\n", offsetof(emulated_memory,BRDC) );
            printf( "const int PTSL   = 0x%04zx; // (0x032d in sargon_x86.asm)\n", offsetof(emulated_memory,PTSL) );
            printf( "const int PTSW1  = 0x%04zx; // (0x032e in sargon_x86.asm)\n", offsetof(emulated_memory,PTSW1) );
            printf( "const int PTSW2  = 0x%04zx; // (0x032f in sargon_x86.asm)\n", offsetof(emulated_memory,PTSW2) );
            printf( "const int MTRL   = 0x%04zx; // (0x0330 in sargon_x86.asm)\n", offsetof(emulated_memory,MTRL) );
            printf( "const int BC0    = 0x%04zx; // (0x0331 in sargon_x86.asm)\n", offsetof(emulated_memory,BC0) );
            printf( "const int MV0    = 0x%04zx; // (0x0332 in sargon_x86.asm)\n", offsetof(emulated_memory,MV0) );
            printf( "const int PTSCK  = 0x%04zx; // (0x0333 in sargon_x86.asm)\n", offsetof(emulated_memory,PTSCK) );
            printf( "const int BMOVES = 0x%04zx; // (0x0334 in sargon_x86.asm)\n", offsetof(emulated_memory,BMOVES) );
            printf( "const int LINECT = 0x%04zx; // (0x0340 in sargon_x86.asm)\n", offsetof(emulated_memory,LINECT) );
            printf( "const int MVEMSG = 0x%04zx; // (0x0341 in sargon_x86.asm)\n", offsetof(emulated_memory,MVEMSG) );
            printf( "const int MLIST  = 0x%04zx; // (0x0400 in sargon_x86.asm)\n", offsetof(emulated_memory,MLIST) );
            printf( "const int MLEND  = 0x%04zx; // (0xee60 in sargon_x86.asm)\n", offsetof(emulated_memory,MLEND) );
            exit(0);
        }
    }
};

#endif // ZARGON_H_INCLUDED

