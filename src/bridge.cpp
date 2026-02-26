//
//  Testing Bridge between Sargon-x86 and Zargon
//

#include <string>
#include <vector>
#include "util.h"
#include "bridge.h"
#include "sargon-asm-interface.h"
#include "sargon-interface.h"
#include "zargon.h"

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
    static uint64_t log_nbr;
    std::string diag = sargon_ptr_print();
    bool diff = (diag != current_status);
    if( diff || insist )
    {
        current_status = diag;
        std::string msg = util::sprintf( "%s() %s%s %llu\n%s", lookup[cb], in?"IN":"OUT", diff?"":" (unchanged)", ++log_nbr, diag.c_str() );
        printf( "%s\n", msg.c_str() );
        //extern void minimax_log( std::string msg );
        //minimax_log( msg );
    }
}

// Sargon data structure
static emulated_memory &m = gbl_emulated_memory;

//
// Alpha-Beta pruning example
// 
// From https://en.wikipedia.org/wiki/Alpha%E2%80%93beta_pruning
//
//  ### indicates pruned branches
// 
//                             SCORE                                        MOVE ORDER
// 
// 0 MAX->                       6                                                
//                             _/|\_                                           _/|\_
//                         ___/  |  \___                                   ___/  |  \___    
//                    ____/      |      \____                         ____/      |      \____
//                   /           |           \                       /           |           \
// 1 MIN->          3			 6		      5                     0			 1		      2            
//                 / \           |\          /#                    / \           |\          /#
//                /   \          | \        / #                   /   \          | \        / #
// 2 MAX->       5     3         6  7      5  8                  3     4        14 15     23 24            
//              / \     \       / \  \     |  ##                / \     \       / \  \     |  ##
//             /   \     \     /   \  \    |  # #              /   \     \     /   \  \    |  # #
// 3 MIN->    5     4     3   6     6  7   5  8  6            5     6    12  16    17 21  25  x  x         
//           /|    /|#    |   |    /#  |   |  ##  #          /|    /|#    |   |    /#  |   |  ##  #
//          / |   / | #   |   |   / #  |   |  # #  #        / |   / | #   |   |   / #  |   |  # #  #
// 4       5  6  7  4  5  3   6  6  9  7   5  9  8  6      7  8  9 10 11 13  18 19 20 22  26  x  x  x      


const char *wikipedia_tree[] =
{
    "1 365",     // 0,1,2
    "2 53",      // 3,4
    "3 54",      // 5,6    
    "4 56",      // 7,8 
    "4 74@5",    // 9,10,11  
    "3 3",       // 12   
    "4 3",       // 13   
    "2 67",      // 14,15  
    "3 66",      // 16,17  
    "4 6",       // 18   
    "4 6@9",     // 19,20   
    "3 7",       // 21   
    "4 7",       // 22   
    "2 58",      // 23,24  
    "3 5",       // 25   
    "4 5",       // 26   
    "@ 86",      //        
    "@ 98",      //         
    "@ 6"        //       
};

static int wikipedia_nbr_strings = sizeof(wikipedia_tree)/sizeof(wikipedia_tree[0]);
static int wikipedia_string_nbr    = 0;
static int wikipedia_string_offset = 2;

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
        const char *s = wikipedia_tree[wikipedia_string_nbr] + 2;
        const char *t = s;
        int nbr_pruned=0;
        while( *t )
        {
            if( *t++ == '@' )
                nbr_pruned++;
        }
        admove_count = 0;
        admove_limit = (int)strlen(s) - nbr_pruned;     // eg "74@5" -> 3. '5' is pruned but the move must be generated
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
        if( wikipedia_tree[wikipedia_string_nbr][wikipedia_string_offset] == '@' )
            wikipedia_string_offset += 2;   // skip over a pruned move
        if( wikipedia_tree[wikipedia_string_nbr][wikipedia_string_offset] == '\0' )
        {
            wikipedia_string_offset=2;
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

