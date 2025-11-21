#ifndef Z80_CPU_H_INCLUDED
#define Z80_CPU_H_INCLUDED
#include <stdint.h>
#include "z80_registers.h"

// Emulate Z80 machine
struct z80_cpu
{
    union
    {
        struct
        {
            uint8_t A;    // Use the x86 convention, lo = al, hi flags, not the
            uint8_t F;    //  Z80 hardware convention, for x86 interoperability.
        };                //  The Sargon Z80 assembly code doesn't do anything
                          //  that cares one way or the other.
        uint16_t AF;
    };
    union
    {
        struct
        {
            // We are assuming a little endian machine, C is the ls byte of BC
            uint8_t C;
            uint8_t B;
        };
        uint16_t BC;
    };
    union
    {
        struct
        {
            uint8_t E;
            uint8_t D;
        };
        uint16_t DE;
    };
    union
    {
        struct
        {
            uint8_t L;
            uint8_t H;
        };
        uint16_t HL;
    };
    uint16_t  IX;
    uint16_t  IY;
    bool      z,cy,m,po;
    uint16_t  ex_bc,ex_de,ex_hl;
    uint16_t  ex_af;
    uint16_t  ex_temp;
    uint16_t  stack[2048];     
    uint16_t  sp = 2048;
    uint8_t   nib1,nib2,nib3,nib4;
};

extern z80_cpu gbl_z80_cpu;

#endif // Z80_CPU_H_INCLUDED
