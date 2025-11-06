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
    switch(api_command_code)
    {
        case api_INITBD: INITBD(); break;
        case api_ROYALT: ROYALT(); break;
        case api_CPTRMV: CPTRMV(); break;
        case api_VALMOV: VALMOV(); break;
        case api_ASNTBI: ASNTBI(); break;
        case api_EXECMV: EXECMV(); break;
    }
}

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
