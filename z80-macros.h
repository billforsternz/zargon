#ifndef Z80_MACROS_H_INCLUDED
#define Z80_MACROS_H_INCLUDED

// Emulate Z80 machine
struct z80_registers
{
    union
    {
        struct
        {
            uint8_t F;
            uint8_t A;
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

#define Z80_REGISTERS z80_registers r; bool Z,C,M; uint16_t ex_temp
#define NZ !Z
#define NC !C
#define P  !M
#define a r.A
#define b r.B
#define c r.C
#define d r.D
#define e r.E
#define h r.H
#define l r.L
#define af r.AF
#define bc r.BC
#define de r.DE
#define hl r.HL
#define ix r.IX
#define iy r.IY

// Emulate OPCODES with macros
#define FUNC_BEGIN(func)    void func() {
#define FUNC_END            }
#define LD(dst,src)     (dst) = (src)
#define addr(var)       offsetof(emulated_memory,var)
//#define chk(var)        offsetof(emulated_memory,var)
#define chk(var)        *((uint16_t *)(&mem.var))
#define val(var)        *((uint8_t *)(&mem.var))
#define ptr(addr)       *( ((uint8_t *)&mem) + (addr))
#define INC(x)          Z=(x+1==0), C==((uint16_t)(x)+1>=0x100), M=((int8_t)((int8_t)(x)+1) < 0), (x) = (x)+1
#define DEC(x)          Z=(x-1==0), C==((uint16_t)(x)-1>=0x100), M=((int8_t)((int8_t)(x)-1) < 0), (x) = (x)-1
#define DJNZ(label)     if(--r.B != 0) goto label
#define ADD(x,y)        Z=((x)+(y)==(uint8_t)0), C==((uint16_t)(x)+(uint16_t)(y)>=0x100), M=((int8_t)((int8_t)(x)+(int8_t)(y)) < 0), (x)+=(y)
#define ADD16(x,y)      Z=((x)+(y)==(uint16_t)0), C==((uint32_t)(x)+(uint32_t)(y)>=0x10000), M=((int16_t)((int16_t)(x)+(int16_t)(y)) < 0), (x)+=(y)
#define SBC(x,y)        Z=((x)==(y+C?1:0)), C==((y+C?1:0)>(x)), M=((int16_t)((int16_t)(x)-(int16_t)(y+C?1:0)) < 0), (x)-=(y+C?1:0)
#define SUB(y)          Z=(a==(y)),   C==((y)>a), M=((int8_t)((int8_t)(a)-(int8_t)(y)) < 0), a-=(y)
#define CP(y)           Z=(a==(y)),   C==((y)>a), M=((int8_t)((int8_t)(a)-(int8_t)(y)) < 0)
#define NEG             Z=(a==0),     C==(false), M=((int8_t)a > 0), a = (int8_t)(0 - (int8_t)a)
#define EX(x,y)         ex_temp=x; x=y; y=ex_temp
#define JR(cond,label)  if(cond) goto label
#define JP(cond,label)  if(cond) goto label
#define CALL(cond,func) if(cond) (func)()
#define AND(y)          Z=((a&(y))==0),   C==false, M=((a&(y)) > 0x7f), a = a&(y)
#define RET(cond)       if(cond) return
#define XOR(y)          Z=((a^(y))==0),   C==false, M=((a^(y)) > 0x7f), a = a^(y)
#define RETu            return;
#define JRu(label)      goto label
#define JPu(label)      goto label
#define CALLu(func)     (func)()
#define PUSH(x)
#define POP(x)
#define Z80_EXAF        
#define Z80_CPIR
#define RLD        
#define RRD        
#define EXX        
#define SLA(x)        Z=(((x)<<1)==0), C=( ((x)&0x80) != 0 ), (x)=(x<<1)
#define SRL(x)        Z=(((x)>>1)==0), C=( ((x)&0x01) != 0 ), (x)=(x>>1)

// Emulate OPCODES with functions
void SET( uint8_t bit_nbr, uint8_t &reg );
void RES( uint8_t bit_nbr, uint8_t &reg );
void BIT( uint8_t bit_nbr, uint8_t &reg );

#endif // Z80_MACROS_H_INCLUDED
