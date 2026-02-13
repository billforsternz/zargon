//
//  Testing Bridge between Sargon-x86 and Zargon
//

#include <string>
#include <vector>
#include "util.h"
#include "bridge.h"
#include "sargon-asm-interface.h"
#include "z80_registers.h"

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
        bool in_range = THRES>=0 && (THRES<=run_count && run_count<=THRES+1000);
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

std::string mem_dump()
{
    std::string s;
#if 0
    std::vector<std::pair<int,const char *>> vars=
    {
        { BOARDA  ,"BOARDA" },
        { ATKLST  ,"ATKLST" },
        { PLISTA  ,"PLISTA" },
        { POSK    ,"POSK" },
        { POSQ    ,"POSQ" },
        { SCORE   ,"SCORE" },
        { PLYIX   ,"PLYIX" },
        { M1      ,"M1" },
        { M2      ,"M2" },
        { M3      ,"M3" },
        { M4      ,"M4" },
        { T1      ,"T1" },
        { T2      ,"T2" },
        { T3      ,"T3" },
        { INDX1   ,"INDX1" },
        { INDX2   ,"INDX2" },
        { NPINS   ,"NPINS" },
        { MLPTRI  ,"MLPTRI" },
        { MLPTRJ  ,"MLPTRJ" },
        { SCRIX   ,"SCRIX" },
        { BESTM   ,"BESTM" },
        { MLLST   ,"MLLST" },
        { MLNXT   ,"MLNXT" },
        { KOLOR   ,"KOLOR" },
        { COLOR   ,"COLOR" },
        { P1      ,"P1" },
        { P2      ,"P2" },
        { P3      ,"P3" },
        { PMATE   ,"PMATE" },
        { MOVENO  ,"MOVENO" },
        { PLYMAX  ,"PLYMAX" },
        { NPLY    ,"NPLY" },
        { CKFLG   ,"CKFLG" },
        { MATEF   ,"MATEF" },
        { VALM    ,"VALM" },
        { BRDC    ,"BRDC" },
        { PTSL    ,"PTSL" },
        { PTSW1   ,"PTSW1" },
        { PTSW2   ,"PTSW2" },
        { MTRL    ,"MTRL" },
        { BC0     ,"BC0" },
        { MV0     ,"MV0" },
        { PTSCK   ,"PTSCK" },
        { BMOVES  ,"BMOVES" },
        { LINECT  ,"LINECT" },
        { MVEMSG  ,"MVEMSG" },
        { MLIST   ,"MLIST" },
        { MLEND   ,"MLEND" }
    };
    bool need_newline = false;
    int offset=0;
    std::string line;
    std::string s;
    for( const std::pair<int,const char *> v: vars )
    {
        int nlines=16;
        switch( v.first )
        {
            case M1:        nlines=2;   break;
            case MLIST:     nlines=2;   break;
        }
        int len = v.first-offset;
        int blank_lines = 0;
        line.clear();
        for( int lines=0, i=0; v.first!=BOARDA && i<len; i++ )
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
                    line += " ";
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
        offset = v.first;
        if( need_newline)
        {
            if( blank_lines > 0 )
                s += util::sprintf("  (%d blank_line%s)\n", blank_lines, blank_lines>1?"s":"" );
            s += line;
            s += "\n";
        }
        s += util::sprintf("%-6s %04x:\n", v.second, offset );
    }
    s += "\n";
#endif
    return s;
}
