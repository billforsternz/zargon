//
//  Testing Bridge between Sargon-x86 and Zargon
//
#ifndef BRIDGE_H_INCLUDED
#define BRIDGE_H_INCLUDED
#include <string>
#include "thc.h"

// Allow different types of tracing in DEBUG and RELEASE
// #define DEBUG_FUNC_TRACE
// #define DEBUG_FUNC_TRACE_STUB
// #define DEBUG_FUNC_TRACE_FULL
// #define DEBUG_RESTRICTED_MOVES
// #define DEBUG_GUIDED_MOVES
// #define DEBUG_MOVE_EXTENSIONS
// #define DEBUG_SHOW_POSITIONS
#define DEBUG_TRACK_SCORE
#define DEBUG_SHOW_TREE
#define DEBUG_SINGLE_STEP                                            
// Callback enumeration
enum CB
{
    CB_null,
    CB_LDAR,                    // First of the original Sargon x86 callbacks
    CB_AFTER_GENMOV,
    CB_END_OF_POINTS,
    CB_AFTER_FNDMOV,
    CB_YES_BEST_MOVE,
    CB_NO_BEST_MOVE,
    CB_SUPPRESS_KING_MOVES,
    CB_ALPHA_BETA_CUTOFF,       // Last of the original Sargon x86 callbacks
    CB_PATH,                    // First of the Zargon function callbacks
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
    CB_ASCEND                   // Last of the Zargon function callbacks
};

// tracef(), extraf() - show progress of chess algorithm
#define LOG_EXTRA 1         
#define LOG_TRACE 2         
#define LOG_DETAILED 3      
#define LOG_LEVEL LOG_EXTRA
void tracef( const char *fmt, ... );
void extraf( const char *fmt, ... );

// logf()   - show all the details
void logf( const char *fmt, ... );

std::string show_node();
std::string show_scores();
std::string show_score( uint8_t val );
struct ML;
std::string show_ply_chains( ML *parm1=0, const char *parm1_name=0,
                             ML *parm2=0, const char *parm2_name=0,
                             ML *parm3=0, const char *parm3_name=0  );

class function_in_out
{
    CB saved_cb;
public:
    bool early_exit;
    function_in_out( CB cb );
    ~function_in_out();
    void log( CB cb, bool in, bool insist );
};

//
//  Optionally include Zargon function tracing. Useful for
//   1. Debugging
//   2. Building tree building models to understand Sargon's
//      tree construction - the heart of the program

#ifdef DEBUG_FUNC_TRACE
#define callback_zargon_bridge(cb)      function_in_out temp_fio(cb)
#define callback_zargon_bridge_void(cb) function_in_out temp_fio(cb);  if(temp_fio.early_exit) return
#else
#define callback_zargon_bridge(cb)
#define callback_zargon_bridge_void(cb)
#endif

// For guided tests
void callback_restricted_moves_register( std::string guide );
void callback_restricted_moves_clear();

// Misc diagnostics
void callback_start_position_register( const thc::ChessPosition &cp );
void bridge_score_updated( uint8_t *p, uint8_t score );
void bridge_score_descend();
bool callback_restart_test();

std::string score_descriptors[];

#endif  // BRIDGE_H_INCLUDED
