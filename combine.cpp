#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <stdio.h>
#include "util.h"

struct Identifier
{
    std::string name;
    int line_nbr  = 1;
    int nbr_lines = 0;
    int nbr_proceeding_comment_lines = 0;
};

void cpp_process( std::ifstream &fin, std::vector<Identifier> &cpp_identifiers );
void asm_process( std::ifstream &fin, std::vector<Identifier> &asm_identifiers );
void tidy_cpp_line( std::string &line );

int combine( int argc, const char *argv[] )
{
    const int COL_COMMENT = 40;
    const char *arg1 = "/users/bill/documents/github/zargon/sargon-z80-modified.cpp";
    const char *arg2 = "/users/bill/documents/github/zargon/sargon-z80.asm";
    const char *arg3 = "/users/bill/documents/github/zargon/zargon.cpp";
    #if 0
    if( argc != 4 )
    {
        printf( "Simple helper while converting Sargon to C\n" );
        printf( "Usage:\n" );
        printf( " combine in1 in2 out\n" );
        printf( "  in1 = asm-as-cpp-with-macros\n" );
        printf( "  in2 = asm\n" );
        printf( "  out = in1 with in2 lined up as comments\n" );
        return -1;
    }
    arg1 = argv[1];
    arg2 = argv[2];
    arg3 = argv[3];
    #endif
    std::ifstream fin1(arg1);
    if( !fin1 )
    {
        printf( "Cannot open %s for reading\n", arg1 );
        return -1;
    }
    std::ifstream fin2(arg2);
    if( !fin2 )
    {
        printf( "Cannot open %s for reading\n", arg2 );
        return -1;
    }
    std::ofstream fout(arg3);
    if( !fout )
    {
        printf( "Cannot open %s for writing\n", arg3 );
        return -1;
    }
    std::vector<Identifier> cpp_identifiers;
    cpp_process( fin1, cpp_identifiers );
    std::vector<Identifier> asm_identifiers;
    asm_process( fin2, asm_identifiers );
    fin1.clear();
    fin1.seekg(0, std::ios::beg); 
    fin2.clear();
    fin2.seekg(0, std::ios::beg); 
    int line_nbr_in = 0;
    int line_nbr_out = 0;
    size_t isection = 0;
    while( isection<cpp_identifiers.size() && isection<asm_identifiers.size() )
    {
        Identifier &p = cpp_identifiers[isection];
        Identifier &q = asm_identifiers[isection];
        if( p.name != q.name )
        {
            printf( ".cpp and .asm out of sync, aborting\n" );
            return -1;
        }
        if( p.name == "PLYIX" )
            printf( "Debug\n" );
        
        // top    print this many from the top
        // bottom print this many from the bottom

		// First estimate
        int top    =  isection>0 ? cpp_identifiers[isection-1].nbr_lines : 5;      // 5 title lines first
        int bottom =  asm_identifiers[isection].nbr_proceeding_comment_lines + 1;  // +1 for identifier itself
        isection++;

		// Number of cpp lines in next section is number of lines we need
        int need_total  =  p.line_nbr - line_nbr_out;
        if( need_total < 0 )
            need_total = 0;
		
		// Number of asm lines in next section is number of lines we have
        int have_total  =  q.line_nbr - line_nbr_in;
        if( have_total < 0 )
            have_total = 0;

		// While top + bottom > have_total reduce bottom to 1 then reduce top
        while( top+bottom > have_total )
        {
            if( bottom>1 )
                bottom--;
            else if( top>0 )
                top--;
        }
			
		// While top + bottom > need_total reduce bottom to 1 then reduce top
        while( top+bottom > need_total )
        {
            if( bottom>1 )
                bottom--;
            else if( top>0 )
                top--;
        }
			
        // Number of empty lines, may be zero
		int empty = need_total - (top+bottom);

		// Get all .cpp lines for this section
        std::vector<std::string> vcpp;
        for( int i=0; i<need_total; i++ )
        {
            std::string line;
            if( !std::getline(fin1, line) )
                break;
            tidy_cpp_line(line);
            vcpp.push_back(line);
        }

		// Get all .asm lines for this section
        std::vector<std::string> vasm;
        for( int i=0; i<have_total; i++ )
        {
            std::string line;
            if( !std::getline(fin2, line) )
                break;
            line_nbr_in++;
            vasm.push_back(line);
        }

		// Print top from the top of vasm
        int icpp=0;
        for( int i=0; i<top; i++ )
        {
            std::string s = vasm[i];
            std::string scpp = vcpp[icpp++];
            util::putline(fout,scpp + "//" + s);
            line_nbr_out++;
        }

		// Print empty lines
        for( int i=0; i<empty; i++ )
        {
            std::string scpp = vcpp[icpp++];
            util::putline(fout,scpp + "//" );
            line_nbr_out++;
        }

		// Print bottom from the bottom of vasm
        int n = (int)vasm.size();
        for( int i=0; i<bottom; i++ )
        {
            std::string s = vasm[n-bottom+i];
            std::string scpp = vcpp[icpp++];
            util::putline(fout,scpp + "//" + s);
            line_nbr_out++;
        }
    }

    // Print the last section
    size_t n = cpp_identifiers.size();
    for( int i=0; n>0 && i<cpp_identifiers[n-1].nbr_lines; i++ )
    {
        std::string line1;
        if( !std::getline(fin1, line1) )
            break;
        tidy_cpp_line(line1);
        std::string line2;
        if( !std::getline(fin2, line2) )
            break;
        util::putline(fout,line1 + "//" + line2);
        line_nbr_out++;
    }
    return 0;
}

struct Waypoint
{
	const char *name;
	bool skip_cpp_first;
	bool is_func;
};

Waypoint waypoints[] =
{
	{ "PAWN",     false, false },
	{ "KNIGHT",   false, false },
	{ "BISHOP",   false, false },
	{ "ROOK",     false, false },
	{ "QUEEN",    false, false },
	{ "KING",     false, false },
	{ "WHITE",    false, false },
	{ "BLACK",    false, false },
	{ "BPAWN",    false, false },
	{ "TBASE",    false, false },
	{ "DIRECT",   false, false },
	{ "DPOINT",   false, false },
	{ "DCOUNT",   false, false },
	{ "PVALUE",   false, false },
	{ "PIECES",   false, false },
	{ "BOARD",    false, false },
	{ "WACT",     false, false },
	{ "PLIST",    false, false },
	{ "POSK",     false, false },
	{ "SCORE",    false, false },
	{ "PLYIX",    false, false },
	{ "M1",       false, false },
	{ "M2",       false, false },
	{ "M3",       false, false },
	{ "M4",       false, false },
	{ "T1",       false, false },
	{ "T2",       false, false },
	{ "T3",       false, false },
	{ "INDX1",    false, false },
	{ "INDX2",    false, false },
	{ "NPINS",    false, false },
	{ "MLPTRI",   false, false },
	{ "MLPTRJ",   false, false },
	{ "SCRIX",    false, false },
	{ "BESTM",    false, false },
	{ "MLLST",    false, false },
	{ "MLNXT",    false, false },
	{ "KOLOR",    false, false },
	{ "COLOR",    false, false },
	{ "P1",       false, false },
	{ "P2",       false, false },
	{ "P3",       false, false },
	{ "PMATE",    false, false },
	{ "MOVENO",   false, false },
	{ "PLYMAX",   false, false },
	{ "NPLY",     false, false },
	{ "CKFLG",    false, false },
	{ "MATEF",    false, false },
	{ "VALM",     false, false },
	{ "BRDC",     false, false },
	{ "PTSL",     false, false },
	{ "PTSW1",    false, false },
	{ "PTSW2",    false, false },
	{ "MTRL",     false, false },
	{ "BC0",      false, false },
	{ "MV0",      false, false },
	{ "PTSCK",    false, false },
	{ "BMOVES",   false, false },
	{ "ML",       false, false },
	{ "MLEND",    false, false },
	{ "INITBD",   true,  true  },
	{ "PATH",     false, true  },
	{ "MPIECE",   false, true  },
	{ "ENPSNT",   false, true  },
	{ "ADJPTR",   false, true  },
	{ "CASTLE",   false, true  },
	{ "ADMOVE",   false, true  },
	{ "GENMOV",   false, true  },
	{ "INCHK",    false, true  },
	{ "INCHK1",   false, true  },
	{ "ATTACK",   false, true  },
	{ "ATKSAV",   false, true  },
	{ "PNCK",     false, true  },
	{ "PINFND",   false, true  },
	{ "XCHNG",    false, true  },
	{ "NEXTAD",   false, true  },
	{ "POINTS",   false, true  },
	{ "LIMIT",    false, true  },
	{ "MOVE",     false, true  },
	{ "UNMOVE",   false, true  },
	{ "SORTM",    false, true  },
	{ "EVAL",     false, true  },
	{ "FNDMOV",   false, true  },
	{ "ASCEND",   false, true  },
	{ "BOOK",     false, true  },
	{ "CPTRMV",   false, true  },
	{ "BITASN",   false, true  },
	{ "ASNTBI",   false, true  },
	{ "VALMOV",   false, true  },
	{ "ROYALT",   false, true  },
	{ "DIVIDE",   false, true  },
	{ "MLTPLY",   false, true  },
	{ "EXECMV",   false, true  },
	{ NULL,       false, false },
};

void cpp_process( std::ifstream &fin, std::vector<Identifier> &cpp_identifiers )
{
    int idx=0;
    int line_nbr=0;
    const Waypoint *w = waypoints;
    bool  skip = w->skip_cpp_first;
    for(;;)
    {
        std::string line;
        if( !std::getline(fin, line) )
            break;
        line_nbr++;
        util::rtrim(line);
        size_t n = cpp_identifiers.size();
        bool col0_comment = util::prefix(line,"//");
        if( n >= 1 && col0_comment && cpp_identifiers[n-1].nbr_lines == 0 )
        {
            cpp_identifiers[n-1].nbr_lines = line_nbr - cpp_identifiers[n-1].line_nbr;
        }
        size_t offset = line.find_first_of(" \t");
        if( offset == std::string::npos )
        {
            continue;
        }
        line = line.substr(offset);
        util::ltrim(line);
        offset = line.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789");
        if( offset != std::string::npos )
            line = line.substr(0,offset);
        line =  util::toupper(line);
        if( w->name && line==w->name && !col0_comment )
        {
            if( skip )
                skip = false;
            else
            {
                Identifier identifier;
                identifier.name = w->name;
                identifier.line_nbr = w->is_func ? line_nbr+1 : line_nbr;
                cpp_identifiers.push_back(identifier);
                size_t n = cpp_identifiers.size();
                if( n >= 2 && cpp_identifiers[n-2].nbr_lines == 0 )
                {
                    cpp_identifiers[n-2].nbr_lines = line_nbr - cpp_identifiers[n-2].line_nbr;
                }
                w++;
                skip = w->skip_cpp_first;
            }
        }
    }
    for( const Identifier &identifier: cpp_identifiers )
    {
        printf( "%-4d: %s %d lines\n", identifier.line_nbr, identifier.name.c_str(), identifier.nbr_lines ); 
    }
}

void asm_process( std::ifstream &fin, std::vector<Identifier> &asm_identifiers )
{
    int idx=0;
    int line_nbr=0;
    const Waypoint *w = waypoints;
    int  nbr_proceeding_comment_lines = 0;
    for(;;)
    {
        std::string line;
        if( !std::getline(fin, line) )
            break;
        line_nbr++;
        bool match = w->name && util::prefix(line,w->name);
        if( match )
        {
            Identifier identifier;
            identifier.name = w->name;
            identifier.line_nbr = line_nbr;
            identifier.nbr_proceeding_comment_lines = nbr_proceeding_comment_lines;
            asm_identifiers.push_back(identifier);
            w++;
        }
        size_t offset = line.find_first_not_of(" \t");
        bool is_comment = (offset == std::string::npos || line[offset]==';' );
        if( is_comment )
            nbr_proceeding_comment_lines++;
        else
            nbr_proceeding_comment_lines = 0;
    }
    for( const Identifier &identifier: asm_identifiers )
    {
        printf( "%-4d: %s %d proceeding comment lines\n", identifier.line_nbr, identifier.name.c_str(), identifier.nbr_proceeding_comment_lines ); 
    }
}

void tidy_cpp_line( std::string &line )
{
    const int COL_COMMENT = 40;
    const int COL_Z80_ASM = 68;
    util::rtrim(line);
    size_t offset = line.find("//");
    if( offset==std::string::npos || offset==0 )
        ;
    else if( offset == COL_COMMENT )
        ;
    else if( offset > COL_COMMENT )
    {
        std::string slice = line.substr(COL_COMMENT,offset-COL_COMMENT);
        bool all_spaces = (std::string::npos == slice.find_first_not_of(" \t") );
        if( all_spaces )
        {
            std::string start = line.substr(0,COL_COMMENT);
            std::string end   = line.substr(offset);
            line = start + end;
        }
    }
    else if( offset < COL_COMMENT )
    {
        std::string slice = line.substr(offset,COL_COMMENT);
        if( slice.length() >= 2 )
            slice = slice.substr(2);        // slice starts with "//" unless it's very short
        else if( slice.length() >= 1 )
            slice = slice.substr(1);
        bool all_spaces = (std::string::npos == slice.find_first_not_of(" \t") );
        if( all_spaces )
        {
            std::string start = line.substr(0,offset);
            std::string end   = line.substr(offset);
            line = start + slice + end;
        }
    }
    while( line.length() < COL_Z80_ASM )
        line += " ";
    return;
}
