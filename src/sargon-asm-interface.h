#ifndef SARGON_ASM_INTERFACE_H_INCLUDED
#define SARGON_ASM_INTERFACE_H_INCLUDED
#include <stdint.h>
#include "z80_registers.h"
#include "z80_cpu.h"
#include "bridge.h"

// First byte of Sargon data
extern unsigned char *sargon_base_address;

// Call Sargon core (original z80 code, now translated, call selected
//  functions, optionally can set input registers (and/or inspect
//  returned registers)
// Returns bool ok (used for api_VALMOV only)
bool sargon( int api_command_code, z80_registers *registers=NULL );

// Data offsets for peeking and poking
const int BOARDA = 0x0134; // (0x0134 in sargon_x86.asm)
const int ATKLST = 0x01ac; // (0x01ac in sargon_x86.asm)
const int PLISTA = 0x01ba; // (0x01ba in sargon_x86.asm)
const int POSK   = 0x01ce; // (0x01ce in sargon_x86.asm)
const int POSQ   = 0x01d0; // (0x01d0 in sargon_x86.asm)
const int SCORE  = 0x0200; // (0x0200 in sargon_x86.asm)
const int PLYIX  = 0x022a; // (0x022a in sargon_x86.asm)
const int M1     = 0x0300; // (0x0300 in sargon_x86.asm)
const int M2     = 0x0302; // (0x0302 in sargon_x86.asm)
const int M3     = 0x0304; // (0x0304 in sargon_x86.asm)
const int M4     = 0x0306; // (0x0306 in sargon_x86.asm)
const int T1     = 0x0308; // (0x0308 in sargon_x86.asm)
const int T2     = 0x030a; // (0x030a in sargon_x86.asm)
const int T3     = 0x030c; // (0x030c in sargon_x86.asm)
const int INDX1  = 0x030e; // (0x030e in sargon_x86.asm)
const int INDX2  = 0x0310; // (0x0310 in sargon_x86.asm)
const int NPINS  = 0x0312; // (0x0312 in sargon_x86.asm)
const int MLPTRI = 0x0314; // (0x0314 in sargon_x86.asm)
const int MLPTRJ = 0x0316; // (0x0316 in sargon_x86.asm)
const int SCRIX  = 0x0318; // (0x0318 in sargon_x86.asm)
const int BESTM  = 0x031a; // (0x031a in sargon_x86.asm)
const int MLLST  = 0x031c; // (0x031c in sargon_x86.asm)
const int MLNXT  = 0x031e; // (0x031e in sargon_x86.asm)
const int KOLOR  = 0x0320; // (0x0320 in sargon_x86.asm)
const int COLOR  = 0x0321; // (0x0321 in sargon_x86.asm)
const int P1     = 0x0322; // (0x0322 in sargon_x86.asm)
const int P2     = 0x0323; // (0x0323 in sargon_x86.asm)
const int P3     = 0x0324; // (0x0324 in sargon_x86.asm)
const int PMATE  = 0x0325; // (0x0325 in sargon_x86.asm)
const int MOVENO = 0x0326; // (0x0326 in sargon_x86.asm)
const int PLYMAX = 0x0327; // (0x0327 in sargon_x86.asm)
const int NPLY   = 0x0328; // (0x0328 in sargon_x86.asm)
const int CKFLG  = 0x0329; // (0x0329 in sargon_x86.asm)
const int MATEF  = 0x032a; // (0x032a in sargon_x86.asm)
const int VALM   = 0x032b; // (0x032b in sargon_x86.asm)
const int BRDC   = 0x032c; // (0x032c in sargon_x86.asm)
const int PTSL   = 0x032d; // (0x032d in sargon_x86.asm)
const int PTSW1  = 0x032e; // (0x032e in sargon_x86.asm)
const int PTSW2  = 0x032f; // (0x032f in sargon_x86.asm)
const int MTRL   = 0x0330; // (0x0330 in sargon_x86.asm)
const int BC0    = 0x0331; // (0x0331 in sargon_x86.asm)
const int MV0    = 0x0332; // (0x0332 in sargon_x86.asm)
const int PTSCK  = 0x0333; // (0x0333 in sargon_x86.asm)
const int BMOVES = 0x0334; // (0x0334 in sargon_x86.asm)
const int LINECT = 0x0340; // (0x0340 in sargon_x86.asm)
const int MVEMSG = 0x0341; // (0x0341 in sargon_x86.asm)
const int MLIST  = 0x0400; // (0x0400 in sargon_x86.asm)
const int MLEND  = 0xee60; // (0xee60 in sargon_x86.asm)

#endif //SARGON_ASM_INTERFACE_H_INCLUDED
