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
void (*callback_zargon)( CB cb );    // fn ptr
bool zargon_tests;

// main()
int main( int argc, const char *argv[] )
{

    // For debugging allow easy selection of command line args
    #ifdef _DEBUG
    const char *test_args[] =
    {
        "Debug/sargon-tests.exe",
        "p",
        "-2"
    };
    argc = sizeof(test_args) / sizeof(test_args[0]);
    argv = test_args;
    #endif

    // If no arguments, run UCI engine
    int ret = 0;
    if( argc == 1 )
    {
        zargon_tests = false;
        callback_zargon = callback_zargon_uci;
        ret = main_uci( argc, argv );
    }

    // Else run tests
    else
    {
        zargon_tests = true;
        callback_zargon = callback_zargon_tests;
        ret = main_tests( argc, argv );
    }
    return ret;
}
