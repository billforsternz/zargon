//
//  Later - Data part of zargon.cpp
//
#ifndef ZARGON_H_INCLUDED
#define ZARGON_H_INCLUDED
#include <stdio.h>

// Later Instantiate this class to create definitions to paste into sargon-asm-interface.h
#if 0
#define addr(var)       offsetof(emulated_memory,var)
class zargon_data_defs_regen
{
    zargon_data_defs_regen()
    {
        printf( "const int BOARDA = 0x%04zx; // (0x0134 in sargon_x86.asm)\n", addr(BOARDA) );
        printf( "const int ATKLST = 0x%04zx; // (0x01ac in sargon_x86.asm)\n", addr(ATKLST) );
        printf( "const int PLISTA = 0x%04zx; // (0x01ba in sargon_x86.asm)\n", addr(PLISTA) );
        printf( "const int POSK   = 0x%04zx; // (0x01ce in sargon_x86.asm)\n", addr(POSK) );
        printf( "const int POSQ   = 0x%04zx; // (0x01d0 in sargon_x86.asm)\n", addr(POSQ) );
        printf( "const int SCORE  = 0x%04zx; // (0x0200 in sargon_x86.asm)\n", addr(SCORE) );
        printf( "const int PLYIX  = 0x%04zx; // (0x022a in sargon_x86.asm)\n", addr(PLYIX) );
        printf( "const int M1     = 0x%04zx; // (0x0300 in sargon_x86.asm)\n", addr(M1) );
        printf( "const int M2     = 0x%04zx; // (0x0302 in sargon_x86.asm)\n", addr(M2) );
        printf( "const int M3     = 0x%04zx; // (0x0304 in sargon_x86.asm)\n", addr(M3) );
        printf( "const int M4     = 0x%04zx; // (0x0306 in sargon_x86.asm)\n", addr(M4) );
        printf( "const int T1     = 0x%04zx; // (0x0308 in sargon_x86.asm)\n", addr(T1) );
        printf( "const int T2     = 0x%04zx; // (0x030a in sargon_x86.asm)\n", addr(T2) );
        printf( "const int T3     = 0x%04zx; // (0x030c in sargon_x86.asm)\n", addr(T3) );
        printf( "const int INDX1  = 0x%04zx; // (0x030e in sargon_x86.asm)\n", addr(INDX1) );
        printf( "const int INDX2  = 0x%04zx; // (0x0310 in sargon_x86.asm)\n", addr(INDX2) );
        printf( "const int NPINS  = 0x%04zx; // (0x0312 in sargon_x86.asm)\n", addr(NPINS) );
        printf( "const int MLPTRI = 0x%04zx; // (0x0314 in sargon_x86.asm)\n", addr(MLPTRI) );
        printf( "const int MLPTRJ = 0x%04zx; // (0x0316 in sargon_x86.asm)\n", addr(MLPTRJ) );
        printf( "const int SCRIX  = 0x%04zx; // (0x0318 in sargon_x86.asm)\n", addr(SCRIX) );
        printf( "const int BESTM  = 0x%04zx; // (0x031a in sargon_x86.asm)\n", addr(BESTM) );
        printf( "const int MLLST  = 0x%04zx; // (0x031c in sargon_x86.asm)\n", addr(MLLST) );
        printf( "const int MLNXT  = 0x%04zx; // (0x031e in sargon_x86.asm)\n", addr(MLNXT) );
        printf( "const int KOLOR  = 0x%04zx; // (0x0320 in sargon_x86.asm)\n", addr(KOLOR) );
        printf( "const int COLOR  = 0x%04zx; // (0x0321 in sargon_x86.asm)\n", addr(COLOR) );
        printf( "const int P1     = 0x%04zx; // (0x0322 in sargon_x86.asm)\n", addr(P1) );
        printf( "const int P2     = 0x%04zx; // (0x0323 in sargon_x86.asm)\n", addr(P2) );
        printf( "const int P3     = 0x%04zx; // (0x0324 in sargon_x86.asm)\n", addr(P3) );
        printf( "const int PMATE  = 0x%04zx; // (0x0325 in sargon_x86.asm)\n", addr(PMATE) );
        printf( "const int MOVENO = 0x%04zx; // (0x0326 in sargon_x86.asm)\n", addr(MOVENO) );
        printf( "const int PLYMAX = 0x%04zx; // (0x0327 in sargon_x86.asm)\n", addr(PLYMAX) );
        printf( "const int NPLY   = 0x%04zx; // (0x0328 in sargon_x86.asm)\n", addr(NPLY) );
        printf( "const int CKFLG  = 0x%04zx; // (0x0329 in sargon_x86.asm)\n", addr(CKFLG) );
        printf( "const int MATEF  = 0x%04zx; // (0x032a in sargon_x86.asm)\n", addr(MATEF) );
        printf( "const int VALM   = 0x%04zx; // (0x032b in sargon_x86.asm)\n", addr(VALM) );
        printf( "const int BRDC   = 0x%04zx; // (0x032c in sargon_x86.asm)\n", addr(BRDC) );
        printf( "const int PTSL   = 0x%04zx; // (0x032d in sargon_x86.asm)\n", addr(PTSL) );
        printf( "const int PTSW1  = 0x%04zx; // (0x032e in sargon_x86.asm)\n", addr(PTSW1) );
        printf( "const int PTSW2  = 0x%04zx; // (0x032f in sargon_x86.asm)\n", addr(PTSW2) );
        printf( "const int MTRL   = 0x%04zx; // (0x0330 in sargon_x86.asm)\n", addr(MTRL) );
        printf( "const int BC0    = 0x%04zx; // (0x0331 in sargon_x86.asm)\n", addr(BC0) );
        printf( "const int MV0    = 0x%04zx; // (0x0332 in sargon_x86.asm)\n", addr(MV0) );
        printf( "const int PTSCK  = 0x%04zx; // (0x0333 in sargon_x86.asm)\n", addr(PTSCK) );
        printf( "const int BMOVES = 0x%04zx; // (0x0334 in sargon_x86.asm)\n", addr(BMOVES) );
        printf( "const int LINECT = 0x%04zx; // (0x0340 in sargon_x86.asm)\n", addr(LINECT) );
        printf( "const int MVEMSG = 0x%04zx; // (0x0341 in sargon_x86.asm)\n", addr(MVEMSG) );
        printf( "const int MLIST  = 0x%04zx; // (0x0400 in sargon_x86.asm)\n", addr(MLIST) );
        printf( "const int MLEND  = 0x%04zx; // (0xee60 in sargon_x86.asm)\n", addr(MLEND) );
    }
 };
#endif

 #endif // ZARGON_H_INCLUDED
