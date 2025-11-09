#include <string>
#include <vector>
#include "util.h"
#include "sargon-asm-interface.h"
#include "sargon-pv.h"
#include "thc.h"
#include "z80-macros.h"

void sargon_minimax_main(void)
{
}

bool sargon_minimax_regression_test(bool)
{
    return true;
}

extern void INITBD();
extern void ROYALT();
extern void CPTRMV();
extern void VALMOV();
extern void ASNTBI();
extern void EXECMV();

void callback( CB cb )
{
    switch(cb)
    {
        case CB_END_OF_POINTS:
        {
            sargon_pv_callback_end_of_points();
            break;
        }
        case CB_YES_BEST_MOVE:
        {
            sargon_pv_callback_yes_best_move();            
            break;
        }
    }
}

// Sargon calls C, parameters serves double duty - saved registers on the
//  stack, can optionally be inspected by C program
void callback( uint32_t edi, uint32_t esi, uint32_t ebp, uint32_t esp,
                uint32_t ebx, uint32_t edx, uint32_t ecx, uint32_t eax,
                uint32_t eflags )
{
}


std::string mem_dump()
{
    std::vector<std::pair<int,const char *>> vars=
    {
        { BOARDA  ,"BOARDA" },
        { ATKLST  ,"ATKLST" },
        { PLISTA  ,"PLISTA" },
        { POSK    ,"POSK" },
        { POSQ    ,"POSQ" },
        { SCORE   ,"SCORE" },
        { PLYIX   ,"PLYIX" },
        { M1      ,"M1" },
        { M2      ,"M2" },
        { M3      ,"M3" },
        { M4      ,"M4" },
        { T1      ,"T1" },
        { T2      ,"T2" },
        { T3      ,"T3" },
        { INDX1   ,"INDX1" },
        { INDX2   ,"INDX2" },
        { NPINS   ,"NPINS" },
        { MLPTRI  ,"MLPTRI" },
        { MLPTRJ  ,"MLPTRJ" },
        { SCRIX   ,"SCRIX" },
        { BESTM   ,"BESTM" },
        { MLLST   ,"MLLST" },
        { MLNXT   ,"MLNXT" },
        { KOLOR   ,"KOLOR" },
        { COLOR   ,"COLOR" },
        { P1      ,"P1" },
        { P2      ,"P2" },
        { P3      ,"P3" },
        { PMATE   ,"PMATE" },
        { MOVENO  ,"MOVENO" },
        { PLYMAX  ,"PLYMAX" },
        { NPLY    ,"NPLY" },
        { CKFLG   ,"CKFLG" },
        { MATEF   ,"MATEF" },
        { VALM    ,"VALM" },
        { BRDC    ,"BRDC" },
        { PTSL    ,"PTSL" },
        { PTSW1   ,"PTSW1" },
        { PTSW2   ,"PTSW2" },
        { MTRL    ,"MTRL" },
        { BC0     ,"BC0" },
        { MV0     ,"MV0" },
        { PTSCK   ,"PTSCK" },
        { BMOVES  ,"BMOVES" },
        { LINECT  ,"LINECT" },
        { MVEMSG  ,"MVEMSG" },
        { MLIST   ,"MLIST" },
        { MLEND   ,"MLEND" }
    };
    bool need_newline = false;
    int offset=0;
    std::string s = util::sprintf("%-6s %04x:\n", "base", offset );
    for( const std::pair<int,const char *> v: vars )
    {
        extern unsigned char *sargon_base_address;
        int len = v.first-offset;
        for( int lines=0, i=0; i<len; i++ )
        {
            if( i%8==0 && i%16 != 0)
                s += " ";
            s += util::sprintf( " %02x", *(sargon_base_address+offset+i) );
            need_newline = true;
            if( (i+1)%16 == 0 )
            {
                need_newline = false;
                lines++;
                if( lines < 16 )
                    s += "\n";
                else
                {
                    s += "\n....\n";
                    break;
                }
            }
        }
        offset = v.first;
        if( need_newline)
            s += "\n";
        s += util::sprintf("%-6s %04x:\n", v.second, offset );
    }
    s += "\n";
    return s;
}

void sargon( int api_command_code, z80_registers_mini *registers )
{
    extern z80_registers *sargon_registers;
    if( registers )
    {
        sargon_registers->AF = registers->af;
        sargon_registers->BC = registers->bc;
        sargon_registers->DE = registers->de;
        sargon_registers->HL = registers->hl;
        sargon_registers->IX = registers->ix;
        sargon_registers->IY = registers->iy;
    }
    std::string s = "??";
    switch(api_command_code)
    {
        case api_INITBD: s = "INITBD"; break;
        case api_ROYALT: s = "ROYALT"; break;
        case api_CPTRMV: s = "CPTRMV"; break;
        case api_VALMOV: s = "VALMOV"; break;
        case api_ASNTBI: s = "ASNTBI"; break;
        case api_EXECMV: s = "EXECMV"; break;
    }
    s += "()\n";
    std::string before = mem_dump();
    switch(api_command_code)
    {
        case api_INITBD: INITBD(); break;
        case api_ROYALT: ROYALT(); break;
        case api_CPTRMV: CPTRMV(); break;
        case api_VALMOV: VALMOV(); break;
        case api_ASNTBI: ASNTBI(); break;
        case api_EXECMV: EXECMV(); break;
    }
    #if 0
    s += util::sprintf("%c %-52s %s\n", ' ', "Before", "After" );
    std::string after = mem_dump();
    size_t offset = 0;
    for(;;)
    {
        size_t offset2 = before.find('\n',offset);
        if( offset2 == std::string::npos )
            break;
        if( offset2 != after.find('\n',offset) )
            break;
        std::string left  = before.substr(offset,offset2-offset);
        std::string right = after.substr(offset,offset2-offset);
        offset = offset2+1;
        s += util::sprintf("%c %-52s %s\n", left==right?' ':'*', left.c_str(), right.c_str() );
    }
    printf( "%s", s.c_str() );
    #endif
    if( registers )
    {
        registers->af = sargon_registers->AF;
        registers->bc = sargon_registers->BC;
        registers->de = sargon_registers->DE;
        registers->hl = sargon_registers->HL;
        registers->ix = sargon_registers->IX;
        registers->iy = sargon_registers->IY;
    }
}

