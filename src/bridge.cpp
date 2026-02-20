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
bool callback_admove();

function_in_out::function_in_out( CB cb )
{
    early_exit = false;
    saved_cb = cb;
    if( cb == CB_PATH ) return;
    else if( cb == CB_GENMOV ) callback_genmov();
    else if( cb == CB_ADMOVE ) early_exit = callback_admove();
    log( cb, true );
}
function_in_out::~function_in_out()
{
    if( saved_cb == CB_PATH ) return;
    log( saved_cb, false );
}

void function_in_out::log( CB cb, bool in )
{
    std::string diag = sargon_ptr_print();
    if( diag != current_status )
    {
        current_status = diag;
        std::string msg = util::sprintf( "%s() %s\n%s", lookup[cb], in?"IN":"OUT", diag.c_str() );
        printf( "%s\n", msg.c_str() );
        //extern void minimax_log( std::string msg );
        //minimax_log( msg );
    }
}


// Sargon data structure
static emulated_memory &m = gbl_emulated_memory;

std::string mem_dump()
{
#define xBOARDA  offsetof(emulated_memory,BOARDA)
#define xATKLST  offsetof(emulated_memory,wact)
#define xPLISTA  offsetof(emulated_memory,PLISTA)
#define xPOSK    offsetof(emulated_memory,POSK)
#define xPOSQ    offsetof(emulated_memory,POSQ)
#define xSCORE   offsetof(emulated_memory,SCORE)
#define xPLYIX   offsetof(emulated_memory,PLYIX)
#define xM1      offsetof(emulated_memory,M1)
#define xM2      offsetof(emulated_memory,M2)
#define xM3      offsetof(emulated_memory,M3)
#define xM4      offsetof(emulated_memory,M4)
#define xT1      offsetof(emulated_memory,T1)
#define xT2      offsetof(emulated_memory,T2)
#define xT3      offsetof(emulated_memory,T3)
#define xINDX1   offsetof(emulated_memory,INDX1)
#define xINDX2   offsetof(emulated_memory,INDX2)
#define xNPINS   offsetof(emulated_memory,NPINS)
#define xMLPTRI  offsetof(emulated_memory,MLPTRI)
#define xMLPTRJ  offsetof(emulated_memory,MLPTRJ)
#define xSCRIX   offsetof(emulated_memory,SCRIX)
#define xBESTM   offsetof(emulated_memory,BESTM)
#define xMLLST   offsetof(emulated_memory,MLLST)
#define xMLNXT   offsetof(emulated_memory,MLNXT)
#define xKOLOR   offsetof(emulated_memory,KOLOR)
#define xCOLOR   offsetof(emulated_memory,COLOR)
#define xP1      offsetof(emulated_memory,P1)
#define xP2      offsetof(emulated_memory,P2)
#define xP3      offsetof(emulated_memory,P3)
#define xPMATE   offsetof(emulated_memory,PMATE)
#define xMOVENO  offsetof(emulated_memory,MOVENO)
#define xPLYMAX  offsetof(emulated_memory,PLYMAX)
#define xNPLY    offsetof(emulated_memory,NPLY)
#define xCKFLG   offsetof(emulated_memory,CKFLG)
#define xMATEF   offsetof(emulated_memory,MATEF)
#define xVALM    offsetof(emulated_memory,VALM)
#define xBRDC    offsetof(emulated_memory,BRDC)
#define xPTSL    offsetof(emulated_memory,PTSL)
#define xPTSW1   offsetof(emulated_memory,PTSW1)
#define xPTSW2   offsetof(emulated_memory,PTSW2)
#define xMTRL    offsetof(emulated_memory,MTRL)
#define xBC0     offsetof(emulated_memory,BC0)
#define xMV0     offsetof(emulated_memory,MV0)
#define xPTSCK   offsetof(emulated_memory,PTSCK)
#define xBMOVES  offsetof(emulated_memory,BMOVES)
#define xLINECT  offsetof(emulated_memory,LINECT)
#define xMVEMSG  offsetof(emulated_memory,MVEMSG)
#define xMLIST   offsetof(emulated_memory,MLIST)
#define xMLEND   offsetof(emulated_memory,MLEND)
    std::string s;
    std::vector<std::pair<size_t,const char *>> vars=
    {
        { xBOARDA  ,"BOARDA" },
        { xATKLST  ,"ATKLST" },
        { xPLISTA  ,"PLISTA" },
        { xPOSK    ,"POSK" },
        { xPOSQ    ,"POSQ" },
        { xSCORE   ,"SCORE" },
        { xPLYIX   ,"PLYIX" },
        { xM1      ,"M1" },
        { xM2      ,"M2" },
        { xM3      ,"M3" },
        { xM4      ,"M4" },
        { xT1      ,"T1" },
        { xT2      ,"T2" },
        { xT3      ,"T3" },
        { xINDX1   ,"INDX1" },
        { xINDX2   ,"INDX2" },
        { xNPINS   ,"NPINS" },
        { xMLPTRI  ,"MLPTRI" },
        { xMLPTRJ  ,"MLPTRJ" },
        { xSCRIX   ,"SCRIX" },
        { xBESTM   ,"BESTM" },
        { xMLLST   ,"MLLST" },
        { xMLNXT   ,"MLNXT" },
        { xKOLOR   ,"KOLOR" },
        { xCOLOR   ,"COLOR" },
        { xP1      ,"P1" },
        { xP2      ,"P2" },
        { xP3      ,"P3" },
        { xPMATE   ,"PMATE" },
        { xMOVENO  ,"MOVENO" },
        { xPLYMAX  ,"PLYMAX" },
        { xNPLY    ,"NPLY" },
        { xCKFLG   ,"CKFLG" },
        { xMATEF   ,"MATEF" },
        { xVALM    ,"VALM" },
        { xBRDC    ,"BRDC" },
        { xPTSL    ,"PTSL" },
        { xPTSW1   ,"PTSW1" },
        { xPTSW2   ,"PTSW2" },
        { xMTRL    ,"MTRL" },
        { xBC0     ,"BC0" },
        { xMV0     ,"MV0" },
        { xPTSCK   ,"PTSCK" },
        { xBMOVES  ,"BMOVES" },
        { xLINECT  ,"LINECT" },
        { xMVEMSG  ,"MVEMSG" },
        { xMLIST   ,"MLIST" },
        { xMLEND   ,"MLEND" }
    };
    bool need_newline = false;
    int offset=0;
    std::string line;
    for( const std::pair<size_t,const char *> v: vars )
    {
        int nlines=16;
        switch( v.first )
        {
            case xM1:        nlines=2;   break;
            case xMLIST:     nlines=2;   break;
        }
        int len = (int)(v.first-offset);
        int blank_lines = 0;
        line.clear();
        for( int lines=0, i=0; v.first!=xBOARDA && i<len; i++ )
        {
            bool blank = false;
            if( i%16==0 && i+16<=len && 0==memcmp(sargon_mem_base+offset+i,"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",16) )
            {
                blank = true;
                line = "  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00";
                i += 15;
            }
            else
            {
                if( i%8 == 0 )
                {
                    line += " ";
                    if( v.first == xMLEND && i%16!=0 )
                    {
                        thc::Square from, to;
                        bool ok1 = sargon_export_square( *(sargon_mem_base+offset+i), from );
                        bool ok2 = sargon_export_square( *(sargon_mem_base+offset+i+1), to  );
                        if( ok1 && ok2 )
                            line += util::sprintf( " %c%c->%c%c", get_file(from), get_rank(from), get_file(to), get_rank(to) );
                    }
                }
                line += util::sprintf( " %02x", *(sargon_mem_base+offset+i) );
                need_newline = true;
            }
            if( (i+1)%16 == 0 )
            {
                need_newline = false;
                lines++;
                if( lines < nlines )
                {
                    s += line;
                    s += "\n";
                }
                else
                {
                    if( blank )
                    {
                        blank_lines++;
                        if( i+1 >= len )
                        {
                            s += util::sprintf("  (%d blank_line%s)\n", blank_lines, blank_lines>1?"s":"" );
                            break;
                        }
                        else
                        {
                            int thres = nlines<16 ? 1 : 100;
                            if( blank_lines >= thres )
                            {
                                s += util::sprintf("  (at least %d blank_line%s)....\n", thres, thres>1?"s":"" );
                                break;
                            }
                        }
                    }
                    else
                    {
                        if( blank_lines == 0 )
                        {
                            s += line;
                            s += "\n";
                        }
                        else
                        {
                            s += util::sprintf("  (%d blank_lines)\n", blank_lines );
                            blank_lines = 0;
                        }
                    }
                }
                line.clear();
            }
        }
        offset = (int)v.first;
        if( need_newline)
        {
            if( blank_lines > 0 )
                s += util::sprintf("  (%d blank_line%s)\n", blank_lines, blank_lines>1?"s":"" );
            s += line;
            s += "\n";
        }
        s += util::sprintf("%-6s %04x:\n", v.second, offset );
        if( v.first == xSCRIX )
        {
            s += util::sprintf( "  (SCRIX offset = 0x%llx)\n", (m.SCRIX - (uint8_t *)&m) );
        }
    }
    s += "\n";
    return s;
}

static int admove_count = 0;
static int admove_limit = 2;


void callback_genmov()
{
    static int nbr_calls;
    printf( "GENMOV call %d\n", ++nbr_calls );
    admove_count = 0;
    admove_limit = m.NPLY+1;
}

bool callback_admove()
{
    if( admove_count >= admove_limit )
        return true;
    admove_count++;
    return false;
}

