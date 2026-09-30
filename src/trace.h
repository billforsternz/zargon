//
//  Was bridge.h (testing Bridge between Sargon-x86 and Zargon)
//  Now trace.h (trace and debug facilities for Zargon)
//
#ifndef TRACE_H_INCLUDED
#define TRACE_H_INCLUDED
#include <string>
#include "thc.h"

// Allow different types of logging in DEBUG and RELEASE
// #define DEBUG_FUNC_LOG
// #define DEBUG_RESTRICTED_MOVES
// #define DEBUG_GUIDED_MOVES
#ifdef DEBUG_FUNC_LOG
    #define DEBUG_FUNC_CALLBACKS
#endif
#ifdef DEBUG_GUIDED_MOVES
    #define DEBUG_FUNC_CALLBACKS
#endif
#ifdef DEBUG_RESTRICTED_MOVES
    #define DEBUG_FUNC_CALLBACKS
#endif

// Some different use/testing scenarios
//#define SCENARIO_PRODUCTION
//#define SCENARIO_BASIC_DEBUGGING
#define SCENARIO_SINGLE_STEPPING

// Production, eliminate all overheads
#ifdef SCENARIO_PRODUCTION
#endif

// Debugging, show the essentials
#ifdef SCENARIO_BASIC_DEBUGGING
#define DEBUG_KEEP_TRACEF
#define DEBUG_KEEP_LOGF
#endif

// Debugging with single stepping
#ifdef SCENARIO_SINGLE_STEPPING
#define DEBUG_KEEP_TRACEF
#define DEBUG_KEEP_LOGF
#define DEBUG_SINGLE_STEP
#endif

// Sargon functions enumeration
enum FUNC_ENUM
{
    FE_null,
    FE_PATH,
    FE_MPIECE,
    FE_ENPSNT,
    FE_ADJPTR,
    FE_CASTLE,
    FE_ADMOVE,
    FE_GENMOV,
    FE_ATTACK,
    FE_ATKSAV,
    FE_PNCK,
    FE_PINFND,
    FE_XCHNG,
    FE_NEXTAD,
    FE_POINTS,
    FE_MOVE,
    FE_UNMOVE,
    FE_SORTM,
    FE_EVAL,
    FE_FNDMOV,
    FE_ASCEND
};

// tracef() - show progress of chess algorithm
#ifdef DEBUG_KEEP_TRACEF
void tracef( const char *fmt, ... );
#else
#define tracef(format, ...) (void)0
#endif

// logf()   - show miscellaneous details
#ifdef DEBUG_KEEP_LOGF
void logf( const char *fmt, ... );
#else
#define logf(format, ...) (void)0
#endif

std::string show_node();
struct ML;
std::string show_ply_chains( bool show_score=false );

class function_in_out
{
    FUNC_ENUM saved_fe;
public:
    bool early_exit;
    function_in_out( FUNC_ENUM fe );
    ~function_in_out();
    void log( FUNC_ENUM fe, bool in, bool insist );
};

//  Optionally include Zargon function callbacks for
//   guided tests and logging
#ifdef DEBUG_FUNC_CALLBACKS
#define trace_func(fe)      function_in_out temp_fio(fe)
#define trace_func_void(fe) function_in_out temp_fio(fe);  if(temp_fio.early_exit) return
#else
#define trace_func(fe)
#define trace_func_void(fe)
#endif

// For guided tests
void callback_restricted_moves_register( std::string guide );
void callback_restricted_moves_clear();

// Misc diagnostics
void callback_start_position_register( const thc::ChessPosition &cp );
bool callback_restart_test();

#endif  // TRACE_H_INCLUDED
