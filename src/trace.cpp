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
    static int extra_details=3;
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
        std::string s = show_node();
        if( extra_details > 0 )
        {
            std::string scores = show_scores_short();
            scores += " ";
            scores += s;
            s = scores;
        }
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
        if( extra_details > 1 )
        {
            bool with_move_scores = (extra_details>2);
            std::string x2 = show_ply_chains( with_move_scores );
            printf( "%s", x2.c_str() );
        }
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
            printf( "q,d,r,[+/-]n,pn,v|V,s,x (quit,debug,run,goto n,goto ply,view,scores,extra)>" );
            char buf[80];
            buf[0] = '\0';
            fgets( buf, sizeof(buf)-2, stdin );
            while( buf[0]=='v' || buf[0]=='V' )
            {
                std::string s = show_ply_chains( buf[0]=='V' );
                printf( "%s", s.c_str() );
                fgets( buf, sizeof(buf)-2, stdin );
            }
            while( buf[0]=='s' || buf[0]=='S' )
            {
                std::string s = show_scores_long();
                printf( "%s", s.c_str() );
                fgets( buf, sizeof(buf)-2, stdin );
            }
            if( buf[0]=='q' || buf[0]=='Q' )
            {
                exit(0);
                return;
            }
            if( buf[0]=='x' || buf[0]=='X' )
            {
                if( extra_details < 3)
                {
                    ++extra_details;
                    printf( "Single step detail level increased to %d\n", extra_details );
                }
                else
                {
                    extra_details = 0;
                    printf( "Single step detail level reset to 0\n" );
                }
                keep_going = true;
                extra_count--;
                continue;
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

/*

    The diagnostics emitted by this function get to the heart of how
    Sargon works. All the generated moves are stored in one big list
    in memory. The list starts with all the moves at ply 1. One and
    only one of those moves is the current move, the list continues
    with all available ply 2 moves after the ply 1 current move is
    played. Then one of these ply 2 moves is the current move at that
    ply giving us a current position after 2 moves and the list
    continues with all available ply moves in that position.
    
    As an example, imagine Sargon is trying to find a move from the
    opening position, at depth 4 ply (with no opening book).

    First it generates all the moves in the opening position and
    statically sorts them in place using a linked list. This is
    provisional, rough sorting that we later refine with search, but
    let's imagine it's pretty smart and gives us an order like
    e4,d4,c4,Nf3 ... f3 (Is 1.f3 the worst opening move? not sure).
    
    Then it internally "plays" the first move (so 1.e4) and
    generates and sorts all the ply two moves after that. Hopefully
    something like e5,c5,c6,e6,g6,Nf6. Remember all these moves
    are stored in one big list, it's actually a very efficient way
    to build a move tree.
    
    The process continues, again we "play" first move in this ply 2
    list so we now have e4 e5 on the board, and we generate and sort
    all legal moves at ply 3 storing them immediately after the ply
    2 moves. Of course we keep a stack of pointers to keep track of
    where each ply's move list ends and the next ply's start. Now
    we have something at ply 3 like Nf3,Nc3,Bc4,f4... etc for the
    position after e4 e5. Next we play Nf3 and generate the ply 4
    moves after e4 e5 Nf3 adding them to the end of the one big
    list (of course). At this point we deviate from the process so
    far. We are now at ply 4, the specified search depth and we
    don't sort the moves. Sorting at the earlier plies speeds up
    Alpha-Beta pruning (we'll get to that later) but isn't useful
    at the final ply so it's omitted to make things as fast as
    possible.

    This is a picture of how our big list looks now;

    1: ->e4,d4,c4,Nf3,g3,b3,f4 ... f3
    2: ->e5,c5,c6,e6,g6,Nf3,d6 ... b5
    3: ->Nf3,Nc3,Bc4,f4,d4,c3,Be2 ... b4
    4u: ->a5,a6,b5,b6 ... Ba3,Ne7,Nf6,Nh6

    The u in 4u indicates unsorted, chess players will immediately
    recognise the weird order, compared to the earlier plies.

    Sargon keeps two stacks of ply pointers, one for the current
    location in each ply (shown with ->) and one for the end of
    storage of each ply.
    
    At the moment the current location corresponds to the start
    of each ply list, but that situation is only temporary. Note
    that Sargon's end of ply storage pointers don't correspond
    to the start and end of the sorted lines, because the sort is
    performed in place with a linked list (the moves themselves
    don't move, only the link pointers).

    Sargon doesn't actually keep pointers to the start of the ply
    lists, it doesn't need them. To trace and debug Sargon's
    workings it is useful to have these pointers though, so there
    is now debug only code to calculate these positions. Sargon
    does keep track of the end of the ply lists of course, using
    a conventional NULL link pointer to indicate end of list.

    Returning to Sargon's depth 4 search for the best White
    opening move, Sargon now statically evaluates each 'leaf'
    position after each ply 4 move in turn, to find the best
    such Black move. The score of the best move is in turn the
    backed up (refined) score of White move Nf3 in the sequence
    e4 e5 Nf3.

    Up until now we've always been going in one direction, we've
    been 'descending' down the tree. To make further progress
    it's now time to do our first 'ascending' up the tree.

    Sargon undoes Nf3 at ply 3 and plays Nc3, the second move of
    the ply 3 list instead. Then the old Nf3 ply 4 moves are
    efficiently flushed by popping the last end of ply storage
    pointer off the stack and using that location to generate
    and store a new list of Nc3 ply 4 moves.

    Sargon evaluates all these ply 4 moves to get a backed up
    score for Nc3 in the sequence e4 e5 Nc3.

    I'm sure you see where this is going. Sargon rinses and
    repeats until it has backed up scores for all of Nf3, Nc3,
    Bc4 etc. The best score for that White move establishes the
    backed up score for Black move e5 in the sequence e4 e5.

    Then Sargon will undo e5 and try c5 instead. That will
    require a new ply 3 list for the first time, and a whole
    series of ply 4 lists. But eventually we get the score for
    c5 then all the other ply 2 moves yielding the best Black
    reply to e4 which establishes the score for e4.

    Once that's done Sargon will undo e4 and try d4 at ply 1, and
    for the first time we get a new ply 2 list, and of course
    many ply 3 lists and many many ply 4 lists.

    A bit later still, the moves in memory will look like this;
    
    1: e4,d4,->c4,Nf3,g3,b3,f4 ... f3
    2: Nf6,->e5,c5,e6,c6,d6,g6 ... f6
    3: g3,->Nc3,e3 ... f4
    4u: a5,a6,b5,->b6 ... Ba3,Ne7,Nf6,Nh6

    Note that Sargon is done with the moves before arrows. So
    1.e4 and 1.d4 have been completely analysed, that is their
    final backed up score has been established, and the best
    one kept (this diagram doesn't show the scores of 1.e4 or
    1.d4 so we don't know which one is the best move so far).

    At ply 2 c4 Nf6 has been completely analysed and its
    final backed up score established. This means that we have
    a partial score for 1.c4, it will be the score of c4 Nf6
    or worse if Sargon finds a better reply than Nf6. Sargon
    is currently working on c4 e5 to see if it is better than
    c4 Nf6. If c4 e5 is better than c4 Nf6 then we know the
    score for 1.c4 will be score of c4 e5 or worse. This is
    minimax, the score of 1.c4 gets worse as the score to the
    responses to 1.c4 get better.

    Note that the final backed up scores of each of
    c4 e5 g3 and c4 e5 Nc3 a5 and c4 e5 Nc3 a6 and
    c4 e5 Nc3 b5 have all been established as we work on the
    partially complete c4 e5 score. Every node in the tree
    is either completely backed up (we know it's score),
    partially backed up (we know it's score is equal to or
    worse than the best score found so far), or it's
    provisional (only the static score has been calculated).

    To track partial and complete scores we only need to store
    the best score at each ply, so for this 4 ply search we
    need an array of four scores.

    If we imagine the 1.d4 score is better than the 1.e4 score
    then the entry in the array for ply 1 is the score for 1.d4.

    The entry in the array for ply 2 is the c4 Nf6 score. It
    will be replaced by the score of c4 e5 if c4 e5 scores
    better than c4 Nf6.

    The entry in the array for ply 3 is the c4 e5 g3 score. It
    will be replaced by the score of c4 e5 Nc3 if c4 e5 Nc3
    scores better than c4 e5 g3.

    The entry in the array for ply 4 is whichever one of the
    c4 e5 Nc3 a5, c4 e5 Nc3 a6, c4 e5 Nc3 b5 scores is best.
    It might be about to be replaced by the score of
    c4 e5 Nc3 b6 if that turns out to be the best ply 4
    score so far.

    So the ply 1 score is the best opening position score (so
    far), the ply 2 score is the best c4 score (so far) the
    ply 3 score is the best c4 e5 score so far, the ply 4 score
    is the best c4 e5 Nc3 score so far.
    
    Sorry to belabour these points but a complete and clear
    understanding of this process is essential to really
    understanding the Sargon code.

    Now imagine that the score for c4 e5 when we finish the
    checking all the backed up ply 3 White moves is great
    for Black (bad for White). Then the score for c4 e5
    replaces the score for c4 Nf6 at ply 2. But wait there's
    more. We should also routinely compare this score to the
    ply 1 score above (currently the score for 1.d4). The
    score for 1.c4 is going to be this (bad for White)
    score for 1.c4 e5 or even worse. So 1.c4 is refuted
    already, it will not supplant 1.d4 we can abort the full
    analysis of 1.c4 already and move on to 1.Nf3.

    This is Alpha Beta pruning, the implementation is almost
    trivially easy in Sargon - a real advantage over more
    conventional recursive implementations of minimax and
    alpha-beta (which can be very mind bending in my
    opinion).

    If we think of ply 1 as a maximising White ply, ply 2
    as a maximising Black ply, ply 3 as a maximising White
    ply etc. then Alpha Beta comparisons are comparison
    of adjacent plies for the same colour, plies 1 and 3
    in the example just given. We perform the comparison
    with all such adjacent colour plies (ply delta is
    two), not just ply 1 and 3.
    
    A possible example of Alpha Beta at plies 2 and 4;
    If c4 e5 doesn't turn out to be great for Black as
    it was in the last example we will of course keep on
    working on 1.c4 by moving on to c4 c5. Ply 3 might
    then be quite different, for example we might soon
    reach the following picture.

    1: e4,d4,->c4,Nf3,g3,b3,f4 ... f3
    2: Nf6,e5,->c5,e6,c6,d6,g6 ... f6
    3: Nc3,->Nf3,g3,e3 ... h3
    4u: a5,->a6,b5,b6 ... Qb6,Qa5,Nf6,Nh6

    Ply 4 is a little different too, the Black queen
    has been liberated (by c5) rather than the king side
    minor pieces (by e5).

    It's a bit of a stretch but imagine that completing
    the list of ply 4 moves establishes that
    c4 c5 Nf3 is tremendous for White. So c4 c5 will have
    this score or *worse*. Consequently, assuming c4 Nf6
    or c4 e5 (whichever was best) isn't as bad as that
    score, we can prune further analysis of c4 c5 and
    move on to c4 e6 without bothering about c4 c5 g3,
    c4 c5 e3 etc.

    A detail worth noting is that the alpha beta comparison
    is a better than or equal comparison rather than a
    better than comparison (eg is c4 c5 Nf3 is better than or
    equal to c4 Nf6?). This is an optimisation, it's worth
    pruning c4 c5 analysis now rather than continuing when we
    know c4 c5 can perhaps match c4 Nf6 but definitely can't
    beat it.

     
     
     Alpha beta often arises in routine exchanges. White to move can exchange
     minor pieces for no nominal gain, only one Black piece can recapture.
     When ply 1 is the capture, the ply 2 list will presumably start with the
     recapture.
     The recapture will be the completely analysed and will establish the
     initial ply 1 score in the scoring table. The alternatives to recapture
     will all be abandoned by Alpha-Beta very quickly because they are so easy
     to refute (any White move that keeps the free material will )


    Sargon keeps an array of best scores but it doesn't keep an
    array of best moves - in other words it doesn't remember
    the variation that it considers best play from the start
    position. Sargon was originally just a player not an
    analyst, it just remembered the best move at ply 1 (1.d4
    in the partially complete calculation we are following).

    However we have extended the basic Sargon code to track
    the PV (principal variation) but the algorithm is out of
    scope for this introduction.

 */

 /*

    Putting these ideas together gives us FNDMOV() (calculate
    best move) pseudo code, the heart of Sargon. This pseudo
    code ignores some details, including an extra ply beyond
    PLYMAX whenever a move gives check, to check for checkmate;

    Set NPLY = 1 and generate move ply 1 list
    If not at max ply, score and sort the moves
    Loop through the current move list
         If no more moves in move list
            If NPLY == 1 return
            Get SCORE (a) from score per ply array and Ascend (NPLY--, undo move)
         Else if more moves in move list
            Make the move
            If not yet at max depth
                Generate moves at next ply and Descend (NPLY++)
                If not at max ply, score and sort the moves
                Continue loop to iterate through the new move list
            Else if max depth
                Evaluate SCORE (b) at leaf node using POINTS()
                Unmake the move
        SCORE available, from (a) or (b) above
        If score is <= (better or equal to) score above in score per ply array
            Alpha Beta cutoff, Ascend (NPLY--, undo move), abandon
             this move list, the move that created the position
             that spawns this move list is worse (or at least no
             better) than an alternative. The alternative might not
             be be fully analysed but we've establised a lower
             bound that it will be as good as or better.
        If score is < (better than) score in score per ply array
            Update score per ply array
            If NPLY == 1 update best move found to date, if it is
             mate on the move return
    End loop


     Sargon points system is squeezed into 1 byte of dynamic range, for ease
     of programming on Z80. Zargon retains this, but some useful simplification
     could be achieved by changing to int16 or int32 representaton. For now
     the 1 byte system is retained and the details broken down as follows;
     
     Initially the score is calculated as a signed 8 bit integer. Such values
     are intrinsically limited to the range -128 to 127, Sargon uses almost
     all of this range for its points calculation (specifically -126 to 126).

     The value is calculated as 4*LIMIT(30,material) + LIMIT(6,board_control)
     The LIMIT(n,x) function is a saturation function that returns either
     the value of x, or n if x is greater than n, or -n if x is less than
     -n. Therefore initially at least scores are constrained to the range
     -126 to 126 (because 4*30+6 = 126). More positive scores favour White.

     Material and board control are calculated separately. Board control is
     the number of squares controlled, material is the amount of material
     (using a 1,3,3,5,9 convention) in half pawns. Both of these are 'net'
     values to enable a one byte variable to be practical. In fact two
     netting calculations are made for each variable, first a White - Black
     subtraction, then a Current position - Initial position calculation.
     So the material and board control variables are effectively extra
     material gained by White and extra board control gained by White.
     Negative numbers indicate gains by Black.

     Importantly, Sargon uses a rudimentary SOMA (Swopping Off Material
     Analyzer, see routine XCHNG() and the pin and attack list routines that
     enable it to do its work) to adjust the material count to reflect
     material that's en-prise. This is an attempt to calculate complete
     routine material exchanges in the absense of the extra ply that could
     do that more comprehensively and accurately. It attempts to account
     for at least some pins but it is of course vulnerable to zwichenzugs
     and other tactical nuances messing up its evaluations. This is an
     inevitable downside of simple, fixed depth search.

     The Minimax and Alpha Beta algorithms operate on unsigned 8 bit scores
     rather than signed 8 bit scores. The main reason for this is that the
     Z80 can compare unsigned 8 bit numbers more efficiently than signed 8
     bit numbers. To get the unsigned scores, Sargon adds 128 to the score,
     shifting it into the unsigned range 2 to 254 (midpoint 128).

     In the original Z80 code Sargon somewhat confusingly uses the Z80
     negate opcode to operate on these numbers that it is clearly treating
     as unsigned 8 bit values. This confused me for a very long time! It
     turns out that negating signed integers is accomplished by the neat
     'twos complement' trick of inverting every bit and adding 1. This
     does something useful even if we are treating our bits as an
     unsigned integer;

     0 -> 0         (0x00 -> 0x00)
     1 -> 255       (0x01 -> 0xff)
     2 -> 254       (0x02 -> 0xfe)
     3 -> 253       (0x03 -> 0xfd)
     ...
     127 -> 129     (0x7f -> 0x81)
     128 -> 128     (0x80 -> 0x80)
     129 -> 127     (0x81 -> 0x7f)
     ...
     253 -> 3       (0xfd -> 0x03)
     254 -> 2       (0xfe -> 0x02)
     255 -> 1       (0xff -> 0x01)

     So for our purposes 'negate' does a chesswise negate on the scores,
     scores that are good for White and bad for Black are flipped around to
     be good for Black and bad for White. The midpoint of 128 is unaffected
     and 0 can be neatly reserved as a sentinel. Summarising, we have a
     balanced system with 127 favourable scores for each side, one balanced
     score (128) and one available sentinel value (0). A negate operation
     flips the evaluation symmetrically exactly as we would like it.



     extreme/sentinel values (more about that later). The signedness is converted
     to a different convention, 127 is 1/8 pawn better for the side to move
     (rather than White), 128 is balanced, 129 is 1/8 pawn worse for the side to
     move. Smaller values are increasingly better for the side to move, larger
     values are increasingly worse.

     Scores approaching zero are about 16 pawns better for the side to move,
     scores approaching 255 are about 16 pawns worse for the side to move.
     But what about 0 and 255?

     Sargon reserves 0 to mean illegal move and 255 to mean mate for the side
     to move. That's kind of consistent 0 being even worse than any legal move
     and mate in 1 being better than any other legal move.

     Once all these calculations and adjustments are made, the material and
     board control scores are combined into the signed 8 bit representation
     using the formula

        points = 4*LIMIT(30,material) + LIMIT(6,board control)

     The LIMIT() function saturates the material and board control values
     to +- 30 half pawns and +- 6 squares respectively, and the multiplication
     by 4 weights material more highly. This calculation limits points to
     the range -126 to +126 (2 to 254 after converting to unsigned representation)
     neatly avoiding the sentinel values.

     One problem with unmodified Sargon is that it considers all mates to be
     equal, a mate discovered at ply 3 is not weighted more highly than
     a mate at ply 6 (say). It plays the mate it finds first, not the quickest
     mate. This can be quite annoying and I was happy to apply a fairly simple
     fix in Zargon. We start by modifying the basic points formula slightly

        points = 4*LIMIT(29,material) + LIMIT(6,board control)

     I decided distinguishing between 14.5 and 15 pawns of extra material is
     rarely important (certainly not in any of my test positions). Note that
     some of Sargon's material adjustment calculations do introduce half pawn
     values, so 14.5 is not the same as 14, despite the basic 1,3,3,5,9
     material convention.

     The benefit of limiting to 29, rather than 30, is that the points values
     are now in the range -122 to +122 (signed) and 6 to 250 (unsigned).
     This makes room for for additional mate sentinel values, 251,252,253
     and 254. Mate in 1 remains 255, mate in 2 is now 254, mate in 3 is
     253, mate in 4 is 252, mate in 5 is 251.







        0xff=-127, 0xfe=-126 ... 0x81=-1, 0x80=0, 0x7f=1 ... 0x01=127 0x00=flag/illegal
      127 positive scores uint8_t 0x7f-0x01 (8 points is one pawn, so 127/8 = 15.75 pawns is max score)
        1 zero score 0x80
      127 negative scores uint8_t 0x81-0xff
        1 special flag/sentinel value 0, means illegal move

     Confusingly, more negative scores are better: so 0xff = -127 is the best
     move. In fact 0xff is reserved for mate.
     In original Sargon, the negative or positive score tops out at 126 leaving
     room for mate [basic formula is 4*LIMIT(30,material) + LIMIT(6,board_control)]
     So top score is actually 126/8 = 15.5 pawns.
     We have tweaked this, changing the LIMIT from 30 to 29 creating room for
     4 more "mate" scores, 0xfe (mate in 2), 0xfd (mate in 3), 0xfc (mate in 4)
     and 0xfb (mate in 5 or more). 0xff now means mate in 1.
     The extra mate codes mean Zargon now no longer considers all mates to be
     equivalent

    */

    s += "Ply linked lists\n";
    thc::ChessRules cr(start_position);
    thc::Move mv;
    for( int idx=1; idx<=m.NPLY; idx++ )
    {
        s += util::sprintf( "%d:", idx );

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
            if( ml == m.PLYIX[idx].link_ptr )
                s += "current->";
            if( ml == m.BESTM )
                s += "BESTM->";
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
            printf( "%s illegal, broken tree?\n", terse.c_str() );
            break;
        }
    }
    return s;
}

/*
std::string show_ply_chains( ML *parm1, const char *parm1_name,
                             ML *parm2, const char *parm2_name,
                             ML *parm3, const char *parm3_name  )
{
    std::string space1_name = parm1_name ? " "+std::string(parm1_name) : "?$?";
    std::string space2_name = parm2_name ? " "+std::string(parm2_name) : "?$?";
    std::string space3_name = parm3_name ? " "+std::string(parm3_name) : "?$?";
    std::string s;
    s += "PLYIX[]\n";
    int first_ply = (m.MLPTRI == &m.PLYIX[-1] ? -1 : 0);
    int final_ply = 0;
    for( int i = sizeof(m.PLYIX)/sizeof(m.PLYIX[0]) - 1; i>=0; i-- )
    {
        ML *ml = m.PLYIX[i].link_ptr;
        if( ml )
        {
            final_ply = i;
            break;
        }
    }
    if( m.MLPTRI - m.PLYIX >final_ply )
        final_ply = (int)(m.MLPTRI - m.PLYIX);
    bool flag_mlptri=false, flag_mlptrj=false, flag_mlnxt=false, flag_mllst=false, flag_bestm=false;
    bool flag_parm1=false,  flag_parm2=false,  flag_parm3=false;
    if( !parm1_name )
        flag_parm1 = true;
    if( !parm2_name )
        flag_parm2 = true;
    if( !parm3_name )
        flag_parm3 = true;
    for( int i=first_ply; i<=final_ply; i++ )
    {
        s += util::sprintf( "%d: ", i );
        ML *ml = (ML *)&m.PLYIX[i];
        if( i%2 != 0 )
        {
            ml = ml->link_ptr;
            if( ml >= m.MLIST )
                s += util::sprintf( " m.LIST[%d]", (int)(ml - m.MLIST) );
        }
        else
        {
            if( (ML *)m.MLPTRI == ml )
            {
                s += "<-MLPTRI";
                flag_mlptri = true;
            }
            if( m.MLPTRJ == ml )
            {
                s += "<-MLPTRJ";
                flag_mlptrj = true;
            }
            if( m.MLLST == ml )
            {
                s += "<-MLLST";
                flag_mllst = true;
            }
            ml = ml->link_ptr;
            for( int j=0; j<1000 && ml; j++ )
            {
                if( m.MLPTRJ == ml )
                {
                    s += " MLPTRJ";
                    flag_mlptrj = true;
                }
                if( m.MLNXT == ml )
                {
                    s += " MLNXT";
                    flag_mlnxt = true;
                }
                if( m.MLLST == ml )
                {
                    s += " MLLST";
                    flag_mllst = true;
                }
                if( m.BESTM == ml )
                {
                    s += " BESTM";
                    flag_bestm = true;
                }
                if( parm1 == ml )
                {
                    s += space1_name;
                    flag_parm1 = true;
                }
                if( parm2 == ml )
                {
                    s += space2_name;
                    flag_parm2 = true;
                }
                if( parm3 == ml )
                {
                    s += space3_name;
                    flag_parm3 = true;
                }
                if( ml >= m.MLIST )
                #ifdef DEBUG_MOVE_EXTENSIONS
                    s += util::sprintf( " (%d,%lu,%d)", (int)(ml - m.MLIST), ml->creation_count, ml->creation_ply );
                #else
                    s += util::sprintf( " (%d)", (int)(ml - m.MLIST) );
                #endif
                else
                    s += " ???";
                std::string t;
                #ifdef DEBUG_MOVE_EXTENSIONS
                t += ml->creation_piece;
                t += ml->terse;
                #else
                t = sargon_export_move( ml );
                if( t == "" )
                    t = "----";
                #endif
                s += t;
                s += util::sprintf( "[%s]",  show_score(ml->val).c_str() );
                ml = ml->link_ptr;
            }
        }
        s += "\n";
    }

    // If any unaccounted for, show them
    if( !flag_mlptri || !flag_mlptrj || !flag_mlnxt || !flag_mllst || !flag_bestm ||
        !flag_parm1  || !flag_parm2  || !flag_parm3 )
    {
        s += "others:";
        const char *desc="?";
        ML *ml = 0;
        bool flag = false;
        for( int i=0; i<8; i++ )
        {
            switch(i)
            {
                case 0: desc=" MLPTRI";      flag = flag_mlptri;     ml = (ML *)m.MLPTRI;   break;
                case 1: desc=" MLPTRJ";      flag = flag_mlptrj;     ml = m.MLPTRJ;         break;
                case 2: desc=" MLNXT";       flag = flag_mlnxt;      ml = m.MLNXT;          break;
                case 3: desc=" MLLST";       flag = flag_mllst;      ml = m.MLLST;          break;
                case 4: desc=" BESTM";       flag = flag_bestm;      ml = m.BESTM;          break;
                case 5: desc=space1_name.c_str(); flag = flag_parm1; ml = parm1;            break;
                case 6: desc=space2_name.c_str(); flag = flag_parm2; ml = parm2;            break;
                case 7: desc=space3_name.c_str(); flag = flag_parm3; ml = parm3;            break;
            }
            if( !flag )
            {
                s += desc;
                if( !ml )
                    s += " NULL";
                else if( &m.PLYIX[0]<=(ML_HEAD *)ml && (ML_HEAD *)ml<=&m.PLYIX[40] )
                    s += util::sprintf( " PLYIX[%d]", (int)((ML_HEAD *)ml - &m.PLYIX[0]) );
                else if( ml >= m.MLIST )
                {
                    s += util::sprintf( " (%d)", (int)(ml - m.MLIST) );
                    std::string t = sargon_export_move( ml );
                    if( t == "" )
                        t = "----";
                    s += t;
                    s += util::sprintf( "[%s]",  show_score(ml->val).c_str() );
                }
                else
                    s += " ???";
            }
        }
        s += "\n";
    }
    return s;
}

*/

