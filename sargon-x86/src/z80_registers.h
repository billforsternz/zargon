//
//  Basic z80 register set
//
#ifndef Z80_REGISTERS_H_INCLUDED
#define Z80_REGISTERS_H_INCLUDED

struct z80_registers
{
    uint16_t af;    // Use the x86 convention, lo = al, hi flags, not the
                    //  Z80 hardware convention, for x86 interoperability.
                    //  The Sargon Z80 assembly code doesn't care.
    uint16_t hl;    // x86 = bx
    uint16_t bc;    // x86 = cx
    uint16_t de;    // x86 = dx
    uint16_t ix;    // x86 = si
    uint16_t iy;    // x86 = di
};

#endif // Z80_REGISTERS_H_INCLUDED
