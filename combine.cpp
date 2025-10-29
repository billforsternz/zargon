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

int main( int argc, const char *argv[] )
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
    fin2.clear();
    fin2.seekg(0, std::ios::beg); 
    int line_nbr = 0;
    size_t i = 0;
    while( i<cpp_identifiers.size() && i<asm_identifiers.size() )
    {
        Identifier &p = cpp_identifiers[i];
        Identifier &q = asm_identifiers[i];
        
        // top    print this many from the top
        // bottom print this many from the bottom

		// First estimate
        int top    =  i>0 ? cpp_identifiers[i-1].nbr_lines : 0;
        int bottom =  asm_identifiers[i].nbr_proceeding_comment_lines;
        i++;

		// Number of cpp lines in next section is number of lines we need
        int need_total  =  p.line_nbr - line_nbr;
        if( need_total < 0 )
            need_total = 0;
		
		// Number of asm lines in next section is number of lines we have
        int have_total  =  q.line_nbr - line_nbr;
        if( have_total < 0 )
            have_total = 0;

		// while top + bottom > have_total reduce bottom to 0 then reduce top
        while( top+bottom > have_total )
        {
            if( bottom>0 )
                bottom--;
            else if( top>0 )
                top--;
        }
			
		// while top + bottom > need_total reduce bottom to 0 then reduce top
        while( top+bottom > need_total )
        {
            if( bottom>0 )
                bottom--;
            else if( top>0 )
                top--;
        }
			
        // Number of empty lines, may be zero
		int empty = need_total - (top+bottom);

		// May or may not print all asm lines
        std::vector<std::string> v;
        while( have_total > 0 )
        {
            have_total--;
            std::string line;
            if( !std::getline(fin2, line) )
                break;
            line_nbr++;
            v.push_back(line);
        }

		// print top from the top of v
        for( int i=0; i<top; i++ )
        {
            std::string s = v[i];
            util::putline(fout,s);
        }

		// print empty lines
        for( int i=0; i<empty; i++ )
        {
            util::putline(fout,"");
        }

		// print bottom from the bottom of v
        int n = (int)v.size();
        for( int i=0; i<bottom; i++ )
        {
            std::string s = v[n-bottom+i];
            util::putline(fout,s);
        }
    }

    // Print the last section
    size_t n = cpp_identifiers.size();
    for( int i=0; i<cpp_identifiers[n-1].nbr_lines; i++ )
    {
        std::string line;
        if( !std::getline(fin2, line) )
            break;
        util::putline(fout,line);
    }
    return 0;
}

struct Waypoint
{
	const char *name;
	bool skip_cpp_first;
	bool skip_asm;
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
	{ "LINECT",   false, true  },
	{ "ML",       false, false },
	{ "MLEND",    false, false },
	{ "INITBD",   true,  false },
	{ "PATH",     false, false },
	{ "MPIECE",   false, false },
	{ "ENPSNT",   false, false },
	{ "ADJPTR",   false, false },
	{ "CASTLE",   false, false },
	{ "ADMOVE",   false, false },
	{ "GENMOV",   false, false },
	{ "INCHK",    false, false },
	{ "INCHK1",   false, false },
	{ "ATTACK",   false, false },
	{ "ATKSAV",   false, false },
	{ "PNCK",     false, false },
	{ "PINFND",   false, false },
	{ "XCHNG",    false, false },
	{ "NEXTAD",   false, false },
	{ "POINTS",   false, false },
	{ "LIMIT",    false, false },
	{ "MOVE",     false, false },
	{ "UNMOVE",   false, false },
	{ "SORTM",    false, false },
	{ "EVAL",     false, false },
	{ "FNDMOV",   false, false },
	{ "ASCEND",   false, false },
	{ "BOOK",     false, false },
	{ "CPTRMV",   false, false },
	{ "BITASN",   false, false },
	{ "ASNTBI",   false, false },
	{ "VALMOV",   false, false },
	{ "ROYALT",   false, false },
	{ "DIVIDE",   false, false },
	{ "MLTPLY",   false, false },
	{ "EXECMV",   false, false },
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
                identifier.line_nbr = line_nbr;
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
            if( w->skip_asm )
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


#if 0
        if( line.length() <= 2 )
        {
            util::putline(fout,line);
            continue;
        }
        size_t offset = line.find(";");
        bool have_semi = (offset!=std::string::npos);
        size_t offset2 = line.find("//");
        bool have_comment = (offset2!=std::string::npos);
        bool modify = have_semi && !have_comment;
        if( !modify )
        {
            // Actually I lied, modify it if it ends in ")"
            util::rtrim(line);
            size_t len = line.length();
            bool trim_func_end_whitespace = util::suffix(line,")");
            if( trim_func_end_whitespace )
            {
                line = line.substr(0,len-1);  // remove end ")"
                util::rtrim(line);            // remove space
                line += ");";                 // put back ")" + ";"
            }
            util::putline(fout,line);
            continue;
        }
        std::string start_of_line = line.substr(0,offset);
        std::string end_of_line   = line.substr(offset+1);
        util::rtrim(start_of_line);
        size_t len = start_of_line.length();
        if( len == 0 )
        {
            line.clear();
            line += "//";
            line += end_of_line;
        }
        else
        {
            bool trim_func_end_whitespace = util::suffix(start_of_line,")");
            if( trim_func_end_whitespace )
            {
                start_of_line = start_of_line.substr(0,len-1);  // remove end ")"
                util::rtrim(start_of_line);                     // remove space
                start_of_line += ")";                           // put back ")"
            }
            start_of_line += "; ";
            len = start_of_line.length();
            while( len < COL_COMMENT )
            {
                start_of_line += " ";
                len++;
            }
            line = start_of_line;
            line += "// ";
            line += end_of_line;
        }
        util::rtrim(line);
        util::putline(fout,line);
    }

}
#endif
