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
#include <stdint.h>
#include "trace.h"

extern int main( int argc, const char *argv[] );
extern int main_uci( int argc, const char *argv[] );
extern int main_tests( int argc, const char *argv[] );

// True if main_tests() running, false if main_uci() running
extern bool zargon_tests;

// True if minimax and documentation tests
// False if UCI or performance tests
//  so false if maximum performance required,
extern bool zargon_executable_documentation;

// Individual callback functions
void callback_ldar( uint8_t &out_random_number );
void callback_yes_best_move();
void callback_after_points( uint8_t &points );
void callback_after_genmov();
void callback_test_alpha_beta_cutoff( uint8_t score_smaller_is_better, const uint8_t *p );
void callback_is_this_best_move( uint8_t score_bigger_is_better, const uint8_t *p );

// UCI needs only a small subset of the callbacks
void callback_uci_after_genmove();
void callback_uci_after_points();
void callback_uci_yes_best_move();

#endif MAIN_H_INCLUDED
