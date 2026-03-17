//
//  Testing Bridge between Sargon-x86 and Zargon
//

#include <string>
#include <vector>
#include <stdarg.h>  // For va_start, etc.
#include "util.h"
#include "bridge.h"
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

function_in_out::function_in_out( CB cb )
{
    early_exit = false;
    saved_cb = cb;
    bool insist = false;
    if( cb == CB_PATH ) return;
    else if( cb == CB_SORTM )  insist=true;
    else if( cb == CB_MOVE )   insist=true;
    else if( cb == CB_UNMOVE ) insist=true;
    else if( cb == CB_GENMOV ) { callback_genmov(); insist=true; }
    else if( cb == CB_POINTS ) { early_exit = callback_points(); }
    else if( cb == CB_ADMOVE ) early_exit = callback_admove();
    #ifndef DEBUG_FUNC_TRACE_STUB
    log( cb, true, insist );
    #endif
}
function_in_out::~function_in_out()
{
    bool insist = false;
    if( saved_cb == CB_PATH ) return;
    else if( saved_cb == CB_EVAL && m.NPLY>=m.PLYMAX )  insist=true;
    else if( saved_cb == CB_SORTM )  insist=true;
    else if( saved_cb == CB_MOVE )   insist=true;
    else if( saved_cb == CB_UNMOVE ) insist=true;
    else if( saved_cb==CB_ADMOVE && !early_exit ) callback_admove_exit();
    #ifndef DEBUG_FUNC_TRACE_STUB
    log( saved_cb, false, insist );
    #endif
}

void function_in_out::log( CB cb, bool in, bool insist )
{
    static uint64_t log_nbr;
    #ifdef DEBUG_FUNC_TRACE_FULL
    std::string diag = show_scores();
    diag += show_ply_chains();
    bool diff = (diag != current_status);
    if( diff || insist )
    {
        current_status = diag;
    #else
    if( insist )
    {
        bool diff=true;
        std::string diag = show_scores();
        diag += show_ply_chains();
    #endif
        std::string msg = util::sprintf( "%s() %s%s %llu\n%s", lookup[cb], in?"IN":"OUT", diff?"":" (unchanged)", ++log_nbr, diag.c_str() );
        if( insist )
            tracef( "%s\n", msg.c_str() );
        else
            logf( "%s\n", msg.c_str() );
    }
    #ifdef DEBUG_SHOW_POSITIONS
    if( !in && (cb==CB_MOVE || cb==CB_UNMOVE) )
    {
        thc::ChessPosition cp;
        sargon_export_position(cp);
        std::string s = cp.ToDebugStr(cb==CB_MOVE?"Position after MOVE()":"Position after UNMOVE()");
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
    //    1 special flag/sentinel value 0, means eg illegal move
    std::string s = (val==0 ? "0" : util::sprintf( "%u:%d,%.2f", val, n, f ));
    return s;
}

std::string score_descriptors[40];

std::string show_scores()
{
    std::string s;
    uint8_t *p = m.SCORE;
    int run=0;
    s += util::sprintf( "VALM: %s\n", show_score(m.VALM).c_str() );
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
    if( m.SCRIX-m.SCORE > last_score )
        last_score = (int)(m.SCRIX-m.SCORE);
    for( int i=0; i<=last_score; i++ )
    {
        if( i == m.SCRIX-m.SCORE )
            s += "SCRIX->";
        s += util::sprintf( "%d: (%u) %s\n", i, m.SCORE[i], score_descriptors[i].c_str() );
    }
    return s;
}

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
        ML *ml = &m.PLYIX[i];
        if( i%2 != 0 )
        {
            ml = ml->link_ptr;
            if( ml >= m.MLIST )
                s += util::sprintf( " m.LIST[%d]", (int)(ml - m.MLIST) );
        }
        else
        {
            if( m.MLPTRI == ml )
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
                case 0: desc=" MLPTRI";      flag = flag_mlptri;     ml = m.MLPTRI;      break;
                case 1: desc=" MLPTRJ";      flag = flag_mlptrj;     ml = m.MLPTRJ;      break;
                case 2: desc=" MLNXT";       flag = flag_mlnxt;      ml = m.MLNXT;       break;
                case 3: desc=" MLLST";       flag = flag_mllst;      ml = m.MLLST;       break;
                case 4: desc=" BESTM";       flag = flag_bestm;      ml = m.BESTM;       break;
                case 5: desc=space1_name.c_str(); flag = flag_parm1; ml = parm1;         break;
                case 6: desc=space2_name.c_str(); flag = flag_parm2; ml = parm2;         break;
                case 7: desc=space3_name.c_str(); flag = flag_parm3; ml = parm3;         break;
            }
            if( !flag )
            {
                s += desc;
                if( !ml )
                    s += " NULL";
                else if( &m.PLYIX[0]<=ml && ml<=&m.PLYIX[40] )
                    s += util::sprintf( " PLYIX[%d]", (int)(ml - &m.PLYIX[0]) );
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
void extraf( const char *fmt, ... )
{
    if( log_level < LOG_EXTRA )
        return;
    static bool suppress_output;
    static unsigned long debug_count;
    static bool free_run;
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
        printf( "q,d,r,[+/-]n,pn,v,s (quit,debug,run,goto n,goto ply,view,scores)>" );
        char buf[80];
        fgets( buf, sizeof(buf)-2, stdin );
        while( buf[0]=='v' || buf[0]=='V' )
        {
            std::string s = show_ply_chains();
            printf( "%s", s.c_str() );
            fgets( buf, sizeof(buf)-2, stdin );
        }
        while( buf[0]=='s' || buf[0]=='S' )
        {
            std::string s = show_scores();
            printf( "%s", s.c_str() );
            fgets( buf, sizeof(buf)-2, stdin );
        }
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
void bridge_score_updated( uint8_t *p, uint8_t score )
{
    std::string s = show_node();
    s += " ";
    s += show_score(score);
    int idx = (int)(p-m.SCORE);
    score_descriptors[idx] = s;
    extraf( "SCORE created %s\n", s.c_str() );
}

void bridge_score_descend()
{
    int idx = (int)(m.SCRIX-m.SCORE);
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
    ML *ml = m.PLYIX[2*idx].link_ptr;
    while( idx <= m.NPLY )
    {
        while( idx==m.NPLY && ml && ml!= m.MLPTRJ )
            ml = ml->link_ptr;
        if( idx==m.NPLY && ml!= m.MLPTRJ )
        {
            printf( "### Internal error - couldn't find current move\n" );
            ml = m.PLYIX[2*idx].link_ptr;
        }
        std::string terse = sargon_export_move(ml);
        mv.TerseIn( &cr, terse.c_str() );
        if( idx > 1 )
            s += ' ';
        s += util::sprintf( "%s", mv.NaturalOut(&cr).c_str() );
        cr.PlayMove(mv);
        idx++;
        ml = m.PLYIX[2*idx].link_ptr;
    }
    return s;
}

