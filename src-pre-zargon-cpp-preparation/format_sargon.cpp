#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include "util.h"

int main( int argc, const char *argv[] )
{
    const int COL_COMMENT = 40;
    const char *arg1 = "/users/bill/documents/github/zargon/sargon-z80-modified.asm";
    const char *arg2 = "/users/bill/documents/github/zargon/sargon-z80-modified.cpp";
    #if 0
    if( argc != 3 )
    {
        printf( "Simple helper while converting Sargon to C\n" );
        printf( "Usage:\n" );
        printf( " format in out\n" );
        printf( "Looks for lines with ; but no //\n" );
        printf( "Replace ; by //\n" );
        printf( "If there's prior text on such a line, terminate it with ;\n" );
        printf( "If the termination is now \"   );\" remove the spaces\n" ); 
        printf( "Outdent the inserted // to column COL_COMMENT\n" );
        return -1;
    }
    arg1 = argv[1];
    arg2 = argv[2];
    #endif
    std::ifstream fin(arg1);
    if( !fin )
    {
        printf( "Cannot open %s for reading\n", argv[1] );
        return -1;
    }
    std::ofstream fout(arg2);
    if( !fout )
    {
        printf( "Cannot open %s for writing\n", argv[2] );
        return -1;
    }
    for(;;)
    {
        std::string line;
        if( !std::getline(fin, line) )
            break;
        util::rtrim(line);
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
    return 0;
}

