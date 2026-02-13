/****************************************************************************
 * This project is a Windows port of the classic program Sargon, as
 * presented in the book "Sargon a Z80 Computer Chess Program" by Dan
 * and Kathe Spracklen (Hayden Books 1978).
 *
 * File: main.cpp
 *       main() entry point and dispatch to either tests or UCI engine
 *
 * Bill Forster, https://github.com/billforsternz/retro-sargon
 ****************************************************************************/

#include <stdio.h>
#include "main.h"

// Instantiate globals
bool zargon_tests;

// main()
int main( int argc, const char *argv[] )
{

    // For debugging allow easy selection of command line args
    #ifdef _DEBUG
    const char *test_args[] =
    {
        "Debug/zargon.exe", "u"
        //"cg",
        //"-3"
    };
    argc = sizeof(test_args) / sizeof(test_args[0]);
    argv = test_args;
    #endif

    // If no arguments, run UCI engine
    int ret = 0;
    if( argc == 1 )
    {
        zargon_tests = false;
        ret = main_uci( argc, argv );
    }

    // Else run tests
    else
    {
        zargon_tests = true;
        ret = main_tests( argc, argv );
    }
    return ret;
}
