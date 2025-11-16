#include <string>
#include <vector>
#include "util.h"
#include "sargon-asm-interface.h"
#include "sargon-pv.h"
#include "thc.h"
#include "z80_cpu.h"
#include "bridge.h"

extern void INITBD();
extern void ROYALT();
extern void CPTRMV();
extern void VALMOV();
extern void ASNTBI();
extern void EXECMV();

void sargon( int api_command_code, z80_registers *registers )
{
    extern z80_cpu *sargon_cpu;
    if( registers )
    {
        sargon_cpu->AF = registers->af;
        sargon_cpu->BC = registers->bc;
        sargon_cpu->DE = registers->de;
        sargon_cpu->HL = registers->hl;
        sargon_cpu->IX = registers->ix;
        sargon_cpu->IY = registers->iy;
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
        registers->af = sargon_cpu->AF;
        registers->bc = sargon_cpu->BC;
        registers->de = sargon_cpu->DE;
        registers->hl = sargon_cpu->HL;
        registers->ix = sargon_cpu->IX;
        registers->iy = sargon_cpu->IY;
    }
}

