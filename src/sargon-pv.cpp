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
struct PV_MOVE
{
    uint8_t  from;
    uint8_t  to;
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
    sz = y=f(x)+1 for x=N (we store 1 move at max depth)

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
static PV_MOVE pv_history[ MAX_DEPTH*(MAX_DEPTH+1) - (MAX_DEPTH*(MAX_DEPTH-1))/2 + 1 + ALLOCATION_PADDING ];
static thc::ChessRules pv_base_position;
static int lookup_table[ (MAX_DEPTH+1) + ALLOCATION_PADDING ];

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

    // The current move is copied to the destination, then len-1
    //  moves are copied from below
    PV_MOVE *dst_node = &pv_history[offset];
    dst_node->from    = z.MLPTRJ->from;
    dst_node->to      = z.MLPTRJ->to;

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
        PV_MOVE *p = &pv_history[i-1];
        if( p->from == 0 )
            break;

        // We only go above z.PLYMAX if we are in check
        if( i > z.PLYMAX )
        {
            thc::Square sq = (cr.WhiteToPlay() ? cr.wking_square : cr.bking_square);
            bool in_check = cr.AttackedPiece(sq);
            if( !in_check )
                break;  // Presumambly this >PLYMAX move is not in the PV tree
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
/*
    Now that we understand Sargon's scoring and searching so much better
    we can actually use the actual score it calculated during FNDMOV()
    rather than recalculating it. Not only is it simpler, we could replace
    the poor PV implementation in Sargon-x86 with this much improved
    refined version too. One problem though is that the actual scores are
    a bit (not dramatically) different for many tests.

    Working an example let's look at test 32 of 38 which actually was also
    the only one we needed to tweak from the rather poor implementation
    we used in Sargon-X86 (we felt our way to that one - kind of a
    poorly understood version of what we now have with C++ POINTS())

    The tweak required was 150 to 125 centipawns. If we adopted
    SIMPLER_CENTIPAWNS_CALCULATION we'd have to change it again to 200.
    Why is that, and what makes most sense?

    FEN is r4r2/6kp/2pqppp1/p1R5/b2P4/4QN2/1P3PPP/2R3K1 w - - 0 1
    PV at PLYMAX=5 is Qa3 Bb5 Rxb5 Qxa3 Rb7+ Rf7

    In the initial position
    MV0=-2, BC0=6

    In the final position after ...Rf7 (without including BC0, MV0)
    MTRL=-13, BRDC=-6  

    There's a single (but huge!) adjustment, the en-prise Black queen on
    a3, so 17 lost half pawns (from Black's viewpoint, the last side to
    move).

    By calling POINTS() with BC0=0 and MV0=0 we get a bigger_is_better
    score of 4*MTRL + BC + adjustment = 4*(17-13) - 6 = 10 points, or
    10/8 pawns 125 centipawns.

    The SIMPLER_CENTIPAWNS_CALCULATION (ironically a little more
    complicated to follow) is based on what we can recover without
    calculation; MV0=-2, BC0=6 and the best relative score with
    adjustment at ply 1 from the score table (18 which must be
    4*(MTRL-MV0) + (BRDC-BC0) + adjustment).

    points = absolute score + adjustment
           = base score + (relative score + adjustment)
           = base score + (absolute score - base score + adjustment)
           = 4*MV0+BC0 + 4*(MTRL-MV0)+(BRDC-BC0)+adjustment
           = 4*-2 + 6 + 18
           = 16 = 16/8 pawns = 200 centipawns

    But surely
             4*MV0+BC0 + 4*(MTRL-MV0)+(BRDC-BC0)+adjustment
           = 4*MTRL + BC + adjustment

    So why isn't the answer 125 centipawns as before? The explanation
    is (BRDC-BC0) gets LIMIT()ed (constrained) to -6 instead of -6
    - 6 = -12 the simplification implies. So we end up with 6 extra
    points compared to simply 4*MTRL + BC + adjustment.

    So SIMPLER_CENTIPAWNS_CALCULATION bigger_is_better score for
    White is 10+6 = 16 points or 16/8 pawns or 200 centipawns.

    Which one is better? I think 200 centipawns is a better score
    but I think the SIMPLER_CENTIPAWNS_CALCULATION gets us a
    better score for the wrong reasons. The MV0, BC0 variables
    are only there to extend the dynamic range of 8 bit variables
    for the Z80, if we rework the code they'll go away. Reducing
    the impact of BRDC is good though, but the reason is that
    Black's big BRDC advantage (which SIMPLER_CENTIPAWNS_CALCULATION
    reduces, increasing White's score) is bogus because the lost
    en-prise queen controls a lot of squares. So the improvement
    comes from a random dynamic range factor (could well work the
    other way in other examples, maybe?) when what is needed is to
    not  count the board control of lost material - so a possible
    future better POINTS() calculation.

    Conclusion: Leave SIMPLER_CENTIPAWNS_CALCULATION
    undefined for now.
*/

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
