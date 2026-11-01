/****************************************************************************
 * This project is a Windows port of the classic program Sargon, as
 * presented in the book "Sargon a Z80 Computer Chess Program" by Dan
 * and Kathe Spracklen (Hayden Books 1978).
 *
 * File: sargon-pv.cpp
 *       Sargon PV (Principal Variation) Calculation
 *
 * Bill Forster, https://github.com/billforsternz/retro-sargon
 ****************************************************************************/

#include <string>
#include <vector>
#include "util.h"
#include "thc.h"
#include "sargon-interface.h"
#include "sargon-pv.h"
#include "zargon.h"
#include "zargon_functions.h"

// Sargon data structure
static Zargon &z = gbl_zargon;

// A move in Sargon's evaluation graph, in this program a move that is marked as
//  the best move found so far at a given level
struct NODE
{
    uint8_t  from;
    uint8_t  to;
    NODE() : from(0), to(0) {}
    NODE( uint8_t f, uint8_t t ) :
                from(f), to(t) {}
};


/*

    New improved PV algorithm

    This algorithm is much improved on our original potentially
    unbounded vector algorithm. More in the spirit of Sargon,
    we use a single static allocation. Although we do need a
    two dimensional array, around 1K bytes. (But see refinement
    below - maybe 500 bytes instead :)

    Store best moves (just from,to) in a 2D array, eg store a ply 4
    move like this (maybe PLYMAX is 3 and 4 is as deep as we go);

           0      1      2      3      4
       +------+------+------+------+------+
    0  |      |      |      |      |      |
       +------+------+------+------+------+
    1  |      |      |      |      |      |
       +------+------+------+------+------+
    2  |      |      |      |      |      |
       +------+------+------+------+------+
    3  |      |      |      |      |      |
       +------+------+------+------+------+
    4  |      |      |      |      | mv4  |
       +------+------+------+------+------+

    Later store a ply 3 move like this;

           0      1      2      3      4
       +------+------+------+------+------+
    0  |      |      |      |      |      |
       +------+------+------+------+------+
    1  |      |      |      |      |      |
       +------+------+------+------+------+
    2  |      |      |      |      |      |
       +------+------+------+------+------+
    3  |      |      |      | mv3  |      |
       +------+------+------+------+------+
    4  |      |      |      |      | mv4  |
       +------+------+------+------+------+


    But then pull up mv4, in fact the complete
    continuation, from the row below. Each row
    ends up containing a complete continuation;

           0      1      2      3      4
       +------+------+------+------+------+
    0  |      |      |      |      |      |
       +------+------+------+------+------+
    1  |      |      |      |      |      |
       +------+------+------+------+------+
    2  |      |      |      |      |      |
       +------+------+------+------+------+
    3  |      |      |      | mv3  | mv4  |
       +------+------+------+------+-^----+
    4  |      |      |      |      | |mv4 |
       +------+------+------+------+------+

    When we have a ply 1 best move we pull up
    the complete PV continuation from ply 2
    onwards from the row below;

           0      1      2      3      4
       +------+------+------+------+------+
    0  |      |      |      |      |      |
       +------+------+------+------+------+
    1  |      |  mv1 |  mv2 |  mv3 |  mv4 |
       +------+------+-^----+-^----+-^----+
    2  |      |      | |mv2 | |mv3 | |mv4 |
       +------+------+------+------+------+
    3  |      |      |      | mv3  | mv4  |
       +------+------+------+------+------+
    4  |      |      |      |      | mv4  |
       +------+------+------+------+------+

Refinement, unroll into a one dimensional array to
avoid wasting half of the memory (the bottom left
triangle) of the two dimensional array.

Let
    N = length of maximum variation = MAX_DEPTH+1, 
    N also = maximum depth (one more than MAX_DEPTH because we go
             one ply deeper if check)
    x = depth 1..N inclusive
    y = storage offset for that depth
         x=1 => y=0
         x=2 => y=N
         x=3 => y=2N-1
         x=4 => y=3N-3
         x=5 => y=4N-6
         x=6 => y=5N-10 etc.
    sz = y=f(x)+1 for x=N (we store 1 node at max depth)

Offset formula is;
    y = f(x) = (x-1)*N - (x-1)*(x-2)/2

Example N = 10
    x   y              f(x)
    1   0              0  - 0*-1/2  = 0
    2   10 = N         10 - 1*0/2   = 10
    3   19 = 2N - 1    2*10 - 2*1/2 = 19
    4   27 = 3N - 3    3*10 - 3*2/2 = 27
    5   34 = 4N - 6    4*10 - 4*3/2 = 34
    6   40 = 5N - 10   5*10 - 5*4/2 = 40
    7   45 = 6N - 15   6*10 - 6*5/2 = 45
    ...
*/

//
//  Build Sargon's PV (Principal Variation)
//

//  f(x) = (x-1)*N - (x-1)*(x-2)/2
//  sz   = f(x)+1 for x=N (we store 1 move at depth N)
//       = (N-1)*N - (N-1)*(N-2)/2 + 1
//       N = MAX_DEPTH+1
static NODE pv_history[ MAX_DEPTH*(MAX_DEPTH+1) - (MAX_DEPTH*(MAX_DEPTH-1))/2 + 1 + ALLOCATION_PADDING ];
static thc::ChessRules pv_base_position;
static int lookup_table[ (MAX_DEPTH+1) + ALLOCATION_PADDING];

void sargon_pv_clear( const thc::ChessPosition &current_position )
{
    memset( pv_history, 0, sizeof(pv_history) );
    pv_base_position = current_position;
    int N = (int)z.PLYMAX+1;
    for( int x=1; x<=N; x++ )
    {
        int offset = (x-1)*N - ((x-1)*(x-2))/2;
        lookup_table[x] = offset;
    }
}

void sargon_pv_callback_yes_best_move()
{
    int x = (int)z.NPLY;
    int N = (int)z.PLYMAX+1;
    if( x<1 || x>N )
        return;
    int offset = //(x-1)*N - ((x-1)*(x-2))/2;
                lookup_table[x];
    int len    = N + 1 - x;     // len must be N for x=1, then 1
                                //  less for each successive x
                                //  and l for x=N. len is the nbr
                                //  of moves stored at level x

    // The current move is copied to the destination
    NODE *dst_node = &pv_history[offset];
    dst_node->from = z.MLPTRJ->from;
    dst_node->to   = z.MLPTRJ->to;

    // All moves one level down are appended after that
    uint8_t *dst = (uint8_t *)(dst_node+1);
    uint8_t *src = (uint8_t *)&pv_history[offset+len];
    size_t sz = src-dst;
    memcpy( dst, src, sz );
}

PV sargon_pv_get()
{
    PV pv;
    thc::ChessRules cr = pv_base_position;
    pv.variation.clear();
    uint8_t last_move_to = SQ_a1;
    for( int i=1; i<=z.PLYMAX+1; i++ )
    {
        NODE *p = &pv_history[i-1];
        if( p->from == 0 )
            break;

        // We only go above z.PLYMAX if we are in check
        if( i > z.PLYMAX )
        {
            thc::Square sq = (cr.WhiteToPlay() ? cr.wking_square : cr.bking_square);
            bool in_check = cr.AttackedPiece(sq);
            if( !in_check )
                break;  // Presumambly this >PLYMAX node is not in the PV tree
        }
        thc::Square src;
        bool ok = sargon_export_square( p->from, src );
        if( !ok )
            break;
        thc::Square dst;
        ok = sargon_export_square( p->to, dst );
        if( !ok )
            break;
        char buf[5];
        buf[0] = thc::get_file(src);
        buf[1] = thc::get_rank(src);
        buf[2] = thc::get_file(dst);
        buf[3] = thc::get_rank(dst);
        buf[4] = '\0';
        thc::Move mv;
        bool legal = mv.TerseIn( &cr, buf );
        if( !legal )
            break;
        else
        {
            last_move_to = p->to;
            cr.PlayMove(mv);
            pv.variation.push_back(mv);
            thc::TERMINAL score_terminal;
            cr.Evaluate( score_terminal );
            if( score_terminal != thc::NOT_TERMINAL )
                break;
        }
    }
    thc::TERMINAL score_terminal;
    cr.Evaluate( score_terminal );
    int points = 0;
    if( pv.variation.size() > 0 )
    {

// #define SIMPLER_CENTIPAWNS_CALCULATION
//  Now that we understand Sargon's scoring and searching so much better
//  we can actually use the actual score it calculated during FNDMOV()
//  rather than recalculating it. Not only is it simpler, we could replace
//  the poor PV implementation in Sargon-x86 with this much improved
//  refined version too. The only problem is that the actual scores are
//  a bit (not dramatically) different for many tests. They might be
//  (probably are) better scores though. For example test 32 of 38 we
//  already tweaked from 150 to 125 (which less accurately reflected the
//  position) but if we adopted SIMPLER_CENTIPAWNS_CALCULATION we'd have
//  to change it again to 200 which MORE accurately reflects the position
//  I think.
#ifdef SIMPLER_CENTIPAWNS_CALCULATION
        points = (int)z.SCORE[1];   // unsigned bigger is better for the side that played first
        points -= 128;              // signed bigger is better for the side that played first
        if( !pv_base_position.WhiteToPlay() )
            points = 0-points;      // signed bigger is better for White
        int base_points = 4*LIMIT(29,z.MV0) + LIMIT(6,z.BC0);   // base bigger is better for White
        points += base_points;
#else

        bool white_last = !cr.WhiteToPlay();

        // Save things we are changing
        uint8_t sv_color = z.COLOR;
        uint8_t sv_to    = z.MLPTRJ->to;
        uint8_t sv_val1  = z.MLPTRJ->val;
        uint8_t sv_MV0   = z.MV0;
        uint8_t sv_BC0   = z.BC0;
        thc::ChessPosition sv_cp;
        sargon_export_position(sv_cp);

        // Temporarily set Sargon position to end position
        sargon_import_position(cr);
        z.COLOR      = white_last?0:0x80;
        z.MLPTRJ->to = last_move_to;

        // Set base values to zero for an absolute rather than
        //  a relative points calculation
        z.MV0 = 0;
        z.BC0 = 0;

        // Calculate points
        points = POINTS();      // 2-254, unsigned smaller_is better for last side to move in leaf position
        points -= 128;          // signed smaller_is better for last side to move in leaf position
        if( white_last )
            points = 0-points;   // signed bigger_is_better for white  

        // Restore things we changed
        z.COLOR       = sv_color;
        z.MLPTRJ->to  = sv_to;
        z.MLPTRJ->val = sv_val1; 
        z.MV0         = sv_MV0;
        z.BC0         = sv_BC0;
        sargon_import_position(sv_cp);
#endif
    }

    // Final adjustments
    double centipawns = points * 100.0/8.0;
    pv.value = static_cast<int>(centipawns);
    if( score_terminal != thc::NOT_TERMINAL )
    {
        switch( score_terminal )
        {
            case thc::TERMINAL_WCHECKMATE:  // White is checkmated
            case thc::TERMINAL_BCHECKMATE:  // Black is checkmated
            {
                int nbr = (int)pv.variation.size();
                int value = 6000;                           // mate in 1 is worth 60 pawns (say)
                int nbr_full_moves_to_mate = (nbr+1)/2;     // nbr=1,2 =>1 3,4=>2
                if( nbr_full_moves_to_mate == 0 )           // eg initial position is checkmate
                    nbr_full_moves_to_mate = 1;
                value -= (nbr_full_moves_to_mate-1)*100;    // subtract 1 pawn for mate in 2, 2 pawns for mate in 3, etc
                pv.value = (score_terminal==thc::TERMINAL_BCHECKMATE ? value : 0-value);
                break;
            }
            case thc::TERMINAL_WSTALEMATE:
            case thc::TERMINAL_BSTALEMATE:
            {
                pv.value=0;
                break;
            }
        }
    }
    return pv;
}

std::string sargon_pv_report_stats()
{
    return "";
}
