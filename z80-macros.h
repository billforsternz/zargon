#ifndef Z80_MACROS_H_INCLUDED
#define Z80_MACROS_H_INCLUDED

// Emulate Z80 machine
struct z80_registers
{
    union
    {
        struct
        {
            uint8_t f;
            uint8_t a;
        };
        uint16_t af;
    };
    union
    {
        struct
        {
            uint8_t c;
            uint8_t b;
        };
        uint16_t bc;
    };
    union
    {
        struct
        {
            uint8_t e;
            uint8_t d;
        };
        uint16_t de;
    };
    union
    {
        struct
        {
            uint8_t l;
            uint8_t h;
        };
        uint16_t hl;
    };
    uint16_t ix;
    uint16_t iy;
};

#define Z80_REGISTERS z80_registers r;

// Emulate OPCODES with macros
#define FUNC_BEGIN(func)    void func() {
#define FUNC_END            }
#define LD(dst,src)     (dst) = (src)
#define addr(var)       offsetof(emulated_memory,var)
#define ptr(addr)       *( ((uint8_t *)&mem) + (addr))
#define INC(var)        var++
#define DJNZ(label)     r.b--; if(r.b != 0) goto label
#define RET             return

// Emulate OPCODES with functions
void SET( uint8_t bit_nbr, uint8_t &reg );

#endif // Z80_MACROS_H_INCLUDED
