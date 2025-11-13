#ifndef Z80_CPU_H_INCLUDED
#define Z80_CPU_H_INCLUDED
#include <stdint.h>

// Emulate Z80 machine
struct z80_cpu
{
    union
    {
        struct
        {
            uint8_t A;
            uint8_t F;
        };
        uint16_t AF;
    };
    union
    {
        struct
        {
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
    uint16_t IX;
    uint16_t IY;
};

#define Z80_CPU                    \
    static z80_cpu   r;            \
    static bool      Z,C,M,PO

#endif // Z80_CPU_H_INCLUDED
