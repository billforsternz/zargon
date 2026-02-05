/****************************************************************************
 * This project is a Windows port of the classic program Sargon, as
 * presented in the book "Sargon a Z80 Computer Chess Program" by Dan
 * and Kathe Spracklen (Hayden Books 1978).
 *
 * File: main.h
 *       main() entry point and dispatch to either tests or UCI engine
 *
 * Bill Forster, https://github.com/billforsternz/retro-sargon
 ****************************************************************************/

#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED
#include "bridge.h"

extern int main( int argc, const char *argv[] );
extern int main_uci( int argc, const char *argv[] );
extern int main_tests( int argc, const char *argv[] );

// Sargon core calls back to calling code use gb_z80_cpu to inspect
//  and/or change registers
extern void (*callback_zargon)( CB cb );    // fn ptr
extern void callback_zargon_uci( CB cb );   // redirect callback here ...
extern void callback_zargon_tests( CB cb ); // .. or here

// True if main_tests() running, false if main_uci() running
extern bool zargon_tests;

// Individual callback functions
void callback_ldar( uint8_t &out_random_number );

#endif MAIN_H_INCLUDED
