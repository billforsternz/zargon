//
//  Testing Bridge between Sargon-x86 and Zargon
//

#include <string>
#include <vector>
#include "util.h"
#include "bridge.h"
#include "sargon-asm-interface.h"
#include "sargon-interface.h"
#include "z80_registers.h"
#include "zargon.h"

// Init hooks
static const unsigned char *sargon_mem_base;
void bridge_init( const unsigned char *mem_base )
{
    sargon_mem_base = mem_base;
}

static uint64_t pnck_count;
static uint64_t run_count;
class bridge_exit
{
    public:
    ~bridge_exit() {printf("run_count=%llu\n", run_count );};    
};
//static bridge_exit bridge_exit_instance;

// Callback function names
const char *lookup[] =
{
    "null",
    "LDAR",
    "AFTER_GENMOV",
    "END_OF_POINTS",
    "AFTER_FNDMOV",
    "YES_BEST_MOVE",
    "NO_BEST_MOVE",
    "SUPPRESS_KING_MOVES",
    "ALPHA_BETA_CUTOFF",
    "PATH",
    "MPIECE",
    "ENPSNT",
    "ADJPTR",
    "CASTLE",
    "ADMOVE",
    "GENMOV",
    "ATTACK",
    "ATKSAV",
    "PNCK",
    "PINFND",
    "XCHNG",
    "NEXTAD",
    "POINTS",
    "MOVE",
    "UNMOVE",
    "SORTM",
    "EVAL",
    "FNDMOV",
    "ASCEND"
};

std::string current_status;
std::string sargon_ptr_print();
void callback_genmov();
bool callback_points();
bool callback_admove();

function_in_out::function_in_out( CB cb )
{
    early_exit = false;
    saved_cb = cb;
    bool insist = false;
    if( cb == CB_PATH ) return;
    else if( cb == CB_SORTM )  insist=true;
    else if( cb == CB_GENMOV ) { callback_genmov(); insist=true; }
    else if( cb == CB_POINTS ) { early_exit = callback_points(); insist=true; }
    else if( cb == CB_ADMOVE ) early_exit = callback_admove();
    log( cb, true, insist );
}
function_in_out::~function_in_out()
{
    if( saved_cb == CB_PATH ) return;
    log( saved_cb, false, false );
}

void function_in_out::log( CB cb, bool in, bool insist )
{
    std::string diag = sargon_ptr_print();
    bool diff = (diag != current_status);
    if( diff || insist )
    {
        current_status = diag;
        std::string msg = util::sprintf( "%s() %s%s\n%s", lookup[cb], in?"IN":"OUT", diff?"":" (unchanged)", diag.c_str() );
        printf( "%s\n", msg.c_str() );
        //extern void minimax_log( std::string msg );
        //minimax_log( msg );
    }
}


// Sargon data structure
static emulated_memory &m = gbl_emulated_memory;


std::string mem_dump()
{
    return "";
}

const char *wikipedia_tree[] =
{
  //"06",       // 0   6
    "1365",     // 1   3,6,5
    "253",      // 2   5,3
    "354",      // 3   5,4
    "456",      // 4   5,6
    "4745",     // 4   7,4,5
    "33",       // 3   3
    "43",       // 4   3
    "267",      // 2   6,7
    "366",      // 3   6,6
    "46",       // 4   6
    "469",      // 4   6,9
    "37",       // 3   7
    "47",       // 4   7
    "258",      // 2   5,8
    "35",       // 3   5
    "45",       // 4   5
    "386",      // 3   8,6
    "498",      // 4   9,8
    "46"        // 4   6
};

static int wikipedia_nbr_strings = sizeof(wikipedia_tree)/sizeof(wikipedia_tree[0]);
static int wikipedia_string_nbr    = 0;
static int wikipedia_string_offset = 1;

static int admove_count = 0;
static int admove_limit = 2;

void callback_genmov()
{
    static int nbr_calls;
    thc::ChessPosition cp;
    sargon_export_position(cp);
    std::string s = cp.ToDebugStr();
    printf( "GENMOV() call %d, NPLY=%d%s\n", ++nbr_calls, m.NPLY, s.c_str() );

    if( wikipedia_string_nbr < wikipedia_nbr_strings
        && *wikipedia_tree[wikipedia_string_nbr] <= '4'
        && *wikipedia_tree[wikipedia_string_nbr] == ('0'+m.NPLY)
    )
    {
        admove_count = 0;
        admove_limit = (int)strlen(wikipedia_tree[wikipedia_string_nbr]) - 1;
        if( m.NPLY < 4 )
            wikipedia_string_nbr++;
        printf( "GENMOV() %d moves please!\n", admove_limit );
    }
    else
    {
        printf( "ERROR: GENMOV() tree wikipedia_string_nbr=%d\n",  wikipedia_string_nbr );
        if( wikipedia_string_nbr < wikipedia_nbr_strings )
            printf( "*wikipedia_tree[wikipedia_string_nbr]=%c m.NPLY=%d\n", *wikipedia_tree[wikipedia_string_nbr], m.NPLY );
        exit(0);
    }
}

bool callback_points()
{
    if( m.NPLY == 0 )
        return false;
    static int nbr_calls;
    thc::ChessPosition cp;
    sargon_export_position(cp);
    std::string s = cp.ToDebugStr();
    printf( "POINTS() call %d, NPLY=%d%s\n", ++nbr_calls, m.NPLY, s.c_str() );
    if( m.NPLY < 4 )
    {
        int score = 0;
        unsigned int val = sargon_import_value( 1.0 * score );
        m.VALM = val;
        m.MLPTRJ->val = m.VALM;
        printf( "POINTS() injected placeholder %d,%f\n", val, score*1.0 );
    }
    else if( wikipedia_string_nbr < wikipedia_nbr_strings
        && *wikipedia_tree[wikipedia_string_nbr] == ('0'+m.NPLY)
        && *wikipedia_tree[wikipedia_string_nbr] == '4'
        &&  wikipedia_string_offset < strlen(wikipedia_tree[wikipedia_string_nbr])
    )
    {
        int score = wikipedia_tree[wikipedia_string_nbr][wikipedia_string_offset++] - '0';
        score = 0-score;
        unsigned int val = sargon_import_value( 1.0 * score );
        m.VALM = val;
        m.MLPTRJ->val = m.VALM;
        if( wikipedia_tree[wikipedia_string_nbr][wikipedia_string_offset] == '\0' )
        {
            wikipedia_string_offset=1;
            wikipedia_string_nbr++;
        }
        printf( "POINTS() injected %d,%f\n", val, score*1.0 );
    }
    else
    {
        printf( "ERROR: POINTS() tree wikipedia_string_nbr=%d\n",  wikipedia_string_nbr );
        if( wikipedia_string_nbr < wikipedia_nbr_strings )
            printf( "*wikipedia_tree[wikipedia_string_nbr]=%s wikipedia_string_offset=%d\n", wikipedia_tree[wikipedia_string_nbr], wikipedia_string_offset );
        exit(0);
    }
    return true;
}

bool callback_admove()
{
    if( admove_count >= admove_limit )
        return true;
    admove_count++;
    return false;
}

