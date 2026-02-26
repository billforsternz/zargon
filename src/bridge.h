//
//  Testing Bridge between Sargon-x86 and Zargon
//
#ifndef BRIDGE_H_INCLUDED
#define BRIDGE_H_INCLUDED

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

class function_in_out
{
    CB saved_cb;
public:
    bool early_exit;
    function_in_out( CB cb );
    ~function_in_out();
    void log( CB cb, bool in, bool insist );
};

//#define BRIDGE_CALLBACK_TRACE
//#define BRIDGE_CALLBACK_TRACE_DETAILED
#ifdef BRIDGE_CALLBACK_TRACE_DETAILED
#define callback_zargon_bridge(cb)      function_in_out temp_fio(cb)
#define callback_zargon_bridge_void(cb) function_in_out temp_fio(cb);  if(temp_fio.early_exit) return
#else
#define callback_zargon_bridge(cb)
#define callback_zargon_bridge_void(cb)
#endif

#endif  // BRIDGE_H_INCLUDED
