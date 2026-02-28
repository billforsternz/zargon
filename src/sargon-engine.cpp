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
static emulated_memory &m = gbl_emulated_memory;

// Measure elapsed time, nodes    
static unsigned long base_time;

// Misc
static int depth_option;    // 0=auto, other values for fixed depth play
static std::string logfile_name;
static unsigned long total_callbacks;
static unsigned long genmov_callbacks;
static unsigned long bestmove_callbacks;
static unsigned long end_of_points_callbacks;
static std::vector<thc::Move> the_game;
static int the_level=6;
static bool gbl_interactive=true;
static bool gbl_orientation;

// The current 'Master' postion
static thc::ChessRules the_position;
static thc::ChessRules the_position_base;

// The current 'Master' PV
static PV the_pv;

// Play down mating sequence without recalculation if possible
struct MATING
{
    bool                   active;
    thc::ChessRules        position;    // start position
    std::vector<thc::Move> variation;   // leads to mate
    unsigned int           idx;         // idx into variation
    int                    nbr;         // nbr of moves left to mate
};
static MATING mating;

// The list of repetition moves to avoid, normally empty
static std::vector<thc::Move> the_repetition_moves;

// Command line interface
static std::string interactive_screen( bool help_screen, int nbr_lines );
static std::string ascii_board( thc::ChessPosition const &cp);
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
static bool        cmd_interactive_flip();
static bool        cmd_interactive_undo();

// Misc
static bool is_new_game();
static int log( const char *fmt, ... );
static bool run_sargon( int plymax, bool avoid_book );
static std::string generate_progress_report( bool &we_are_forcing_mate, bool &we_are_stalemating_now );
static thc::Move calculate_next_move( bool new_game, unsigned long ms_time, unsigned long ms_inc, int depth );
static bool repetition_calculate( thc::ChessRules &cr, std::vector<thc::Move> &repetition_moves );
static bool test_whether_move_repeats( thc::ChessRules &cr, thc::Move mv );
static void repetition_remove_moves( const std::vector<thc::Move> &repetition_moves );
static bool repetition_test();

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

// main()
int main_uci( int argc, const char *argv[] )
{
    logfile_name = std::string(argv[0]) + "-log.txt"; // wake this up for early logging
#ifdef _DEBUG
    static const std::vector<std::string> test_sequence =
    {
#if 0
        "setoption name FixedDepth value 6",
        "position fen 8/3R2pk/4Np1p/5r2/8/1P2PK2/5PPP/8 w - - 9 41 moves f3e2 h7h8 d7d8 h8h7\n",
        "go\n"
        "isready\n",
        "quit\n"
#endif
#if 0
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
        "position fen 8/3R2pk/4Np1p/5r2/8/1P2PK2/5PPP/8 w - - 9 41 moves f3e2 h7h8 d7d8 h8h7\n",
        "go depth 6 wtime 55068 btime 889329 winc 1000 binc 5000\n",
        "isready\n",
        "quit\n"
#endif
#if 0
        "position fen 7K/5k1P/8/8/8/8/8/8 b - - 0 1\n",
        "go depth wtime 1000 btime 1000 winc 1000 binc 5000\n",
        "isready\n",
        "quit\n"
#endif
#if 0
        "position startpos moves e2e4 c7c5 b1c3 a7a6 d1h5 e7e6 g1f3 b7b5 f3e5 g7g6 h5f3 g8f6 f1e2 f8g7 e5d3 d7d6 e4e5 f6d5 c3d5 e6d5 f3d5 a8a7 e5d6 e8g8 d5c5 a7d7 a2a4 d7d6 a4b5 a6b5 c5b5 c8d7 b5c5 d7c6 e1g1 f8e8 e2f3 c6f3 g2f3 d6d5 c5c4 d5d4 c4b5 d4d5 b5a4 d5d4 a4a5 d4d5 a5d8 e8d8 f1e1 b8c6 a1a3 d5g5 g1h1 g5h5 h1g2 d8d5 e1e8 g7f8 a3a8 d5g5 g2h1 c6e5 e8f8 g8g7 f3f4 h5h2 h1h2 e5f3 h2h3 f3g1 h3h4 g1f3 h4h3 f3g1 h3h2 g1f3\n",
        "go depth 5 wtime 88428 btime 883250 winc 1000 binc 5000\n",
        "isready\n",
        "quit\n"
#endif
#if 0
        "position startpos moves e2e4 c7c5 b1c3 a7a6 d1h5 e7e6 g1f3 b7b5 f3e5 g7g6 h5f3 g8f6 f1e2 f8g7 e5d3 d7d6 e4e5 f6d5 c3d5 e6d5 f3d5 a8a7 e5d6 e8g8 d5c5 a7d7 a2a4 d7d6 a4b5 a6b5 c5b5 c8d7 b5c5 d7c6 e1g1 f8e8 e2f3 c6f3 g2f3 d6d5 c5c4 d5d4 c4b5 d4d5 b5a4 d5d4 a4a5 d4d5 a5d8 e8d8 f1e1 b8c6 a1a3 d5g5 g1h1 g5h5 h1g2 d8d5 e1e8 g7f8 a3a8 d5g5 g2h1 c6e5 e8f8 g8g7 f3f4 h5h2 h1h2 e5f3 h2h3 f3g1 h3h4 g1f3\n",
        "go depth 5 wtime 55068 btime 889329 winc 1000 binc 5000\n",
        "isready\n",
        "quit\n"
#endif
#if 0
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
    //bool ok = repetition_test();
    //printf( "repetition test %s\n", ok?"passed":"failed" );
    if( test_sequence.size() > 0 )
    {
        for( std::string s: test_sequence )
        {
            util::rtrim(s);
            log( "cmd>%s\n", s.c_str() );
            process(s);
        }
        return 0;
    }
#endif
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
    for(;;)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        {
            std::lock_guard<std::mutex> lck(timer_mtx);
            if( future_time == -1 )
                break;
            else if( future_time != 0 )
            {
                long now_time = elapsed_milliseconds();	
                long time_remaining = future_time - now_time;
                if( time_remaining <= 0 )
                {
                    future_time = 0;
                    async_queue.enqueue( "TIMEOUT" );
                }
            }
        }
    }
}

// Clear the timer
static void timer_clear()
{
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

    // Prevent generation of a stale "TIMEOUT" event
    future_time = 0;

    // Remove stale "TIMEOUT" events from queue
    std::queue<std::string> temp;
    while( !async_queue.empty() )
    {
        std::string s = async_queue.dequeue();
        if( s != "TIMEOUT" )
            temp.push(s);
    }
    while( !temp.empty() )
    {
        std::string s = temp.front();
        temp.pop();
        async_queue.enqueue(s);
    }

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
            if( s == "quit" )
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
        aborted = true;
    else
        sargon_run_engine(the_position,plymax,the_pv,avoid_book); // the_pv updated only if not aborted
    return aborted;
}

static const char * help[] =
{
    "                       ",
    " Sargon 1978 64 bit    ",
    " By Dan and Kathe      ",
    "  Spracklen            ",
    "                       ",
    " UCI engine with bonus ",
    "  interactive shell    ",
    "                       ",
    " Commands are;         ",
    " Nf3   ;(eg) make move ",
    " go    ;start game     ",
    " stop  ;stop game      ",
    " flip  ;flip board     ",
    " undo  ;undo move      ",
    " reset ;start position ",
    " uci   ;exit to UCI    ",
    " quit  ;exit program   ",
    " level n ;set level    ",
    " r3k2r/8/8/8/8/8/8/4K2R",
    "  w Kkq - 0 1 ;(eg)    ",
    "  (to set FEN position)",
    "                       ",
    " Use -? on command line",
    " for more information  ",
    "                       "
};

// Command line top level handler
static bool process( const std::string &s, bool first )
{
    static int nbr_lines = 0;
    static bool auto_play = false;
    bool show_help = false;
    bool ok = true;
    std::string err_msg;
    if( first )
    {
        std::string pos = ascii_board(the_position);
        size_t offset = pos.find('\n');
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
        if( !first )
            return false;
    }
    std::string parm1 = fields.size()<=1 ? "" : fields[1];
    if( gbl_interactive )
    {
        if( cmd=="uci" || cmd=="quit" )
        {
            gbl_interactive = false;
            fprintf( stderr, "Leaving interactive mode\n" );
        }
        else if( cmd=="stop" )
            auto_play = false;
        else if( cmd=="level" )
        {
            if( fields.size() == 1 )
            {
                ok = false;
                err_msg = util::sprintf( "Level is currently %d, valid range is 1-20", the_level );
            }
            else
            {
                ok = cmd_interactive_level( parm1 );
                if( !ok )
                    err_msg = "Level out of range, valid range is 1-20";
            }
        }
        else if( cmd=="undo" )
        {
            ok = cmd_interactive_undo();
            auto_play = false;
            if( !ok )
                err_msg = "Error: No move to undo";
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
        else if( cmd=="show" )
        {
            ; // just redraw
        }
        else if( cmd=="help" || cmd=="?" || cmd=="-?" )
        {
            show_help = true;
        }
        else if( cmd=="go" )
        {
            auto_play = true;
            cmd_interactive_go( nbr_lines );
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
                        cmd_interactive_go( nbr_lines );
                }
                else
                {
                    ok = false;
                    err_msg = "Not a valid command, move or position. type ? for help, show to redraw";
                }
            }
        }
    }
    if( !gbl_interactive )
    {
        if( cmd == "quit" ) // special command because read_stdin() tests it
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
             "genmov callbacks=%lu\n"
             "end of points callbacks=%lu\n",
                cmd.c_str(),
                total_callbacks,
                bestmove_callbacks,
                genmov_callbacks,
                end_of_points_callbacks );
        log( "%s\n", sargon_pv_report_stats().c_str() );
    }
    if( gbl_interactive )
    {
        if( !ok )
            fprintf( stderr, "%s\ncmd>", err_msg.c_str() );
        else
        {
            bool help_screen = first || show_help || the_game.size()==0;
            std::string s = interactive_screen( help_screen , nbr_lines );
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
    std::string pos = ascii_board(the_position);
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
            ret += util::sprintf( "\n%s  %s",
                s.c_str(), right.c_str() );
        }
    }
    return ret;
}

static std::string ascii_board( thc::ChessPosition const &cp )
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


    // 8 * 3 = 24 lines
    for( int rank='8'; rank>='1'; rank--, p+=8 )
    {
        bool dark = (rank=='7'||rank=='5'||rank=='3'||rank=='1');
        for( int row=0; row<3; row++ )
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
                int array_row = 3 * (offset/8) + row;
                int array_col = 7 * (offset%8);
                const char *src = &graphics[array_row][array_col];
                for( int i=0; i<7; i++ ) // next 7 characters from src
                    s += *src++;
                // char slice[8];  // next 7 characters from src
                // memset(slice,0,sizeof(slice));
                // memcpy(slice,src,sizeof(slice)-1);
                // s += slice;
                if( file == 'h' )
                    s += "\n";
                dark = !dark;
            }
        }
    }
    // util::replace_all(s,":",".");
    return s;
}

static std::string cmd_uci()
{
    std::string rsp=
    "id name " ENGINE_NAME " " VERSION "\n"
    "id author Dan and Kathe Spracklin, Zargon C++ port by Bill Forster\n"
    "option name FixedDepth type spin min 0 max 20 default 0\n"
    "option name LogFileName type string default\n"
    "uciok\n";
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
    //  Range is 0-20, default is 0. 0 indicates auto depth selection,
    //   others are fixed depth
    // eg "setoption name FixedDepth value 3"
    if( fields.size()>4 && fields[1]=="name" && fields[2]=="fixeddepth" && fields[3]=="value" )
    {
        depth_option = atoi(fields[4].c_str());
        if( depth_option<0 || depth_option>20 )
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
    bestmove_callbacks = 0;
    genmov_callbacks = 0;
    end_of_points_callbacks = 0;

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
    thc::Move bestmove = calculate_next_move( new_game, ms_time, ms_inc, depth );
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
    bestmove_callbacks = 0;
    genmov_callbacks = 0;
    end_of_points_callbacks = 0;
    while( !aborted )
    {
        aborted = run_sargon(plymax,true);  // note avoid_book = true
        if( plymax < 20 )
            plymax++;
        if( !aborted )
        {
            bool we_are_forcing_mate, we_are_stalemating_now;
            std::string out = generate_progress_report( we_are_forcing_mate, we_are_stalemating_now );
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
        std::string bestmove = sargon_export_best_move_temp();
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
    thc::Move bestmove = calculate_next_move( true, 1000000, 1000000, the_level );
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
        the_position_base = the_position;
    return ok;
}

static bool cmd_interactive_level( const std::string &parm )
{
    bool ok = false;
    int n = atol(parm.c_str());
    if( 1<=n && n<=20 )
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

// Return true if PV has us (the engine) forcing mate
static std::string generate_progress_report( bool &we_are_forcing_mate, bool &we_are_stalemating_now )
{
    we_are_forcing_mate    = false;
    we_are_stalemating_now = false;
    int     score_cp   = the_pv.value;
    int     depth      = the_pv.depth;
    thc::ChessRules ce = the_position;
    int score_overide;
    std::string buf_pv;
    std::string buf_score;
    bool done=false;
    unsigned long now_time = elapsed_milliseconds();	
    int nodes = end_of_points_callbacks;
    unsigned long elapsed_time = now_time-base_time;
    if( elapsed_time == 0 )
        elapsed_time++;
    bool overide = false;
    for( unsigned int i=0; i<the_pv.variation.size(); i++ )
    {
        bool okay;
        thc::Move move=the_pv.variation[i];
        ce.PlayMove( move );
        buf_pv += " ";
        buf_pv += move.TerseOut();
        thc::TERMINAL score_terminal;
        okay = ce.Evaluate( score_terminal );
        if( okay )
        {
            if( score_terminal == thc::TERMINAL_BCHECKMATE ||
                score_terminal == thc::TERMINAL_WCHECKMATE )
            {
                overide = true;
                score_overide = (i+2)/2;    // 0,1 -> 1; 2,3->2; 4,5->3 etc 
                if( score_terminal == thc::TERMINAL_WCHECKMATE )
                    score_overide = 0-score_overide; //negative if black is winning
            }
            else if( score_terminal == thc::TERMINAL_BSTALEMATE ||
                        score_terminal == thc::TERMINAL_WSTALEMATE )
            {
                overide = true;
                score_overide = 0;
                if( i == 0 )
                    we_are_stalemating_now = true;
            }
        }
        if( !okay || overide )
            break;
    }    
    if( the_position.white )
    {
        if( overide ) 
        {
            if( score_overide > 0 ) // are we mating ?
            {
                mating.active    = true;
                mating.position  = the_position;
                mating.variation = the_pv.variation;
                mating.idx       = 0;
                mating.nbr       = score_overide;
                buf_score = util::sprintf( "mate %d", mating.nbr );
                we_are_forcing_mate = true;
                the_pv.value = 120;
            }
            else if( score_overide < 0 ) // are me being mated ?
            {
                buf_score = util::sprintf( "mate -%d", (0-score_overide) );
                the_pv.value = -120;
            }
            else if( score_overide == 0 ) // is it a stalemate draw ?
            {
                buf_score = util::sprintf( "cp 0" );
                the_pv.value = 0;
            }
        }
        else
        {
            buf_score = util::sprintf( "cp %d", score_cp );
        }
    }
    else
    {
        if( overide ) 
        {
            if( score_overide < 0 ) // are we mating ?
            {
                mating.active    = true;
                mating.position  = the_position;
                mating.variation = the_pv.variation;
                mating.idx       = 0;
                mating.nbr       = 0-score_overide;
                buf_score = util::sprintf( "mate %d", mating.nbr );
                we_are_forcing_mate = true;
                the_pv.value = -120;
            }
            else if( score_overide > 0 ) // are me being mated ?        
            {
                buf_score = util::sprintf( "mate -%d", score_overide );
                the_pv.value = 120;
            }
            else if( score_overide == 0 ) // is it a stalemate draw ?
            {
                buf_score = util::sprintf( "cp 0" );
                the_pv.value = 0;
            }
        }
        else
        {
            buf_score = util::sprintf( "cp %d", 0-score_cp );
        }
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

    Calculate next move efficiently using the available time.

    What is the time management algorithm?

    Each move we loop increasing plymax. We set a timer to cut us off if
    we spend too long.

    Basic idea is to loop the same number of times as on the previous
    move - i.e. we target the same plymax. The first time through
    we don't know the target unfortunately, so use the cut off timer
    to set the initial target. Later on the cut off timer should only
    kick in for unexpectedly long calculations (because we expect to
    take about the same amount of time as last time).
    
    Our algorithm is;

    loop
      if first time through
        set cutoff timer to MEDIUM time, establishes target
      else
        set cutoff timer to HIGH time, loop to target
        if hit target in less that LOW time
          increment target and loop again
        if we are cut
          decrement target

     HOWEVER: We don't always loop, and we don't always use the timer

     1) Once a PV leading to Sargon mating is calculated, play out the mate
        without further ado, always. Terminate looping on discovery and
        don't loop as the moves are played out. If opponent deviates from
        the mating PV though, normal service is resumed. (But see 3)).
     2) In fixed depth mode, (either the FixedDepth engine option is set
        or the depth parameter to the go command is set, don't loop and don't
        use the timer.
     3) Exception to 2), if normal service is resumed after Sargon has
        identified a forced mate always loop.
     4) Repetition avoidance: If Sargon repeats the position when it thinks
        it's better, loop once more (even if we aren't otherwise looping) in
        a special mode with all repeating moves excluded. Play the new best
        move or fallback if we are worse. Use one lower depth unless we are
        using fixed depth.


     Pseudo code

     States

     ADAPTIVE_NO_TARGET_YET
     ADAPTIVE_WITH_TARGET
     FIXED
     FIXED_WITH_LOOPING
     PLAYING_OUT_MATE_ADAPTIVE
     PLAYING_OUT_MATE_FIXED
     REPEATING_ADAPTIVE
     REPEATING_FIXED
     REPEATING_FIXED_WITH_LOOPING

     if new_game
         calculate initial state
     state_machine_initial
     loop
         aborted,elapsed,mating,pv = run sargon
         if mating
            state = PLAYING_OUT_MATE_ADAPTIVE or PLAYING_OUT_MATE_FIXED
            return mating_move
         ready = state_machine_loop(aborted,elapsed,pv)
         if ready and repeats and we are better
             state = REPEATING_ADAPTIVE or REPEATING_FIXED or REPEATING_FIXED_WITH_LOOPING
             if state == REPEATING_ADAPTIVE
                level--
                clear timer
             not ready
         if ready
            return best move

     state_machine_initial:
     PLAYING_OUT_MATE_ADAPTIVE
        if opponent follows line
            play move and exit
        else
            state = ADAPTIVE_WITH_NO_TARGET_YET fall through
     ADAPTIVE_NO_TARGET_YET
        set cutoff timer to MEDIUM time
        plymax = 1
     PLAYING_OUT_MATE_FIXED
        if opponent follows line
            play move and exit
        else
            state = FIXED_WITH_LOOPING fall through
     FIXED_WITH_LOOPING
        plymax  = 1
        target = FIXED_DEPTH
     ADAPTIVE_WITH_TARGET
        set cutoff timer to HIGH time
        plymax = 1
     FIXED
        plymax = 1,2,3 then FIXED_DEPTH
        (1,2 and 3 are almost instantaneous and will pick up quick mates)
     REPEATING_ADAPTIVE
     REPEATING_FIXED
     REPEATING_FIXED_WITH_LOOPING

     state_machine_loop:
     ADAPTIVE_NO_TARGET_YET
        if aborted
            target = plymax-1
            state = ADAPTIVE_WITH_TARGET
            return ready
        else
            plymax++
            return not ready
     ADAPTIVE_WITH_TARGET
        if aborted
            target = plymax-1
            return ready
        else if plymax < target
            plymax++
            return not ready
        else if plymax >= target && elapsed < lo_timer
            plymax++
            target++
            return not ready
        else
            return ready
     FIXED
        set plymax = 1,2,3 then FIXED_DEPTH
        return ready after FIXED_DEPTH
     FIXED_WITH_LOOPING
        if plymax >= target
            state = FIXED (forcing mate hasn't worked)
            return ready
        else
            plymax++
            return not ready
     REPEATING_ADAPTIVE
        if we are not better
            revert
        state = ADAPTIVE_WITH_TARGET
        return ready
     REPEATING_FIXED
        if we are not better
            revert
        state = FIXED
        return ready
     REPEATING_FIXED_WITH_LOOPING
        if we are not better
            revert
        state = FIXED_WITH_LOOPING
        return ready

*/

// States
enum PlayingState
{
    ADAPTIVE_NO_TARGET_YET,
    ADAPTIVE_WITH_TARGET,
    FIXED,
    FIXED_WITH_LOOPING,
    PLAYING_OUT_MATE_ADAPTIVE,
    PLAYING_OUT_MATE_FIXED,
    REPEATING_ADAPTIVE,
    REPEATING_FIXED,
    REPEATING_FIXED_WITH_LOOPING
};

static void log_state_changes( const std::string &msg, PlayingState old_state, PlayingState state )
{
    const char *txt=NULL, *old_txt=NULL, *new_txt=NULL;
    for( int i=0; i<2; i++ )
    {
        PlayingState temp = (i==0 ? old_state : state );
        switch( temp )
        {
            case ADAPTIVE_NO_TARGET_YET:        txt = "ADAPTIVE_NO_TARGET_YET";       break;
            case ADAPTIVE_WITH_TARGET:          txt = "ADAPTIVE_WITH_TARGET";         break;
            case FIXED:                         txt = "FIXED";                        break;
            case FIXED_WITH_LOOPING:            txt = "FIXED_WITH_LOOPING";           break;
            case PLAYING_OUT_MATE_ADAPTIVE:     txt = "PLAYING_OUT_MATE_ADAPTIVE";    break;
            case PLAYING_OUT_MATE_FIXED:        txt = "PLAYING_OUT_MATE_FIXED";       break;
            case REPEATING_ADAPTIVE:            txt = "REPEATING_ADAPTIVE";           break;
            case REPEATING_FIXED:               txt = "REPEATING_FIXED";              break;
            case REPEATING_FIXED_WITH_LOOPING:  txt = "REPEATING_FIXED_WITH_LOOPING"; break;
        }
        if( i == 0 )
            old_txt = txt;
        else
            new_txt = txt;
    }
    if( state == old_state )
        log( "%s, state = %s\n", msg.c_str(), old_txt );
    else
        log( "%s, state = %s -> %s\n", msg.c_str(), old_txt, new_txt );
}

static thc::Move calculate_next_move( bool new_game, unsigned long ms_time, unsigned long ms_inc, int depth )
{
    // Timers
    const unsigned long LOW  = 100;
    const unsigned long MED  = 30;
    const unsigned long HIGH = 16;
    unsigned long ms_lo  = ms_time / LOW;
    unsigned long ms_med = ms_time / MED;
    unsigned long ms_hi  = ms_time / HIGH;
    bool timer_running = false;

    // States
    static PlayingState state = ADAPTIVE_NO_TARGET_YET;
    PlayingState old_state;

    // Misc
    static int plymax_target;
    int plymax = 1;
    int stalemates = 0;
    unsigned long base = elapsed_milliseconds();
    PV repetition_fallback_pv;
    the_repetition_moves.clear();

    //  if new_game
    //     calculate initial state
    if( depth_option > 0 )
        depth = depth_option;
    if( new_game )
    {
        state = depth>0 ? FIXED : ADAPTIVE_NO_TARGET_YET;
    }

    // else sanity check on states
    // (sadly new_game == false is not entirely reliable - eg if we run
    //  kibiter before starting game new_game will be false, even though
    //  for our purposes it is really true)
    else
    {
        if( depth > 0 )
        {
            switch( state )
            {
                case ADAPTIVE_NO_TARGET_YET:        state = FIXED;  break;
                case ADAPTIVE_WITH_TARGET:          state = FIXED;  break;
                case REPEATING_ADAPTIVE:            state = FIXED;  break;
                case REPEATING_FIXED:               state = FIXED;  break;
                case REPEATING_FIXED_WITH_LOOPING:  state = FIXED;  break;
            }
        }
        else
        {
            switch( state )
            {
                case FIXED:                         state = ADAPTIVE_NO_TARGET_YET; break;
                case FIXED_WITH_LOOPING:            state = ADAPTIVE_NO_TARGET_YET; break;
                case REPEATING_ADAPTIVE:            state = ADAPTIVE_NO_TARGET_YET; break;
                case REPEATING_FIXED:               state = ADAPTIVE_NO_TARGET_YET; break;
                case REPEATING_FIXED_WITH_LOOPING:  state = ADAPTIVE_NO_TARGET_YET; break;
            }
        }
    }

    // Initial state machine
    old_state = state;
    switch( state )
    {
        //  PLAYING_OUT_MATE_ADAPTIVE
        //      if opponent follows line
        //          play move and exit
        //      else
        //          state = ADAPTIVE_WITH_NO_TARGET_YET fall through
        //  PLAYING_OUT_MATE_FIXED
        //      if opponent follows line
        //          play move and exit
        //      else
        //          state = FIXED_WITH_LOOPING fall through
        case PLAYING_OUT_MATE_ADAPTIVE:
        case PLAYING_OUT_MATE_FIXED:
        {
            // Play out a mating sequence
            bool opponent_follows_line = false;
            if( mating.active && mating.idx+2 < mating.variation.size() )
            {
                mating.position.PlayMove(mating.variation[mating.idx++]);
                mating.position.PlayMove(mating.variation[mating.idx++]);
                if( mating.position == the_position )
                    opponent_follows_line = true;
            }
            if( opponent_follows_line )
            {
                thc::ChessRules cr = mating.position;
                thc::Move mating_move = mating.variation[mating.idx];
                std::string buf_pv;
                for( unsigned int i=0; mating.idx+i<mating.variation.size(); i++ )
                {
                    thc::Move move=mating.variation[mating.idx+i];
                    cr.PlayMove( move );
                    buf_pv += " ";
                    buf_pv += move.TerseOut();
                }
                std::string out = util::sprintf( "info score mate %d pv%s\n",
                    --mating.nbr,
                    buf_pv.c_str() );
                if( mating.nbr <= 1 )
                {
                    mating.active = false;
                    state = (state==PLAYING_OUT_MATE_ADAPTIVE ? ADAPTIVE_NO_TARGET_YET : FIXED);
                }
                if(!gbl_interactive)
                {
                    fprintf( stdout, out.c_str() );
                    fflush( stdout );
                }
                log( "rsp>%s\n", out.c_str() );
                log( "(%s mating line)\n", mating.nbr<=1 ? "Finishing" : "Continuing" );
                stop_rsp = util::sprintf( "bestmove %s\n", mating_move.TerseOut().c_str() ); 
                return mating_move;
            }
            else
            {
                // If the opponent chooses an alternative path, we still have forced mate (if we trust
                //  Sargon's fundamental chess algorithms), so used FIXED_WITH_LOOPING rather than
                //  FIXED, in order to find it quickly and efficiently (this is why we have
                //  FIXED_WITH_LOOPING)
                mating.active = false;
                state = (state==PLAYING_OUT_MATE_ADAPTIVE ? ADAPTIVE_NO_TARGET_YET : FIXED_WITH_LOOPING);

                // Simulate fall through to ADAPTIVE_NO_TARGET_YET / FIXED_WITH_LOOPING
                if( state == ADAPTIVE_NO_TARGET_YET )
                {
                    timer_set( ms_med );
                    timer_running = true;
                    plymax = 1;
                }
                else // if( state == FIXED_WITH_LOOPING )
                {
                    plymax_target = depth;
                    plymax  = 1;
                }
            }
            break;
        }

        //  ADAPTIVE_NO_TARGET_YET
        //      set cutoff timer to MEDIUM time
        //      plymax = 1
        case ADAPTIVE_NO_TARGET_YET:
        {
            timer_set( ms_med );
            timer_running = true;
            plymax = 1;
            break;
        }

        //  ADAPTIVE_WITH_TARGET
        //      set cutoff timer to HIGH time
        //      plymax = 1
        case ADAPTIVE_WITH_TARGET:
        {
            timer_set( ms_hi );
            timer_running = true;
            plymax = 1;
            break;
        }

        //  FIXED_WITH_LOOPING
        //      plymax  = 1
        //      target = FIXED_DEPTH
        case FIXED_WITH_LOOPING:
        {
            plymax = 1;
            plymax_target = depth;
            break;
        }

        //  FIXED
        //      plymax = 1,2,3 then FIXED_DEPTH
        //      (1,2 and 3 are almost instantaneous and will pick up quick mates)
        case FIXED:
        {
            plymax = 1;
            break;
        }
    }   // end switch
    log_state_changes( "Initial state machine:", old_state, state );

    //  loop
    //      aborted,elapsed,mating,pv = run sargon
    bool we_are_stalemating_now = false;
    //bool just_once = true;
    for(;;)
    {
        bool aborted = run_sargon(plymax,false);
        unsigned long now = elapsed_milliseconds();
        unsigned long elapsed = (now-base);

        // The special case, where Sargon minimax never ran should only be book move
        if( the_pv.variation.size() == 0 )    
        {
            std::string bestmove_terse = sargon_export_best_move_temp();
            thc::Move bestmove;
            bestmove.Invalid();
            bool have_move = bestmove.TerseIn( &the_position, bestmove_terse.c_str() );
            if( have_move )
                log( "No PV, expect Sargon is playing book move - %s\n%s", bestmove_terse.c_str(), the_position.ToDebugStr().c_str() );
            else
                log( "Sargon doesn't find move - %s\n%s", bestmove_terse.c_str(), the_position.ToDebugStr().c_str() );
            stop_rsp = util::sprintf( "bestmove %s\n", bestmove_terse.c_str() );
            if( timer_running )
                timer_clear();
            return bestmove;
        }

        // Report on each normally concluded iteration
        bool we_are_forcing_mate = false;
        std::string info;
        if( aborted )
        {
            log( "aborted=%s, elapsed=%lu, ms_lo=%lu, plymax=%d, plymax_target=%d\n",
                    aborted?"true @@":"false", //@@ marks move in log
                    elapsed, ms_lo, plymax, plymax_target );
        }
        else
        {
            std::string s = the_pv.variation[0].TerseOut();
            std::string bestm = sargon_export_best_move_temp();
            if( s.substr(0,4) != bestm )
                log( "Unexpected event: BESTM=%s != PV[0]=%s\n%s", bestm.c_str(), s.c_str(), the_position.ToDebugStr().c_str() );
            info = generate_progress_report( we_are_forcing_mate, we_are_stalemating_now );
            bool repeating = (state==REPEATING_ADAPTIVE || state==REPEATING_FIXED || state==REPEATING_FIXED_WITH_LOOPING);
            if( (!repeating||we_are_forcing_mate) && info.length() > 0 )
            {
                if( !gbl_interactive )
                {
                    fprintf( stdout, info.c_str() );
                    fflush( stdout );
                }
                log( "rsp>%s\n", info.c_str() );
                stop_rsp = util::sprintf( "bestmove %s\n", the_pv.variation[0].TerseOut().c_str() ); 
                info.clear();
            }
        }

        //  if mating
        //     state = PLAYING_OUT_MATE_ADAPTIVE or PLAYING_OUT_MATE_FIXED
        //     return mating_move
        if( we_are_forcing_mate )
        {
            if( mating.variation.size() == 1 )
            {
                log( "(Immediate mate in one available, play it)\n" );
                mating.active = false;
            }
            else
            {
                log( "(Starting mating line)\n" );
                switch(state)
                {
                    case ADAPTIVE_NO_TARGET_YET:        state = PLAYING_OUT_MATE_ADAPTIVE;  break;
                    case ADAPTIVE_WITH_TARGET:          state = PLAYING_OUT_MATE_ADAPTIVE;  break;
                    case FIXED:                         state = PLAYING_OUT_MATE_FIXED;     break;
                    case FIXED_WITH_LOOPING:            state = PLAYING_OUT_MATE_FIXED;     break;
                    case PLAYING_OUT_MATE_ADAPTIVE:     state = PLAYING_OUT_MATE_ADAPTIVE;  break;
                    case PLAYING_OUT_MATE_FIXED:        state = PLAYING_OUT_MATE_FIXED;     break;
                    case REPEATING_ADAPTIVE:            state = PLAYING_OUT_MATE_ADAPTIVE;  break;
                    case REPEATING_FIXED:               state = PLAYING_OUT_MATE_FIXED;     break;
                    case REPEATING_FIXED_WITH_LOOPING:  state = PLAYING_OUT_MATE_FIXED;     break;
                }
            }
            if( timer_running )
                timer_clear();
            return mating.variation[mating.idx];
        }

        // ready = state_machine_loop(aborted,elapsed,pv)
        bool ready = false;
        bool after_repetition_avoidance = false;

        // Loop state machine
        old_state = state;
        switch(state)
        {
            //  ADAPTIVE_NO_TARGET_YET
            //      if aborted
            //          target = plymax-1
            //          state = ADAPTIVE_WITH_TARGET
            //          return ready
            //      else
            //          plymax++
            //          return not ready
            case ADAPTIVE_NO_TARGET_YET:
            {
                if( aborted )
                {
                    plymax_target = (plymax>=2 ? plymax-1 : 1);
                    state  = ADAPTIVE_WITH_TARGET;
                    ready = true;
                }
                else
                {
                    plymax++;
                }
                break;
            }

            //  ADAPTIVE_WITH_TARGET
            //      if aborted
            //          target = plymax-1
            //          return ready
            //      else if plymax < target
            //          plymax++
            //          return not ready
            //      else if plymax >= target && elapsed < lo_timer
            //          plymax++
            //          target++
            //          return not ready
            //      else
            //          return ready
            case ADAPTIVE_WITH_TARGET:          
            {
                if( !aborted && plymax>=plymax_target )
                    log( "Reached adaptive target, increase target if elapsed=%lu < ms_lo=%lu\n", elapsed, ms_lo );
                if( aborted )
                {
                    plymax_target = (plymax>=2 ? plymax-1 : 1);
                    ready = true;
                }
                else if( plymax < plymax_target )
                {
                    plymax++;
                }
                else if( plymax>=plymax_target && elapsed<ms_lo )
                {
                    plymax++;
                    plymax_target++;
                    log( "Increase adaptive target\n" );
                }
                //else if( plymax>=plymax_target && we_are_stalemating_now && just_once )
                //{
                //    just_once = false;
                //    plymax++;  // try one higher depth if we are stalemating
                //}
                else
                {
                    ready = true;
                }
                break;
            }

            //  FIXED
            //      set plymax = 1,2,3 then FIXED_DEPTH
            //      return ready after FIXED_DEPTH
            case FIXED:                         
            {
                if( 1<=plymax && plymax<3 && plymax<depth )
                {
                    plymax++;
                }
                else if( plymax < depth )
                {
                    plymax = depth;
                }
                else
                {
                    ready = true;
                }
                break;
            }

            //  FIXED_WITH_LOOPING
            //      if plymax >= target
            //          state = FIXED (forcing mate hasn't worked)
            //          return ready
            //      else
            //          plymax++
            //          return not ready
            case FIXED_WITH_LOOPING:            
            {
                if( plymax >= plymax_target )
                {
                    state = FIXED;
                    ready = true;
                }
                else
                {
                    plymax++;
                }
                break;
            }
            case PLAYING_OUT_MATE_ADAPTIVE:     
            {
                break;
            }
            case PLAYING_OUT_MATE_FIXED:        
            {
                break;
            }

            //  REPEATING_ADAPTIVE
            //  REPEATING_FIXED
            //  REPEATING_FIXED_WITH_LOOPING
            //      if we are not better
            //          revert
            //      state = ADAPTIVE_WITH_TARGET / FIXED / FIXED_WITH_LOOPING
            //      return ready
            case REPEATING_ADAPTIVE:            
            case REPEATING_FIXED:               
            case REPEATING_FIXED_WITH_LOOPING:  
            {
                // If attempts to avoid repetition leave us worse, fall back
                after_repetition_avoidance = true;
                bool fallback=false;
                int score_after_repetition_avoid = the_pv.value;
                if( the_position.white )
                    fallback = (score_after_repetition_avoid <= 0);
                else
                    fallback = (score_after_repetition_avoid >= 0);
                if( aborted )
                    fallback = true;
                if( fallback )
                    the_pv = repetition_fallback_pv;
                else if( info.length() > 0 )
                {
                    if(!gbl_interactive)
                    {
                        fprintf( stdout, info.c_str() );
                        fflush( stdout );
                    }
                    log( "rsp>%s\n", info.c_str() );
                    stop_rsp = util::sprintf( "bestmove %s\n", the_pv.variation[0].TerseOut().c_str() ); 
                    info.clear();
                }
                log( "After repetition avoidance, new value=%d, so %s\n", score_after_repetition_avoid, fallback ? "did fallback" : "didn't fallback" );
                ready = true;
                switch(state)
                {
                    case REPEATING_ADAPTIVE:            state = ADAPTIVE_WITH_TARGET;  break;
                    case REPEATING_FIXED:               state = FIXED;                 break;
                    case REPEATING_FIXED_WITH_LOOPING:  state = FIXED_WITH_LOOPING;    break;
                }
                break;
            }
        }
        log_state_changes( "Loop state machine:", old_state, state );


        //  if ready and repeats and we are better
        //      state = REPEATING_ADAPTIVE or REPEATING_FIXED or REPEATING_FIXED_WITH_LOOPING
        //      if state == REPEATING_ADAPTIVE
        //         level--
        //         clear timer
        //      not ready
        // If we are about to play move, and we're better; consider whether to kick in repetition avoidance
        if( !after_repetition_avoidance && ready && (the_position.white? the_pv.value>0 : the_pv.value<0) )
        {
            thc::Move mv = the_pv.variation[0];
            if( test_whether_move_repeats(the_position,mv) )
            {
                log( "Repetition avoidance, %s repeats\n", mv.TerseOut().c_str() );
                bool ok = repetition_calculate( the_position, the_repetition_moves );
                if( !ok )
                {
                    the_repetition_moves.clear();   // don't do repetition avoidance - all moves repeat
                }
                else
                {
                    repetition_fallback_pv = the_pv;
                    switch(state)
                    {
                        case ADAPTIVE_NO_TARGET_YET:        state = REPEATING_ADAPTIVE;             break;
                        case ADAPTIVE_WITH_TARGET:          state = REPEATING_ADAPTIVE;             break;
                        case FIXED:                         state = REPEATING_FIXED;                break;
                        case FIXED_WITH_LOOPING:            state = REPEATING_FIXED_WITH_LOOPING;   break;
                        case PLAYING_OUT_MATE_ADAPTIVE:     state = REPEATING_ADAPTIVE;             break;
                        case PLAYING_OUT_MATE_FIXED:        state = REPEATING_FIXED;                break;
                        case REPEATING_ADAPTIVE:            state = REPEATING_ADAPTIVE;             break;
                        case REPEATING_FIXED:               state = REPEATING_FIXED;                break;
                        case REPEATING_FIXED_WITH_LOOPING:  state = REPEATING_FIXED_WITH_LOOPING;   break;
                    }
                    if( state == REPEATING_ADAPTIVE )
                    {
                        if( plymax > 1 )
                            plymax--;
                        if( timer_running )
                        {
                            timer_running = false;
                            timer_clear();
                        }
                    }
                    ready = false;
                }
            }
        }

        //  if ready
        //     return best move
        if( ready )
            break;
    }
    thc::Move mv = the_pv.variation[0];
    if( timer_running )
        timer_clear();
    return mv;
}

#if 0
static thc::Move calculate_next_move( bool new_game, unsigned long ms_time, unsigned long ms_inc, int depth )
{
    // Play out a mating sequence
    if( mating.active && mating.idx+2 < mating.variation.size() )
    {
        mating.position.PlayMove(mating.variation[mating.idx++]);
        mating.position.PlayMove(mating.variation[mating.idx++]);
        if( mating.position != the_position )
            mating.active = false;
        else
        {
            thc::ChessRules cr = mating.position;
            thc::Move mating_move = mating.variation[mating.idx];
            std::string buf_pv;
            for( unsigned int i=0; mating.idx+i<mating.variation.size(); i++ )
            {
                thc::Move move=mating.variation[mating.idx+i];
                cr.PlayMove( move );
                buf_pv += " ";
                buf_pv += move.TerseOut();
            }
            std::string out = util::sprintf( "info score mate %d pv%s\n",
                --mating.nbr,
                buf_pv.c_str() );
            fprintf( stdout, out.c_str() );
            fflush( stdout );
            log( "rsp>%s\n", out.c_str() );
            stop_rsp = util::sprintf( "bestmove %s\n", mating_move.TerseOut().c_str() ); 
            return mating_move;
        }
    }

    // If not playing out a mate, start normal algorithm
    log( "Input ms_time=%lu, ms_inc=%lu, depth=%d\n", ms_time, ms_inc, depth );
    static int plymax_target;
    unsigned long ms_lo=0;
    bool go_straight_to_fixed_depth = false;
    the_repetition_moves.clear();

    // There are multiple reasons why we would run fixed depth, without a timer
    if( depth == 0 )
    {
        if( depth_option > 0 )
        {
            depth = depth_option;
            go_straight_to_fixed_depth = true;  // Only this way do we go straight to
                                                //  specified depth without iterating
        }
        else if( ms_time == 0 )
            depth = 3;  // Set plymax=3 as a baseline, it's more or less instant
    }
    bool fixed_depth = (depth>0);
    if( fixed_depth )
        timer_clear();

    // Otherwise set a cutoff timer
    else
    {
        const unsigned long LOW =100;
        const unsigned long MED=30;
        const unsigned long HIGH =16;
        ms_lo  = ms_time / LOW;
        unsigned long ms_med = ms_time / MED;
        unsigned long ms_hi  = ms_time / HIGH;


        // Use the cut off timer, with a medium cutoff if we haven't yet
        //  established a target plymax
        if( new_game || plymax_target == 0 )
        {
            plymax_target = 0;
            timer_set( ms_med );
        }

        // Else the cut off timer is more of an emergency brake, and normally
        //  we just re-run Sargon until we hit plymax_target
        else
        {
            timer_set( ms_hi );
        }
    }
    int plymax = 1;     // V1.00 Set plymax=3 as a baseline, it's more or less instant
                        // V1.01 Start from plymax=1 to support ultra bullet for example
    if( fixed_depth )
    {
        plymax = depth;  // It's more efficient but less informative (no progress reports)
                         //  to go straight to the final depth without iterating
    }
    int stalemates = 0;
    std::string bestmove_terse;
    unsigned long base = elapsed_milliseconds();
    bool repetition_avoid = false;
    PV repetition_fallback_pv;
    for(;;)
    {
        bool aborted = run_sargon(plymax,false);
        unsigned long now = elapsed_milliseconds();
        unsigned long elapsed = (now-base);
        if( aborted  || the_pv.variation.size()==0 )
        {
            log( "aborted=%s, pv.variation.size()=%d, elapsed=%lu, ms_lo=%lu, plymax=%d, plymax_target=%d\n",
                    aborted?"true @@":"false", the_pv.variation.size(), //@@ marks move in log
                    elapsed, ms_lo, plymax, plymax_target );
        }

        // Report on each normally concluded iteration
        if( !aborted && !repetition_avoid )
            generate_progress_report();

        // Check for situations where Sargon minimax never ran
        if( the_pv.variation.size() == 0 )    
        {
            // maybe book move
            bestmove_terse = sargon_export_best_move_temp();
            plymax = 0; // to set plymax_target for next time
            break;
        }
        else
        {

            // If attempts to avoid repetition leave us worse, fall back
            if( repetition_avoid && !aborted )
            {
                bool fallback=false;
                int score_after_repetition_avoid = the_pv.value;
                if( the_position.white )
                    fallback = (score_after_repetition_avoid <= 0);
                else
                    fallback = (score_after_repetition_avoid >= 0);
                if( fallback )
                    the_pv = repetition_fallback_pv;
                else
                    generate_progress_report();
                log( "After repetition avoidance, new value=%d, so %s\n", score_after_repetition_avoid, fallback ? "did fallback" : "didn't fallback" );
            }

            // Best move found
            bestmove_terse = the_pv.variation[0].TerseOut();
            std::string bestm = sargon_export_best_move_temp();
            if( !aborted && bestmove_terse.substr(0,4) != bestm )
                log( "Unexpected event: BESTM=%s != PV[0]=%s\n%s", bestm.c_str(), bestmove_terse.c_str(), the_position.ToDebugStr().c_str() );

            // Repetition avoidance always ends looping
            if( repetition_avoid )
            {
                plymax++;   // repetition avoidance run used decremented plymax, restore it
                break;
            }

            // If we have a move, and it checkmates opponent, don't iterate further!
            thc::TERMINAL score_terminal;
            thc::ChessRules ce = the_position;
            ce.PlayMove(the_pv.variation[0]);
            bool okay = ce.Evaluate( score_terminal );
            if( okay )
            {
                if( score_terminal == thc::TERMINAL_BCHECKMATE ||
                    score_terminal == thc::TERMINAL_WCHECKMATE )
                {
                    break;  // We're done, play the move to checkmate opponent
                }
                if( score_terminal == thc::TERMINAL_BSTALEMATE ||
                    score_terminal == thc::TERMINAL_WSTALEMATE )
                {
                    stalemates++;
                }
            }

            // If we timed out, target plymax should be reduced
            if( aborted )
            {
                plymax--;
                break;
            }

            // Otherwise keep iterating or not
            bool keep_going = false;
            if( fixed_depth )
            {
                // If fixed_depth, iterate until required depth
                if( plymax < depth )
                    keep_going = true;  // haven't reached required depth
            }
            else
            {
                // If not fixed_depth, according to the time management algorithm
                if( plymax_target<=0 || plymax<plymax_target )
                    keep_going = true;  // no target or haven't reached target
                else if( plymax_target>0 && plymax>=plymax_target && elapsed<ms_lo )
                    keep_going = true;  // reached target very quickly, so extend target
                else if( stalemates == 1 )
                    keep_going = true;  // try one more ply if we stalemate opponent!
                log( "elapsed=%lu, ms_lo=%lu, plymax=%d, plymax_target=%d, keep_going=%s\n", elapsed, ms_lo, plymax, plymax_target, keep_going?"true":"false @@" );  // @@ marks move in log
            }

            // If we are about to play move, and we're better; consider whether to kick in repetition avoidance
            if( !keep_going && (the_position.white? the_pv.value>0 : the_pv.value<0) )
            {
                thc::Move mv = the_pv.variation[0];
                if( test_whether_move_repeats(the_position,mv) )
                {
                    log( "Repetition avoidance, %s repeats\n", bestmove_terse.c_str() );
                    repetition_calculate( the_position, the_repetition_moves );
                    repetition_avoid = true;
                    repetition_fallback_pv = the_pv;
                    keep_going = true;
                    timer_clear();
                }
            }

            // Either keep going or end now
            if( !keep_going )
                break;  // end now
            else
            {
                if( !fixed_depth )
                {
                    if( !repetition_avoid )
                        plymax++;   // normal iteration
                    else
                    {
                        // have already used a full quota of time, so use less for the recalc
                        if( plymax > 0 )
                            plymax--; 
                    }
                }
            }
        }
    }
    if( !fixed_depth )
    {
        timer_clear();
        plymax_target = plymax;
    }
    thc::Move bestmove;
    bestmove.Invalid();
    bool have_move = bestmove.TerseIn( &the_position, bestmove_terse.c_str() );
    if( !have_move )
        log( "Sargon doesn't find move - %s\n%s", bestmove_terse.c_str(), the_position.ToDebugStr().c_str() );
    return bestmove;
}
#endif

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

// Calculate a list of moves that cause the position to repeat
//  Returns bool ok. If not ok, all moves repeat
static bool repetition_calculate( thc::ChessRules &cr, std::vector<thc::Move> &repetition_moves )
{
    bool ok=false;
    repetition_moves.clear();
    std::vector<thc::Move> v;
    cr.GenLegalMoveList(v);
    for( thc::Move mv: v )
    {
        if( test_whether_move_repeats(cr,mv) )
            repetition_moves.push_back(mv);
        else
            ok = true;
    }
    return ok;
}

static bool test_whether_move_repeats( thc::ChessRules &cr, thc::Move mv )
{
    // Unfortunately we can't PushMove() / PopMove() because PushMove()
    //  doesn't update the history buffer and the counts. PlayMove() does
    //  but we have to take care to restore the history buffer and the
    //  counts so that the change is temporary.
    // The need for this complication is a bit of a flaw in our thc
    //  library unfortunately and even needed a change; history_idx from
    //  protected to public.
    int restore_history = cr.history_idx;
    int restore_full    = cr.full_move_count;
    int restore_half    = cr.half_move_clock;
    cr.PlayMove(mv);
    int repetition_count = cr.GetRepetitionCount();
    cr.PopMove(mv);
    cr.history_idx      = restore_history;
    cr.full_move_count  = restore_full;
    cr.half_move_clock  = restore_half;
    return (repetition_count > 1);
}

// Returns book okay
static bool repetition_test()
{
    ML *mlist = m.MLIST;

    // Test function repetition_calculate()
    //  After 1. Nf3 Nf6 2. Ng1 the move 2... Nf6-g8 repeats the initial position
    thc::ChessRules cr;
    thc::Move mv;
    mv.TerseIn(&cr,"g1f3");
    cr.PlayMove(mv);
    mv.TerseIn(&cr,"g8f6");
    cr.PlayMove(mv);
    mv.TerseIn(&cr,"f3g1");
    cr.PlayMove(mv);
    std::vector<thc::Move> w;
    repetition_calculate(cr,w);
    bool ok = true;
    if( w.size() != 1 )
        ok = false;
    else
    {
        if( w[0].src != thc::f6 )
            ok = false;
        if( w[0].dst != thc::g8 )
            ok = false;
    }

    // Non destructive test of function  repetition_remove_moves()
    ML *plyix = m.PLYIX[0].link_ptr;
    ML *mllst = m.MLLST;
    ML *mlnxt = m.MLNXT;
    const uint8_t *src = (const uint8_t *)&m.MLIST[0];
    unsigned char buf[sizeof(ML)*3];
    memcpy(buf,src,sizeof(buf));

    // Write 2 candidate moves into Sargon, with ptrs; First move is f3e5
    m.PLYIX[0].link_ptr = mlist;
    ML *p       = mlist;
    p->link_ptr = p+1; 
    p->from     = SQ_f3;
    p->to       = SQ_e5;
    p->flags    = 0;
    p->val      = 0;
    m.MLLST     = mlist+1;

    // O-O
    p->link_ptr = 0;
    p->from     = SQ_e1;
    p->to       = SQ_g1;
    p->flags    = 0x40;
    p->val      = 0;

    // O-O second move
    p->link_ptr = 0;
    p->from     = SQ_h1;
    p->to       = SQ_f1;
    p->flags    = 0;
    p->val      = 0;
    m.MLNXT = mlist+3;
    //sargon_show();

    // Remove f3e5
    std::vector<thc::Move> v;
    mv.src = thc::f3;
    mv.dst = thc::e5;
    v.push_back(mv);
    repetition_remove_moves(v);
    //sargon_show();

    // Check whether it matches our expectations
    if( mlist != m.MLLST )
        ok = false;
    if( mlist+2 != m.MLNXT )
        ok = false;
    const ML *q = (const ML *)&m.MLIST;
    if( q->link_ptr != 0 )
        ok = false;
    if( q->from != SQ_e1 )
        ok = false;
    if( q->to != SQ_g1 )
        ok = false;
    if( q->flags != 0x40 )
        ok = false;
    if( q->val != 0 )
        ok = false;
    q++;
    if( q->link_ptr != 0 )
        ok = false;
    if( q->from != SQ_h1 )
        ok = false;
    if( q->to != SQ_f1 )
        ok = false;
    if( q->flags != 0 )
        ok = false;
    if( q->val != 0 )
        ok = false;

    // Undo all changes
    uint8_t *dst = (uint8_t *)mlist;
    memcpy(dst,buf,sizeof(buf));
    m.PLYIX->link_ptr = plyix;
    m.MLLST    = mllst;
    m.MLNXT    = mlnxt;
    return ok;
}

// Remove candidate moves that will cause the position to repeat
static void repetition_remove_moves(  const std::vector<thc::Move> &repetition_moves  )
{
    //sargon_show();
    ML *mlist = m.MLIST;

    // Locate the list of candidate moves (ptr ends up being 0x400=mlist always)
    ML *bin_ptr = m.PLYIX->link_ptr;

    // Read a vector of NativeMove
    ML *mlnxt = m.MLNXT;
    if( bin_ptr!=mlist || mlnxt<=bin_ptr || ((mlnxt-bin_ptr)>250) )
        return; // sanity checks
    std::vector<ML> vin;
    while( bin_ptr < mlnxt )
    {
        ML *p = bin_ptr;
        ML nm;
        nm.link_ptr =   p->link_ptr;
        nm.from = p->from;
        nm.to = p->to;
        nm.flags =      p->flags;
        nm.val =      p->val;
        vin.push_back(nm);
        bin_ptr += sizeof(ML);
    }

    // Create an edited (reduced) vector
    std::vector<ML> vout;
    bool second_byte=false;
    bool copy_move_and_second_byte_if_present = true;
    for( ML nm: vin )
    {
        if( second_byte )
            second_byte = false;
        else
        {
            if( nm.flags & 0x40 )
                second_byte = true;
            thc::Square src, dst;
            copy_move_and_second_byte_if_present = true;
            if( sargon_export_square(nm.from,src) && sargon_export_square(nm.to,dst) )
            {
                for( thc::Move mv: repetition_moves )
                {
                    if( mv.src==src && mv.dst==dst )
                    {
                        copy_move_and_second_byte_if_present = false;
                        break;
                    }
                }
            }
        }
        if( copy_move_and_second_byte_if_present )
            vout.push_back(nm);
    }

    // Fixup ptr fields
    bin_ptr = m.PLYIX->link_ptr;
    ML *ptr_final_move = bin_ptr;
    ML *ptr_end = bin_ptr + vout.size();
    second_byte=false;
    for( ML &nm: vout )
    {
        if( second_byte )
        {
            second_byte = false;
            nm.link_ptr = 0;
        }
        else
        {
            if( nm.flags & 0x40 )
                second_byte = true;
            ptr_final_move = bin_ptr;
            ML *ptr_next = (second_byte ? bin_ptr+2 : bin_ptr+1 );
            if( ptr_next == ptr_end )
                ptr_next = 0;
            nm.link_ptr = ptr_next;
        }
        bin_ptr++;
    }

    // Write vector back
    if( vout.size() )  // but if no moves left, make no changes
    {
        m.MLLST = ptr_final_move;
        m.MLNXT = ptr_end;
        ML *p   = m.PLYIX->link_ptr;
        for( ML nm: vout )
            *p++ = nm;
    }
    //sargon_show();
}

//
// UCI specific callback handling (minimal)
//

// Called by callback_after_genmove()
void callback_uci_after_genmove()
{
    total_callbacks++;
    genmov_callbacks++;
    if( m.NPLY==1 && the_repetition_moves.size()>0 )
        repetition_remove_moves( the_repetition_moves );
}

// Called by callback_end_of_points()
void callback_uci_end_of_points()
{
    total_callbacks++;
    end_of_points_callbacks++;

    // sargon_pv_callback_end_of_points() is
    // now called by callback_end_of_points()

    // Need to call the following regularly, but no need for all three
    // routines to do it, so now just do it at end of points

    // Abort run_sargon() if new event in queue (and not PLYMAX==1 which is
    //  effectively instantaneous, finds a baseline move)
    if( !async_queue.empty() && m.PLYMAX>1 )
    {
        longjmp( jmp_buf_env, 1 );
    }
}

// Called by callback_yes_best_move()
void callback_uci_yes_best_move()
{
    total_callbacks++;
    bestmove_callbacks++;

    // sargon_pv_callback_yes_best_move() is
    // now called by by callback_yes_best_move()
}

