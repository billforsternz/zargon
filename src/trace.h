//
//  Was bridge.h (testing Bridge between Sargon-x86 and Zargon)
//  Now trace.h (trace and debug facilities for Zargon)
//
#ifndef TRACE_H_INCLUDED
#define TRACE_H_INCLUDED
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
// #define DEBUG_SINGLE_STEP
// #define DEBUG_TRACK_SCORE
// #define DEBUG_SHOW_TREE
// #define DEBUG_KEEP_EXTRAF
#define LOG_NONE 0         
#define LOG_SUPER 1         
#define LOG_EXTRA 2
#define LOG_TRACE 3         
#define LOG_DETAILED 4      

// Some different use/testing scenarios
//#define SCENARIO_PRODUCTION
#define SCENARIO_BASIC_DEBUGGING
//#define SCENARIO_SINGLE_STEPPING

// Production, eliminate all overheads
#ifdef SCENARIO_PRODUCTION
#define LOG_LEVEL LOG_NONE
#endif

// Debugging, show the essentials
#ifdef SCENARIO_BASIC_DEBUGGING
#define LOG_LEVEL LOG_EXTRA
#define DEBUG_TRACK_SCORE
#define DEBUG_SHOW_TREE
#define DEBUG_KEEP_EXTRAF
#endif

// Debugging with single stepping
#ifdef SCENARIO_SINGLE_STEPPING
#define LOG_LEVEL LOG_EXTRA
#define DEBUG_SINGLE_STEP
#define DEBUG_TRACK_SCORE
#define DEBUG_SHOW_TREE
#define DEBUG_KEEP_EXTRAF
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

// tracef(), extraf() - show progress of chess algorithm
#ifdef DEBUG_KEEP_EXTRAF
void extraf( const char *fmt, ... );
void superf( const char *fmt, ... );
#else
#define extraf(format, ...) (void)0
#define superf(format, ...) (void)0
#endif
void tracef( const char *fmt, ... );

// logf()   - show all the details
void logf( const char *fmt, ... );

std::string show_node();
std::string show_scores();
std::string show_scores_short();
std::string show_scores_long();
std::string show_score( uint8_t val );
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

//
//  Optionally include Zargon function tracing. Useful for
//   1. Debugging
//   2. Building tree building models to understand Sargon's
//      tree construction - the heart of the program

#ifdef DEBUG_FUNC_TRACE
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
void trace_score_updated( uint8_t *p, uint8_t score );
void trace_score_descend();
bool callback_restart_test();

std::string score_descriptors[];

#endif  // TRACE_H_INCLUDED
