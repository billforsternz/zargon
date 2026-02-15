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

#ifdef BRIDGE_CALLBACK_TRACE
void bridge_callback_trace( CB cb, const z80_registers *reg )
{
    
    // static bool trigger;
    // static int count = 0;
    // static bool detail = false;
    const char *name = 0; //"??";
    bool pnck = false;
    const int64_t THRES=-1; // 151000;
    //if( (cb==CB_END_OF_POINTS || cb==CB_YES_BEST_MOVE) )
    //{
    //    detail = false;
    //    count++;
    //    if( count == 4859 )
    //        trigger = true;
    //    if( count == 4862 )
    //        exit(0);
    //}
    switch(cb)
    {
        //case CB_END_OF_POINTS:
        //{
        //    name = "END_OF_POINTS";
        ////    printf( "CB_END_OF_POINTS(%u) %s\n", count, reg_dump(reg).c_str() );
        ////    if( count > 4850 )
        ////        printf( "%s", mem_dump().c_str() );
        //    break;
        //}
        //case CB_YES_BEST_MOVE:
        //{
        //    name = "YES_BEST_MOVE";
        ////    printf( "CB_YES_BEST_MOVE(%u) %s\n", count, reg_dump(reg).c_str() );
        ////    if( count > 4850 )
        ////        printf( "%s", mem_dump().c_str() );
        //    break;
        //}
        case CB_PATH  :  name = "PATH";   break;
        case CB_MPIECE:  name = "MPIECE"; break;
        case CB_ENPSNT:  name = "ENPSNT"; break;
        case CB_ADJPTR:  name = "ADJPTR"; break;
        case CB_CASTLE:  name = "CASTLE"; break;
        case CB_ADMOVE:  name = "ADMOVE"; break;
        case CB_GENMOV:  name = "GENMOV"; break;
        case CB_ATTACK:  name = "ATTACK"; break;
        case CB_ATKSAV:  name = "ATKSAV"; break;
        case CB_PNCK  :  name = "PNCK";
                         pnck = true;
                         pnck_count++;    break;
        case CB_PINFND:  name = "PINFND"; break;
        case CB_XCHNG :  name = "XCHNG";  break;
        case CB_NEXTAD:  name = "NEXTAD"; break;
        case CB_POINTS:  name = "POINTS"; break;
        case CB_MOVE  :  name = "MOVE";   break;
        case CB_UNMOVE:  name = "UNMOVE"; break;
        case CB_SORTM:   name = "SORTM";  break;
        case CB_EVAL:    name = "EVAL";   break;
        case CB_FNDMOV:  name = "FNDMOV"; break;
        case CB_ASCEND:  name = "ASCEND"; break;
    }
    if( name )
    {
        run_count++;
        //if( run_count == 161 )
        //    printf("This call to ATTACK() should not set P2 to 0\n" );
        if( pnck && pnck_count == 5046 )
            printf("This call to PNCK() should set wact[0] to 1\n" );
        bool in_range = true; //THRES>=0 && (THRES<=run_count && run_count<=THRES+1000);
        if( in_range )
            printf( "%s(%llu) %s\n", name, pnck?pnck_count:run_count, mem_dump().c_str() );
    }
    // run_count++;
    // if( detail )
    // {
    //       printf( "%s(%llu) %s\n", name, run_count, mem_dump().c_str() );
    //    static int second_level_count;
    //    if( trigger )
    //    {
    //        printf( "%s(%u) %s\n", name, ++second_level_count, reg_dump(reg).c_str() );
    //        printf( "%s", mem_dump().c_str() );
    //        if( second_level_count == 2124 )
    //            printf("");
    //    }
    // }
}
#endif

std::string reg_dump( const z80_registers *reg )
{
    std::string s;
    if( !reg )
        s = "Z80 registers not available";
    else
    {
        s = util::sprintf( "a=%02x, bc=%04x, de=%04x, hl=%04x, ix=%04x, iy=%04x",
                reg->af & 0xff, reg->bc, reg->de, reg->hl, reg->ix, reg->iy );
    }
    return s;
}

// Sargon data structure
static emulated_memory &m = gbl_emulated_memory;

std::string mem_dump()
{
#define xBOARDA  offsetof(emulated_memory,BOARDA)
#define xATKLST  offsetof(emulated_memory,ATKLST)
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
            s += util::sprintf( "  (SCRIX offset = 0x%llx)\n", (m.SCRIX - (byte_ptr)&m) );
        }
    }
    s += "\n";
    return s;
}
