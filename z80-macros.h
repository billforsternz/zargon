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

#define Z80_REGISTERS                    \
    static z80_registers   r;            \
    static uint16_t  ex_bc,ex_de,ex_hl;  \
    static uint16_t  ex_af;              \
    static uint16_t  ex_temp;            \
    static bool      Z,C,M,PO;           \
    static uint16_t  stack[128];         \
    static uint16_t  sp = 128;           \
    static uint8_t   nib1,nib2,nib3,nib4 \

#define NZ (!Z)
#define NC (!C)
#define P  (!M)
#define PE (!PO)
#define a  (r.A)
#define b  (r.B)
#define c  (r.C)
#define d  (r.D)
#define e  (r.E)
#define h  (r.H)
#define l  (r.L)
#define af (r.AF)
#define bc (r.BC)
#define de (r.DE)
#define hl (r.HL)
#define ix (r.IX)
#define iy (r.IY)

// Emulate OPCODES with macros
#define LD(dst,src)     (dst) = (src)
#define addr(var)       offsetof(emulated_memory,var)
#define val16(var)      *((uint16_t *)(&mem.var))
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
#define FLAGS_IN        r.F = ((C?1:0) + (Z?2:0) + (M?4:0) + (PO?8:0)) 
#define FLAGS_OUT       PO=((r.F&8)==8), M=((r.F&4)==4), Z=((r.F&2)==2), C=((r.F&1)==1) 
#define PUSHf(x)        FLAGS_IN, stack[--sp] = (x)
#define POPf(x)         (x) = stack[sp++], FLAGS_OUT
#define PUSH(x)         stack[--sp] = (x)
#define POP(x)          (x) = stack[sp++]
#define Z80_EXAF        FLAGS_IN, EX(af,ex_af), FLAGS_OUT
#define NIB_OUT(x,hi,lo)  hi=(((x)>>4)&0x0f), lo=((x)&0x0f)
#define NIB_IN(x,hi,lo)   x = (((hi)<<4)&0xf0)|(lo)
#define RLD               NIB_OUT(a,nib1,nib2), NIB_OUT(ptr(hl),nib3,nib4), \
                          NIB_IN (a,nib1,nib3), NIB_IN (ptr(hl),nib4,nib2)
#define RRD               NIB_OUT(a,nib1,nib2), NIB_OUT(ptr(hl),nib3,nib4), \
                          NIB_IN(a,nib1,nib4),  NIB_IN(ptr(hl),nib2,nib3)
#define EXX               EX(bc,ex_bc), EX(de,ex_de), EX(hl,ex_hl)
#define SLA(x)            C=( ((x)&0x80) != 0 ), (x)=(x<<1),                       Z=((x)==0)
#define SRA(x)            C=( ((x)&0x01) != 0 ), (x) = ((uint8_t)((int8_t)(x)/2)), Z=((x)==0)
#define SRL(x)            C=( ((x)&0x01) != 0 ), (x)=(x>>1),                       Z=((x)==0)
#define RLA(x)            C=( ((x)&0x80) != 0 ), (x)=((x<<1)+(C?0x01:0)),          Z=((x)==0)
#define RR(x)             C=( ((x)&0x01) != 0 ), (x)=((x>>1)+(C?0x80:0)),          Z=((x)==0)
#define SET(bit_nbr,reg ) switch(bit_nbr) {\
                           case 7: (reg)|=0x80; break; \
                           case 6: (reg)|=0x40; break; \
                           case 5: (reg)|=0x20; break; \
                           case 4: (reg)|=0x10; break; \
                           case 3: (reg)|=0x08; break; \
                           case 2: (reg)|=0x04; break; \
                           case 1: (reg)|=0x02; break; \
                           case 0: (reg)|=0x01; break; }
#define RES(bit_nbr,reg ) switch(bit_nbr) {\
                           case 7: (reg)&=0x7f; break; \
                           case 6: (reg)&=0xb0; break; \
                           case 5: (reg)&=0xd0; break; \
                           case 4: (reg)&=0xe0; break; \
                           case 3: (reg)&=0xf7; break; \
                           case 2: (reg)&=0xfb; break; \
                           case 1: (reg)&=0xfd; break; \
                           case 0: (reg)&=0xfe; break; }

#define BIT(bit_nbr,reg ) switch(bit_nbr) {\
                           case 7: Z = ((reg)&0x80)==0; break; \
                           case 6: Z = ((reg)&0x40)==0; break; \
                           case 5: Z = ((reg)&0x20)==0; break; \
                           case 4: Z = ((reg)&0x10)==0; break; \
                           case 3: Z = ((reg)&0x08)==0; break; \
                           case 2: Z = ((reg)&0x04)==0; break; \
                           case 1: Z = ((reg)&0x02)==0; break; \
                           case 0: Z = ((reg)&0x01)==0; break; }

// Z80_CPIR MACRO
// ;CPIR reference, from the Zilog Z80 Users Manual
// ;A - (HL), HL => HL+1, BC => BC - 1
// ;If decrementing causes BC to go to 0 or if A = (HL), the instruction is terminated.
// ;P/V is set if BC - 1 does not equal 0; otherwise, it is reset.
// ;
// ;So result of the subtraction discarded, but flags are set, (although CY unaffected).
// ;*BUT* P flag (P/V flag in Z80 parlance as it serves double duty as an overflow
// ;flag after some instructions in that CPU) reflects the result of decrementing BC
// ;rather than A - (HL)
// ;
// ;We support reflecting the result in the Z and P flags. The possibilities are
// ;Z=1 P=1 (Z and PE)  -> first match found, counter hasn't expired
// ;Z=1 P=0 (Z and PO)  -> match found in last position, counter has expired
// ;Z=0 P=0 (NZ and PO) -> no match found, counter has expired
#define Z80_CPIR do {               \
        bool found = false;         \
        while( !found )             \
        {                           \
            found = (a == ptr(hl)); \
            hl++;                   \
            bc--;                   \
            if( bc == 0 )           \
                break;              \
        }                           \
        PO = (bc==0);               \
        Z  = found;                 \
    } while(0)

#endif // Z80_MACROS_H_INCLUDED
