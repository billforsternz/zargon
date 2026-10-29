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

//
//  Build Sargon's PV (Principal Variation)
//

static NODE track_pv[ MAX_DEPTH + ALLOCATION_PADDING ][ MAX_DEPTH + ALLOCATION_PADDING ];
static thc::ChessRules pv_base_position;

void sargon_pv_clear( const thc::ChessPosition &current_position )
{
    memset( track_pv, 0, sizeof(track_pv) );
    pv_base_position = current_position;
}


/*

    New improved PV algorithm

    This algorithm is much improved on our original potentially
    unbounded vector algorithm. More in the spirit of Sargon,
    we use a single static allocation. Although we do need a
    two dimensional array, around 1K bytes.

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

*/

void sargon_pv_callback_yes_best_move()
{
    // Collect the best moves' attributes
    ML *p = z.MLPTRJ;
    uint8_t level  = z.NPLY;
    uint8_t from  = p->from;
    uint8_t to    = p->to;
    NODE n(from,to);
    track_pv[level][level] =  n;

    // Pull up the current list of continuation nodes from the array row below
    if( track_pv[level+1][level+1].from != 0 )
    {
        NODE *src  = &track_pv[level+1][level+1];
        NODE *last = &track_pv[level+1][z.PLYMAX+1];
        NODE *dst  = &track_pv[level][level+1];
        size_t sz = (uint8_t *)(last+1) - (uint8_t *)(src);
        memcpy( dst, src, sz );
    }
}

PV sargon_pv_get()
{
    PV pv;
    thc::ChessRules cr = pv_base_position;
    pv.variation.clear();

    uint8_t level = 1;
    uint8_t plymax = z.PLYMAX;
    uint8_t last_move_to = SQ_a1;
    for(;; )
    {
        NODE *p = &track_pv[1][level];
        if( p->from == 0 )
            break;

        // We only go above z.PLYMAX if we are in check
        if( level > z.PLYMAX )
        {
            thc::Square sq = (cr.WhiteToPlay() ? cr.wking_square : cr.bking_square);
            bool in_check = cr.AttackedPiece(sq);
            if( !in_check )
                break;  // Presumambly this >PLYMAX node is not in the PV tree
        }
        level++;
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
        }
    }
    thc::TERMINAL score_terminal;
    cr.Evaluate( score_terminal );
    int points = 0;
    if( pv.variation.size() > 0 )
    {

        bool white_last = !cr.WhiteToPlay();

        // Save things we are changing
        uint8_t sv_color = z.COLOR;
        uint8_t sv_to    = z.MLPTRJ->to;
        uint8_t sv_val1  = z.MLPTRJ->val;
        uint8_t sv_val2  = z.VALM;
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
        POINTS();
        points = z.VALM;        // 1-254, unsigned smaller_is better for last side to move in leaf position
        points -= 128;          // signed smaller_is better for last side to move in leaf position
        if( white_last )
            points = 0-points;   // signed bigger_is_better for white  

        // Restore things we changed
        z.COLOR       = sv_color;
        z.MLPTRJ->to  = sv_to;
        z.MLPTRJ->val = sv_val1; 
        z.VALM        = sv_val2;
        z.MV0         = sv_MV0;
        z.BC0         = sv_BC0;
        sargon_import_position(sv_cp);
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
