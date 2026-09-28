//
//  Was bridge.cpp (testing Bridge between Sargon-x86 and Zargon)
//  Now trace.cpp (trace and debug facilities for Zargon)
//

#include <string>
#include <vector>
#include <stdarg.h>  // For va_start, etc.
#include "util.h"
#include "trace.h"
#include "sargon-interface.h"
#include "zargon.h"

static int log_level = LOG_LEVEL;
static thc::ChessPosition start_position;

// Sargon data structure
static emulated_memory &m = gbl_emulated_memory;


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
void callback_genmov();
bool callback_points();
bool callback_admove();
void callback_admove_exit();

function_in_out::function_in_out( FUNC_ENUM fe )
{
    early_exit = false;
    saved_fe = fe;
    bool insist = false;
    if(      fe == FE_PATH ) return;
    else if( fe == FE_SORTM )  insist=true;
    else if( fe == FE_MOVE )   insist=true;
    else if( fe == FE_UNMOVE ) insist=true;
    else if( fe == FE_GENMOV ) { callback_genmov(); insist=true; }
    else if( fe == FE_POINTS ) { early_exit = callback_points(); }
    else if( fe == FE_ADMOVE ) early_exit = callback_admove();
    #ifndef DEBUG_FUNC_TRACE_STUB
    log( fe, true, insist );
    #endif
}
function_in_out::~function_in_out()
{
    bool insist = false;
    if(      saved_fe == FE_PATH ) return;
    else if( saved_fe == FE_EVAL && m.NPLY>=m.PLYMAX )  insist=true;
    else if( saved_fe == FE_SORTM )  insist=true;
    else if( saved_fe == FE_MOVE )   insist=true;
    else if( saved_fe == FE_UNMOVE ) insist=true;
    else if( saved_fe==FE_ADMOVE && !early_exit ) callback_admove_exit();
    #ifndef DEBUG_FUNC_TRACE_STUB
    log( saved_fe, false, insist );
    #endif
}

void function_in_out::log( FUNC_ENUM fe, bool in, bool insist )
{
    static uint64_t log_nbr;
    #ifdef DEBUG_FUNC_TRACE_FULL
    std::string diag = show_scores_long();
    diag += show_ply_chains();
    bool diff = (diag != current_status);
    if( diff || insist )
    {
        current_status = diag;
    #else
    if( insist )
    {
        bool diff=true;
        std::string diag = show_scores_long();
        diag += show_ply_chains();
    #endif
        std::string msg = util::sprintf( "%s() %s%s %llu\n%s", lookup[fe], in?"IN":"OUT", diff?"":" (unchanged)", ++log_nbr, diag.c_str() );
        if( insist )
            tracef( "%s\n", msg.c_str() );
        else
            logf( "%s\n", msg.c_str() );
    }
    #ifdef DEBUG_SHOW_POSITIONS
    if( !in && (fe==FE_MOVE || fe==FE_UNMOVE) )
    {
        thc::ChessPosition cp;
        sargon_export_position(cp);
        std::string s = cp.ToDebugStr(fe==FE_MOVE?"Position after MOVE()":"Position after UNMOVE()");
        tracef( "%s\n", s.c_str() );
    }
    #endif
}

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

static std::vector<std::string> restricted_moves;

void callback_start_position_register( const thc::ChessPosition &cp )
{
    start_position = cp;
}

void callback_restricted_moves_register( std::string guide )
{
    restricted_moves.push_back(guide);
}

void callback_restricted_moves_clear()
{
    restricted_moves.clear();
}

void callback_genmov()
{
    static int nbr_calls;
    thc::ChessPosition cp;
    sargon_export_position(cp);
    std::string s = cp.ToDebugStr();
    printf( "GENMOV() call %d, NPLY=%d%s\n", ++nbr_calls, m.NPLY, s.c_str() );
    if( restricted_moves.size() != 0 )
        return;        
    bool in_range = wikipedia_string_nbr < wikipedia_nbr_strings;
    const char *guide = in_range ? wikipedia_tree[wikipedia_string_nbr] : 0;
    if( guide && *guide == ('0'+m.NPLY) )
    {
        const char *t = guide+2;
        int nbr_pruned=0;
        while( *t )
        {
            if( *t++ == '@' )
                nbr_pruned++;
        }
        admove_count = 0;
        admove_limit = (int)strlen(guide+2) - nbr_pruned;     // eg "74@5" -> 3. '5' is pruned but the move must be generated
        if( m.NPLY < 4 )
            wikipedia_string_nbr++;
        printf( "GENMOV() %d moves please!\n", admove_limit );
    }
    else
    {
        printf( "ERROR: GENMOV() tree wikipedia_string_nbr=%d\n",  wikipedia_string_nbr );
        if( guide )
            printf( "*guide=%c m.NPLY=%d\n", *guide, m.NPLY );
        exit(0);
    }
}

bool callback_points()
{
    if( m.NPLY == 0 )   // single call to POINTS() at ply 0
        return false;   // Normal processing
    if( restricted_moves.size() != 0 )
        return false;   // Normal processing     
    static int nbr_calls;
    thc::ChessPosition cp;
    sargon_export_position(cp);
    std::string s = cp.ToDebugStr();
    printf( "POINTS() call %d, NPLY=%d%s\n", ++nbr_calls, m.NPLY, s.c_str() );
    bool in_range = wikipedia_string_nbr < wikipedia_nbr_strings;
    const char *guide = in_range ? wikipedia_tree[wikipedia_string_nbr] : 0;
    if( m.NPLY < 4 )
    {
        int score = 0;
        unsigned int val = sargon_import_value( 1.0 * score );
        m.VALM = val;
        m.MLPTRJ->val = m.VALM;
        printf( "POINTS() injected placeholder %d,%f\n", val, score*1.0 );
    }
    else if( guide && *guide=='4' && wikipedia_string_offset < strlen(guide) )
    {
        int score = *(guide  + wikipedia_string_offset++) - '0';
        score = 0-score;
        unsigned int val = sargon_import_value( 1.0 * score );
        m.VALM = val;
        m.MLPTRJ->val = m.VALM;
        if( *(guide  + wikipedia_string_offset) == '@' )
            wikipedia_string_offset += 2;   // skip over a pruned move
        if( *(guide  + wikipedia_string_offset) == '\0' )
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
    bool early_exit=true;

    // Wikipedia example (or other) guide
    #ifdef DEBUG_GUIDED_MOVES
    if( admove_count < admove_limit )
    {
        admove_count++;
        early_exit = false;
    }
    #endif

    // Restricted move guide, early exit UNLESS we find pending move in
    //  restricted move list
    #ifdef DEBUG_RESTRICTED_MOVES
    thc::Square from, to;
    sargon_export_square(m.M1,from);
    sargon_export_square(m.M2,to);
    int ply = 1;
    for( const std::string s: restricted_moves )
    {
        if( ply == m.NPLY )
        {
            size_t len = s.length();
            for( size_t offset=0; offset+4 <= len; offset+=5 )
            {
                std::string terse = s.substr(offset,4);
                if( terse == "****" )
                {
                    early_exit = false;
                    return early_exit;
                }
                thc::Square src = thc::make_square( terse[0], terse[1] );
                thc::Square dst = thc::make_square( terse[2], terse[3] );
                if( from==src && to==dst )
                {
                    early_exit = false;
                    return early_exit;
                }
            }
        }
        ply++;
    }
    #endif
    return early_exit;
}

void callback_admove_exit()
{
    #ifdef DEBUG_MOVE_EXTENSIONS
    static uint32_t creation_count;
    if( m.MLNXT )
    {
        ML *ml = m.MLNXT-1;
        ml->creation_count = ++creation_count;
        ml->creation_ply   = m.NPLY;
        uint8_t piece = m.BOARDA[ml->from];
        const char *lookup = (piece&0x80) ? "?pnbrqk?" : "?PNBRQK?";
        char c = lookup[piece&7];
        ml->creation_piece = c;
        std::string terse = sargon_export_move(ml);
        memcpy( ml->terse, terse.c_str(), 4 );
        ml->terse[4] = '\0';
    }
    #endif
}

std::string show_score( uint8_t val )
{
    int n = val>=0x80 ? val-0x80 : 0-(0x80-val);
    double f = sargon_export_value( val );

    // Sargon points system is;
    //    0xff=-127, 0xfe=-126 ... 0x81=-1, 0x80=0, 0x7f=1 ... 0x01=127 0x00=flag/illegal
    //  127 positive scores uint8_t 0x7f-0x01 (8 points is one pawn, so 127/8 = 15.75 pawns is max score)
    //    1 zero score 0x80
    //  127 negative scores uint8_t 0x81-0xff
    //    1 special flag/sentinel value 0, means illegal move

    // Confusingly, more negative scores are better: so 0xff = -127 is the best
    // move. In fact 0xff is reserved for mate.
    // In original Sargon, the negative or positive score tops out at 126 leaving
    // room for mate [basic formula is 4*LIMIT(30,material) + LIMIT(6,board_control)]
    // So top score is actually 126/8 = 15.5 pawns.
    // We have tweaked this, changing the LIMIT from 30 to 29 creating room for
    // 4 more "mate" scores, 0xfe (mate in 2), 0xfd (mate in 3), 0xfc (mate in 4)
    // and 0xfb (mate in 5 or more). 0xff now means mate in 1.
    // The extra mate codes mean Zargon now no longer considers all mates to be
    // equivalent
    std::string s = (val==0 ? "0" : util::sprintf( "%u:%d,%.2f", val, n, f ));
    if( 0xfc<=val && val<=0xff )
        s += util::sprintf(" mate in %d", (0xff-val)+1);
    else if( 0xfb == val )
        s += " mate in 5 or more";
    return s;
}

std::string score_descriptors[40];

// Default version
std::string show_scores()
{
    std::string s;
    s += util::sprintf( "VALM: %u SCORE[", m.VALM );
    for( int i=0; i<=m.PLYMAX; i++ )
    {
        if( i == m.NPLY )
            s += "NPLY->";
        s += util::sprintf( "%u", m.SCORE[i] );
        if( i+1<=m.PLYMAX )
            s+=", ";
    }
    s += "]";
    return s;
}

// Short version
std::string show_scores_short()
{
    std::string s = "[";
    for( int i=0; i<=m.PLYMAX; i++ )
    {
        s += util::sprintf( "%u", m.SCORE[i] );
        if( i+1<=m.PLYMAX )
            s+=",";
    }
    s += "]";
    return s;
}

// Long version
std::string show_scores_long()
{
    std::string s;
    s += util::sprintf( "%s\n", show_scores().c_str() );
    s += "SCORE[]:";
    s += "\n";
    int last_score = 0;
    for( int i=sizeof(score_descriptors)/sizeof(score_descriptors[0])-1; i>=0; i-- )
    {
        if( last_score==0 && score_descriptors[i] != "" && score_descriptors[i] != "0" )
            last_score = i;
        if( score_descriptors[i] == "" )
            score_descriptors[i] = "0";
    }
    if( m.NPLY > last_score )
        last_score = m.NPLY;
    for( int i=0; i<=last_score; i++ )
    {
        if( i == m.NPLY )
            s += "NPLY->";
        s += util::sprintf( "%d: (%u) %s\n", i, m.SCORE[i], score_descriptors[i].c_str() );
    }
    return s;
}

// tracef() - show progress of chess algorithm
void tracef( const char *fmt, ... )
{
    if( log_level < LOG_TRACE )
        return;
    int size = (int)strlen(fmt) * 3;   // guess at size
    std::string str;
    va_list ap;
    for(;;)
    {
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char *)str.data(), size, fmt, ap);
        va_end(ap);
        if( n>-1 && n<size )    // are we done yet?
        {
            str.resize(n);
            break;
        }
        if( n > size )  // Needed size returned
            size = n + 1;   // For null char
        else
            size *= 4;      // Guess at a larger size
    }
    printf( "%s", str.c_str() );
}

static bool restart_test;
static unsigned long extra_count;
bool callback_restart_test()
{
    bool yes_restart = restart_test;
    restart_test = false;
    if( yes_restart )
    {
        extra_count = 0;
    }
    return yes_restart;
}

// extraf() - show progress of chess algorithm
#ifdef DEBUG_KEEP_EXTRAF
void extraf( const char *fmt, ... )
{
    if( log_level < LOG_EXTRA )
        return;
    static bool suppress_output;
    static unsigned long debug_count;
    #ifdef DEBUG_SINGLE_STEP
    static bool free_run=false;
    #else
    static bool free_run=true;
    #endif
    if( suppress_output )
    {
        if( extra_count==debug_count )
        {
            free_run = false;
            suppress_output = false;
            printf("\n");
        }
        else
        {
            extra_count++;
            return;
        }
    }
    for( bool keep_going=true; keep_going; )
    {
        keep_going=false;
        printf( "%lu) ", ++extra_count );
        std::string s = show_node();
        printf("%s ",s.c_str() );
        int size = (int)strlen(fmt) * 3;   // guess at size
        std::string str;
        va_list ap;
        for(;;)
        {
            str.resize(size);
            va_start(ap, fmt);
            int n = vsnprintf((char *)str.data(), size, fmt, ap);
            va_end(ap);
            if( n>-1 && n<size )    // are we done yet?
            {
                str.resize(n);
                break;
            }
            if( n > size )  // Needed size returned
                size = n + 1;   // For null char
            else
                size *= 4;      // Guess at a larger size
        }
        printf("%s",str.c_str() );
        std::string x2 = show_ply_chains( true );
        printf( "%s", x2.c_str() );
        printf( "\n" );
        static uint8_t target_ply;
        #ifndef DEBUG_SINGLE_STEP
        #ifdef _DEBUG
        if( extra_count == debug_count )
           __debugbreak();
        #endif
        #else
        if( free_run )
        {
            if( extra_count==debug_count )
                free_run = false;
            else if( m.NPLY==target_ply && target_ply!=0 )
            {
                target_ply = 0;
                free_run = false;
            }
        }
        if( !free_run )
        {
            printf( "q,d,r,[+/-]n,pn (quit,debug,run,goto n,goto ply)>" );
            char buf[80];
            buf[0] = '\0';
            fgets( buf, sizeof(buf)-2, stdin );
            if( buf[0]=='q' || buf[0]=='Q' )
            {
                exit(0);
                return;
            }
            if( buf[0]=='r' || buf[0]=='R' )
            {
                free_run = true;
                return;
            }
            if( buf[0]=='d' || buf[0]=='D' )
            {
               #ifdef _DEBUG
               __debugbreak();
               #else
               printf("Sorry, step to debugger in debug builds only\n");
               #endif
               return;
            }
            const char *txt = buf;
            if( buf[0]=='+' || buf[0]=='-' || (buf[0]=='p'||buf[0]=='P') )
                txt++;
            std::string nbr(txt);
            size_t len = nbr.length();
            if( len>0 && nbr[len-1]=='\n' )
                nbr = nbr.substr(0,len-1);
            unsigned long n = (unsigned long)atoll(nbr.c_str());
            if( n > 0 )
            {
                free_run = true;
                if( buf[0]=='p' || buf[0]=='P' )
                    target_ply = (uint8_t)n;
                else
                {
                    if( n > 0 )
                        n--; //best by test
                    if( buf[0] == '+' )
                        debug_count = extra_count+n;
                    else if( buf[0] == '-' )
                        debug_count = extra_count-n;
                    else
                        debug_count = n;
                    if( debug_count < extra_count )
                    {
                        restart_test = true;
                        suppress_output = true;
                        printf( "Test continues and restarts before logging recommences...\n" );
                    }
                }
            }
        }
        #endif
    }
}
#endif

#ifdef DEBUG_KEEP_EXTRAF
void superf( const char *fmt, ... )
{
    if( log_level < LOG_SUPER )
        return;
    std::string s = show_node();
    int col = printf("%s",s.c_str() );
    while( col < 28 )
        col += printf(" ");
    for( int i=0; i<m.NPLY; i++ )
        printf( " " );
    int size = (int)strlen(fmt) * 3;   // guess at size
    std::string str;
    va_list ap;
    for(;;)
    {
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char *)str.data(), size, fmt, ap);
        va_end(ap);
        if( n>-1 && n<size )    // are we done yet?
        {
            str.resize(n);
            break;
        }
        if( n > size )  // Needed size returned
            size = n + 1;   // For null char
        else
            size *= 4;      // Guess at a larger size
    }
    size_t len = str.length();
    if( len>0 && str[len-1] == '\n' )
    {
        str = str.substr(0,len-1);
        printf( "%s (%d:%lu)\n", str.c_str(), m.NPLY, ++extra_count );
    }
    else
    {
        printf( "%s (%d:%lu)", str.c_str(), m.NPLY, ++extra_count );
    }
}
#endif

// logf()   - show all the details
void logf( const char *fmt, ... )
{
    if( log_level < LOG_DETAILED )
        return;
    int size = (int)strlen(fmt) * 3;   // guess at size
    std::string str;
    va_list ap;
    for(;;)
    {
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char *)str.data(), size, fmt, ap);
        va_end(ap);
        if( n>-1 && n<size )    // are we done yet?
        {
            str.resize(n);
            break;
        }
        if( n > size )  // Needed size returned
            size = n + 1;   // For null char
        else
            size *= 4;      // Guess at a larger size
    }
    printf( "%s", str.c_str() );
}

#ifdef DEBUG_TRACK_SCORE
void trace_score_updated( uint8_t *p, uint8_t score )
{
    std::string s = show_node();
    s += " ";
    s += show_score(score);
    int idx = (int)(p-m.SCORE);
    score_descriptors[idx] = s;
    extraf( "SCORE created %s\n", s.c_str() );
}

void trace_score_descend()
{
    int idx = m.NPLY-1;
    score_descriptors[idx+2] = score_descriptors[idx];
    extraf( "SCORE descends %d->%d %s\n", idx, idx+2, score_descriptors[idx].c_str() );
}
#endif

std::string show_node()
{
    std::string s;
    thc::ChessRules cr(start_position);
    thc::Move mv;
    int idx=1;
    while( idx <= m.NPLY )
    {
        ML *ml = m.PLYIX[idx].link_ptr;
        std::string terse = sargon_export_move(ml);
        bool illegal_move = !mv.TerseIn( &cr, terse.c_str() );
        if( idx > 1 )
            s += ' ';
        if( illegal_move )
        {
            s += util::sprintf( "(%s)", terse.c_str() );
            break;
        }
        else
        {
            std::string txt = mv.NaturalOut(&cr);
            s += util::sprintf( "%s", txt.c_str() );
            cr.PlayMove(mv);
            idx++;
        }
    }
    return s;
}

std::string to_algebraic( int sq )
{
    char file = 'a' + sq%10-1;  // 21->a1, 28->h1, 91->a8, 98->h8
    char rank = '1' + sq/10-2;
    std::string ret;
    ret += file;
    ret += rank;
    return ret;
}

std::string show_ply_chains( bool show_score )
{
    std::string s;
    thc::ChessRules cr(start_position);
    thc::Move mv;
    for( int idx=1; idx<=m.NPLY; idx++ )
    {
        s += util::sprintf( "%d%s: [%u] ", idx, idx<m.PLYMAX? "" :"u", m.SCORE[idx] );

        // All moves for this ply are in adjacent memory, with boundaries
        //  of this section of memory stored in array PLYIX_nxt[]
        ML *ml = m.PLYIX_nxt[idx-1];
        ML *ml_nxt = idx<m.NPLY ? m.PLYIX_nxt[idx] : m.MLNXT;

        // An O(N^2) algorithm to find the move with the longest chain of ptrs
        //  starting at that move. If the ply has been sorted, then that is 
        //  the best (static eval) score in this ply, and all moves in the ply
        //  will be in the chain of pointers starting at that move.
        // If the ply hasn't been sorted, then the first move will have the
        //  longest list (and the list will be equivalent to incrementing through
        //  adjacent moves in memory)
        int longest_so_far = 0;
        ML *ml_longest_so_far = ml;
        while( ml < ml_nxt )
        {
            int len = 0;
            for( ML *ml_rover = ml; ml_rover; ml_rover=ml_rover->link_ptr )
                len++;
            if( len >= longest_so_far )
            {
                longest_so_far = len;
                ml_longest_so_far = ml;
            }
            bool double_move = IS_DOUBLE_MOVE(ml->flags);
            ml++;
            if( double_move )
                ml++;
        }

        // Show the moves in linked list order
        ml = ml_longest_so_far;
        while( ml )
        {
            s += ' ';

            // One move in the chain is the current move we are evaluating
            //  at in this ply
            bool gap=false;
            bool need_arrow=false;
            if( ml == m.BESTM )
            {
                s += "BESTM";
                gap = true;
                need_arrow=true;
            }
            if( m.MLPTRI && ml == m.MLPTRI->link_ptr )
            {
                if( gap )
                    s += " ";
                s += "MLPTRI";
                gap = true;
                need_arrow=true;
            }
            if( ml == m.MLPTRJ )
            {
                if( gap )
                    s += " ";
                s += "MLPTRJ";
                need_arrow=true;
            }
            if( ml == m.PLYIX[idx].link_ptr )
            {
                need_arrow=true;
            }
            if( need_arrow )
            {
                s += "->";
            }
            std::string terse = sargon_export_move(ml);
            bool illegal_move = !mv.TerseIn( &cr, terse.c_str() );
            std::string txt = mv.NaturalOut(&cr);
            const char *mv_txt = txt.c_str();
            if( illegal_move )
                s += util::sprintf( "(%s)", terse.c_str() );
            else
            {
                std::string txt = mv.NaturalOut(&cr);
                const char *mv_txt = txt.c_str();
                s += util::sprintf( "%s", mv_txt );
            }

            // Show the score as well
            bool show = show_score && !illegal_move;
            if( show )
                s += util::sprintf( "(%d)", ml->val );
            ml = ml->link_ptr;
        }
        s += "\n";
        ml = m.PLYIX[idx].link_ptr;
        std::string terse = sargon_export_move(ml);
        bool ok = mv.TerseIn( &cr, terse.c_str() );
        if( ok )
            cr.PlayMove(mv);
        else
        {
            printf( "%s illegal\n", terse.c_str() );
            break;
        }
    }
    return s;
}

