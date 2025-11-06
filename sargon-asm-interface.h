#ifndef SARGON_ASM_INTERFACE_H_INCLUDED
#define SARGON_ASM_INTERFACE_H_INCLUDED
#include <stdint.h>

// First byte of Sargon data
extern unsigned char *sargon_base_address;

// Calls to sargon() can set and read back registers
struct z80_registers_mini
{
    uint16_t af;    // x86 = lo al, hi flags
    uint16_t hl;    // x86 = bx
    uint16_t bc;    // x86 = cx
    uint16_t de;    // x86 = dx
    uint16_t ix;    // x86 = si
    uint16_t iy;    // x86 = di
};

// Call Sargon from C, call selected functions, optionally can set input
//  registers (and/or inspect returned registers)
void sargon( int api_command_code, z80_registers_mini *registers=NULL );

// Sargon calls C, parameters serves double duty - saved registers on the
//  stack, can optionally be inspected by C program
void callback( uint32_t edi, uint32_t esi, uint32_t ebp, uint32_t esp,
                uint32_t ebx, uint32_t edx, uint32_t ecx, uint32_t eax,
                uint32_t eflags );

// Data offsets for peeking and poking
const int BOARDA = 0x0134; // (0x0134 in sargon_x86.asm)
const int ATKLST = 0x01ac; // (0x01ac in sargon_x86.asm)
const int PLISTA = 0x01ba; // (0x01ba in sargon_x86.asm)
const int POSK   = 0x01ce; // (0x01ce in sargon_x86.asm)
const int POSQ   = 0x01d0; // (0x01d0 in sargon_x86.asm)
const int SCORE  = 0x01d4; // (0x0200 in sargon_x86.asm)
const int PLYIX  = 0x01fc; // (0x022a in sargon_x86.asm)
const int M1     = 0x0224; // (0x0300 in sargon_x86.asm)
const int M2     = 0x0226; // (0x0302 in sargon_x86.asm)
const int M3     = 0x0228; // (0x0304 in sargon_x86.asm)
const int M4     = 0x022a; // (0x0306 in sargon_x86.asm)
const int T1     = 0x022c; // (0x0308 in sargon_x86.asm)
const int T2     = 0x022e; // (0x030a in sargon_x86.asm)
const int T3     = 0x0230; // (0x030c in sargon_x86.asm)
const int INDX1  = 0x0232; // (0x030e in sargon_x86.asm)
const int INDX2  = 0x0234; // (0x0310 in sargon_x86.asm)
const int NPINS  = 0x0236; // (0x0312 in sargon_x86.asm)
const int MLPTRI = 0x0238; // (0x0314 in sargon_x86.asm)
const int MLPTRJ = 0x023a; // (0x0316 in sargon_x86.asm)
const int SCRIX  = 0x023c; // (0x0318 in sargon_x86.asm)
const int BESTM  = 0x023e; // (0x031a in sargon_x86.asm)
const int MLLST  = 0x0240; // (0x031c in sargon_x86.asm)
const int MLNXT  = 0x0242; // (0x031e in sargon_x86.asm)
const int KOLOR  = 0x0244; // (0x0320 in sargon_x86.asm)
const int COLOR  = 0x0245; // (0x0321 in sargon_x86.asm)
const int P1     = 0x0246; // (0x0322 in sargon_x86.asm)
const int P2     = 0x0247; // (0x0323 in sargon_x86.asm)
const int P3     = 0x0248; // (0x0324 in sargon_x86.asm)
const int PMATE  = 0x0249; // (0x0325 in sargon_x86.asm)
const int MOVENO = 0x024a; // (0x0326 in sargon_x86.asm)
const int PLYMAX = 0x024b; // (0x0327 in sargon_x86.asm)
const int NPLY   = 0x024c; // (0x0328 in sargon_x86.asm)
const int CKFLG  = 0x024d; // (0x0329 in sargon_x86.asm)
const int MATEF  = 0x024e; // (0x032a in sargon_x86.asm)
const int VALM   = 0x024f; // (0x032b in sargon_x86.asm)
const int BRDC   = 0x0250; // (0x032c in sargon_x86.asm)
const int PTSL   = 0x0251; // (0x032d in sargon_x86.asm)
const int PTSW1  = 0x0252; // (0x032e in sargon_x86.asm)
const int PTSW2  = 0x0253; // (0x032f in sargon_x86.asm)
const int MTRL   = 0x0254; // (0x0330 in sargon_x86.asm)
const int BC0    = 0x0255; // (0x0331 in sargon_x86.asm)
const int MV0    = 0x0256; // (0x0332 in sargon_x86.asm)
const int PTSCK  = 0x0257; // (0x0333 in sargon_x86.asm)
const int BMOVES = 0x0258; // (0x0334 in sargon_x86.asm)
const int LINECT = 0x0271; // (0x0340 in sargon_x86.asm)
const int MVEMSG = 0x0264; // (0x0341 in sargon_x86.asm)
const int MLIST  = 0x0272; // (0x0400 in sargon_x86.asm)
const int MLEND  = 0x0a6a; // (0xee60 in sargon_x86.asm)

// API constants
const int api_INITBD = 1;
const int api_ROYALT = 2;
const int api_CPTRMV = 3;
const int api_VALMOV = 4;
const int api_ASNTBI = 5;
const int api_EXECMV = 6;

#endif //SARGON_ASM_INTERFACE_H_INCLUDED
