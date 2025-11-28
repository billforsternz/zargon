#ifndef Z80_OPCODES_H_INCLUDED
#define Z80_OPCODES_H_INCLUDED
#include <stdlib.h>
#include <time.h>
#include "z80_cpu.h"

//#define Z80_OPCODES                      \
//    static uint16_t  ex_bc,ex_de,ex_hl;  \
//    static uint16_t  ex_af;              \
//    static uint16_t  ex_temp;            \
//    static uint16_t  stack[2048];         \
//    static uint16_t  sp = 2048;           \
//    static uint8_t   nib1,nib2,nib3,nib4

#define Z  (gbl_z80_cpu.z)
#define NZ (!Z)
#define CY (gbl_z80_cpu.cy)
#define NC (!CY)
#define M  (gbl_z80_cpu.m)
#define P  (!M)                     
#define PO (gbl_z80_cpu.po)
#define PE (!PO)

#define a  (gbl_z80_cpu.A)
#define b  (gbl_z80_cpu.B)
#define c  (gbl_z80_cpu.C)
#define d  (gbl_z80_cpu.D)
#define e  (gbl_z80_cpu.E)
#define h  (gbl_z80_cpu.H)
#define l  (gbl_z80_cpu.L)
#define af (gbl_z80_cpu.AF)
#define bc (gbl_z80_cpu.BC)
#define de (gbl_z80_cpu.DE)
#define hl (gbl_z80_cpu.HL)
#define ix (gbl_z80_cpu.IX)
#define iy (gbl_z80_cpu.IY)

// Emulate OPCODES with macros
#define LD(dst,src)     (dst) = (src)
#define addr(var)       offsetof(emulated_memory,var)
#define v16(var)        *((uint16_t *)(&m.var))
#define val(var)        *((uint8_t *) (&m.var))
#define ptr(addr)       *( ((uint8_t *)&m) + (addr))
#define DJNZ(label)     if(--gbl_z80_cpu.B != 0) goto label
#define INC(x)          (x) = (x)+1, M=(((x)&0x80)==0x80), Z=((x)==0)
#define DEC(x)          (x) = (x)-1, M=(((x)&0x80)==0x80), Z=((x)==0)
#define INC16(x)        (x) = (x)+1
#define DEC16(x)        (x) = (x)-1
#define ADD(x,y)        CY=((uint16_t)(x)+(uint16_t)(y)>=0x100),  (x)+=(y),   M=(((x)&0x80)==0x80),  Z=((x)==0)
#define ADC(x,y)        do { bool temp=( ((uint16_t)(x)+(CY?1:0)+(uint16_t)(y)>=0x100);  (x)+=(((uint8_t)y)+(CY?1:0)), M=(((x)&0x80)==0x80);  Z=((x)==0); CY=temp; } while(0)
#define ADD16(x,y)      CY=((uint32_t)(x)+(uint32_t)(y)>=0x10000),         (x)+=(y)   // anomalous 8080 instruction, only CY affected
#define ADC16(x,y)      do { bool temp=((uint32_t)(x)+(CY?1:0)+(uint32_t)(y)>=0x10000); (x)+=(((uint16_t)y)+(CY?1:0));  M=(((x)&0x8000)==0x8000); Z=((x)==0); CY=temp; } while(0)
#define SUB(x)          CY=((x)>a), a-=(x), M=((a&0x80)==0x80),  Z=(a==0)
#define SBC(x)          do { bool temp=((((uint8_t)x)+(CY?1:0))>a);  a-=(((uint8_t)x)+(CY?1:0)); M=((a&0x80)==0x80);  Z=(a==0); CY=temp; } while(0)
// #define SUB16(x,y)      // No such instruction
#define SBC16(x,y)      do { bool temp=((y+(CY?1:0))>(x));  (x)-=(y+(CY?1:0)); M=(((x)&0x8000)==0x8000); Z=((x)==0); CY=temp; } while(0)
#define CP(x)           CY=(((uint8_t)x)>a), M=(((a-((uint8_t)x))&0x80)==0x80), Z=((a-((uint8_t)x))==0)
#define NEG             Z=(a==0),  CY=(a!=0), a = (int8_t)(0 - (int8_t)a), M=(((a)&0x80)==0x80)
#define AND(x)          CY=false, a = a&(x), M=((a&0x80)==0x80), Z=(a==0)
#define OR(x)           CY=false, a = a|(x), M=((a&0x80)==0x80), Z=(a==0)
#define XOR(x)          CY=false, a = a^(x), M=((a&0x80)==0x80), Z=(a==0)
#define EX(x,y)         gbl_z80_cpu.ex_temp=x; x=y; y=gbl_z80_cpu.ex_temp
#define JR(cond,label)  if(cond) goto label
#define JP(cond,label)  if(cond) goto label
#define CALL(cond,func) if(cond) (func)()
#define RET(cond)       if(cond) return
#define RETu            return
#define JRu(label)      goto label
#define JPu(label)      goto label
#define CALLu(func)     (func)()
#define FLAGS_IN        gbl_z80_cpu.F = ((CY?1:0) + (Z?2:0) + (M?4:0) + (PO?8:0)) 
#define FLAGS_OUT       PO=((gbl_z80_cpu.F&8)==8), M=((gbl_z80_cpu.F&4)==4), Z=((gbl_z80_cpu.F&2)==2), CY=((gbl_z80_cpu.F&1)==1) 
#define PUSHf(x)        FLAGS_IN, gbl_z80_cpu.stack[--gbl_z80_cpu.sp] = (x); if( gbl_z80_cpu.sp < min_so_far )  min_so_far = gbl_z80_cpu.sp
#define POPf(x)         (x) = gbl_z80_cpu.stack[gbl_z80_cpu.sp++], FLAGS_OUT
#define PUSH(x)         gbl_z80_cpu.stack[--gbl_z80_cpu.sp] = (x); if( gbl_z80_cpu.sp < min_so_far )  min_so_far = gbl_z80_cpu.sp
#define POP(x)          (x) = gbl_z80_cpu.stack[gbl_z80_cpu.sp++]
#define Z80_EXAF        FLAGS_IN, EX(af,gbl_z80_cpu.ex_af), FLAGS_OUT
#define NIB_OUT(x,hi,lo)  hi=(((x)>>4)&0x0f), lo=((x)&0x0f)
#define NIB_IN(x,hi,lo)   x = (((hi)<<4)&0xf0)|(lo)
#define RLD               NIB_OUT(a,gbl_z80_cpu.nib1,gbl_z80_cpu.nib2), NIB_OUT(ptr(hl),gbl_z80_cpu.nib3,gbl_z80_cpu.nib4), \
                          NIB_IN (a,gbl_z80_cpu.nib1,gbl_z80_cpu.nib3), NIB_IN (ptr(hl),gbl_z80_cpu.nib4,gbl_z80_cpu.nib2), M=(((a)&0x80)==0x80), Z=((a)==0)
#define RRD               NIB_OUT(a,gbl_z80_cpu.nib1,gbl_z80_cpu.nib2), NIB_OUT(ptr(hl),gbl_z80_cpu.nib3,gbl_z80_cpu.nib4), \
                          NIB_IN (a,gbl_z80_cpu.nib1,gbl_z80_cpu.nib4),  NIB_IN(ptr(hl),gbl_z80_cpu.nib2,gbl_z80_cpu.nib3), M=(((a)&0x80)==0x80), Z=((a)==0)
#define EXX               EX(bc,gbl_z80_cpu.ex_bc), EX(de,gbl_z80_cpu.ex_de), EX(hl,gbl_z80_cpu.ex_hl)
#define SLA(x)            CY=( ((x)&0x80) != 0 ), (x)=(x<<1),                       M=(((x)&0x80)==0x80), Z=((x)==0)
#define SRA(x)            CY=( ((x)&0x01) != 0 ), (x) = ((uint8_t)((int8_t)(x)/2)), M=(((x)&0x80)==0x80), Z=((x)==0)
#define SRL(x)            CY=( ((x)&0x01) != 0 ), (x)=(x>>1),                       M=(((x)&0x80)==0x80), Z=((x)==0)
#define RLA               do { bool temp=( ((a)&0x80) != 0 ); (a)=((a<<1)+(CY?0x01:0));  M=(((a)&0x80)==0x80); Z=((a)==0); CY=temp; } while(0)
#define RR(x)             do { bool temp=( ((x)&0x01) != 0 ); (x)=((x>>1)+(CY?0x80:0));  M=(((x)&0x80)==0x80); Z=((x)==0); CY=temp; } while(0)
#define SET(bit_nbr,reg ) switch(bit_nbr) {            \
                           case 7: (reg)|=0x80; break; \
                           case 6: (reg)|=0x40; break; \
                           case 5: (reg)|=0x20; break; \
                           case 4: (reg)|=0x10; break; \
                           case 3: (reg)|=0x08; break; \
                           case 2: (reg)|=0x04; break; \
                           case 1: (reg)|=0x02; break; \
                           case 0: (reg)|=0x01; break; }
#define RES(bit_nbr,reg ) switch(bit_nbr) {            \
                           case 7: (reg)&=0x7f; break; \
                           case 6: (reg)&=0xbf; break; \
                           case 5: (reg)&=0xdf; break; \
                           case 4: (reg)&=0xef; break; \
                           case 3: (reg)&=0xf7; break; \
                           case 2: (reg)&=0xfb; break; \
                           case 1: (reg)&=0xfd; break; \
                           case 0: (reg)&=0xfe; break; }

#define BIT(bit_nbr,reg ) switch(bit_nbr) {                    \
                           case 7: Z = ((reg)&0x80)==0; break; \
                           case 6: Z = ((reg)&0x40)==0; break; \
                           case 5: Z = ((reg)&0x20)==0; break; \
                           case 4: Z = ((reg)&0x10)==0; break; \
                           case 3: Z = ((reg)&0x08)==0; break; \
                           case 2: Z = ((reg)&0x04)==0; break; \
                           case 1: Z = ((reg)&0x02)==0; break; \
                           case 0: Z = ((reg)&0x01)==0; break; }

// Z80_LDAR MACRO
//  Loads A register with the RAM refresh register, in practice a way to get
//  a random number
#define Z80_LDAR do {                          \
    static bool has_run_at_least_once;         \
    if( !has_run_at_least_once )               \
    {                                          \
        has_run_at_least_once = true;          \
        srand((unsigned int)time(NULL));       \
    }                                          \
    int random_number = rand();                \
    a = (uint8_t)(random_number&0xff);         \
} while(0)

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

// Fortunately we don't need these two very much
#define v16_offset(var,offset)  *( (uint16_t *) (offset + ((uint8_t*)(&m.var)))  )
#define val_offset(var,offset)  *( (uint8_t *)  (offset + ((uint8_t*)(&m.var)))  )

#endif // Z80_OPCODES_H_INCLUDED
