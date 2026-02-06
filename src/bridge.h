//
//  Testing Bridge between Sargon-x86 and Zargon
//
#ifndef BRIDGE_H_INCLUDED
#define BRIDGE_H_INCLUDED
#include <string>
#include "z80_registers.h"
#include "z80_cpu.h"

// Include callback tracing code
//#define BRIDGE_CALLBACK_TRACE
//#define BRIDGE_CALLBACK_TRACE_DETAILED
#ifdef BRIDGE_CALLBACK_TRACE_DETAILED
#define callback_zargon_bridge(cb) bridge_callback_trace(cb,0)
#else
#define callback_zargon_bridge(cb)
#endif

void bridge_init( const unsigned char *mem_base );
std::string reg_dump( const z80_registers *reg=0 );
std::string mem_dump();

// Callback enumeration
enum CB
{
    CB_null,
    CB_LDAR,
    CB_AFTER_GENMOV,
    CB_END_OF_POINTS,
    CB_AFTER_FNDMOV,
    CB_YES_BEST_MOVE,
    CB_NO_BEST_MOVE,
    CB_SUPPRESS_KING_MOVES,
    CB_ALPHA_BETA_CUTOFF,
    CB_PATH,
    CB_MPIECE,
    CB_ENPSNT,
    CB_ADJPTR,
    CB_CASTLE,
    CB_ADMOVE,
    CB_GENMOV,
    CB_ATTACK,
    CB_ATKSAV,
    CB_PNCK,
    CB_PINFND,
    CB_XCHNG,
    CB_NEXTAD,
    CB_POINTS,
    CB_MOVE,
    CB_UNMOVE,
    CB_SORTM,
    CB_EVAL,
    CB_FNDMOV,
    CB_ASCEND
};

void bridge_callback_trace( CB cb, const z80_registers *reg );

#endif  // BRIDGE_H_INCLUDED

