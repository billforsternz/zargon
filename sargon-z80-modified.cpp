//***********************************************************
;
//               SARGON
;
//       Sargon is a computer chess playing program designed
// and coded by Dan and Kathe Spracklen.  Copyright 1978. All
// rights reserved.  No part of this publication may be
// reproduced without the prior written permission.
//***********************************************************


//***********************************************************
// EQUATES
//***********************************************************
;
#define PAWN    1
#define KNIGHT  2
#define BISHOP  3
#define ROOK    4
#define QUEEN   5
#define KING    6
#define WHITE   0
#define BLACK   0x80
#define BPAWN   BLACK+PAWN

//
// 1) TABLES
//
//***********************************************************
// TABLES SECTION
//***********************************************************

//There are multiple tables used for fast table look ups
//that are declared relative to TBASE. In each case there
//is a table (say DIRECT) and one or more variables that
//index into the table (say INDX2). The table is declared
//as a relative offset from the TBASE like this;
;
//DIRECT = .-TBASE  ;In this . is the current location
//                  ;($ rather than . is used in most assemblers)
;
//The index variable is declared as;
//INDX2    .WORD TBASE
;
//TBASE itself is page aligned, for example TBASE = 100h
//Although 2 bytes are allocated for INDX2 the most significant
//never changes (so in our example it's 01h). If we want
//to index 5 bytes into DIRECT we set the low byte of INDX2
//to 5 (now INDX2 = 105h) and load IDX2 into an index
//register. The following sequence loads register C with
//the 5th byte of the DIRECT table (Z80 mnemonics)
//        LD      A,5
//        LD      [INDX2],A
//        LD      IY,INDX2
//        LD      C,[IY+DIRECT]
;
//It's a bit like the little known C trick where array[5]
//can also be written as 5[array].
;
//The Z80 indexed addressing mode uses a signed 8 bit
//displacement offset (here DIRECT) in the range -128
//to 127. Sargon needs most of this range, which explains
//why DIRECT is allocated 80h bytes after start and 80h
//bytes *before* TBASE, this arrangement sets the DIRECT
//displacement to be -80h bytes (-128 bytes). After the 24
//byte DIRECT table comes the DPOINT table. So the DPOINT
//displacement is -128 + 24 = -104. The final tables have
//positive displacements.
;
//The negative displacements are not necessary in X86 where
//the equivalent mov reg,[di+offset] indexed addressing
//is not limited to 8 bit offsets, so in the X86 port we
//put the first table DIRECT at the same address as TBASE,
//a more natural arrangement I am sure you'll agree.
;
//In general it seems Sargon doesn't want memory allocated
//in the first page of memory, so we start TBASE at 100h not
//at 0h. One reason is that Sargon extensively uses a trick
//to test for a NULL pointer; it tests whether the hi byte of
//a pointer == 0 considers this as a equivalent to testing
//whether the whole pointer == 0 (works as long as pointers
//never point to page 0).
;
//Also there is an apparent bug in Sargon, such that MLPTRJ
//is left at 0 for the root node and the MLVAL for that root
//node is therefore written to memory at offset 5 from 0 (so
//in page 0). It's a bit wasteful to waste a whole 256 byte
//page for this, but it is compatible with the goal of making
//as few changes as possible to the inner heart of Sargon.
//In the X86 port we lock the uninitialised MLPTRJ bug down
//so MLPTRJ is always set to zero and rendering the bug
//harmless (search for MLPTRJ to find the relevant code).

//**********************************************************
// DIRECT  --  Direction Table.  Used to determine the dir-
//             ection of movement of each piece.
//***********************************************************
//DIRECT  EQU     $-TBASE
uint8_t DIRECT[] = {
	+9,+11,-11,-9,
	+10,-10,+1,-1,
	-21,-12,+08,+19,
	+21,+12,-08,-19,
	+10,+10,+11,+9,
	-10,-10,-11,-9
};

//***********************************************************
// DPOINT  --  Direction Table Pointer. Used to determine
//             where to begin in the direction table for any
//             given piece.
//***********************************************************
//DPOINT  EQU     $-TBASE
uint8_t DPOINT[] = {
	20,16,8,0,4,0,0
};

//***********************************************************
// DCOUNT  --  Direction Table Counter. Used to determine
//             the number of directions of movement for any
//             given piece.
//***********************************************************
DCOUNT  EQU     $-TBASE
uint8_t DCOUNT[] = {
    4,4,8,4,4,8,8
};
//***********************************************************
// PVALUE  --  Point Value. Gives the point value of each
//             piece, or the worth of each piece.
//***********************************************************
PVALUE  EQU     $-TBASE-1
uint8_t PVALUE[] = {
	1,3,3,5,9,10
};

//***********************************************************
// PIECES  --  The initial arrangement of the first rank of
//             pieces on the board. Use to set up the board
//             for the start of the game.
//***********************************************************
PIECES  EQU     $-TBASE
uint8_t PIECES[] = {
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
BOARD   EQU     $-TBASE
uint8_t BOARD[120];                     //

//***********************************************************
// ATKLIST -- Attack List. A two part array, the first
//            half for white and the second half for black.
//            It is used to hold the attackers of any given
//            square in the order of their value.
;
// WACT   --  White Attack Count. This is the first
//            byte of the array and tells how many pieces are
//            in the white portion of the attack list.
;
// BACT   --  Black Attack Count. This is the eighth byte of
//            the array and does the same for black.
//***********************************************************
WACT    EQU     ATKLST
BACT    EQU     ATKLST+7
ATKLST  DW      0,0,0,0,0,0,0
uint8_t		WACT[7];                       //
uint8_t		BACT[7];                       //

//***********************************************************
// PLIST   --  Pinned Piece Array. This is a two part array.
//             PLISTA contains the pinned piece position.
//             PLISTD contains the direction from the pinned
//             piece to the attacker.
//***********************************************************
PLIST   EQU     $-TBASE-1
PLISTD  EQU     PLIST+10
PLISTA  DW      0,0,0,0,0,0,0,0,0,0
uint8_t		PLISTA[10];		// pinned pieces
uint8_t		PLISTD[10];		// corresponding directions

//***********************************************************
// POSK    --  Position of Kings. A two byte area, the first
//             byte of which hold the position of the white
//             king and the second holding the position of
//             the black king.
;
// POSQ    --  Position of Queens. Like POSK,but for queens.
//***********************************************************
POSK
uint8_t		POSK[] = {
	24,95
};
POSQ
uint8_t		POSQ[] = {
	14,94,
	-1
};

//***********************************************************
// SCORE   --  Score Array. Used during Alpha-Beta pruning to
//             hold the scores at each ply. It includes two
//             "dummy" entries for ply -1 and ply 0.
//***********************************************************
SCORE
uint16_t	SCORE[] = {
	0,0,0,0,0,0;                           // Z80 max 6 ply
};

//***********************************************************
// PLYIX   --  Ply Table. Contains pairs of pointers, a pair
//             for each ply. The first pointer points to the
//             top of the list of possible moves at that ply.
//             The second pointer points to which move in the
//             list is the one currently being considered.
//***********************************************************
PLYIX
uint16_t	PLYIX[] = {
	0,0,0,0,0,0,0,0,0,0
	0,0,0,0,0,0,0,0,0,0
};

//
// 2) PTRS to TABLES
//

//***********************************************************
// 2) TABLE INDICES SECTION
;
// M1-M4   --  Working indices used to index into
//             the board array.
;
// T1-T3   --  Working indices used to index into Direction
//             Count, Direction Value, and Piece Value tables.
;
// INDX1   --  General working indices. Used for various
// INDX2       purposes.
;
// NPINS   --  Number of Pins. Count and pointer into the
//             pinned piece list.
;
// MLPTRI  --  Pointer into the ply table which tells
//             which pair of pointers are in current use.
;
// MLPTRJ  --  Pointer into the move list to the move that is
//             currently being processed.
;
// SCRIX   --  Score Index. Pointer to the score table for
//             the ply being examined.
;
// BESTM   --  Pointer into the move list for the move that
//             is currently considered the best by the
//             Alpha-Beta pruning process.
;
// MLLST   --  Pointer to the previous move placed in the move
//             list. Used during generation of the move list.
;
// MLNXT   --  Pointer to the next available space in the move
//             list.
;
//***********************************************************
uint16_t M1      =      TBASE;          //
uint16_t M2      =      TBASE;          //
uint16_t M3      =      TBASE;          //
uint16_t M4      =      TBASE;          //
uint16_t T1      =      TBASE;          //
uint16_t T2      =      TBASE;          //
uint16_t T3      =      TBASE;          //
uint16_t INDX1   =      TBASE;          //
uint16_t INDX2   =      TBASE;          //
uint16_t NPINS   =      TBASE;          //
uint16_t MLPTRI  =      PLYIX;          //
uint16_t MLPTRJ  =      0;              //
uint16_t SCRIX   =      0;              //
uint16_t BESTM   =      0;              //
uint16_t MLLST   =      0;              //
uint16_t MLNXT   =      MLIST;          //

//
// 3) MISC VARIABLES
//

//***********************************************************
// VARIABLES SECTION
;
// KOLOR   --  Indicates computer's color. White is 0, and
//             Black is 80H.
;
// COLOR   --  Indicates color of the side with the move.
;
// P1-P3   --  Working area to hold the contents of the board
//             array for a given square.
;
// PMATE   --  The move number at which a checkmate is
//             discovered during look ahead.
;
// MOVENO  --  Current move number.
;
// PLYMAX  --  Maximum depth of search using Alpha-Beta
//             pruning.
;
// NPLY    --  Current ply number during Alpha-Beta
//             pruning.
;
// CKFLG   --  A non-zero value indicates the king is in check.
;
// MATEF   --  A zero value indicates no legal moves.
;
// VALM    --  The score of the current move being examined.
;
// BRDC    --  A measure of mobility equal to the total number
//             of squares white can move to minus the number
//             black can move to.
;
// PTSL    --  The maximum number of points which could be lost
//             through an exchange by the player not on the
//             move.
;
// PTSW1   --  The maximum number of points which could be won
//             through an exchange by the player not on the
//             move.
;
// PTSW2   --  The second highest number of points which could
//             be won through a different exchange by the player
//             not on the move.
;
// MTRL    --  A measure of the difference in material
//             currently on the board. It is the total value of
//             the white pieces minus the total value of the
//             black pieces.
;
// BC0     --  The value of board control(BRDC) at ply 0.
;
// MV0     --  The value of material(MTRL) at ply 0.
;
// PTSCK   --  A non-zero value indicates that the piece has
//             just moved itself into a losing exchange of
//             material.
;
// BMOVES  --  Our very tiny book of openings. Determines
//             the first move for the computer.
;
//***********************************************************
uint8_t	KOLOR   =      0;               //
uint8_t	COLOR   =      0;               //
uint8_t	P1      =      0;               //
uint8_t	P2      =      0;               //
uint8_t	P3      =      0;               //
uint8_t	PMATE   =      0;               //
uint8_t	MOVENO  =      0;               //
uint8_t	PLYMAX  =      2;               //
uint8_t	NPLY    =      0;               //
uint8_t	CKFLG   =      0;               //
uint8_t	MATEF   =      0;               //
uint8_t	VALM    =      0;               //
uint8_t	BRDC    =      0;               //
uint8_t	PTSL    =      0;               //
uint8_t	PTSW1   =      0;               //
uint8_t	PTSW2   =      0;               //
uint8_t	MTRL    =      0;               //
uint8_t	BC0     =      0;               //
uint8_t	MV0     =      0;               //
uint8_t	PTSCK   =      0;               //
uint8_t BMOVES[] = { 35,55,0x10,
	34,54,0x10,
	85,65,0x10,
	84,64,0x10,
};

//
// 4) MOVE ARRAY
//

//***********************************************************
// MOVE LIST SECTION
;
// MLIST   --  A 2048 byte storage area for generated moves.
//             This area must be large enough to hold all
//             the moves for a single leg of the move tree.
;
// MLEND   --  The address of the last available location
//             in the move list.
;
// MLPTR   --  The Move List is a linked list of individual
//             moves each of which is 6 bytes in length. The
//             move list pointer(MLPTR) is the link field
//             within a move.
;
// MLFRP   --  The field in the move entry which gives the
//             board position from which the piece is moving.
;
// MLTOP   --  The field in the move entry which gives the
//             board position to which the piece is moving.
;
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
;
// MLVAL   --  The field in the move entry which contains the
//             score assigned to the move.
;
//***********************************************************
struct ML {
	uint16_t	MLPTR;                        //
	uint8_t		MLFRP;                        //
	uint8_t		MLTOP;                        //
	uint8_t		MLFLG;                        //
	uint8_t		MLVAL;                        //
};
ML MLIST[340];                          //

//***********************************************************

//**********************************************************
// PROGRAM CODE SECTION
//**********************************************************

;
// Miscellaneous stubs
;
FCDMAT:  RET
TBCPMV:  RET
MAKEMV:  RET
PRTBLK   MACRO   name,len
         ENDM
CARRET   MACRO
         ENDM

//**********************************************************
// BOARD SETUP ROUTINE
//***********************************************************
// FUNCTION:   To initialize the board array, setting the
//             pieces in their initial positions for the
//             start of the game.
;
// CALLED BY:  DRIVER
;
// CALLS:      None
;
// ARGUMENTS:  None
//***********************************************************
INITBD: LD      (b,120);                //  Pre-fill board with -1's
        LD      (hl,addr(BOARDA));
back01: LD      (ptr(hl),-1);
        INC     (hl);
        DJNZ    (back01);
        LD      (b,8);
        LD      (ix,addr(BOARDA));
IB2:    LD      (a,ptr(ix-8));          //  Fill non-border squares
        LD      (ptr(ix+21),a);         //  White pieces
        SET     (7,a);                  //  Change to black
        LD      (ptr(ix+91),a);         //  Black pieces
        LD      (ptr(ix+31),PAWN);      //  White Pawns
        LD      (ptr(ix+81),BPAWN);     //  Black Pawns
        LD      (ptr(ix+41),0);         //  Empty squares
        LD      (ptr(ix+51),0);
        LD      (ptr(ix+61),0);
        LD      (ptr(ix+71),0);
        INC     (ix);
        DJNZ    (IB2);
        LD      (ix,addr(POSK));        //  Init King/Queen position list
        LD      (ptr(ix+0),25);
        LD      (ptr(ix+1),95);
        LD      (ix,addr(POSQ));
        LD      (ptr(ix+0),24);
        LD      (ptr(ix+1),94);
        RET

//***********************************************************
// PATH ROUTINE
//***********************************************************
// FUNCTION:   To generate a single possible move for a given
//             piece along its current path of motion including:

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
;
// CALLED BY:  MPIECE
//             ATTACK
//             PINFND
;
// CALLS:      None
;
// ARGUMENTS:  Direction from the direction array giving the
//             constant to be added for the new position.
//***********************************************************
PATH:   LD      (hl,addr(M2));          //  Get previous position
        LD      (a,ptr(hl));            //
        ADD     (a,c);                  //  Add direction constant
        LD      (ptr(hl),a);            //  Save new position
        LD      (ix,chk(M2));           //  Load board index
        LD      (a,ptr(ix+BOARD));      //  Get contents of board
        CP      (-1);                   //  In border area ?
        JR      (Z,PA2);                //  Yes - jump
        LD      (P2,a);                 //  Save piece
        AND     (7);                    //  Clear flags
        LD      (T2,a);                 //  Save piece type
        RET     (Z);                    //  Return if empty
        LD      (a,P2);                 //  Get piece encountered
        LD      (hl,addr(P1));          //  Get moving piece address
        XOR     (ptr(hl));              //  Compare
        BIT     (7,a);                  //  Do colors match ?
        JR      (Z,PA1);                //  Yes - jump
        LD      (a,1);                  //  Set different color flag
        RET;                            //  Return
PA1:    LD      (a,2);                  //  Set same color flag
        RET;                            //  Return
PA2:    LD      (a,3);                  //  Set off board flag
        RET;                            //  Return

//***********************************************************
// PIECE MOVER ROUTINE
//***********************************************************
// FUNCTION:   To generate all the possible legal moves for a
//             given piece.
;
// CALLED BY:  GENMOV
;
// CALLS:      PATH
//             ADMOVE
//             CASTLE
//             ENPSNT
;
// ARGUMENTS:  The piece to be moved.
//***********************************************************
MPIECE: XOR     (ptr(hl));              //  Piece to move
        AND     (0x87);                 //  Clear flag bit
        CP      (BPAWN);                //  Is it a black Pawn ?
        JR      (NZ,rel001);            //  No-Skip
        DEC     (a);                    //  Decrement for black Pawns
rel001: AND     (7);                    //  Get piece type
        LD      (T1,a);                 //  Save piece type
        LD      (iy,chk(T1));           //  Load index to DCOUNT/DPOINT
        LD      (b,ptr(iy+DCOUNT));     //  Get direction count
        LD      (a,ptr(iy+DPOINT));     //  Get direction pointer
        LD      (INDX2,a);              //  Save as index to direct
        LD      (iy,chk(INDX2));        //  Load index
MP5:    LD      (c,ptr(iy+DIRECT));     //  Get move direction
        LD      (a,M1);                 //  From position
        LD      (M2,a);                 //  Initialize to position
MP10:   CALL    (PATH);                 //  Calculate next position
        CP      (2);                    //  Ready for new direction ?
        JR      (NC,MP15);              //  Yes - Jump
        AND     (a);                    //  Test for empty square
        Z80_EXAF;                       //  Save result
        LD      (a,T1);                 //  Get piece moved
        CP      (PAWN+1);               //  Is it a Pawn ?
        JR      (C,MP20);               //  Yes - Jump
        CALL    (ADMOVE);               //  Add move to list
        Z80_EXAF;                       //  Empty square ?
        JR      (NZ,MP15);              //  No - Jump
        LD      (a,T1);                 //  Piece type
        CP      (KING);                 //  King ?
        JR      (Z,MP15);               //  Yes - Jump
        CP      (BISHOP);               //  Bishop, Rook, or Queen ?
        JR      (NC,MP10);              //  Yes - Jump
MP15:   INC     (iy);                   //  Increment direction index
        DJNZ    (MP5);                  //  Decr. count-jump if non-zerc
        LD      (a,T1);                 //  Piece type
        CP      (KING);                 //  King ?
        CALL    (Z,CASTLE);             //  Yes - Try Castling
        RET;                            //  Return
// ***** PAWN LOGIC *****
MP20:   LD      (a,b);                  //  Counter for direction
        CP      (3);                    //  On diagonal moves ?
        JR      (C,MP35);               //  Yes - Jump
        JR      (Z,MP30);               //  -or-jump if on 2 square move
        Z80_EXAF;                       //  Is forward square empty?
        JR      (NZ,MP15);              //  No - jump
        LD      (a,M2);                 //  Get "to" position
        CP      (91);                   //  Promote white Pawn ?
        JR      (NC,MP25);              //  Yes - Jump
        CP      (29);                   //  Promote black Pawn ?
        JR      (NC,MP26);              //  No - Jump
MP25:   LD      (hl,addr(P2));          //  Flag address
        SET     (5,ptr(hl));            //  Set promote flag
MP26:   CALL    (ADMOVE);               //  Add to move list
        INC     (iy);                   //  Adjust to two square move
        DEC     (b);                    //
        LD      (hl,addr(P1));          //  Check Pawn moved flag
        BIT     (3,ptr(hl));            //  Has it moved before ?
        JR      (Z,MP10);               //  No - Jump
        JP      (MP15);                 //  Jump
MP30:   Z80_EXAF;                       //  Is forward square empty ?
        JR      (NZ,MP15);              //  No - Jump
MP31:   CALL    (ADMOVE);               //  Add to move list
        JP      (MP15);                 //  Jump
MP35:   Z80_EXAF;                       //  Is diagonal square empty ?
        JR      (Z,MP36);               //  Yes - Jump
        LD      (a,M2);                 //  Get "to" position
        CP      (91);                   //  Promote white Pawn ?
        JR      (NC,MP37);              //  Yes - Jump
        CP      (29);                   //  Black Pawn promotion ?
        JR      (NC,MP31);              //  No- Jump
MP37:   LD      (hl,addr(P2));          //  Get flag address
        SET     (5,ptr(hl));            //  Set promote flag
        JR      (MP31);                 //  Jump
MP36:   CALL    (ENPSNT);               //  Try en passant capture
        JP      (MP15);                 //  Jump

//***********************************************************
// EN PASSANT ROUTINE
//***********************************************************
// FUNCTION:   --  To test for en passant Pawn capture and
//                 to add it to the move list if it is
//                 legal.
;
// CALLED BY:  --  MPIECE
;
// CALLS:      --  ADMOVE
//                 ADJPTR
;
// ARGUMENTS:  --  None
//***********************************************************
ENPSNT: LD      (a,M1);                 //  Set position of Pawn
        LD      (hl,addr(P1));          //  Check color
        BIT     (7,ptr(hl));            //  Is it white ?
        JR      (Z,rel002);             //  Yes - skip
        ADD     (a,10);                 //  Add 10 for black
rel002: CP      (61);                   //  On en passant capture rank ?
        RET     (C);                    //  No - return
        CP      (69);                   //  On en passant capture rank ?
        RET     (NC);                   //  No - return
        LD      (ix,chk(MLPTRJ)));      //  Get pointer to previous move
        BIT     (4,ptr(ix+MLFLG));      //  First move for that piece ?
        RET     (Z);                    //  No - return
        LD      (a,ptr(ix+MLTOP));      //  Get "to" position
        LD      (M4,a);                 //  Store as index to board
        LD      (ix,chk(M4));           //  Load board index
        LD      (a,ptr(ix+BOARD));      //  Get piece moved
        LD      (P3,a);                 //  Save it
        AND     (7);                    //  Get piece type
        CP      (PAWN);                 //  Is it a Pawn ?
        RET     (NZ);                   //  No - return
        LD      (a,M4);                 //  Get "to" position
        LD      (hl,addr(M2));          //  Get present "to" position
        SUB     (ptr(hl));              //  Find difference
        JP      (P,rel003);             //  Positive ? Yes - Jump
        NEG;                            //  Else take absolute value
rel003: CP      (10);                   //  Is difference 10 ?
        RET     (NZ);                   //  No - return
        LD      (hl,addr(P2));          //  Address of flags
        SET     (6,ptr(hl));            //  Set double move flag
        CALL    (ADMOVE);               //  Add Pawn move to move list
        LD      (a,M1);                 //  Save initial Pawn position
        LD      (M3,a);                 //
        LD      (a,M4);                 //  Set "from" and "to" positions
// for dummy move
        LD      (M1,a);                 //
        LD      (M2,a);                 //
        LD      (a,P3);                 //  Save captured Pawn
        LD      (P2,a);                 //
        CALL    (ADMOVE);               //  Add Pawn capture to move list
        LD      (a,M3);                 //  Restore "from" position
        LD      (M1,a);                 //

//***********************************************************
// ADJUST MOVE LIST POINTER FOR DOUBLE MOVE
//***********************************************************
// FUNCTION:   --  To adjust move list pointer to link around
//                 second move in double move.
;
// CALLED BY:  --  ENPSNT
//                 CASTLE
//                 (This mini-routine is not really called,
//                 but is jumped to to save time.)
;
// CALLS:      --  None
;
// ARGUMENTS:  --  None
//***********************************************************
ADJPTR: LD      (hl,chk(MLLST));        //  Get list pointer
        LD      (de,-6);                //  Size of a move entry
        ADD     (hl,de);                //  Back up list pointer
        LD      (chk(MLLST),hl);        //  Save list pointer
        LD      (ptr(hl),0);            //  Zero out link, first byte
        INC     (hl);                   //  Next byte
        LD      (ptr(hl),0);            //  Zero out link, second byte
        RET;                            //  Return

//***********************************************************
// CASTLE ROUTINE
//***********************************************************
// FUNCTION:   --  To determine whether castling is legal
//                 (Queen side, King side, or both) and add it
//                 to the move list if it is.
;
// CALLED BY:  --  MPIECE
;
// CALLS:      --  ATTACK
//                 ADMOVE
//                 ADJPTR
;
// ARGUMENTS:  --  None
//***********************************************************
CASTLE: LD      (a,P1);                 //  Get King
        BIT     (3,a);                  //  Has it moved ?
        RET     (NZ);                   //  Yes - return
        LD      (a,CKFLG);              //  Fetch Check Flag
        AND     (a);                    //  Is the King in check ?
        RET     (NZ);                   //  Yes - Return
        LD      (bc,0xFF03);            //  Initialize King-side values
CA5:    LD      (a,M1);                 //  King position
        ADD     (a,c);                  //  Rook position
        LD      (c,a);                  //  Save
        LD      (M3,a);                 //  Store as board index
        LD      (ix,chk(M3));           //  Load board index
        LD      (a,ptr(ix+BOARD));      //  Get contents of board
        AND     (0x7F);                 //  Clear color bit
        CP      (ROOK);                 //  Has Rook ever moved ?
        JR      (NZ,CA20);              //  Yes - Jump
        LD      (a,c);                  //  Restore Rook position
        JR      (CA15);                 //  Jump
CA10:   LD      (ix,chk(M3));           //  Load board index
        LD      (a,ptr(ix+BOARD));      //  Get contents of board
        AND     (a);                    //  Empty ?
        JR      (NZ,CA20);              //  No - Jump
        LD      (a,M3);                 //  Current position
        CP      (22);                   //  White Queen Knight square ?
        JR      (Z,CA15);               //  Yes - Jump
        CP      (92);                   //  Black Queen Knight square ?
        JR      (Z,CA15);               //  Yes - Jump
        CALL    (ATTACK);               //  Look for attack on square
        AND     (a);                    //  Any attackers ?
        JR      (NZ,CA20);              //  Yes - Jump
        LD      (a,M3);                 //  Current position
CA15:   ADD     (a,b);                  //  Next position
        LD      (M3,a);                 //  Save as board index
        LD      (hl,addr(M1));          //  King position
        CP      (ptr(hl));              //  Reached King ?
        JR      (NZ,CA10);              //  No - jump
        SUB     (b);                    //  Determine King's position
        SUB     (b);
        LD      (M2,a);                 //  Save it
        LD      (hl,addr(P2));          //  Address of flags
        LD      (ptr(hl),0x40);         //  Set double move flag
        CALL    (ADMOVE);               //  Put king move in list
        LD      (hl,addr(M1));          //  Addr of King "from" position
        LD      (a,ptr(hl));            //  Get King's "from" position
        LD      ((hl),c);               //  Store Rook "from" position
        SUB     (b);                    //  Get Rook "to" position
        LD      (M2,a);                 //  Store Rook "to" position
        XOR     (a);                    //  Zero
        LD      (P2,a);                 //  Zero move flags
        CALL    (ADMOVE);               //  Put Rook move in list
        CALL    (ADJPTR);               //  Re-adjust move list pointer
        LD      (a,M3);                 //  Restore King position
        LD      (M1,a);                 //  Store
CA20:   LD      (a,b);                  //  Scan Index
        CP      (1);                    //  Done ?
        RET     (Z);                    //  Yes - return
        LD      (bc,0x01FC);            //  Set Queen-side initial values
        JP      (CA5);                  //  Jump

//***********************************************************
// ADMOVE ROUTINE
//***********************************************************
// FUNCTION:   --  To add a move to the move list
;
// CALLED BY:  --  MPIECE
//                 ENPSNT
//                 CASTLE
;
// CALLS:      --  None
;
// ARGUMENT:  --  None
//***********************************************************
ADMOVE: LD      (de,chk(MLNXT));        //  Addr of next loc in move list
        LD      (hl,addr(MLEND));       //  Address of list end
        AND     (a);                    //  Clear carry flag
        SBC     (hl,de);                //  Calculate difference
        JR      (C,AM10);               //  Jump if out of space
        LD      (hl,chk(MLLST));        //  Addr of prev. list area
        LD      (chk(MLLST),de);        //  Save next as previous
        LD      (ptr(hl),e);            //  Store link address
        INC     (hl);
        LD      (ptr(hl),d);
        LD      (hl,addr(P1));          //  Address of moved piece
        BIT     (3,ptr(hl));            //  Has it moved before ?
        JR      (NZ,rel004);            //  Yes - jump
        LD      (hl,addr(P2));          //  Address of move flags
        SET     (4,ptr(hl));            //  Set first move flag
rel004: EX      (de,hl);                //  Address of move area
        LD      (ptr(hl),0);            //  Store zero in link address
        INC     (hl);
        LD      (ptr(hl),0);
        INC     (hl);
        LD      (a,M1);                 //  Store "from" move position
        LD      (ptr(hl),a);
        INC     (hl);
        LD      (a,M2);                 //  Store "to" move position
        LD      (ptr(hl),a);
        INC     (hl);
        LD      (a,P2);                 //  Store move flags/capt. piece
        LD      (ptr(hl),a);
        INC     (hl);
        LD      (ptr(hl),0);            //  Store initial move value
        INC     (hl);
        LD      (chk(MLNXT),hl);        //  Save address for next move
        RET;                            //  Return
AM10:   LD      (ptr(hl),0);            //  Abort entry on table ovflow
        INC     (hl);
        LD      (ptr(hl),0);            //  TODO does this out of memory
        DEC     (hl);                   //       check actually work?
        RET

//***********************************************************
// GENERATE MOVE ROUTINE
//***********************************************************
// FUNCTION:  --  To generate the move set for all of the
//                pieces of a given color.
;
// CALLED BY: --  FNDMOV
;
// CALLS:     --  MPIECE
//                INCHK
;
// ARGUMENTS: --  None
//***********************************************************
GENMOV: CALL    (INCHK);                //  Test for King in check
        LD      (CKFLG,a);              //  Save attack count as flag
        LD      (de,chk(MLNXT));        //  Addr of next avail list space
        LD      (hl,chk(MLPTRI));       //  Ply list pointer index
        INC     (hl);                   //  Increment to next ply
        INC     (hl);
        LD      (ptr(hl),e);            //  Save move list pointer
        INC     (hl);
        LD      (ptr(hl),d);
        INC     (hl);
        LD      (chk(MLPTRI),hl);       //  Save new index
        LD      (chk(MLLST),hl);        //  Last pointer for chain init.
        LD      (a,21);                 //  First position on board
GM5:    LD      (M1,a);                 //  Save as index
        LD      (ix,chk(M1));           //  Load board index
        LD      (a,ptr(ix+BOARD));      //  Fetch board contents
        AND     (a);                    //  Is it empty ?
        JR      (Z,GM10);               //  Yes - Jump
        CP      (-1);                   //  Is it a border square ?
        JR      (Z,GM10);               //  Yes - Jump
        LD      (P1,a);                 //  Save piece
        LD      (hl,COLOR);             //  Address of color of piece
        XOR     (ptr(hl));              //  Test color of piece
        BIT     (7,a);                  //  Match ?
        CALL    (Z,MPIECE);             //  Yes - call Move Piece
GM10:   LD      (a,M1);                 //  Fetch current board position
        INC     (a);                    //  Incr to next board position
        CP      (99);                   //  End of board array ?
        JP      (NZ,GM5);               //  No - Jump
        RET;                            //  Return

//***********************************************************
// CHECK ROUTINE
//***********************************************************
// FUNCTION:   --  To determine whether or not the
//                 King is in check.
;
// CALLED BY:  --  GENMOV
//                 FNDMOV
//                 EVAL
;
// CALLS:      --  ATTACK
;
// ARGUMENTS:  --  Color of King
//***********************************************************
INCHK:  LD      (a,COLOR);              //  Get color
INCHK1: LD      (hl,addr(POSK));        //  Addr of white King position
        AND     (a);                    //  White ?
        JR      (Z,rel005);             //  Yes - Skip
        INC     (hl);                   //  Addr of black King position
rel005: LD      (a,ptr(hl));            //  Fetch King position
        LD      (M3,a);                 //  Save
        LD      (ix,chk(M3));           //  Load board index
        LD      (a,ptr(ix+BOARD));      //  Fetch board contents
        LD      (P1,a);                 //  Save
        AND     (7);                    //  Get piece type
        LD      (T1,a);                 //  Save
        CALL    (ATTACK);               //  Look for attackers on King
        RET;                            //  Return

//***********************************************************
// ATTACK ROUTINE
//***********************************************************
// FUNCTION:   --  To find all attackers on a given square
//                 by scanning outward from the square
//                 until a piece is found that attacks
//                 that square, or a piece is found that
//                 doesn't attack that square, or the edge
//                 of the board is reached.
;
//                 In determining which pieces attack
//                 a square, this routine also takes into
//                 account the ability of certain pieces to
//                 attack through another attacking piece. (For
//                 example a queen lined up behind a bishop
//                 of her same color along a diagonal.) The
//                 bishop is then said to be transparent to the
//                 queen, since both participate in the
//                 attack.
;
//                 In the case where this routine is called
//                 by CASTLE or INCHK, the routine is
//                 terminated as soon as an attacker of the
//                 opposite color is encountered.
;
// CALLED BY:  --  POINTS
//                 PINFND
//                 CASTLE
//                 INCHK
;
// CALLS:      --  PATH
//                 ATKSAV
;
// ARGUMENTS:  --  None
//***********************************************************
ATTACK: PUSH    (bc);                   //  Save Register B
        XOR     (a);                    //  Clear
        LD      (b,16);                 //  Initial direction count
        LD      (INDX2,a);              //  Initial direction index
        LD      (iy,chk(INDX2));        //  Load index
AT5:    LD      (c,ptr(iy+DIRECT));     //  Get direction
        LD      (d,0);                  //  Init. scan count/flags
        LD      (a,M3);                 //  Init. board start position
        LD      (M2,a);                 //  Save
AT10:   INC     (d);                    //  Increment scan count
        CALL    (PATH);                 //  Next position
        CP      (1);                    //  Piece of a opposite color ?
        JR      (Z,AT14A);              //  Yes - jump
        CP      (2);                    //  Piece of same color ?
        JR      (Z,AT14B);              //  Yes - jump
        AND     (a);                    //  Empty position ?
        JR      (NZ,AT12);              //  No - jump
        LD      (a,b);                  //  Fetch direction count
        CP      (9);                    //  On knight scan ?
        JR      (NC,AT10);              //  No - jump
AT12:   INC     (iy);                   //  Increment direction index
        DJNZ    (AT5);                  //  Done ? No - jump
        XOR     (a);                    //  No attackers
AT13:   POP     (bc);                   //  Restore register B
        RET;                            //  Return
AT14A:  BIT     (6,d);                  //  Same color found already ?
        JR      (NZ,AT12);              //  Yes - jump
        SET     (5,d);                  //  Set opposite color found flag
        JP      (AT14);                 //  Jump
AT14B:  BIT     (5,d);                  //  Opposite color found already?
        JR      (NZ,AT12);              //  Yes - jump
        SET     (6,d);                  //  Set same color found flag

;
// ***** DETERMINE IF PIECE ENCOUNTERED ATTACKS SQUARE *****
AT14:   LD      (a,T2);                 //  Fetch piece type encountered
        LD      (e,a);                  //  Save
        LD      (a,b);                  //  Get direction-counter
        CP      (9);                    //  Look for Knights ?
        JR      (C,AT25);               //  Yes - jump
        LD      (a,e);                  //  Get piece type
        CP      (QUEEN);                //  Is is a Queen ?
        JR      (NZ,AT15);              //  No - Jump
        SET     (7,d);                  //  Set Queen found flag
        JR      (AT30);                 //  Jump
AT15:   LD      (a,d);                  //  Get flag/scan count
        AND     (0x0F);                 //  Isolate count
        CP      (1);                    //  On first position ?
        JR      (NZ,AT16);              //  No - jump
        LD      (a,e);                  //  Get encountered piece type
        CP      (KING);                 //  Is it a King ?
        JR      (Z,AT30);               //  Yes - jump
AT16:   LD      (a,b);                  //  Get direction counter
        CP      (13);                   //  Scanning files or ranks ?
        JR      (C,AT21);               //  Yes - jump
        LD      (a,e);                  //  Get piece type
        CP      (BISHOP);               //  Is it a Bishop ?
        JR      (Z,AT30);               //  Yes - jump
        LD      (a,d);                  //  Get flags/scan count
        AND     (0x0F);                 //  Isolate count
        CP      (1);                    //  On first position ?
        JR      (NZ,AT12);              //  No - jump
        CP      (e);                    //  Is it a Pawn ?
        JR      (NZ,AT12);              //  No - jump
        LD      (a,P2);                 //  Fetch piece including color
        BIT     (7,a);                  //  Is it white ?
        JR      (Z,AT20);               //  Yes - jump
        LD      (a,b);                  //  Get direction counter
        CP      (15);                   //  On a non-attacking diagonal ?
        JR      (C,AT12);               //  Yes - jump
        JR      (AT30);                 //  Jump
AT20:   LD      (a,b);                  //  Get direction counter
        CP      (15);                   //  On a non-attacking diagonal ?
        JR      (NC,AT12);              //  Yes - jump
        JR      (AT30);                 //  Jump
AT21:   LD      (a,e);                  //  Get piece type
        CP      (ROOK);                 //  Is is a Rook ?
        JR      (NZ,AT12);              //  No - jump
        JR      (AT30);                 //  Jump
AT25:   LD      (a,e);                  //  Get piece type
        CP      (KNIGHT);               //  Is it a Knight ?
        JR      (NZ,AT12);              //  No - jump
AT30:   LD      (a,T1);                 //  Attacked piece type/flag
        CP      (7);                    //  Call from POINTS ?
        JR      (Z,AT31);               //  Yes - jump
        BIT     (5,d);                  //  Is attacker opposite color ?
        JR      (Z,AT32);               //  No - jump
        LD      (a,1);                  //  Set attacker found flag
        JP      (AT13);                 //  Jump
AT31:   CALL    (ATKSAV);               //  Save attacker in attack list
AT32:   LD      (a,T2);                 //  Attacking piece type
        CP      (KING);                 //  Is it a King,?
        JP      (Z,AT12);               //  Yes - jump
        CP      (KNIGHT);               //  Is it a Knight ?
        JP      (Z,AT12);               //  Yes - jump
        JP      (AT10);                 //  Jump

//***********************************************************
// ATTACK SAVE ROUTINE
//***********************************************************
// FUNCTION:   --  To save an attacking piece value in the
//                 attack list, and to increment the attack
//                 count for that color piece.
;
//                 The pin piece list is checked for the
//                 attacking piece, and if found there, the
//                 piece is not included in the attack list.
;
// CALLED BY:  --  ATTACK
;
// CALLS:      --  PNCK
;
// ARGUMENTS:  --  None
//***********************************************************
ATKSAV: PUSH    (bc);                   //  Save Regs BC
        PUSH    (de);                   //  Save Regs DE
        LD      (a,NPINS);              //  Number of pinned pieces
        AND     (a);                    //  Any ?
        CALL    (NZ,PNCK);              //  yes - check pin list
        LD      (ix,chk(T2));           //  Init index to value table
        LD      (hl,addr(ATKLST));      //  Init address of attack list
        LD      (bc,0);                 //  Init increment for white
        LD      (a,P2);                 //  Attacking piece
        BIT     (7,a);                  //  Is it white ?
        JR      (Z,rel006);             //  Yes - jump
        LD      (c,7);                  //  Init increment for black
rel006: AND     (7);                    //  Attacking piece type
        LD      (e,a);                  //  Init increment for type
        BIT     (7,d);                  //  Queen found this scan ?
        JR      (Z,rel007);             //  No - jump
        LD      (e,QUEEN);              //  Use Queen slot in attack list
rel007: ADD     (hl,bc);                //  Attack list address
        INC     (ptr(hl));              //  Increment list count
        LD      (d,0);
        ADD     (hl,de);                //  Attack list slot address
        LD      (a,ptr(hl));            //  Get data already there
        AND     (0x0f);                 //  Is first slot empty ?
        JR      (Z,AS20);               //  Yes - jump
        LD      (a,ptr(hl));            //  Get data again
        AND     (0xf0);                 //  Is second slot empty ?
        JR      (Z,AS19);               //  Yes - jump
        INC     (hl);                   //  Increment to King slot
        JR      (AS20);                 //  Jump
AS19:   RLD;                            //  Temp save lower in upper
        LD      (a,ptr(ix+PVALUE));     //  Get new value for attack list
        RRD;                            //  Put in 2nd attack list slot
        JR      (AS25);                 //  Jump
AS20:   LD      (a,ptr(ix+PVALUE));     //  Get new value for attack list
        RLD;                            //  Put in 1st attack list slot
AS25:   POP     (de);                   //  Restore DE regs
        POP     (bc);                   //  Restore BC regs
        RET;                            //  Return

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
;
// CALLED BY:  --  ATKSAV
;
// CALLS:      --  None
;
// ARGUMENTS:  --  The direction of the attack. The
//                 pinned piece counnt.
//***********************************************************
PNCK:   LD      (d,c);                  //  Save attack direction
        LD      (e,0);                  //  Clear flag
        LD      (c,a);                  //  Load pin count for search
        LD      (b,0);
        LD      (a,M2);                 //  Position of piece
        LD      (hl,addr(PLISTA));      //  Pin list address
PC1:    Z80_CPIR;                       //  Search list for position
        RET     (NZ);                   //  Return if not found
        Z80_EXAF;                       //  Save search parameters
        BIT     (0,e);                  //  Is this the first find ?
        JR      (NZ,PC5);               //  No - jump
        SET     (0,e);                  //  Set first find flag
        PUSH    (hl);                   //  Get corresp index to dir list
        POP     (ix);
        LD      (a,ptr(ix+9));          //  Get direction
        CP      (d);                    //  Same as attacking direction ?
        JR      (Z,PC3);                //  Yes - jump
        NEG     ();                     //  Opposite direction ?
        CP      (d);                    //  Same as attacking direction ?
        JR      (NZ,PC5);               //  No - jump
PC3:    Z80_EXAF;                       //  Restore search parameters
        JP      (PE,PC1);               //  Jump if search not complete
        RET;                            //  Return
PC5:    POP     (af);                   //  Abnormal exit
        POP     (de);                   //  Restore regs.
        POP     (bc);
        RET;                            //  Return to ATTACK

//***********************************************************
// PIN FIND ROUTINE
//***********************************************************
// FUNCTION:   --  To produce a list of all pieces pinned
//                 against the King or Queen, for both white
//                 and black.
;
// CALLED BY:  --  FNDMOV
//                 EVAL
;
// CALLS:      --  PATH
//                 ATTACK
;
// ARGUMENTS:  --  None
//***********************************************************
PINFND: XOR     (a);                    //  Zero pin count
        LD      (NPINS,a);
        LD      (de,addr(POSK));        //  Addr of King/Queen pos list
PF1:    LD      (a,ptr(de));            //  Get position of royal piece
        AND     (a);                    //  Is it on board ?
        JP      (Z,PF26);               //  No- jump
        CP      (-1);                   //  At end of list ?
        RET     (Z);                    //  Yes return
        LD      (M3,a);                 //  Save position as board index
        LD      (ix,chk(M3));           //  Load index to board
        LD      (a,ptr(ix+BOARD));      //  Get contents of board
        LD      (P1,a);                 //  Save
        LD      (b,8);                  //  Init scan direction count
        XOR     (a);
        LD      (INDX2,a);              //  Init direction index
        LD      (iy,chk(INDX2));
PF2:    LD      (a,M3);                 //  Get King/Queen position
        LD      (M2,a);                 //  Save
        XOR     (a);
        LD      (M4,a);                 //  Clear pinned piece saved pos
        LD      (c,ptr(iy+DIRECT));     //  Get direction of scan
PF5:    CALL    (PATH);                 //  Compute next position
        AND     (a);                    //  Is it empty ?
        JR      (Z,PF5);                //  Yes - jump
        CP      (3);                    //  Off board ?
        JP      (Z,PF25);               //  Yes - jump
        CP      (2);                    //  Piece of same color
        LD      (a,M4);                 //  Load pinned piece position
        JR      (Z,PF15);               //  Yes - jump
        AND     (a);                    //  Possible pin ?
        JP      (Z,PF25);               //  No - jump
        LD      (a,T2);                 //  Piece type encountered
        CP      (QUEEN);                //  Queen ?
        JP      (Z,PF19);               //  Yes - jump
        LD      (l,a);                  //  Save piece type
        LD      (a,b);                  //  Direction counter
        CP      (5);                    //  Non-diagonal direction ?
        JR      (C,PF10);               //  Yes - jump
        LD      (a,l);                  //  Piece type
        CP      (BISHOP);               //  Bishop ?
        JP      (NZ,PF25);              //  No - jump
        JP      (PF20);                 //  Jump
PF10:   LD      (a,l);                  //  Piece type
        CP      (ROOK);                 //  Rook ?
        JP      (NZ,PF25);              //  No - jump
        JP      (PF20);                 //  Jump
PF15:   AND     (a);                    //  Possible pin ?
        JP      (NZ,PF25);              //  No - jump
        LD      (a,M2);                 //  Save possible pin position
        LD      (M4,a);
        JP      (PF5);                  //  Jump
PF19:   LD      (a,P1);                 //  Load King or Queen
        AND     (7);                    //  Clear flags
        CP      (QUEEN);                //  Queen ?
        JR      (NZ,PF20);              //  No - jump
        PUSH    (bc);                   //  Save regs.
        PUSH    (de);
        PUSH    (iy);
        XOR     (a);                    //  Zero out attack list
        LD      (b,14);
        LD      (hl,addr(ATKLST));
back02: LD      (ptr(hl),a);
        INC     (hl);
        DJNZ    (back02);
        LD      (a,7);                  //  Set attack flag
        LD      (T1,a);
        CALL    (ATTACK);               //  Find attackers/defenders
        LD      (hl,addr(WACT));        //  White queen attackers
        LD      (de,addr(BACT));        //  Black queen attackers
        LD      (a,P1);                 //  Get queen
        BIT     (7,a);                  //  Is she white ?
        JR      (Z,rel008);             //  Yes - skip
        EX      (de,hl);                //  Reverse for black
rel008: LD      (a,ptr(hl));            //  Number of defenders
        EX      (de,hl);                //  Reverse for attackers
        SUB     (ptr(hl));              //  Defenders minus attackers
        DEC     (a);                    //  Less 1
        POP     (iy);                   //  Restore regs.
        POP     (de);
        POP     (bc);
        JP      (P,PF25);               //  Jump if pin not valid
PF20:   LD      (hl,addr(NPINS));       //  Address of pinned piece count
        INC     (ptr(hl));              //  Increment
        LD      (ix,chk(NPINS));        //  Load pin list index
        LD      (ptr(ix+PLISTD),c);     //  Save direction of pin
        LD      (a,M4);                 //  Position of pinned piece
        LD      (ptr(ix+PLIST),a);      //  Save in list
PF25:   INC     (iy);                   //  Increment direction index
        DJNZ    (PF27);                 //  Done ? No - Jump
PF26:   INC     (de);                   //  Incr King/Queen pos index
        JP      (PF1);                  //  Jump
PF27:   JP      (PF2);                  //  Jump

//***********************************************************
// EXCHANGE ROUTINE
//***********************************************************
// FUNCTION:   --  To determine the exchange value of a
//                 piece on a given square by examining all
//                 attackers and defenders of that piece.
;
// CALLED BY:  --  POINTS
;
// CALLS:      --  NEXTAD
;
// ARGUMENTS:  --  None.
//***********************************************************
XCHNG:  EXX;                            //  Swap regs.
        LD      (a,(P1));               //  Piece attacked
        LD      (hl,addr(WACT));        //  Addr of white attkrs/dfndrs
        LD      (de,addr(BACT));        //  Addr of black attkrs/dfndrs
        BIT     (7,a);                  //  Is piece white ?
        JR      (Z,rel009);             //  Yes - jump
        EX      (de,hl);                //  Swap list pointers
rel009: LD      (b,ptr(hl));            //  Init list counts
        EX      (de,hl);
        LD      (c,ptr(hl));
        EX      (de,hl);
        EXX;                            //  Restore regs.
        LD      (c,0);                  //  Init attacker/defender flag
        LD      (e,0);                  //  Init points lost count
        LD      (ix,chk(T3));           //  Load piece value index
        LD      (d,ptr(ix+PVALUE));     //  Get attacked piece value
        SLA     (d);                    //  Double it
        LD      (b,d);                  //  Save
        CALL    (NEXTAD);               //  Retrieve first attacker
        RET     (Z);                    //  Return if none
XC10:   LD      (l,a);                  //  Save attacker value
        CALL    (NEXTAD);               //  Get next defender
        JR      (Z,XC18);               //  Jump if none
        Z80_EXAF;                       //  Save defender value
        LD      (a,b);                  //  Get attacked value
        CP      (l);                    //  Attacked less than attacker ?
        JR      (NC,XC19);              //  No - jump
        Z80_EXAF;                       //  -Restore defender
XC15:   CP      (l);                    //  Defender less than attacker ?
        RET     (C);                    //  Yes - return
        CALL    (NEXTAD);               //  Retrieve next attacker value
        RET     (Z);                    //  Return if none
        LD      (l,a);                  //  Save attacker value
        CALL    (NEXTAD);               //  Retrieve next defender value
        JR      (NZ,XC15);              //  Jump if none
XC18:   Z80_EXAF;                       //  Save Defender
        LD      (a,b);                  //  Get value of attacked piece
XC19:   BIT     (0,c);                  //  Attacker or defender ?
        JR      (Z,rel010);             //  Jump if defender
        NEG     ();                     //  Negate value for attacker
rel010: ADD     (a,e);                  //  Total points lost
        LD      (e,a);                  //  Save total
        Z80_EXAF;                       //  Restore previous defender
        RET     (Z);                    //  Return if none
        LD      (b,l);                  //  Prev attckr becomes defender
        JP      (XC10);                 //  Jump

//***********************************************************
// NEXT ATTACKER/DEFENDER ROUTINE
//***********************************************************
// FUNCTION:   --  To retrieve the next attacker or defender
//                 piece value from the attack list, and delete
//                 that piece from the list.
;
// CALLED BY:  --  XCHNG
;
// CALLS:      --  None
;
// ARGUMENTS:  --  Attack list addresses.
//                 Side flag
//                 Attack list counts
//***********************************************************
NEXTAD: INC     (c);                    //  Increment side flag
        EXX;                            //  Swap registers
        LD      (a,b);                  //  Swap list counts
        LD      (b,c);                  //
        LD      (c,a);                  //
        EX      (de,hl);                //  Swap list pointers
        XOR     (a);                    //
        CP      (b);                    //  At end of list ?
        JR      (Z,NX6);                //  Yes - jump
        DEC     (b);                    //  Decrement list count
back03: INC     (hl);                   //  Increment list pointer
        CP      (ptr(hl));              //  Check next item in list
        JR      (Z,back03);             //  Jump if empty
        RRD;                            //  Get value from list
        ADD     (a,a);                  //  Double it
        DEC     (hl);                   //  Decrement list pointer
NX6:    EXX;                            //  Restore regs.
        RET;                            //  Return

//***********************************************************
// POINT EVALUATION ROUTINE
//***********************************************************
//FUNCTION:   --  To perform a static board evaluation and
//                derive a score for a given board position
;
// CALLED BY:  --  FNDMOV
//                 EVAL
;
// CALLS:      --  ATTACK
//                 XCHNG
//                 LIMIT
;
// ARGUMENTS:  --  None
//***********************************************************
POINTS: XOR     (a);                    //  Zero out variables
        LD      (MTRL,a);               //
        LD      (BRDC,a);               //
        LD      (PTSL,a);               //
        LD      (PTSW1,a);              //
        LD      (PTSW2,a);              //
        LD      (PTSCK,a);              //
        LD      (hl,addr(T1));          //  Set attacker flag
        LD      (ptr(hl),7);            //
        LD      (a,21);                 //  Init to first square on board
PT5:    LD      (M3,a);                 //  Save as board index
        LD      (ix,chk(M3));           //  Load board index
        LD      (a,ptr(ix+BOARD));      //  Get piece from board
        CP      (-1);                   //  Off board edge ?
        JP      (Z,PT25);               //  Yes - jump
        LD      (hl,addr(P1));          //  Save piece, if any
        LD      (ptr(hl),a);            //
        AND     (7);                    //  Save piece type, if any
        LD      (T3,a);                 //
        CP      (KNIGHT);               //  Less than a Knight (Pawn) ?
        JR      (C,PT6X);               //  Yes - Jump
        CP      (ROOK);                 //  Rook, Queen or King ?
        JR      (C,PT6B);               //  No - jump
        CP      (KING);                 //  Is it a King ?
        JR      (Z,PT6AA);              //  Yes - jump
        LD      (a,MOVENO);             //  Get move number
        CP      (7);                    //  Less than 7 ?
        JR      (C,PT6A);               //  Yes - Jump
        JP      (PT6X);                 //  Jump
PT6AA:  BIT     (4,ptr(hl));            //  Castled yet ?
        JR      (Z,PT6A);               //  No - jump
        LD      (a,+6);                 //  Bonus for castling
        BIT     (7,ptr(hl));            //  Check piece color
        JR      (Z,PT6D);               //  Jump if white
        LD      (a,-6);                 //  Bonus for black castling
        JP      (PT6D);                 //  Jump
PT6A:   BIT     (3,ptr(hl));            //  Has piece moved yet ?
        JR      (Z,PT6X);               //  No - jump
        JP      (PT6C);                 //  Jump
PT6B:   BIT     (3,ptr(hl));            //  Has piece moved yet ?
        JR      (NZ,PT6X);              //  Yes - jump
PT6C:   LD      (a,-2);                 //  Two point penalty for white
        BIT     (7,ptr(hl));            //  Check piece color
        JR      (Z,PT6D);               //  Jump if white
        LD      (a,+2);                 //  Two point penalty for black
PT6D:   LD      (hl,addr(BRDC));        //  Get address of board control
        ADD     (a,ptr(hl));            //  Add on penalty/bonus points
        LD      (ptr(hl),a);            //  Save
PT6X:   XOR     (a);                    //  Zero out attack list
        LD      (b,14);                 //
        LD      (hl,addr(ATKLST));      //
back04: LD      (ptr(hl),a);            //
        INC     (hl);                   //
        DJNZ    (back04);               //
        CALL    (ATTACK);               //  Build attack list for square
        LD      (hl,addr(BACT));        //  Get black attacker count addr
        LD      (a,WACT);               //  Get white attacker count
        SUB     (ptr(hl));              //  Compute count difference
        LD      (hl,addr(BRDC));        //  Address of board control
        ADD     (a,ptr(hl));            //  Accum board control score
        LD      (ptr(hl),a);            //  Save
        LD      (a,P1);                 //  Get piece on current square
        AND     (a);                    //  Is it empty ?
        JP      (Z,PT25);               //  Yes - jump
        CALL    (XCHNG);                //  Evaluate exchange, if any
        XOR     (a);                    //  Check for a loss
        CP      (e);                    //  Points lost ?
        JR      (Z,PT23);               //  No - Jump
        DEC     (d);                    //  Deduct half a Pawn value
        LD      (a,P1);                 //  Get piece under attack
        LD      (hl,COLOR);             //  Color of side just moved
        XOR     (ptr(hl));              //  Compare with piece
        BIT     (7,a);                  //  Do colors match ?
        LD      (a,e);                  //  Points lost
        JR      (NZ,PT20);              //  Jump if no match
        LD      (hl,PTSL);              //  Previous max points lost
        CP      (ptr(hl));              //  Compare to current value
        JR      (C,PT23);               //  Jump if greater than
        LD      (ptr(hl),e);            //  Store new value as max lost
        LD      (ix,chk(MLPTRJ));       //  Load pointer to this move
        LD      (a,M3);                 //  Get position of lost piece
        CP      (ptr(ix+MLTOP));        //  Is it the one moving ?
        JR      (NZ,PT23);              //  No - jump
        LD      (PTSCK,a);              //  Save position as a flag
        JP      (PT23);                 //  Jump
PT20:   LD      (hl,addr(PTSW1));       //  Previous maximum points won
        CP      (ptr(hl));              //  Compare to current value
        JR      (C,rel011);             //  Jump if greater than
        LD      (a,ptr(hl));            //  Load previous max value
        LD      (ptr(hl),e);            //  Store new value as max won
rel011: LD      (hl,addr(PTSW2));       //  Previous 2nd max points won
        CP      (ptr(hl));              //  Compare to current value
        JR      (C,PT23);               //  Jump if greater than
        LD      (ptr(hl),a);            //  Store as new 2nd max lost
PT23:   LD      (hl,addr(P1));          //  Get piece
        BIT     (7,ptr(hl));            //  Test color
        LD      (a,d);                  //  Value of piece
        JR      (Z,rel012);             //  Jump if white
        NEG     ();                     //  Negate for black
rel012: LD      (hl,addr(MTRL));        //  Get addrs of material total
        ADD     (a,ptr(hl));            //  Add new value
        LD      (ptr(hl),a);            //  Store
PT25:   LD      (a,M3);                 //  Get current board position
        INC     (a);                    //  Increment
        CP      (99);                   //  At end of board ?
        JP      (NZ,PT5);               //  No - jump
        LD      (a,PTSCK);              //  Moving piece lost flag
        AND     (a);                    //  Was it lost ?
        JR      (Z,PT25A);              //  No - jump
        LD      (a,PTSW2);              //  2nd max points won
        LD      (PTSW1,a);              //  Store as max points won
        XOR     (a);                    //  Zero out 2nd max points won
        LD      (PTSW2,a);              //
PT25A:  LD      (a,PTSL);               //  Get max points lost
        AND     (a);                    //  Is it zero ?
        JR      (Z,rel013);             //  Yes - jump
        DEC     (a);                    //  Decrement it
rel013: LD      (b,a);                  //  Save it
        LD      (a,PTSW1);              //  Max,points won
        AND     (a);                    //  Is it zero ?
        JR      (Z,rel014);             //  Yes - jump
        LD      (a,PTSW2);              //  2nd max points won
        AND     (a);                    //  Is it zero ?
        JR      (Z,rel014);             //  Yes - jump
        DEC     (a);                    //  Decrement it
        SRL     (a);                    //  Divide it by 2
rel014: SUB     (b);                    //  Subtract points lost
        LD      (hl,addr(COLOR));       //  Color of side just moved ???
        BIT     (7,ptr(hl));            //  Is it white ?
        JR      (Z,rel015);             //  Yes - jump
        NEG;                            //  Negate for black
rel015: LD      (hl,addr(MTRL));        //  Net material on board
        ADD     (a,ptr(hl));            //  Add exchange adjustments
        LD      (hl,addr(MV0));         //  Material at ply 0
        SUB     (ptr(hl));              //  Subtract from current
        LD      (b,a);                  //  Save
        LD      (a,30);                 //  Load material limit
        CALL    (LIMIT);                //  Limit to plus or minus value
        LD      (e,a);                  //  Save limited value
        LD      (a,BRDC);               //  Get board control points
        LD      (hl,addr(BC0));         //  Board control at ply zero
        SUB     (ptr(hl));              //  Get difference
        LD      (b,a);                  //  Save
        LD      (a,PTSCK);              //  Moving piece lost flag
        AND     (a);                    //  Is it zero ?
        JR      (Z,rel026);             //  Yes - jump
        LD      (b,0);                  //  Zero board control points
rel026: LD      (a,6);                  //  Load board control limit
        CALL    (LIMIT);                //  Limit to plus or minus value
        LD      (d,a);                  //  Save limited value
        LD      (a,e);                  //  Get material points
        ADD     (a,a);                  //  Multiply by 4
        ADD     (a,a);                  //
        ADD     (a,d);                  //  Add board control
        LD      (hl,addr(COLOR));       //  Color of side just moved
        BIT     (7,ptr(hl));            //  Is it white ?
        JR      (NZ,rel016);            //  No - jump
        NEG;                            //  Negate for white
rel016: ADD     (a,0x80);               //  Rescale score (neutral = 80H)
        LD      (VALM,a);               //  Save score
        LD      (ix,chk(MLPTRJ));       //  Load move list pointer
        LD      (ptr(ix+MLVAL),a);      //  Save score in move list
        RET;                            //  Return

//***********************************************************
// LIMIT ROUTINE
//***********************************************************
// FUNCTION:   --  To limit the magnitude of a given value
//                 to another given value.
;
// CALLED BY:  --  POINTS
;
// CALLS:      --  None
;
// ARGUMENTS:  --  Input  - Value, to be limited in the B
//                          register.
//                        - Value to limit to in the A register
//                 Output - Limited value in the A register.
//***********************************************************
LIMIT:  BIT     (7,b);                  //  Is value negative ?
        JP      (Z,LIM10);              //  No - jump
        NEG;                            //  Make positive
        CP      (b);                    //  Compare to limit
        RET     (NC);                   //  Return if outside limit
        LD      (a,b);                  //  Output value as is
        RET;                            //  Return
LIM10:  CP      (b);                    //  Compare to limit
        RET     (C);                    //  Return if outside limit
        LD      (a,b);                  //  Output value as is
        RET;                            //  Return

//***********************************************************
// MOVE ROUTINE
//***********************************************************
// FUNCTION:   --  To execute a move from the move list on the
//                 board array.
;
// CALLED BY:  --  CPTRMV
//                 PLYRMV
//                 EVAL
//                 FNDMOV
//                 VALMOV
;
// CALLS:      --  None
;
// ARGUMENTS:  --  None
//***********************************************************
MOVE:   LD      (hl,chk(MLPTRJ));       //  Load move list pointer
        INC     (hl);                   //  Increment past link bytes
        INC     (hl);
MV1:    LD      (a,ptr(hl));            //  "From" position
        LD      (M1,a);                 //  Save
        INC     (hl);                   //  Increment pointer
        LD      (a,ptr(hl));            //  "To" position
        LD      (M2,a);                 //  Save
        INC     (hl);                   //  Increment pointer
        LD      (d,ptr(hl));            //  Get captured piece/flags
        LD      (ix,chk(M1));           //  Load "from" pos board index
        LD      (e,ptr(ix+BOARD));      //  Get piece moved
        BIT     (5,d);                  //  Test Pawn promotion flag
        JR      (NZ,MV15);              //  Jump if set
        LD      (a,e);                  //  Piece moved
        AND     (7);                    //  Clear flag bits
        CP      (QUEEN);                //  Is it a queen ?
        JR      (Z,MV20);               //  Yes - jump
        CP      (KING);                 //  Is it a king ?
        JR      (Z,MV30);               //  Yes - jump
MV5:    LD      (iy,chk(M2));           //  Load "to" pos board index
        SET     (3,e);                  //  Set piece moved flag
        LD      (ptr(iy+BOARD),e);      //  Insert piece at new position
        LD      (ptr(ix+BOARD),0);      //  Empty previous position
        BIT     (6,d);                  //  Double move ?
        JR      (NZ,MV40);              //  Yes - jump
        LD      (a,d);                  //  Get captured piece, if any
        AND     (7);
        CP      (QUEEN);                //  Was it a queen ?
        RET     (NZ);                   //  No - return
        LD      (hl,addr(POSQ));        //  Addr of saved Queen position
        BIT     (7,d);                  //  Is Queen white ?
        JR      (Z,MV10);               //  Yes - jump
        INC     (hl);                   //  Increment to black Queen pos
MV10:   XOR     (a);                    //  Set saved position to zero
        LD      (ptr(hl),a);
        RET;                            //  Return
MV15:   SET     (2,e);                  //  Change Pawn to a Queen
        JP      (MV5);                  //  Jump
MV20:   LD      (hl,POSQ);              //  Addr of saved Queen position
MV21:   BIT     (7,e);                  //  Is Queen white ?
        JR      (Z,MV22);               //  Yes - jump
        INC     (hl);                   //  Increment to black Queen pos
MV22:   LD      (a,M2);                 //  Get new Queen position
        LD      (ptr(hl),a);            //  Save
        JP      (MV5);                  //  Jump
MV30:   LD      (hl,addr(POSK));        //  Get saved King position
        BIT     (6,d);                  //  Castling ?
        JR      (Z,MV21);               //  No - jump
        SET     (4,e);                  //  Set King castled flag
        JP      (MV21);                 //  Jump
MV40:   LD      (hl,chk(MLPTRJ));       //  Get move list pointer
        LD      (de,8);                 //  Increment to next move
        ADD     (hl,de);                //
        JP      (MV1);                  //  Jump (2nd part of dbl move)

//***********************************************************
// UN-MOVE ROUTINE
//***********************************************************
// FUNCTION:   --  To reverse the process of the move routine,
//                 thereby restoring the board array to its
//                 previous position.
;
// CALLED BY:  --  VALMOV
//                 EVAL
//                 FNDMOV
//                 ASCEND
;
// CALLS:      --  None
;
// ARGUMENTS:  --  None
//***********************************************************
UNMOVE: LD      (hl,chk(MLPTRJ));       //  Load move list pointer
        INC     (hl);                   //  Increment past link bytes
        INC     (hl);
UM1:    LD      (a,ptr(hl));            //  Get "from" position
        LD      (M1,a);                 //  Save
        INC     (hl);                   //  Increment pointer
        LD      (a,ptr(hl));            //  Get "to" position
        LD      (M2,a);                 //  Save
        INC     (hl);                   //  Increment pointer
        LD      (d,ptr(hl));            //  Get captured piece/flags
        LD      (ix,chk(M2));           //  Load "to" pos board index
        LD      (e,ptr(ix+BOARD));      //  Get piece moved
        BIT     (5,d);                  //  Was it a Pawn promotion ?
        JR      (NZ,UM15);              //  Yes - jump
        LD      (a,e);                  //  Get piece moved
        AND     (7);                    //  Clear flag bits
        CP      (QUEEN);                //  Was it a Queen ?
        JR      (Z,UM20);               //  Yes - jump
        CP      (KING);                 //  Was it a King ?
        JR      (Z,UM30);               //  Yes - jump
UM5:    BIT     (4,d);                  //  Is this 1st move for piece ?
        JR      (NZ,UM16);              //  Yes - jump
UM6:    LD      (iy,chk(M1));           //  Load "from" pos board index
        LD      (ptr(iy+BOARD),e);      //  Return to previous board pos
        LD      (a,d);                  //  Get captured piece, if any
        AND     (0x8f);                 //  Clear flags
        LD      (ptr(ix+BOARD),a);      //  Return to board
        BIT     (6,d);                  //  Was it a double move ?
        JR      (NZ,UM40);              //  Yes - jump
        LD      (a,d);                  //  Get captured piece, if any
        AND     (7);                    //  Clear flag bits
        CP      (QUEEN);                //  Was it a Queen ?
        RET     (NZ);                   //  No - return
        LD      (hl,addr(POSQ));        //  Address of saved Queen pos
        BIT     (7,d);                  //  Is Queen white ?
        JR      (Z,UM10);               //  Yes - jump
        INC     (hl);                   //  Increment to black Queen pos
UM10:   LD      (a,M2);                 //  Queen's previous position
        LD      (ptr(hl),a);            //  Save
        RET;                            //  Return
UM15:   RES     (2,e);                  //  Restore Queen to Pawn
        JP      (UM5);                  //  Jump
UM16:   RES     (3,e);                  //  Clear piece moved flag
        JP      (UM6);                  //  Jump
UM20:   LD      (hl,addr(POSQ));        //  Addr of saved Queen position
UM21:   BIT     (7,e);                  //  Is Queen white ?
        JR      (Z,UM22);               //  Yes - jump
        INC     (hl);                   //  Increment to black Queen pos
UM22:   LD      (a,M1);                 //  Get previous position
        LD      (ptr(hl),a);            //  Save
        JP      (UM5);                  //  Jump
UM30:   LD      (hl,addr(POSK));        //  Address of saved King pos
        BIT     (6,d);                  //  Was it a castle ?
        JR      (Z,UM21);               //  No - jump
        RES     (4,e);                  //  Clear castled flag
        JP      (UM21);                 //  Jump
UM40:   LD      (hl,chk(MLPTRJ));       //  Load move list pointer
        LD      (de,8);                 //  Increment to next move
        ADD     (hl,de);
        JP      (UM1);                  //  Jump (2nd part of dbl move)

//***********************************************************
// SORT ROUTINE
//***********************************************************
// FUNCTION:   --  To sort the move list in order of
//                 increasing move value scores.
;
// CALLED BY:  --  FNDMOV
;
// CALLS:      --  EVAL
;
// ARGUMENTS:  --  None
//***********************************************************
SORTM:  LD      (bc,chk(MLPTRI));       //  Move list begin pointer
        LD      (de,0);                 //  Initialize working pointers
SR5:    LD      (h,b);                  //
        LD      (l,c);                  //
        LD      (c,ptr(hl));            //  Link to next move
        INC     (hl);                   //
        LD      (b,ptr(hl));            //
        LD      (ptr(hl),d);            //  Store to link in list
        DEC     (hl);                   //
        LD      (ptr(hl),e);            //
        XOR     (a);                    //  End of list ?
        CP      (b);                    //
        RET     (Z);                    //  Yes - return
SR10:   LD      (chk(MLPTRJ),bc);       //  Save list pointer
        CALL    (EVAL);                 //  Evaluate move
        LD      (hl,chk(MLPTRI));       //  Begining of move list
        LD      (bc,chk(MLPTRJ));       //  Restore list pointer
SR15:   LD      (e,ptr(hl));            //  Next move for compare
        INC     (hl);                   //
        LD      (d,ptr(hl));            //
        XOR     (a);                    //  At end of list ?
        CP      (d);                    //
        JR      (Z,SR25);               //  Yes - jump
        PUSH    (de);                   //  Transfer move pointer
        POP     (ix);                   //
        LD      (a,VALM);               //  Get new move value
        CP      (ptr(ix+MLVAL));        //  Less than list value ?
        JR      (NC,SR30);              //  No - jump
SR25:   LD      (ptr(hl),b);            //  Link new move into list
        DEC     (hl);                   //
        LD      (ptr(hl),c);            //
        JP      (SR5);                  //  Jump
SR30:   EX      (de,hl);                //  Swap pointers
        JP      (SR15);                 //  Jump

//***********************************************************
// EVALUATION ROUTINE
//***********************************************************
// FUNCTION:   --  To evaluate a given move in the move list.
//                 It first makes the move on the board, then if
//                 the move is legal, it evaluates it, and then
//                 restores the board position.
;
// CALLED BY:  --  SORT
;
// CALLS:      --  MOVE
//                 INCHK
//                 PINFND
//                 POINTS
//                 UNMOVE
;
// ARGUMENTS:  --  None
//***********************************************************
EVAL:   CALL    (MOVE);                 //  Make move on the board array
        CALL    (INCHK);                //  Determine if move is legal
        AND     (a);                    //  Legal move ?
        JR      (Z,EV5);                //  Yes - jump
        XOR     (a);                    //  Score of zero
        LD      (VALM,a);               //  For illegal move
        JP      (EV10);                 //  Jump
EV5:    CALL    (PINFND);               //  Compile pinned list
        CALL    (POINTS);               //  Assign points to move
EV10:   CALL    (UNMOVE);               //  Restore board array
        RET;                            //  Return

//***********************************************************
// FIND MOVE ROUTINE
//***********************************************************
// FUNCTION:   --  To determine the computer's best move by
//                 performing a depth first tree search using
//                 the techniques of alpha-beta pruning.
;
// CALLED BY:  --  CPTRMV
;
// CALLS:      --  PINFND
//                 POINTS
//                 GENMOV
//                 SORTM
//                 ASCEND
//                 UNMOVE
;
// ARGUMENTS:  --  None
//***********************************************************
FNDMOV: LD      (a,MOVENO);             //  Current move number
        CP      (1);                    //  First move ?
        CALL    (Z,BOOK);               //  Yes - execute book opening
        XOR     (a);                    //  Initialize ply number to zero
        LD      (NPLY,a);
        LD      (hl,0);                 //  Initialize best move to zero
        LD      (chk(BESTM),hl);
        LD      (hl,addr(MLIST));       //  Initialize ply list pointers
        LD      (chk(MLNXT),hl);
        LD      (hl,addr(PLYIX)-2);
        LD      (chk(MLPTRI),hl);
        LD      (a,KOLOR);              //  Initialize color
        LD      (COLOR,a);
        LD      (hl,addr(SCORE));       //  Initialize score index
        LD      (chk(SCRIX),hl);
        LD      (a,PLYMAX);             //  Get max ply number
        ADD     (a,2);                  //  Add 2
        LD      (b,a);                  //  Save as counter
        XOR     (a);                    //  Zero out score table
back05: LD      (ptr(hl),a);
        INC     (hl);
        DJNZ    (back05);
        LD      (BC0,a);                //  Zero ply 0 board control
        LD      (MV0,a);                //  Zero ply 0 material
        CALL    (PINFND);               //  Compile pin list
        CALL    (POINTS);               //  Evaluate board at ply 0
        LD      (a,BRDC);               //  Get board control points
        LD      (BC0,a);                //  Save
        LD      (a,MTRL);               //  Get material count
        LD      (MV0,a);                //  Save
FM5:    LD      (hl,addr(NPLY));        //  Address of ply counter
        INC     (ptr(hl));              //  Increment ply count
        XOR     (a);                    //  Initialize mate flag
        LD      (MATEF,a);
        CALL    (GENMOV);               //  Generate list of moves
        LD      (a,NPLY);               //  Current ply counter
        LD      (hl,addr(PLYMAX));      //  Address of maximum ply number
        CP      (ptr(hl));              //  At max ply ?
        CALL    (C,SORTM);              //  No - call sort
        LD      (hl,chk(MLPTRI));       //  Load ply index pointer
        LD      (chk(MLPTRJ),hl);       //  Save as last move pointer
FM15:   LD      (hl,chk(MLPTRJ));       //  Load last move pointer
        LD      (e,ptr(hl));            //  Get next move pointer
        INC     (hl);
        LD      (d,ptr(hl));
        LD      (a,d);
        AND     (a);                    //  End of move list ?
        JR      (Z,FM25);               //  Yes - jump
        LD      (chk(MLPTRJ),de);       //  Save current move pointer
        LD      (hl,chk(MLPTRI));       //  Save in ply pointer list
        LD      (ptr(hl),e);
        INC     (hl);
        LD      (ptr(hl),d);
        LD      (a,NPLY);               //  Current ply counter
        LD      (hl,addr(PLYMAX));      //  Maximum ply number ?
        CP      (ptr(hl));              //  Compare
        JR      (C,FM18);               //  Jump if not max
        CALL    (MOVE);                 //  Execute move on board array
        CALL    (INCHK);                //  Check for legal move
        AND     (a);                    //  Is move legal
        JR      (Z,rel017);             //  Yes - jump
        CALL    (UNMOVE);               //  Restore board position
        JP      (FM15);                 //  Jump
rel017: LD      (a,NPLY);               //  Get ply counter
        LD      (hl,addr(PLYMAX));      //  Max ply number
        CP      (ptr(hl));              //  Beyond max ply ?
        JR      (NZ,FM35);              //  Yes - jump
        LD      (a,COLOR);              //  Get current color
        XOR     (0x80);                 //  Get opposite color
        CALL    (INCHK1);               //  Determine if King is in check
        AND     (a);                    //  In check ?
        JR      (Z,FM35);               //  No - jump
        JP      (FM19);                 //  Jump (One more ply for check)
FM18:   LD      (ix,chk(MLPTRJ));       //  Load move pointer
        LD      (a,ptr(ix+MLVAL));      //  Get move score
        AND     (a);                    //  Is it zero (illegal move) ?
        JR      (Z,FM15);               //  Yes - jump
        CALL    (MOVE);                 //  Execute move on board array
FM19:   LD      (hl,addr(COLOR));       //  Toggle color
        LD      (a,0x80);
        XOR     (ptr(hl));
        LD      (ptr(hl),a);            //  Save new color
        BIT     (7,a);                  //  Is it white ?
        JR      (NZ,rel018);            //  No - jump
        LD      (hl,addr(MOVENO));      //  Increment move number
        INC     (ptr(hl));
rel018: LD      (hl,chk(SCRIX));        //  Load score table pointer
        LD      (a,ptr(hl));            //  Get score two plys above
        INC     (hl);                   //  Increment to current ply
        INC     (hl);
        LD      (ptr(hl),a);            //  Save score as initial value
        DEC     (hl);                   //  Decrement pointer
        LD      (chk(SCRIX),hl);        //  Save it
        JP      (FM5);                  //  Jump
FM25:   LD      (MATEF);                //  Get mate flag
        AND     (a);                    //  Checkmate or stalemate ?
        JR      (NZ,FM30);              //  No - jump
        LD      (a,CKFLG);              //  Get check flag
        AND     (a);                    //  Was King in check ?
        LD      (a,0x80);               //  Pre-set stalemate score
        JR      (Z,FM36);               //  No - jump (stalemate)
        LD      (a,MOVENO);             //  Get move number
        LD      (PMATE,a);              //  Save
        LD      (a,0xFF);               //  Pre-set checkmate score
        JP      (FM36);                 //  Jump
FM30:   LD      (a,NPLY);               //  Get ply counter
        CP      (1);                    //  At top of tree ?
        RET     (Z);                    //  Yes - return
        CALL    (ASCEND);               //  Ascend one ply in tree
        LD      (hl,chk(SCRIX));        //  Load score table pointer
        INC     (hl);                   //  Increment to current ply
        INC     (hl);                   //
        LD      (a,ptr(hl));            //  Get score
        DEC     (hl);                   //  Restore pointer
        DEC     (hl);                   //
        JP      (FM37);                 //  Jump
FM35:   CALL    (PINFND);               //  Compile pin list
        CALL    (POINTS);               //  Evaluate move
        CALL    (UNMOVE);               //  Restore board position
        LD      (a,VALM);               //  Get value of move
FM36:   LD      (hl,addr(MATEF));       //  Set mate flag
        SET     (0,ptr(hl));            //
        LD      (hl,chk(SCRIX));        //  Load score table pointer
FM37:
        CP      (ptr(hl));              //  Compare to score 2 ply above
        JR      (C,FM40);               //  Jump if less
        JR      (Z,FM40);               //  Jump if equal
        NEG;                            //  Negate score
        INC     (hl);                   //  Incr score table pointer
        CP      (ptr(hl));              //  Compare to score 1 ply above
        JP      (C,FM15);               //  Jump if less than
        JP      (Z,FM15);               //  Jump if equal
        LD      (ptr(hl),a);            //  Save as new score 1 ply above
        LD      (a,NPLY);               //  Get current ply counter
        CP      (1);                    //  At top of tree ?
        JP      (NZ,FM15);              //  No - jump
        LD      (hl,chk(MLPTRJ));       //  Load current move pointer
        LD      (chk(BESTM),hl);        //  Save as best move pointer
        LD      (a,*(&(SCORE+1)));      //  Get best move score
        CP      (0xff);                 //  Was it a checkmate ?
        JP      (NZ,FM15);              //  No - jump
        LD      (hl,addr(PLYMAX));      //  Get maximum ply number
        DEC     (ptr(hl));              //  Subtract 2
        DEC     (ptr(hl));
        LD      (a,KOLOR);              //  Get computer's color
        BIT     (7,a);                  //  Is it white ?
        RET     (Z);                    //  Yes - return
        LD      (hl,addr(PMATE));       //  Checkmate move number
        DEC     (ptr(hl));              //  Decrement
        RET     ();                     //  Return
FM40:   CALL    (ASCEND);               //  Ascend one ply in tree
        JP      (FM15);                 //  Jump

//***********************************************************
// ASCEND TREE ROUTINE
//***********************************************************
// FUNCTION:  --  To adjust all necessary parameters to
//                ascend one ply in the tree.
;
// CALLED BY: --  FNDMOV
;
// CALLS:     --  UNMOVE
;
// ARGUMENTS: --  None
//***********************************************************
ASCEND: LD      (hl,addr(COLOR));       //  Toggle color
        LD      (a,0x80);               //
        XOR     (ptr(hl));              //
        LD      (ptr(hl),a);            //  Save new color
        BIT     (7,a);                  //  Is it white ?
        JR      (Z,rel019);             //  Yes - jump
        LD      (hl,addr(MOVENO));      //  Decrement move number
        DEC     (ptr(hl));              //
rel019: LD      (hl,chk(SCRIX));        //  Load score table index
        DEC     (hl);                   //  Decrement
        LD      (chk(SCRIX),hl);        //  Save
        LD      (hl,addr(NPLY));        //  Decrement ply counter
        DEC     (ptr(hl));              //
        LD      (hl,chk(MLPTRI));       //  Load ply list pointer
        DEC     (hl);                   //  Load pointer to move list top
        LD      (d,ptr(hl));            //
        DEC     (hl);                   //
        LD      (e,ptr(hl));            //
        LD      (chk(MLNXT),de);        //  Update move list avail ptr
        DEC     (hl);                   //  Get ptr to next move to undo
        LD      (d,ptr(hl));            //
        DEC     (hl);                   //
        LD      (e,ptr(hl));            //
        LD      (chk(MLPTRI),hl);       //  Save new ply list pointer
        LD      (chk(MLPTRJ),de);       //  Save next move pointer
        CALL    (UNMOVE);               //  Restore board to previous ply
        RET;                            //  Return

//***********************************************************
// ONE MOVE BOOK OPENING
// **********************************************************
// FUNCTION:   --  To provide an opening book of a single
//                 move.
;
// CALLED BY:  --  FNDMOV
;
// CALLS:      --  None
;
// ARGUMENTS:  --  None
//***********************************************************
BOOK:   POP     (af);                   //  Abort return to FNDMOV
        LD      (hl,addr(SCORE)+1);     //  Zero out score
        LD      (ptr(hl),0);            //  Zero out score table
        LD      (hl,addr(BMOVES)-2);    //  Init best move ptr to book
        LD      (chk(BESTM),hl);        //
        LD      (hl,addr(BESTM));       //  Initialize address of pointer
        LD      (a,KOLOR);              //  Get computer's color
        AND     (a);                    //  Is it white ?
        JR      (NZ,BM5);               //  No - jump
        LD      (a,rand());             //  Load refresh reg (random no)
        BIT     (0,a);                  //  Test random bit
        RET     (Z);                    //  Return if zero (P-K4)
        INC     (ptr(hl));              //  P-Q4
        INC     (ptr(hl));              //
        INC     (ptr(hl));              //
        RET;                            //  Return
BM5:    INC     (ptr(hl));              //  Increment to black moves
        INC     (ptr(hl));              //
        INC     (ptr(hl));              //
        INC     (ptr(hl));              //
        INC     (ptr(hl));              //
        INC     (ptr(hl));              //
        LD      (ix,chk(MLPTRJ));       //  Pointer to opponents 1st move
        LD      (a,ptr(ix+MLFRP));      //  Get "from" position
        CP      (22);                   //  Is it a Queen Knight move ?
        JR      (Z,BM9);                //  Yes - Jump
        CP      (27);                   //  Is it a King Knight move ?
        JR      (Z,BM9);                //  Yes - jump
        CP      (34);                   //  Is it a Queen Pawn ?
        JR      (Z,BM9);                //  Yes - jump
        RET     (C);                    //  If Queen side Pawn opening -
// return (P-K4)
        CP      (35);                   //  Is it a King Pawn ?
        RET     (Z);                    //  Yes - return (P-K4)
BM9:    INC     (ptr(hl));              //  (P-Q4)
        INC     (ptr(hl));              //
        INC     (ptr(hl));              //
        RET;                            //  Return to CPTRMV

//
//  Omit some Z80 User Interface code, eg
//  user interface data including graphics data and text messages
//  Also macros
//		CARRET, CLRSCR, PRTLIN, PRTBLK and EXIT
//	and functions
//		DRIVER and INTERR
//

//***********************************************************
// COMPUTER MOVE ROUTINE
//***********************************************************
// FUNCTION:   --  To control the search for the computers move
//                 and the display of that move on the board
//                 and in the move list.
;
// CALLED BY:  --  DRIVER
;
// CALLS:      --  FNDMOV
//                 FCDMAT
//                 MOVE
//                 EXECMV
//                 BITASN
//                 INCHK
;
// MACRO CALLS:    PRTBLK
//                 CARRET
;
// ARGUMENTS:  --  None
//***********************************************************
CPTRMV: CALL    (FNDMOV);               //  Select best move
        LD      (hl,chk(BESTM));        //  Move list pointer variable
        LD      (chk(MLPTRJ),hl);       //  Pointer to move data
        LD      (a,*(&SCORE+1));        //  To check for mates
        CP      (1);                    //  Mate against computer ?
        JR      (NZ,CP0C);              //  No - jump
        LD      (c,1);                  //  Computer mate flag
        CALL    (FCDMAT);               //  Full checkmate ?
CP0C:   CALL    (MOVE);                 //  Produce move on board array
        CALL    (EXECMV);               //  Make move on graphics board
// and return info about it
        LD      (a,b);                  //  Special move flags
        AND     (a);                    //  Special ?
        JR      (NZ,CP10);              //  Yes - jump
        LD      (d,e);                  //  "To" position of the move
        CALL    (BITASN);               //  Convert to Ascii
        LD      (chk(MVEMSG+3),hl);     //  Put in move message
        LD      (d,c);                  //  "From" position of the move
        CALL    (BITASN);               //  Convert to Ascii
        LD      (chk(MVEMSG),hl);       //  Put in move message
        PRTBLK  (MVEMSG,5);             //  Output text of move
        JR      (CP1C);                 //  Jump
CP10:   BIT     (1,b);                  //  King side castle ?
        JR      (Z,rel020);             //  No - jump
        PRTBLK  (O_O,5);                //  Output "O-O"
        JR      (CP1C);                 //  Jump
rel020: BIT     (2,b);                  //  Queen side castle ?
        JR      (Z,rel021);             //  No - jump
        PRTBLK  (O_O_O,5);              //  Output "O-O-O"
        JR      (CP1C);                 //  Jump
rel021: PRTBLK  (P_PEP,5);              //  Output "PxPep" - En passant
CP1C:   LD      (a,COLOR);              //  Should computer call check ?
        LD      (b,a);
        XOR     (80H);                  //  Toggle color
        LD      (COLOR,a);
        CALL    (INCHK);                //  Check for check
        AND     (a);                    //  Is enemy in check ?
        LD      (a,b);                  //  Restore color
        LD      (COLOR,a);
        JR      (Z,CP24);               //  No - return
        CARRET;                         //  New line
        LD      (a,*(&SCORE+1));        //  Check for player mated
        CP      (0xFF);                 //  Forced mate ?
        CALL    (NZ,TBCPMV);            //  No - Tab to computer column
        PRTBLK  (CKMSG,5);              //  Output "check"
        LD      (hl,addr(LINECT));      //  Address of screen line count
        INC     (ptr(hl));              //  Increment for message
CP24:   LD      (a,*(&SCORE+1));        //  Check again for mates
        CP      (0xFF);                 //  Player mated ?
        RET     (NZ);                   //  No - return
        LD      (c,0);                  //  Set player mate flag
        CALL    (FCDMAT);               //  Full checkmate ?
        RET;                            //  Return

//
//	Omit some more Z80 user interface stuff, functions
//  FCDMAT, TBPLCL, TBCPCL, TBPLMV and TBCPMV
//

//***********************************************************
// BOARD INDEX TO ASCII SQUARE NAME
//***********************************************************
// FUNCTION:   --  To translate a hexadecimal index in the
//                 board array into an ascii description
//                 of the square in algebraic chess notation.
;
// CALLED BY:  --  CPTRMV
;
// CALLS:      --  DIVIDE
;
// ARGUMENTS:  --  Board index input in register D and the
//                 Ascii square name is output in register
//                 pair HL.
//***********************************************************
BITASN: SUB     (a);                    //  Get ready for division
        LD      (e,10);                 //
        CALL    (DIVIDE);               //  Divide
        DEC     (d);                    //  Get rank on 1-8 basis
        ADD     (a,60H);                //  Convert file to Ascii (a-h)
        LD      (l,a);                  //  Save
        LD      (a,d);                  //  Rank
        ADD     (a,30H);                //  Convert rank to Ascii (1-8)
        LD      (h,a);                  //  Save
        RET;                            //  Return

//
//	Omit some more Z80 user interface stuff, function
//  PLYRMV
//

//***********************************************************
// ASCII SQUARE NAME TO BOARD INDEX
//***********************************************************
// FUNCTION:   --  To convert an algebraic square name in
//                 Ascii to a hexadecimal board index.
//                 This routine also checks the input for
//                 validity.
;
// CALLED BY:  --  PLYRMV
;
// CALLS:      --  MLTPLY
;
// ARGUMENTS:  --  Accepts the square name in register pair HL
//                 and outputs the board index in register A.
//                 Register B = 0 if ok. Register B = Register
//                 A if invalid.
//***********************************************************
ASNTBI: LD      (a,l);                  //  Ascii rank (1 - 8)
        SUB     (30H);                  //  Rank 1 - 8
        CP      (1);                    //  Check lower bound
        JP      (M,AT04);               //  Jump if invalid
        CP      (9);                    //  Check upper bound
        JR      (NC,AT04);              //  Jump if invalid
        INC     (a);                    //  Rank 2 - 9
        LD      (d,a);                  //  Ready for multiplication
        LD      (e,10);
        CALL    (MLTPLY);               //  Multiply
        LD      (a,h);                  //  Ascii file letter (a - h)
        SUB     (40H);                  //  File 1 - 8
        CP      (1);                    //  Check lower bound
        JP      (M,AT04);               //  Jump if invalid
        CP      (9);                    //  Check upper bound
        JR      (NC,AT04);              //  Jump if invalid
        ADD     (a,d);                  //  File+Rank(20-90)=Board index
        LD      (b,0);                  //  Ok flag
        RET;                            //  Return
AT04:   LD      (b,a);                  //  Invalid flag
        RET;                            //  Return

//***********************************************************
// VALIDATE MOVE SUBROUTINE
//***********************************************************
// FUNCTION:   --  To check a players move for validity.
;
// CALLED BY:  --  PLYRMV
;
// CALLS:      --  GENMOV
//                 MOVE
//                 INCHK
//                 UNMOVE
;
// ARGUMENTS:  --  Returns flag in register A, 0 for valid
//                 and 1 for invalid move.
//***********************************************************
VALMOV: LD      (hl,chk(MLPTRJ));       //  Save last move pointer
        PUSH    (hl);                   //  Save register
        LD      (a,KOLOR);              //  Computers color
        XOR     (0x80);                 //  Toggle color
        LD      (COLOR,a);              //  Store
        LD      (hl,addr(PLYIX)-2);     //  Load move list index
        LD      (chk(MLPTRI),hl);       //
        LD      (hl,addr(MLIST)+1024);  //  Next available list pointer
        LD      (chk(MLNXT),hl);        //
        CALL    (GENMOV);               //  Generate opponents moves
        LD      (ix,addr(MLIST)+1024);  //  Index to start of moves
VA5:    LD      (a,MVEMSG);             //  "From" position
        CP      (ptr(ix+MLFRP));        //  Is it in list ?
        JR      (NZ,VA6);               //  No - jump
        LD      (a,*(&MVEMSG+1));       //  "To" position
        CP      (ptr(ix+MLTOP));        //  Is it in list ?
        JR      (Z,VA7);                //  Yes - jump
VA6:    LD      (e,ptr(ix+MLPTR));      //  Pointer to next list move
        LD      (d,ptr(ix+MLPTR+1));    //
        XOR     (a);                    //  At end of list ?
        CP      (d);                    //
        JR      (Z,VA10);               //  Yes - jump
        PUSH    (de);                   //  Move to X register
        POP     (ix);                   //
        JR      (VA5);                  //  Jump
VA7:    LD      (chk(MLPTRJ),ix);       //  Save opponents move pointer
        CALL    (MOVE);                 //  Make move on board array
        CALL    (INCHK);                //  Was it a legal move ?
        AND     (a);                    //
        JR      (NZ,VA9);               //  No - jump
VA8:    POP     (hl);                   //  Restore saved register
        RET;                            //  Return
VA9:    CALL    (UNMOVE);               //  Un-do move on board array
VA10:   LD      (a,1);                  //  Set flag for invalid move
        POP     (hl);                   //  Restore saved register
        LD      (chk(MLPTRJ),hl);       //  Save move pointer
        RET;                            //  Return

//
//	Omit some more Z80 user interface stuff, functions
//  CHARTR, PGIFND, MATED, ANALYS
//

//***********************************************************
// UPDATE POSITIONS OF ROYALTY
//***********************************************************
// FUNCTION:   --  To update the positions of the Kings
//                 and Queen after a change of board position
//                 in ANALYS.
;
// CALLED BY:  --  ANALYS
;
// CALLS:      --  None
;
// ARGUMENTS:  --  None
//***********************************************************
ROYALT: LD      hl,POSK;                //  Start of Royalty array
        LD      b,4;                    //  Clear all four positions
back06: LD      (hl),0;                 //
        INC     hl;                     //
        DJNZ    back06;                 //
        LD      a,21;                   //  First board position
RY04:   LD      (M1),a;                 //  Set up board index
        LD      hl,POSK;                //  Address of King position
        LD      ix,(M1);                //
        LD      a,(ix+BOARD);           //  Fetch board contents
        BIT     7,a;                    //  Test color bit
        JR      Z,rel023;               //  Jump if white
        INC     hl;                     //  Offset for black
rel023: AND     7;                      //  Delete flags, leave piece
        CP      KING;                   //  King ?
        JR      Z,RY08;                 //  Yes - jump
        CP      QUEEN;                  //  Queen ?
        JR      NZ,RY0C;                //  No - jump
        INC     hl;                     //  Queen position
        INC     hl;                     //  Plus offset
RY08:   LD      a,(M1);                 //  Index
        LD      (hl),a;                 //  Save
RY0C:   LD      a,(M1);                 //  Current position
        INC     a;                      //  Next position
        CP      99;                     //  Done.?
        JR      NZ,RY04;                //  No - jump
        RET;                            //  Return

//
//	Omit some more Z80 user interface stuff, functions
//  DSPBRD, BSETUP, INSPCE, CONVRT
//

//***********************************************************
// POSITIVE INTEGER DIVISION
//   inputs hi=A lo=D, divide by E
//   output D, remainder in A
//***********************************************************
DIVIDE: PUSH    (bc);                   //
        LD      (b,8);                  //
DD04:   SLA     (d);                    //
        RLA;                            //
        SUB     (e);                    //
        JP      (M,rel027);             //
        INC     (d);                    //
        JR      (rel024);               //
rel027: ADD     (a,e);                  //
rel024: DJNZ    (DD04);                 //
        POP     (bc);                   //
        RET;                            //

//***********************************************************
// POSITIVE INTEGER MULTIPLICATION
//   inputs D, E
//   output hi=A lo=D
//***********************************************************
MLTPLY: PUSH    (bc);                   //
        SUB     (a);                    //
        LD      (b,8);                  //
ML04:   BIT     (0,d);                  //
        JR      (Z,rel025);             //
        ADD     (a,e);                  //
rel025: SRA     (a);                    //
        RR      (d);                    //
        DJNZ    (ML04);                 //
        POP     (bc);                   //
        RET;                            //

//
//	Omit some more Z80 user interface stuff, function
//  BLNKER
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
;
// CALLED BY:   -- PLYRMV
//                 CPTRMV
;
// CALLS:       -- MAKEMV
;
// ARGUMENTS:   -- Flags set in the B register as described
//                 above.
//***********************************************************
EXECMV: PUSH    (ix);                   //  Save registers
        PUSH    (af);                   //
        LD      (ix,chk(MLPTRJ));       //  Index into move list
        LD      (c,ptr(ix+MLFRP));      //  Move list "from" position
        LD      (e,ptr(ix+MLTOP));      //  Move list "to" position
        CALL    (MAKEMV);               //  Produce move
        LD      (d,ptr(ix+MLFLG));      //  Move list flags
        LD      (b,0);                  //
        BIT     (6,d);                  //  Double move ?
        JR      (Z,EX14);               //  No - jump
        LD      (de,6);                 //  Move list entry width
        ADD     (ix,de);                //  Increment MLPTRJ
        LD      (c,ptr(ix+MLFRP));      //  Second "from" position
        LD      (e,ptr(ix+MLTOP));      //  Second "to" position
        LD      (a,e);                  //  Get "to" position
        CP      (c);                    //  Same as "from" position ?
        JR      (NZ,EX04);              //  No - jump
        INC     (b);                    //  Set en passant flag
        JR      (EX10);                 //  Jump
EX04:   CP      (1AH);                  //  White O-O ?
        JR      (NZ,EX08);              //  No - jump
        SET     (1,b);                  //  Set O-O flag
        JR      (EX10);                 //  Jump
EX08:   CP      (60H);                  //  Black 0-0 ?
        JR      (NZ,EX0C);              //  No - jump
        SET     (1,b);                  //  Set 0-0 flag
        JR      (EX10);                 //  Jump
EX0C:   SET     (2,b);                  //  Set 0-0-0 flag
EX10:   CALL    (MAKEMV);               //  Make 2nd move on board
EX14:   POP     (af);                   //  Restore registers
        POP     (ix);                   //
        RET;                            //  Return

//
//	Omit some more Z80 user interface stuff, function
//  MAKEMV
//
