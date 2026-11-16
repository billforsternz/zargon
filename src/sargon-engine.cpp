/****************************************************************************
 * This project is a Windows port of the classic program Sargon, as
 * presented in the book "Sargon a Z80 Computer Chess Program" by Dan
 * and Kathe Spracklen (Hayden Books 1978).
 *
 * File: sargon-engine.cpp
 *       A simple Windows UCI chess engine interface
 *
 * Bill Forster, https://github.com/billforsternz/retro-sargon
 ****************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <setjmp.h>
#include <io.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <queue>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>

#include "util.h"
#include "thc.h"
#include "sargon-interface.h"
#include "sargon-pv.h"
#include "zargon.h"
#include "zargon_functions.h"

// Sargon data structure
static Zargon &z = gbl_zargon;

// Measure elapsed time, nodes    
static unsigned long base_time;

// Misc
static int depth_option;    // 0=auto, other values for fixed depth play
static std::string logfile_name;
static unsigned long total_callbacks;
static unsigned long after_points_callbacks;
static std::vector<thc::Move> the_game;
static int the_level=6;
static bool gbl_interactive=true;
static bool gbl_orientation;
static bool gbl_big_graphics;

// The current 'Master' postion
static thc::ChessRules the_position;
static thc::ChessRules the_position_base;

// The current 'Master' PV
static PV the_pv;

// Command line interface
static std::string interactive_screen( bool help_screen, int nbr_lines );
static std::string ascii_board( thc::ChessPosition const &cp, bool big );
static bool process( const std::string &s, bool first=false );
static std::string cmd_uci();
static std::string cmd_isready();
static std::string cmd_stop();
static std::string cmd_go( const std::vector<std::string> &fields );
static void        cmd_go_infinite();
static void        cmd_setoption( const std::vector<std::string> &fields );
static void        cmd_position( const std::string &whole_cmd_line, const std::vector<std::string> &fields );
static bool        cmd_interactive_go( int nbr_lines );
static bool        cmd_interactive_move( const std::string &raw_cmd );
static bool        cmd_interactive_position( const std::string &raw_cmd_line );
static bool        cmd_interactive_level( const std::string &parm );
static bool        cmd_interactive_undo();
static void        cmd_interactive_save( const std::string &filename, std::string &output_msg );

// Misc
static bool is_new_game();
static int log( const char *fmt, ... );
static bool run_sargon( int plymax, bool avoid_book );
static std::string generate_progress_report();
static thc::Move calculate_next_move( bool interactive_mode, bool new_game, unsigned long ms_time, unsigned long ms_inc, int depth );

// A threadsafe-queue. (from https://stackoverflow.com/questions/15278343/c11-thread-safe-queue )
template <class T>
class SafeQueue
{
public:
    SafeQueue() : q(), mx(), c() {}
    ~SafeQueue() {}

    // Is queue empty ?
    bool empty()  { return q.empty(); }

    // Add an element to the queue.
    void enqueue(T t)
    {
        std::lock_guard<std::mutex> lock(mx);
        q.push(t);
        c.notify_one();
    }

    // Get the "front" element.
    //  if the queue is empty, wait until an element is available
    T dequeue(void)
    {
        std::unique_lock<std::mutex> lock(mx);
        while(q.empty())
        {
            // release lock as long as the wait and reaquire it afterwards
            c.wait(lock);
        }
        T val = q.front();
        q.pop();
        return val;
    }

private:
    std::queue<T> q;
    mutable std::mutex mx;
    std::condition_variable c;
};

// Threading declarations
static SafeQueue<std::string> async_queue;
static void timer_thread();
static void read_stdin();
static void write_stdout();
static void timer_clear();          // Clear the timer
static void timer_end();            // End the timer subsystem system
static void timer_set( int ms );    // Set a timeout event, ms millisecs into the future (0 and -1 are special values)

static const std::vector<std::string> test_sequence =
{
#if 0
        "",
        "8/8/8/4k/8/8/8/KQ w - - 0 1",
        "level 9",
        "go",
        "Kf4",
        "Ke5",
        "Kf5"
#endif

#if 0
        "uci\n",
        "setoption name FixedDepth value 9",
        "position fen r2q1rk1/pppnbp1p/2n1b3/3P2p1/3PpB2/4P1N1/PP1NBPPP/R2Q1RK1 b - - 0 11 moves e6d5 f4g5 e7g5 g3e4 d5e4 d2e4 f8e8 e2d3 g8g7 d1h5 h7h6 f2f4 d7f6 h5h3 f6e4 d3e4 e8e4 f4g5 d8g5 f1f3 e4g4 a1f1 a8f8 f3f6 g4e4 f1f3 e4e3 f6f7 f8f7 f3e3 c6d4 h3g3 d4f5 g3g5 h6g5 e3e2 f7e7 e2e7 f5e7 g1f2 e7d5 f2f3 d5b4 a2a3 b4d3 b2b3 d3c5 b3b4 c5d7 f3e4 g7f6 g2g3 d7e5 h2h4 e5c4 h4g5 f6g5 e4d5 c4a3 d5e6 g5g4 e6d7 a3b5 d7c8 b7b6 c8b7 g4g3 b7c6 a7a6 c6b7 g3f4 b7a6 b5d6 a6a7 f4e5 a7b8 d6b5 b8b7 e5d4 b7c6 d4c4 c6b7 c4b4 b7c6 b4c4 c6b7 c4d5 b7a6 b5d6 a6a7 b6b5 a7a6 b5b4 a6a5 b4b3 a5b4 b3b2 b4c3 b2b1q c3d2\n",
        "go wtime 1211487 btime 63692 winc 5000 binc 1000\n",
        "isready\n",
        "quit\n"
#endif

#if 0
        "uci\n",
        "setoption name FixedDepth value 6",
        "position fen 8/3R2pk/4Np1p/5r2/8/1P2PK2/5PPP/8 w - - 9 41 moves f3e2 h7h8 d7d8 h8h7\n",
        "go\n"
        "isready\n",
        "quit\n"
#endif
#if 0
        "uci\n",
        "setoption name FixedDepth value 6",
        "position fen 8/3R2pk/4Np1p/5r2/8/1P2PK2/5PPP/8 w - - 9 41\n",
        "go\n",
        "isready\n",
        "position fen 7k/3R2p1/4Np1p/5r2/8/1P2P3/4KPPP/8 w - - 11 42\n",
        "go\n",
        "isready\n",
        "position fen 3R4/6pk/4Np1p/5r2/8/1P2P3/4KPPP/8 w - - 13 43\n",
        "go\n",
        "isready\n",
        "quit\n"
#endif
#if 0
        "uci\n",
        "position fen 8/3R2pk/4Np1p/5r2/8/1P2PK2/5PPP/8 w - - 9 41 moves f3e2 h7h8 d7d8 h8h7\n",
        "go depth 6 wtime 55068 btime 889329 winc 1000 binc 5000\n",
        "isready\n",
        "quit\n"
#endif
#if 0
        "uci\n",
        "position fen 7K/5k1P/8/8/8/8/8/8 b - - 0 1\n",
        "go depth wtime 1000 btime 1000 winc 1000 binc 5000\n",
        "isready\n",
        "quit\n"
#endif
#if 0
        "uci\n",
        "position startpos moves e2e4 c7c5 b1c3 a7a6 d1h5 e7e6 g1f3 b7b5 f3e5 g7g6 h5f3 g8f6 f1e2 f8g7 e5d3 d7d6 e4e5 f6d5 c3d5 e6d5 f3d5 a8a7 e5d6 e8g8 d5c5 a7d7 a2a4 d7d6 a4b5 a6b5 c5b5 c8d7 b5c5 d7c6 e1g1 f8e8 e2f3 c6f3 g2f3 d6d5 c5c4 d5d4 c4b5 d4d5 b5a4 d5d4 a4a5 d4d5 a5d8 e8d8 f1e1 b8c6 a1a3 d5g5 g1h1 g5h5 h1g2 d8d5 e1e8 g7f8 a3a8 d5g5 g2h1 c6e5 e8f8 g8g7 f3f4 h5h2 h1h2 e5f3 h2h3 f3g1 h3h4 g1f3 h4h3 f3g1 h3h2 g1f3\n",
        "go depth 5 wtime 88428 btime 883250 winc 1000 binc 5000\n",
        "isready\n",
        "quit\n"
#endif
#if 0
        "uci\n",
        "position startpos moves e2e4 c7c5 b1c3 a7a6 d1h5 e7e6 g1f3 b7b5 f3e5 g7g6 h5f3 g8f6 f1e2 f8g7 e5d3 d7d6 e4e5 f6d5 c3d5 e6d5 f3d5 a8a7 e5d6 e8g8 d5c5 a7d7 a2a4 d7d6 a4b5 a6b5 c5b5 c8d7 b5c5 d7c6 e1g1 f8e8 e2f3 c6f3 g2f3 d6d5 c5c4 d5d4 c4b5 d4d5 b5a4 d5d4 a4a5 d4d5 a5d8 e8d8 f1e1 b8c6 a1a3 d5g5 g1h1 g5h5 h1g2 d8d5 e1e8 g7f8 a3a8 d5g5 g2h1 c6e5 e8f8 g8g7 f3f4 h5h2 h1h2 e5f3 h2h3 f3g1 h3h4 g1f3\n",
        "go depth 5 wtime 55068 btime 889329 winc 1000 binc 5000\n",
        "isready\n",
        "quit\n"
#endif
#if 0
        "uci\n",
        "position fen 1br4k/6pp/1q6/1r4N1/8/7P/Q5P1/7K w - - 0 1\n",
        "go wtime 3599100 btime 900000 winc 2000 binc 5000\n",
        "isready\n",
        "position fen 1br4k/6pp/1q6/1r4N1/8/7P/Q5P1/7K w - - 0 1 moves g5f7 h8g8\n",
        "go wtime 3569100 btime 899203 winc 2000 binc 5000\n",
        "isready\n",
        "position fen 1br4k/6pp/1q6/1r4N1/8/7P/Q5P1/7K w - - 0 1 moves g5f7 h8g8 f7h6 g8h8\n",
        "go wtime 3568100 btime 901125 winc 2000 binc 5000\n",
        "isready\n",
        "position fen 1br4k/6pp/1q6/1r4N1/8/7P/Q5P1/7K w - - 0 1 moves g5f7 h8g8 f7h6 g8h8 a2g8 c8g8\n",
        "go wtime 3569490 btime 903235 winc 2000 binc 5000\n",
        "isready\n",
        "quit\n"
#endif
#if 0
        "uci\n",
        "isready\n",
        "position fen rnbqkb1r/pppppppp/5n2/8/2P5/5N2/PP1PPPPP/RNBQKB1R b KQkq c3 0 2 moves c7c6 d2d4 d7d5 b1c3 d5c4 a2a4 b8a6 e2e4 c8g4 f1c4 e7e6 c1e3 f8b4 e4e5 f6d5 d1c2 g4f5 c2d2 e8g8 e1g1 d5e3 f2e3 b4c3 b2c3 a6c7 a1e1 b7b5 a4b5 c6b5 c4d3 f5d3 d2d3 f7f5 e3e4 f5e4 d3e4 c7d5 e4d3 a8c8 d3b5 d5c3 b5a6 d8d7 f3g5 d7d4 g1h1 f8f1 e1f1 d4c4 a6e6 c4e6 g5e6 c8e8 e6c5 e8e5 c5d3 e5d5 d3b4 d5d6 h2h3 a7a5 b4c2 d6d2 c2e3 d2d4 f1c1 d4d3 e3c4 a5a4 c4b6 a4a3 b6c4 a3a2 c1a1 d3d1 a1d1 c3d1 h1h2 a2a1q c4d2 a1e5 h2g1 e5d4 g1h2 d4d2 h2g3 d2f2 g3g4 d1e3 g4g5 f2f5 g5h4 g7g5 h4h5 g5g4 h5h4 g8g7 h4g3 g4h3 g2h3\n",
        "go depth 6\n",
        "isready\n",
        "position fen rnbqkb1r/pp1ppppp/2p2n2/8/2PP4/5N2/PP2PPPP/RNBQKB1R b KQkq d3 0 3 moves d7d5 b1c3 d5c4 a2a4 b8a6 e2e4 c8g4 f1c4 e7e6 c1e3 f8b4 e4e5 f6d5 d1c2 g4f5 c2d2 e8g8 e1g1 d5e3 f2e3 b4c3 b2c3 a6c7 a1e1 b7b5 a4b5 c6b5 c4d3 f5d3 d2d3 f7f5 e3e4 f5e4 d3e4 c7d5 e4d3 a8c8 d3b5 d5c3 b5a6 d8d7 f3g5 d7d4 g1h1 f8f1 e1f1 d4c4 a6e6 c4e6 g5e6 c8e8 e6c5 e8e5 c5d3 e5d5 d3b4 d5d6 h2h3 a7a5 b4c2 d6d2 c2e3 d2d4 f1c1 d4d3 e3c4 a5a4 c4b6 a4a3 b6c4 a3a2 c1a1 d3d1 a1d1 c3d1 h1h2 a2a1q c4d2 a1e5 h2g1 e5d4 g1h2 d4d2 h2g3 d2f2 g3g4 d1e3 g4g5 f2f5 g5h4 g7g5 h4h5 g5g4 h5h4 g8g7 h4g3 g4h3 g2h3 h7h5 g3h2\n",
        "go depth 7\n",
        "isready\n",
        "position fen rnbqkb1r/pp2pppp/2p2n2/3p4/2PP4/2N2N2/PP2PPPP/R1BQKB1R b KQkq - 1 4 moves d5c4 a2a4 b8a6 e2e4 c8g4 f1c4 e7e6 c1e3 f8b4 e4e5 f6d5 d1c2 g4f5 c2d2 e8g8 e1g1 d5e3 f2e3 b4c3 b2c3 a6c7 a1e1 b7b5 a4b5 c6b5 c4d3 f5d3 d2d3 f7f5 e3e4 f5e4 d3e4 c7d5 e4d3 a8c8 d3b5 d5c3 b5a6 d8d7 f3g5 d7d4 g1h1 f8f1 e1f1 d4c4 a6e6 c4e6 g5e6 c8e8 e6c5 e8e5 c5d3 e5d5 d3b4 d5d6 h2h3 a7a5 b4c2 d6d2 c2e3 d2d4 f1c1 d4d3 e3c4 a5a4 c4b6 a4a3 b6c4 a3a2 c1a1 d3d1 a1d1 c3d1 h1h2 a2a1q c4d2 a1e5 h2g1 e5d4 g1h2 d4d2 h2g3 d2f2 g3g4 d1e3 g4g5 f2f5 g5h4 g7g5 h4h5 g5g4 h5h4 g8g7 h4g3 g4h3 g2h3 h7h5 g3h2 f5e5 h2g1\n",
        "go depth 7\n",
        "isready\n",
        "position fen rnbqkb1r/pp2pppp/2p2n2/8/P1pP4/2N2N2/1P2PPPP/R1BQKB1R b KQkq a3 0 5 moves b8a6 e2e4 c8g4 f1c4 e7e6 c1e3 f8b4 e4e5 f6d5 d1c2 g4f5 c2d2 e8g8 e1g1 d5e3 f2e3 b4c3 b2c3 a6c7 a1e1 b7b5 a4b5 c6b5 c4d3 f5d3 d2d3 f7f5 e3e4 f5e4 d3e4 c7d5 e4d3 a8c8 d3b5 d5c3 b5a6 d8d7 f3g5 d7d4 g1h1 f8f1 e1f1 d4c4 a6e6 c4e6 g5e6 c8e8 e6c5 e8e5 c5d3 e5d5 d3b4 d5d6 h2h3 a7a5 b4c2 d6d2 c2e3 d2d4 f1c1 d4d3 e3c4 a5a4 c4b6 a4a3 b6c4 a3a2 c1a1 d3d1 a1d1 c3d1 h1h2 a2a1q c4d2 a1e5 h2g1 e5d4 g1h2 d4d2 h2g3 d2f2 g3g4 d1e3 g4g5 f2f5 g5h4 g7g5 h4h5 g5g4 h5h4 g8g7 h4g3 g4h3 g2h3 h7h5 g3h2 f5e5 h2g1 e5f5 g1h2\n",
        "go depth 7\n",
        "isready\n",
        "position fen r1bqkb1r/pp2pppp/n1p2n2/8/P1pPP3/2N2N2/1P3PPP/R1BQKB1R b KQkq e3 0 6 moves c8g4 f1c4 e7e6 c1e3 f8b4 e4e5 f6d5 d1c2 g4f5 c2d2 e8g8 e1g1 d5e3 f2e3 b4c3 b2c3 a6c7 a1e1 b7b5 a4b5 c6b5 c4d3 f5d3 d2d3 f7f5 e3e4 f5e4 d3e4 c7d5 e4d3 a8c8 d3b5 d5c3 b5a6 d8d7 f3g5 d7d4 g1h1 f8f1 e1f1 d4c4 a6e6 c4e6 g5e6 c8e8 e6c5 e8e5 c5d3 e5d5 d3b4 d5d6 h2h3 a7a5 b4c2 d6d2 c2e3 d2d4 f1c1 d4d3 e3c4 a5a4 c4b6 a4a3 b6c4 a3a2 c1a1 d3d1 a1d1 c3d1 h1h2 a2a1q c4d2 a1e5 h2g1 e5d4 g1h2 d4d2 h2g3 d2f2 g3g4 d1e3 g4g5 f2f5 g5h4 g7g5 h4h5 g5g4 h5h4 g8g7 h4g3 g4h3 g2h3 h7h5 g3h2 f5e5 h2g1 e5f5 g1h2 f5e5 h2g1\n",
        "go depth 7\n",
        "isready\n",
        "quit\n"
#endif
#if 0
        "uci\n",
        "isready\n",
        "setoption name FixedDepth value 1\n",           // go straight to depth 1 without iterating
        "setoption name LogFileName value c:\\windows\\temp\\sargon-log-file.txt\n",
        "position fen 7k/2pp2pp/4q3/4b3/4R3/4Q3/6PP/7K w - - 0 1\n",   // can take the bishop 425 centipawns
        "go\n",
        "position fen 7k/2pp2pp/4q3/4b3/4R3/3Q4/6PP/7K w - - 0 1\n",   // can't take the bishop 175 centipawns 
        "go\n"
#endif
#if 0
        "uci\n",
        "isready\n",
        "setoption name FixedDepth value 5\n",           // go straight to depth 5 without iterating
        "setoption name LogFileName value c:\\windows\\temp\\sargon-log-file.txt\n",
        "position fen 7k/8/8/8/8/8/8/N6K b - - 0 1\n",   // an extra knight
        "go depth 6\n",                                  // will override fixed depth option, iterates
        "position fen 7k/8/8/8/8/8/8/N5Kq w - - 0 1\n",  // an extra knight only after capturing queen
        "go\n"                                           // will go straight to depth 5, no iteration
#endif
};

// main()
int main_uci( int argc, const char *argv[] )
{
    // logfile_name = std::string(argv[0]) + "-log.txt"; // wake this up for early logging
    if( test_sequence.size() > 0 )
    {
        bool first = true;
        for( std::string s: test_sequence )
        {
            util::rtrim(s);
            log( "cmd>%s\n", s.c_str() );
            process(s,first);
            first = false;
        }
        return 0;
    }

    std::thread first(read_stdin);
    std::thread second(write_stdout);
    std::thread third(timer_thread);

    // Wait for main threads to finish
    first.join();                // pauses until first finishes
    second.join();               // pauses until second finishes

    // Tell timer thread to finish, then kill it immediately
    timer_end();
    third.detach();
    return 0;
}

// Emulate the simple and very useful Windows GetTickCount() with more elaborate and flexible C++
//  std library calls, at least we can give our function a better name that the misleading
//  GetTickCount() which fails to mention milliseconds in its name
static std::chrono::time_point<std::chrono::steady_clock> base = std::chrono::steady_clock::now();
static unsigned long elapsed_milliseconds()
{
    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - base);
    unsigned long ret = static_cast<unsigned long>(ms.count());
    return ret;
}

// Very simple timer thread, controlled by timer_set(), timer_clear(), timer_end() 
static std::mutex timer_mtx;
static long future_time;
static void timer_thread()
{
    log( "timer thread start\n" );
    for(;;)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        {
            std::lock_guard<std::mutex> lck(timer_mtx);
            if( future_time == -1 )
                break;    // end the timer thread
            else if( future_time != 0 )
            {
                long now_time = elapsed_milliseconds();	
                long time_remaining = future_time - now_time;
                log( "time_remaining=%ld, future_time=%ld, now_time=%ld\n", time_remaining, future_time, now_time );
                if( time_remaining <= 0 )
                {
                    future_time = 0;
                    log( "TIMEOUT\n" );
                    async_queue.enqueue( "TIMEOUT" );
                }
            }
        }
    }
    log( "timer thread end\n" );
}

// Clear the timer
static void timer_clear()
{
    log( "timer_clear()\n" );
    timer_set(0);  // set sentinel value 0
}

// End the timer subsystem system
static void timer_end()
{
    timer_set(-1);  // set sentinel value -1
}

// Set a timeout event, ms millisecs into the future (0 and -1 are special values)
static void timer_set( int ms )
{
    std::lock_guard<std::mutex> lck(timer_mtx);
    log( "timer_set(%d)\n", ms );

    // Prevent generation of a stale "TIMEOUT" event
    future_time = 0;

    // Check for timer_end()
    if( ms == -1 )
        future_time = -1;

    // Schedule a new "TIMEOUT" event (unless 0 = timer_clear())
    else if( ms != 0 )
    {
        long now_time = elapsed_milliseconds();	
        long ft = now_time + ms;
        if( ft==0 || ft==-1 )   // avoid special values
            ft = 1;             //  at cost of 1 or 2 millisecs of error!
        future_time = ft;
        log( "future_time=%ld\n", future_time );
    }
}

// Read commands from stdin and queue them
static void read_stdin()
{
    bool quit=false;
    while(!quit)
    {
        static char buf[8192];
        if( NULL == fgets(buf,sizeof(buf)-2,stdin) )
            quit = true;
        else
        {
            std::string s(buf);
            util::rtrim(s);
            async_queue.enqueue(s);
            if( s == "quit" || s=="q" )
                quit = true;
        }
    }
}

// Read queued commands and process them
static void write_stdout()
{
    bool quit=false;
    process("",true);
    while(!quit)
    {
        std::string s = async_queue.dequeue();
        log( "cmd>%s\n", s.c_str() );
        quit = process(s);
    }
} 

// Run Sargon analysis, until completion or timer abort (see callback() for timer abort)
static jmp_buf jmp_buf_env;
static bool run_sargon( int plymax, bool avoid_book )
{
    bool aborted = false;
    int val;
    val = setjmp(jmp_buf_env);
    if( val )
    {
        aborted = true;
        the_pv = sargon_pv_get();
    }
    else
    {
        sargon_run_engine(the_position,plymax,the_pv,avoid_book);
    }
    return aborted;
}

static const char * help[] =
{
    " Sargon 1978 64 bit    ",
    " By Dan and Kathe      ",
    "  Spracklen            ",
    " UCI engine with bonus ",
    "  interactive shell    ",
    "                       ",
    " Commands are;         ",
    " Nf3   ;(eg) make move ",
    " go    ;start game     ",
    " stop  ;stop game      ",
    " big   ;big graphics   ",
    " flip  ;flip board     ",
    " undo  ;undo move      ",
    " reset ;start position ",
    " uci   ;exit to UCI    ",
    " quit  ;exit program   ",
    " level n ;set level    ",
    " save pgnfile ;save    ",
    " r3k2r/8/8/8/8/8/8/4K2R",
    "  w Kkq - 0 1 ;(eg)    ",
    "  (to set FEN position)",
    "                       ",
    " Use -? on command line",
    " for more information  "
};

// Command line top level handler
static bool process( const std::string &s, bool first )
{
    static int nbr_lines, nbr_lines_big;
    static bool auto_play = false;
    bool show_help = false;
    std::string interactive_msg;
    if( first )
    {
        std::string pos = ascii_board(the_position,true);
        size_t offset = pos.find('\n');
        while( offset != std::string::npos )
        {
            nbr_lines_big++;
            offset = pos.find('\n',offset+1);
        }
        pos = ascii_board(the_position,false);
        offset = pos.find('\n');
        while( offset != std::string::npos )
        {
            nbr_lines++;
            offset = pos.find('\n',offset+1);
        }
    }
    bool quit=false;
    std::string rsp;
    std::vector<std::string> fields_raw, fields;
    std::string raw_cmd_line = s;
    util::split( s, fields_raw );
    for( std::string f: fields_raw )
        fields.push_back( util::tolower(f) ); 
    std::string raw_cmd;
    std::string cmd;
    if( fields_raw.size() > 0 )
    {
        raw_cmd = fields_raw[0];
        cmd = fields[0];
    }
    else
    {
        if( gbl_interactive )
            cmd = "show";
        else
            cmd = "";
    }
    std::string parm1 = fields.size()<=1 ? "" : fields[1];
    if( gbl_interactive )
    {
        if( cmd=="uci" || cmd=="quit" || cmd=="q" )
        {
            gbl_interactive = false;
            fprintf( stderr, "Leaving interactive mode\n" );
        }
        else if( cmd=="stop" )
        {
            if( !auto_play )
                interactive_msg = "Game not running, command 'go' starts it";
            else
            {
                interactive_msg = "Game play stopped, command 'go' restarts it";
                auto_play = false;
            }
        }
        else if( cmd=="level" )
        {
            if( fields.size() == 1 )
            {
                interactive_msg = util::sprintf( "Level is currently %d, valid range is 1-%d", the_level, MAX_DEPTH );
            }
            else
            {
                bool ok = cmd_interactive_level( parm1 );
                if( ok )
                    interactive_msg = util::sprintf( "Level is now %d", the_level );
                else
                    interactive_msg = util::sprintf( "Level out of range, valid range is %d", MAX_DEPTH );
            }
        }
        else if( cmd=="undo" )
        {
            bool ok = cmd_interactive_undo();
            auto_play = false;
            if( !ok )
                interactive_msg = "Error: No move to undo";
        }
        else if( cmd=="reset" )
        {
            auto_play = false;
            thc::ChessRules init;
            the_position = init;
            the_position_base = init;
            the_game.clear();
        }
        else if( cmd=="flip" )
        {
            gbl_orientation = !gbl_orientation;
        }
        else if( cmd=="save" )
        {
            if( fields.size() <= 1 )
                interactive_msg = "Error: No filename specified";
            else
                cmd_interactive_save( parm1, interactive_msg );
        }
        else if( cmd=="big" && !gbl_big_graphics )
        {
            gbl_big_graphics = true;
        }
        else if( cmd=="small" && gbl_big_graphics )
        {
            gbl_big_graphics = false;
        }
        else if( cmd=="show" )
        {
            ; // just redraw
        }
        else if( cmd=="help" || cmd=="?" || cmd=="-?" || cmd=="" )
        {
            show_help = true;
        }
        else if( cmd=="go" )
        {
            auto_play = cmd_interactive_go( gbl_big_graphics ? nbr_lines_big : nbr_lines );
        }
        else if( !first )
        {
            bool is_position = cmd_interactive_position( raw_cmd_line );
            if( !is_position )
            {
                bool is_move = cmd_interactive_move( raw_cmd );
                if( is_move )
                {
                    if( auto_play )
                    {
                        auto_play = cmd_interactive_go( gbl_big_graphics ? nbr_lines_big : nbr_lines );
                        log( "auto_play=%s\n", auto_play?"true":"false" );
                    }
                }
                else
                {
                    interactive_msg = "Not a valid command, move or position. type ? for help, show to redraw";
                }
            }
        }
    }
    if( !gbl_interactive )
    {
        if( cmd == "quit" || cmd=="q" ) // special command because read_stdin() tests it
            quit = true;
        else if( cmd == "timeout" )
            log( "TIMEOUT event\n" );
        else if( cmd == "uci" )
            rsp = cmd_uci();
        else if( cmd == "isready" )
            rsp = cmd_isready();
        else if( cmd == "stop" )
            rsp = cmd_stop();
        else if( cmd=="go" && parm1=="infinite" )
            cmd_go_infinite();
        else if( cmd=="go" )
            rsp = cmd_go(fields);
        else if( cmd=="setoption" )
            cmd_setoption(fields);
        else if( cmd=="position" )
            cmd_position( s, fields );
        if( rsp != "" )
        {
            log( "rsp>%s\n", rsp.c_str() );
            fprintf( stdout, "%s", rsp.c_str() );
            fflush( stdout );
        }
        log( "function process() returns, cmd=%s\n"
             "total callbacks=%lu\n"
             "bestmove callbacks=%lu\n"
             "after points callbacks=%lu\n",
                cmd.c_str(),
                total_callbacks,
                after_points_callbacks );
        log( "%s\n", sargon_pv_report_stats().c_str() );
    }
    if( gbl_interactive )
    {
        if( interactive_msg != "" )
            fprintf( stderr, "%s\ncmd>", interactive_msg.c_str() );
        else
        {
            bool help_screen = first || show_help || the_game.size()==0;
            std::string s = interactive_screen( help_screen, gbl_big_graphics ? nbr_lines_big : nbr_lines  );
            fprintf( stderr, "%s\ncmd>", s.c_str() );
        }
        fflush( stderr );
    }
    return quit;
}

static std::string interactive_screen( bool help_screen, int nbr_lines )
{
    std::string ret;
    std::deque<std::string> right_panel;
    if( help_screen )
    {
        int sz = sizeof(help)/sizeof(help[0]);
        for( int i=0; i<sz; i++ )
            right_panel.push_back(help[i]);
    }
    else
    {
        thc::ChessRules cr = the_position_base;
        int sz = (int)the_game.size();
        std::string s;
        for( int i=0; i<sz; i++ )
        {
            thc::Move mv = the_game[i];
            std::string t = mv.NaturalOut(&cr);
            if( i==0 || cr.WhiteToPlay() )
            {
                s = util::sprintf( "%2d%s %s", cr.full_move_count, cr.WhiteToPlay()?".":"...", t.c_str() );
            }
            else
            {
                s += util::sprintf( " %s", t.c_str() );
                right_panel.push_back(s);
                s.clear();
            }
            cr.PlayMove(mv);         
        }
        if( s.length() > 0 )
            right_panel.push_back(s);
    }
    while( right_panel.size() > nbr_lines )
        right_panel.pop_front();
    while( right_panel.size() < nbr_lines )
        right_panel.push_back("");
    size_t offset=0;
    std::string pos = ascii_board(the_position,gbl_big_graphics);
    for( std::string &right: right_panel )
    {
        size_t offset2 = pos.find('\n');
        if( offset != 0 )
            offset2 = pos.find('\n',offset+1);
        if( offset2 != std::string::npos )
        {
            std::string s = pos.substr(offset,offset2);
            if( offset != 0 )
                s = pos.substr(offset+1,offset2-offset-1);
            offset = offset2;
            const char *right_txt = right.c_str();
            if( gbl_big_graphics &&
                right == " big   ;big graphics   " )
            {
                right_txt = " small ;small graphics ";
            }
            ret += util::sprintf( "\n%s  %s",
                s.c_str(), right_txt );
        }
    }
    return ret;
}

static std::string ascii_board( thc::ChessPosition const &cp, bool big )
{
    std::string s;
    char reverse_orientation[64];
    const char *p = cp.squares;
    if( gbl_orientation )
    {
        char *dst = &reverse_orientation[63];
        for( int i=0; i<64; i++ )
            *dst-- =  *p++;
        p = reverse_orientation;
    }

    const char *graphics[] =
    {
        "       :::::::       :.www.:  +++  :::::::       :::::::",
        " [#=#] :.=o\\.:  (/)  :.(@).:  (@)  :.(/).:  =o\\  :[#=#]:",
        "  /@\\  :./@\\.:  /@\\  :./@\\.:  /@\\  :./@\\.:  /@\\  :./@\\.:",
        ":::::::       :::::::       :::::::       :::::::       ",
        "::@@@::  @@@  ::@@@::  @@@  ::@@@::  @@@  ::@@@::  @@@  ",
        ":::::::       :::::::       :::::::       :::::::       ",
        "       :::::::       :::::::       :::::::       :::::::",
        "       :::::::       :::::::       :::::::       :::::::",
        "       :::::::       :::::::       :::::::       :::::::",
        ":.+++.:  www  :::::::       :::::::       :::::::       ",
        ":.(@).:  (@)  :::::::       :::::::       :::::::       ",
        ":./@\\.:  /@\\  :::::::       :::::::       :::::::       ",
        "  +++  :.www.:       :::::::       :::::::       :::::::",
        "  ( )  :.( ).:       :::::::       :::::::       :::::::",
        "  / \\  :./ \\.:       :::::::       :::::::       :::::::",
        ":::::::       :::::::       :::::::       :::::::       ",
        ":::::::       :::::::       :::::::       :::::::       ",
        ":::::::       :::::::       :::::::       :::::::       ",
        "       :.....:       :.....:       :.....:       :.....:",
        "  OOO  :.OOO.:  OOO  :.OOO.:  OOO  :.OOO.:  OOO  :.OOO.:",
        "       :.....:       :.....:       :.....:       :.....:",
        ":.....:       :.....:  www  :.+++.:       :.....:       ",
        ".[#=#].  =o\\  :.(/).:  ( )  :.( ).:  (/)  :.=o\\.: [#=#] ",
        ":./ \\.:  / \\  :./ \\.:  / \\  :./ \\.:  / \\  :./ \\.:  / \\  ",
    };

    const char *graphics_big[] =
    {
        "           :::::::::::           :: wwww :::   ++++    :::::::::::           :::::::::::",
        "           :::__/\\::::     O     :::(@@)::::   (@@)    :::::O:::::  __/\\     :::::::::::",
        "  [#=#=#]  ::|__O@):::   (@//)   ::::/\\:::::    /\\     :::(@//)::: |__O )    ::[#=#=#]::",
        "   |@@@|   ::::/@@\\:::    /@\\    :::/@@\\::::   /@@\\    ::::/@\\::::   /@@\\    :::|@@@|:::",
        "   /@@@\\   :::/@@@@\\::   /@@@\\   ::(@@@@):::  (@@@@)   :::/@@@\\:::  /@@@@\\   :::/@@@\\:::",
        "  [_@@@_]  ::[_@@@@_]:  [_@@@_]  ::[_@@_]:::  [_@@_]   ::[_@@@_]:: [_@@@@_]  ::[_@@@_]::",
        ":::::::::::           :::::::::::           :::::::::::           :::::::::::           ",
        ":::::_:::::     _     :::::_:::::     _     :::::_:::::     _     :::::_:::::     _     ",
        "::::(@)::::    (@)    ::::(@)::::    (@)    ::::(@)::::    (@)    ::::(@)::::    (@)    ",
        "::::/@\\::::    /@\\    ::::/@\\::::    /@\\    ::::/@\\::::    /@\\    ::::/@\\::::    /@\\    ",
        ":::[_@_]:::   [_@_]   :::[_@_]:::   [_@_]   :::[_@_]:::   [_@_]   :::[_@_]:::   [_@_]   ",
        ":::::::::::           :::::::::::           :::::::::::           :::::::::::           ",
        "           :::::::::::           :::::::::::           :::::::::::           :::::::::::",
        "           :::::::::::           :::::::::::           :::::::::::           :::::::::::",
        "           :::::::::::           :::::::::::           :::::::::::           :::::::::::",
        "           :::::::::::           :::::::::::           :::::::::::           :::::::::::",
        "           :::::::::::           :::::::::::           :::::::::::           :::::::::::",
        "           :::::::::::           :::::::::::           :::::::::::           :::::::::::",
        ":: ++++ :::   wwww    :::::::::::           :::::::::::           :::::::::::           ",
        ":::(@@)::::   (@@)    :::::::::::           :::::::::::           :::::::::::           ",
        "::::/\\:::::    /\\     :::::::::::           :::::::::::           :::::::::::           ",
        ":::/@@\\::::   /@@\\    :::::::::::           :::::::::::           :::::::::::           ",
        "::(@@@@):::  (@@@@)   :::::::::::           :::::::::::           :::::::::::           ",
        "::[____]:::  [____]   :::::::::::           :::::::::::           :::::::::::           ",
        "   ++++    :: wwww :::           :::::::::::           :::::::::::           :::::::::::",
        "   (  )    :::(  )::::           :::::::::::           :::::::::::           :::::::::::",
        "    /\\     ::::/\\:::::           :::::::::::           :::::::::::           :::::::::::",
        "   /  \\    :::/  \\::::           :::::::::::           :::::::::::           :::::::::::",
        "  (____)   ::(____):::           :::::::::::           :::::::::::           :::::::::::",
        "  [____]   ::[____]:::           :::::::::::           :::::::::::           :::::::::::",
        ":::::::::::           :::::::::::           :::::::::::           :::::::::::           ",
        ":::::::::::           :::::::::::           :::::::::::           :::::::::::           ",
        ":::::::::::           :::::::::::           :::::::::::           :::::::::::           ",
        ":::::::::::           :::::::::::           :::::::::::           :::::::::::           ",
        ":::::::::::           :::::::::::           :::::::::::           :::::::::::           ",
        ":::::::::::           :::::::::::           :::::::::::           :::::::::::           ",
        "           :::::::::::           :::::::::::           :::::::::::           :::::::::::",
        "     _     :::::_:::::     _     :::::_:::::     _     :::::_:::::     _     :::::_:::::",
        "    ( )    ::::( )::::    ( )    ::::( )::::    ( )    ::::( )::::    ( )    ::::( )::::",
        "    /_\\    ::::/_\\::::    /_\\    ::::/_\\::::    /_\\    ::::/_\\::::    /_\\    ::::/_\\::::",
        "   [___]   :::[___]:::   [___]   :::[___]:::   [___]   :::[___]:::   [___]   :::[___]:::",
        "           :::::::::::           :::::::::::           :::::::::::           :::::::::::",
        ":::::::::::           :::::::::::   wwww    :: ++++ :::           :::::::::::           ",
        ":::::::::::   __/\\    :::::O:::::   (  )    :::(  )::::     O     ::__/\\:::::           ",
        "::[#-#-#]::  |__O )   :::( //):::    /\\     ::::/\\:::::   ( //)   :|__O )::::  [#-#-#]  ",
        ":::|   |:::    /  \\   ::::/ \\::::   /  \\    :::/  \\::::    / \\    :::/  \\::::   |   |   ",
        ":::/___\\:::   /____\\  :::/___\\:::  (____)   ::(____):::   /___\\   ::/____\\:::   /___\\   ",
        "::[_____]::  [______] ::[_____]::  [____]   ::[____]:::  [_____]  :[______]::  [_____]  "
    };


    // 8 * 3 = 24 lines (small)
    // 8 * 6 = 48 lines (big)
    int nrows_per_rank = big?6:3;
    int ncols_per_file = big?11:7;
    for( int rank='8'; rank>='1'; rank--, p+=8 )
    {
        bool dark = (rank=='7'||rank=='5'||rank=='3'||rank=='1');
        for( int row=0; row<nrows_per_rank; row++ )
        {
            for( int file='a'; file<='h'; file++ )
            {
                char piece = *(p + (file-'a'));
                thc::Square square = thc::a1;
                switch( piece )
                {
                    default:
                    case '.':   square = dark ? thc::a3 : thc::b3;   break;
                    case 'r':   square = dark ? thc::h8 : thc::a8;   break;
                    case 'n':   square = dark ? thc::b8 : thc::g8;   break;
                    case 'b':   square = dark ? thc::f8 : thc::c8;   break;
                    case 'q':   square = dark ? thc::d8 : thc::b5;   break;
                    case 'k':   square = dark ? thc::a5 : thc::e8;   break;
                    case 'p':   square = dark ? thc::a7 : thc::b7;   break;
                    case 'P':   square = dark ? thc::b2 : thc::a2;   break;
                    case 'R':   square = dark ? thc::a1 : thc::h1;   break;
                    case 'N':   square = dark ? thc::g1 : thc::b1;   break;
                    case 'B':   square = dark ? thc::c1 : thc::f1;   break;
                    case 'Q':   square = dark ? thc::b4 : thc::d1;   break;
                    case 'K':   square = dark ? thc::e1 : thc::a4;   break;
                }
                int offset = (int)square;
                int array_row = nrows_per_rank * (offset/8) + row;
                int array_col = ncols_per_file * (offset%8);
                const char *src = &(big?graphics_big:graphics)[array_row][array_col];
                for( int i=0; i<ncols_per_file; i++ )
                    s += *src++;
                if( file == 'h' )
                    s += "\n";
                dark = !dark;
            }
        }
    }
    return s;
}

static std::string cmd_uci()
{
    std::string rsp= util::sprintf(
    "id name " ENGINE_NAME " " VERSION "\n"
    "id author Dan and Kathe Spracklin, Zargon C++ port by Bill Forster\n"
    "option name FixedDepth type spin min 0 max %d default 0\n"
    "option name LogFileName type string default\n"
    "uciok\n", MAX_DEPTH );
    return rsp;
}

static std::string cmd_isready()
{
    return "readyok\n";
}

static std::string stop_rsp;
static std::string cmd_stop()
{
    std::string ret = stop_rsp;
    stop_rsp.clear();
    return ret;
}

static void cmd_setoption( const std::vector<std::string> &fields )
{
    // Option "FixedDepth"
    //  Range is 0-MAX_DEPTH, default is 0. 0 indicates auto depth selection,
    //   others are fixed depth
    // eg "setoption name FixedDepth value 3"
    if( fields.size()>4 && fields[1]=="name" && fields[2]=="fixeddepth" && fields[3]=="value" )
    {
        depth_option = atoi(fields[4].c_str());
        if( depth_option<0 || depth_option>MAX_DEPTH )
            depth_option = 0;
    }

    // Option "LogFileName"
    //   string, default is empty string (no log kept in that case)
    // eg "setoption name LogFileName value c:\windows\temp\sargon-log-file.txt"
    else if( fields.size()>4 && fields[1]=="name" && fields[2]=="logfilename" && fields[3]=="value" )
    {
        logfile_name = fields[4];
    }
}

static std::string cmd_go( const std::vector<std::string> &fields )
{
    the_pv.clear();
    stop_rsp = "";
    base_time = elapsed_milliseconds();
    total_callbacks = 0;
    after_points_callbacks = 0;

    // Work out our time and increment
    // eg cmd ="wtime 30000 btime 30000 winc 0 binc 0"
    std::string stime = "btime";
    std::string sinc  = "binc";
    if( the_position.white )
    {
        stime = "wtime";
        sinc  = "winc";
    }
    bool expecting_time = false;
    bool expecting_inc = false;
    bool expecting_depth = false;
    int ms_time   = 0;
    int ms_inc    = 0;
    int depth     = 0;
    for( std::string parm: fields )
    {
        if( expecting_time )
        {
            ms_time = atoi(parm.c_str());
            expecting_time = false;
        }
        else if( expecting_inc )
        {
            ms_inc = atoi(parm.c_str());
            expecting_inc = false;
        }
        else if( expecting_depth )
        {
            depth = atoi(parm.c_str());
            expecting_depth = false;
        }
        else
        {
            if( parm == stime )
                expecting_time = true;
            if( parm == sinc )
                expecting_inc = true;
            if( parm == "depth" )
                expecting_depth = true;
        }
    }
    bool new_game = is_new_game();
    thc::Move bestmove = calculate_next_move( false, new_game, ms_time, ms_inc, depth ? depth : depth_option );
    return util::sprintf( "bestmove %s\n", bestmove.TerseOut().c_str() );
}

static void cmd_go_infinite()
{
    the_pv.clear();
    stop_rsp = "";
    int plymax=1;
    bool aborted = false;
    base_time = elapsed_milliseconds();
    total_callbacks = 0;
    after_points_callbacks = 0;
    while( !aborted )
    {
        aborted = run_sargon(plymax,true);  // note avoid_book = true
        if( plymax < MAX_DEPTH )
            plymax++;
        if( !aborted )
        {
            std::string out = generate_progress_report();
            if( out.length() > 0 )
            {
                fprintf( stdout, out.c_str() );
                fflush( stdout );
                log( "rsp>%s\n", out.c_str() );
                stop_rsp = util::sprintf( "bestmove %s\n", the_pv.variation[0].TerseOut().c_str() ); 
            }
        }
    }
    if( stop_rsp == "" )    // Shouldn't actually ever happen as callback polling doesn't abort
    {                       //  run_sargon() if plymax is 1
        run_sargon(1,false);
        std::string bestmove = sargon_export_best_move();
        stop_rsp = util::sprintf( "bestmove %s\n", bestmove.c_str() ); 
    }
}

// cmd_position(), set a new (or same or same plus one or two half moves) position
static bool cmd_position_signals_new_game;
static bool is_new_game()
{
    return cmd_position_signals_new_game;
}

static void cmd_position( const std::string &whole_cmd_line, const std::vector<std::string> &fields )
{
    static thc::ChessRules prev_position;
    bool position_changed = true;

    // Get base starting position
    thc::ChessRules tmp;
    the_position = tmp;    //init
    bool look_for_moves = false;
    if( fields.size() > 2 && fields[1]=="fen" )
    {
        size_t offset = whole_cmd_line.find("fen");
        offset = whole_cmd_line.find_first_not_of(" \t",offset+3);
        if( offset != std::string::npos )
        {
            std::string fen = whole_cmd_line.substr(offset);
            the_position.Forsyth(fen.c_str());
            look_for_moves = true;
        }
    }
    else if( fields.size() > 1 && fields[1]=="startpos" )
    {
        thc::ChessRules tmp;
        the_position = tmp;    //init
        look_for_moves = true;
    }

    // Add moves
    if( look_for_moves )
    {
        bool expect_move = false;
        thc::Move last_move, last_move_but_one;
        last_move_but_one.Invalid();
        last_move.Invalid();
        for( std::string parm: fields )
        {
            if( expect_move )
            {
                thc::Move move;
                bool okay = move.TerseIn(&the_position,parm.c_str());
                if( !okay )
                    break;
                the_position.PlayMove( move );
                last_move_but_one = last_move;
                last_move         = move;
            }
            else if( parm == "moves" )
                expect_move = true;
        }
        thc::ChessPosition initial;
        if( the_position == initial )
            position_changed = true;
        else if( the_position == prev_position )
            position_changed = false;

        // Maybe this latest position is the old one with one new move ?
        else if( last_move.Valid() )
        {
            thc::ChessRules temp = prev_position;
            temp.PlayMove( last_move );
            if( the_position == temp )
            {
                // Yes it is! so we are still playing the same game
                position_changed = false;
            }

            // Maybe this latest position is the old one with two new moves ?
            else if( last_move_but_one.Valid() )
            {
                prev_position.PlayMove( last_move_but_one );
                prev_position.PlayMove( last_move );
                if( the_position == prev_position )
                {
                    // Yes it is! so we are still playing the same game
                    position_changed = false;
                }
            }
        }
    }

    cmd_position_signals_new_game = position_changed;
    log( "cmd_position(): %s\nSetting cmd_position_signals_new_game=%s\nFEN = %s\n%s",
        whole_cmd_line.c_str(),
        cmd_position_signals_new_game?"true":"false",
        the_position.ForsythPublish().c_str(),
        the_position.ToDebugStr().c_str() );

    // For next time
    prev_position = the_position;
    the_position_base = the_position;
}

static bool cmd_interactive_go( int nbr_lines )
{
    bool ok = false;
    std::string s = interactive_screen( false, nbr_lines );
    fprintf( stderr, "%s\n", s.c_str() );
    fprintf( stderr, "Thinking [level %d]....", the_level );
    fflush( stderr );
    thc::Move bestmove = calculate_next_move( true, true, 1000000, 1000000, the_level );
    ok = bestmove.Valid();
    if( ok )
    {
        //printf( "Sargon plays %s\n", bestmove.NaturalOut(&the_position).c_str() );
        //printf( "%s", the_position.ToDebugStr("before Sargon move").c_str() );
        the_position.PlayMove( bestmove );
        the_game.push_back(bestmove);
    }
    return ok;
}

static bool cmd_interactive_position( const std::string &raw_cmd_line )
{
    bool ok = the_position.Forsyth(raw_cmd_line.c_str());
    if( ok )
    {
        the_position_base = the_position;
        the_game.clear();
    }
    return ok;
}

static bool cmd_interactive_level( const std::string &parm )
{
    bool ok = false;
    int n = atol(parm.c_str());
    if( 1<=n && n<=MAX_DEPTH )
    {
        the_level = n;
        ok = true;
    }
    return ok;
}

static bool cmd_interactive_undo()
{
    bool ok = the_game.size() > 0;
    if( ok )
    {
        the_game.pop_back();
        thc::ChessRules cr = the_position_base;
        int sz = (int)the_game.size();
        for( int i=0; i<sz; i++ )
        {
            thc::Move mv = the_game[i];
            cr.PlayMove(mv);         
        }
        the_position = cr;
    }
    return ok;
}

static void cmd_interactive_save( const std::string &filename, std::string &output_msg )
{
    FILE *fout;
    size_t offset = filename.find('.');
    bool no_period = (offset==std::string::npos);
    std::string fname = no_period ? filename + ".pgn" : filename;
    bool ok = (0 == fopen_s( &fout, fname.c_str(), "a" ));
    if( !ok )
    {
        output_msg = util::sprintf( "Could not open or create file %s", fname.c_str() );
        return;
    }
    std::string str1;
    str1 += "[Event \"";
    str1 += "Casual chess";
    str1 += "\"]" "\n";
    str1 += "[Site \"";
    str1 += "Desktop computer";
    str1 += "\"]" "\n";
    str1 += "[Date \"";
    struct tm ptime;
    time_t timer = time(NULL);
    localtime_s( &ptime, &timer );
    int yyyy = ptime.tm_year+1900;
    int mm   = ptime.tm_mon+1;
    int dd   = ptime.tm_mday;
    str1 += util::sprintf( "%04d.%02d.%02d", yyyy, mm, dd );
    str1 += "\"]" "\n";
    str1 += "[Round \"";
    str1 += "?";
    str1 += "\"]" "\n";
    str1 += "[White \"";
    str1 += z.COLOR ? "Sargon" : "Human";
    str1 += "\"]" "\n";
    str1 += "[Black \"";
    str1 += z.COLOR ? "Human" : "Sargon";
    str1 += "\"]" "\n";
    std::string result = "*";
    thc::TERMINAL score_terminal;
    the_position.Evaluate( score_terminal );
    if( score_terminal == thc::TERMINAL_WCHECKMATE )
        result = "0-1";
    if( score_terminal == thc::TERMINAL_BCHECKMATE )
        result = "1-0";
    if( score_terminal == thc::TERMINAL_WSTALEMATE )
        result = "1/2-1/2";
    if( score_terminal == thc::TERMINAL_BSTALEMATE )
        result = "1/2-1/2";
    str1 += "[Result \"";
    str1 += result;
    str1 += "\"]" "\n";
    thc::ChessPosition tmp;
    bool needs_fen = (tmp!=the_position_base);
    if( needs_fen )
    {
        str1 += "[FEN \"";
        str1 += the_position_base.ForsythPublish();
        str1 += "\"]" "\n";
    }
    str1 += "\n";
    thc::ChessRules cr = the_position_base;
    int sz = (int)the_game.size();
    std::string str2;
    int col=0;
    for( int i=0; i<sz; i++ )
    {
        thc::Move mv = the_game[i];
        std::string s;
        if( cr.WhiteToPlay() )
            s += util::sprintf( " %d.", cr.full_move_count );
        else if( i==0 && !cr.WhiteToPlay() )
            s += util::sprintf( " %d...", cr.full_move_count );
        if( col + s.length() > 79 )
        {
            str2 += "\n";
            col = 0;
        }
        if( col == 0 )
            util::ltrim(s);
        str2 += s;
        col += (int)(s.length());
        s = " ";
        s += mv.NaturalOut(&cr);
        if( col + s.length() > 79 )
        {
            str2 += "\n";
            col = 0;
        }
        if( col == 0 )
            util::ltrim(s);
        str2 += s;
        col += (int)(s.length());
        cr.PlayMove(mv);         
    }
    std::string s = " ";
    s += result;
    if( col + s.length() > 79 )
    {
        str2 += "\n";
        col = 0;
    }
    if( col == 0 )
        util::ltrim(s);
    str2 += s;
    str2 += "\n";
    str2 += "\n";
    fputs( str1.c_str(), fout );
    fputs( str2.c_str(), fout );
    fclose(fout);
    output_msg = util::sprintf( "Game saved to PGN file %s", fname.c_str() );
    return;
}

static bool cmd_interactive_move( const std::string &raw_cmd )
{
    std::string move_txt = raw_cmd;
    bool ok = false;
    thc::Move move;
    bool okay = move.NaturalIn(&the_position,move_txt.c_str());
    if( okay )
    {
        the_position.PlayMove( move );
        the_game.push_back(move);
        ok = true;
    }
    else if( move_txt.length() > 0 && move_txt[0]>='a' && move_txt[0]<='z' )
    {
        std::string up = move_txt;
        up[0] = move_txt[0]-' ';
        okay = move.NaturalIn(&the_position,up.c_str());
        if( okay )
        {
            the_position.PlayMove( move );
            the_game.push_back(move);
            ok = true;
        }
    }
    return ok;
}

// UCI compatible report
static std::string generate_progress_report()
{
    int     score_cp   = the_pv.value;
    int     depth      = z.PLYMAX;
    thc::ChessRules ce = the_position;
    std::string buf_pv;
    std::string buf_score;
    unsigned long now_time = elapsed_milliseconds();	
    int nodes = after_points_callbacks;
    unsigned long elapsed_time = now_time-base_time;
    if( elapsed_time == 0 )
        elapsed_time++;
    for( unsigned int i=0; i<the_pv.variation.size(); i++ )
    {
        thc::Move move=the_pv.variation[i];
        buf_pv += " ";
        buf_pv += move.TerseOut();
        ce.PlayMove( move );
    }
    buf_score = util::sprintf( "cp %d", score_cp );
    if( the_pv.mate )
    {
        if( the_pv.mate_count != 0 )      // zero is stalemate
            buf_score = util::sprintf( "mate %d", the_pv.mate_count );
    }
    std::string out;
    if( the_pv.variation.size() > 0 )
    {
        out = util::sprintf( "info depth %d score %s time %lu nodes %lu nps %lu pv%s\n",
                    depth,
                    buf_score.c_str(),
                    (unsigned long) elapsed_time,
                    (unsigned long) nodes,
                    1000L * ((unsigned long) nodes / (unsigned long)elapsed_time ),
                    buf_pv.c_str() );
    }
    return out;
}

/*

    Calculate next move, simplified approach for Zargon

    Start at depth = 6

        Call FNDMOV() to calculate move and PV

        Use cutoff timer to cut calculation short if T > T1

        If calculation completes

            If T > T2  depth--

            If T < T3  depth++

        If calculation doesn't complete
            depth -= 2 and retry with half the time. No more than 4 tries
            Last try should be PLYMAX=3 which is nearly instantaneous and
            doesn't ever timeout

    T1, T2, and T3 set at (say) P/2 P/15 P/100 respectively
    where P is the time available.

     1) No complex "play down a mating PV" logic from Sargon predecessor,
        Zargon does not have the same issue with dithering over mate that
        Sargon had, due to mate in 1,2,3 etc. detection.
     2) In fixed depth mode, (either the FixedDepth engine option is set
        or the depth parameter to the go command is set), don't loop and don't
        use the timer.
     3) No complex repetition avoidance

*/

static thc::Move calculate_next_move(  bool interactive_mode, bool new_game, unsigned long ms_time, unsigned long ms_inc, int depth )
{
    // Timers
    const unsigned long LOW  = 100;
    const unsigned long MED  = 30;
    const unsigned long HIGH = 5;
    unsigned long ms_lo  = ms_time / LOW;
    unsigned long ms_med = ms_time / MED;
    unsigned long ms_hi  = ms_time / HIGH;
    bool timer_running = false;

    // Run engine
    static int plymax_adaptive = 6;
    bool fixed_depth = (depth>0);
    int plymax = fixed_depth ? depth : plymax_adaptive;
    int plymax_before = plymax_adaptive;

    // Remove stale "TIMEOUT" events from queue
    std::queue<std::string> temp;
    while( !async_queue.empty() )
    {
        std::string s = async_queue.dequeue();
        log( "flushed(%s)\n", s.c_str() );
        if( s != "TIMEOUT" )
            temp.push(s);
    }

    // Run the engine, possibly multiple times (but no more than 4 times)
    bool book_move=false;
    bool have_move=false;
    bool aborted=false;
    bool timeout=false;
    int  tries=0;
    unsigned long base = elapsed_milliseconds();
    unsigned long elapsed = 0;
    for(;;)
    {
        book_move=false;
        have_move=false;
        timeout=false;

        // Start emergency timer        
        if( !interactive_mode && plymax>3 )
            timer_set( ms_hi );

        // Run the engine, with timeout if plymax>3
        log( "run_sargon(%d) IN\n",plymax );
        aborted = run_sargon(plymax,false);
        tries++;
        log( "run_sargon() OUT returns %s\n", aborted?"aborted=TRUE":"aborted=FALSE" );
        unsigned long now = elapsed_milliseconds();
        elapsed = (now-base);
        ms_hi = ms_hi/2;    // use less maximum time each time

        // Remove stale "TIMEOUT" events from queue
        if( aborted )
        {
            while( !async_queue.empty() )
            {
                std::string s = async_queue.dequeue();
                log( "flushed(%s)\n", s.c_str() );
                if( s == "TIMEOUT" )
                    timeout = true;
                else
                    temp.push(s);
            }
        }

        // Clear the timer
        if( !interactive_mode && plymax>3 )
        {
            timer_clear();
        }

        // If book move, we're done, go with that
        book_move = !aborted && z.BESTM!=0 && the_pv.variation.size()==0;
        log( "book_move=%s\n", book_move?"true":"false" );
        if( book_move )
        {
            break;
        }

        // Have a move ?
        std::string bestmove_terse;
        if( the_pv.variation.size() >= 1 )
        {
            bestmove_terse = the_pv.variation[0].TerseOut();
            have_move = (bestmove_terse.length() >= 4);
        }
        log( "have_move=%s\n", have_move?"true":"false" );

        // Never try more than 4 times, just report on what we have
        if( tries >= 4 )
        {
            log( "tries=%d\n", tries );
            break;
        }

        // Timed out - reduce plymax and retry
        if( aborted && timeout )
        {
            if( tries >= 3 )
            {
                plymax = 3;     // enough is enough
            }
            else
            {
                plymax -= 2;
                if( plymax < 3 )
                    plymax = 3;
            }
            plymax_adaptive = plymax;
            log( "try again plymax=%d\n", plymax );
            continue;   // Try again
        }

        // Aborted but not timed out (eg force immediate move) and have move, go with what we have
        if( aborted && !timeout && (have_move||gbl_interactive) )
        {
            if( have_move )
                log( "aborted && !timeout && have_move so go with that\n" );
            else
                log( "aborted && !timeout && no move but gbl_interactive, fail to deliver a move, that's fine in that scenario\n" );
            break;  // Have move
        }

        // If have a regular move, adjust plymax and we're done
        if( !aborted && have_move )
        {
            if( tries==1 && !fixed_depth )  // if normal adaptive situation
            {
                if( elapsed>ms_med && plymax_adaptive>3 )
                    plymax_adaptive--;   // we used too much time
                else if( elapsed<ms_lo && plymax_adaptive<MAX_DEPTH )
                    plymax_adaptive++;   // we can afford to go deeper next time
            }
            log( "Normal end, !aborted && have_move, tries=%d, plymax_adaptive=%d\n", tries, plymax_adaptive );
            break;  // Have move
        }

        // Any other situation, reduce plymax to minimum and retry
        plymax = 3;
        plymax_adaptive = plymax;
        log( "Reduce plymax to minimum and loop\n" );
    }

    // Re-enqueue anything we flushed (except timeouts)
    while( !temp.empty() )
    {
        std::string s = temp.front();
        temp.pop();
        log( "re-enqueue(%s)\n", s.c_str() );
        async_queue.enqueue(s);
    }

    // Report on book move
    if( book_move )    
    {
        std::string bestmove_terse = sargon_export_best_move();
        thc::Move bestmove;
        bestmove.Invalid();
        bool have_move = bestmove.TerseIn( &the_position, bestmove_terse.c_str() );
        if( have_move )
            log( "No PV, expect Sargon is playing book move - %s\n%s", bestmove_terse.c_str(), the_position.ToDebugStr().c_str() );
        else
            log( "Sargon doesn't find move - %s\n%s", bestmove_terse.c_str(), the_position.ToDebugStr().c_str() );
        stop_rsp = util::sprintf( "bestmove %s\n", bestmove_terse.c_str() );
        return bestmove;
    }

    // Report on regular move
    else if( have_move )
    {
        // Debug state changes
        if( plymax_adaptive != plymax_before )
        {
            log( "aborted=%s, tries=%d, elapsed=%lu, ms_lo=%lu, ms_med=%lu, ms_hi=%lu, plymax=%d -> %d\n",
                    aborted?"true":"false", tries, elapsed, ms_lo, ms_med, ms_hi, plymax_before, plymax_adaptive );
        }
        else
        {
            log( "aborted=%s, tries=%d, elapsed=%lu, ms_lo=%lu, ms_med=%lu, ms_hi=%lu, plymax=%d\n",
                    aborted?"true":"false", tries, elapsed, ms_lo, ms_med, ms_hi, plymax );
        }

        // Report 
        std::string info;
        info = generate_progress_report();
        if( !gbl_interactive )
        {
            fprintf( stdout, info.c_str() );
            fflush( stdout );
        }
        log( "rsp>%s\n", info.c_str() );
        stop_rsp = util::sprintf( "bestmove %s\n", the_pv.variation[0].TerseOut().c_str() ); 
        info.clear();
        thc::Move mv = the_pv.variation[0];
        return mv;
    }

    // Else it's a sad sad situation
    else
    {
        stop_rsp = util::sprintf( "bestmove ????\n" ); 
        thc::Move bestmove;
        bestmove.Invalid();
        return bestmove;
    }
}

// Simple logging facility gives us some debug capability when running under control of a GUI
static int log( const char *fmt, ... )
{
    static std::mutex mtx;
    std::lock_guard<std::mutex> lck(mtx);
	va_list args;
	va_start( args, fmt );
    if( logfile_name != "" )
    {
        static bool first=true;
        FILE *file_log;
        errno_t err = fopen_s( &file_log, logfile_name.c_str(), first? "wt" : "at" );
        first = false;
        if( !err )
        {
            static char buf[1024];
            time_t t = time(NULL);
            struct tm ptm;
            localtime_s( &ptm, &t );
            asctime_s( buf, sizeof(buf), &ptm );
            char *p = strchr(buf,'\n');
            if( p )
                *p = '\0';
            fputs( buf, file_log);
            buf[0] = ':';
            buf[1] = ' ';
            vsnprintf( buf+2, sizeof(buf)-4, fmt, args ); 
            fputs( buf, file_log );
            fclose( file_log );
        }
    }
    va_end(args);
    return 0;
}

//
// UCI specific callback handling (minimal)
//

// Called by callback_after_points()
void callback_uci_after_points()
{
    total_callbacks++;
    after_points_callbacks++;

    // Need to call the following regularly, but no need for all three
    // routines to do it, so now just do it at end of points

    // Abort run_sargon() if new event in queue (and not if PLYMAX<=3 which is
    //  effectively instantaneous)
    if( !async_queue.empty() && z.PLYMAX>3 )
    {
        longjmp( jmp_buf_env, 1 );
    }
}

