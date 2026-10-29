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
    uint8_t  level;
    uint8_t  from;
    uint8_t  to;
    NODE() : level(0), from(0), to(0) {}
    NODE( uint8_t l, uint8_t f, uint8_t t ) :
                level(l), from(f), to(t) {}
};

//
//  Build Sargon's PV (Principal Variation)
//

static thc::ChessRules pv_base_position;
static std::vector< NODE > nodes;
static std::vector< NODE > nodes_pv;
static unsigned long max_len_so_far;

void sargon_pv_clear( const thc::ChessPosition &current_position )
{
    pv_base_position = current_position;
    nodes.clear();
}

void sargon_pv_callback_yes_best_move()
{
    // Collect the best moves' attributes
    ML *p = z.MLPTRJ;
    uint8_t level  = z.NPLY;
    uint8_t from  = p->from;
    uint8_t to    = p->to;
    int sz = (int)nodes.size();
    NODE n(level,from,to);
    static uint8_t previous_level;
    bool level_change = (level!=previous_level);
    previous_level = level;
    if( 1 ) //level_change )    // TODO why doesn't this level_change idea work?
        nodes.push_back(n);
    else
    {
        int idx = sz - 1;
        if( idx > 0 )
            nodes[idx] = n;
    }
    if( sz > (int)max_len_so_far )
        max_len_so_far = (unsigned long)sz;
    if( level == 1 )
    {
        nodes_pv = nodes;   // snapshot every time we select best ply 1 move
        nodes.clear();
    } 
}

// Use our knowledge for the way Sargon does minimax/alpha-beta to build a PV
// When a node is indicated as 'BEST' at level one, we can look back through
//  previously indicated nodes at higher level and construct a PV
PV sargon_pv_get()
{
    PV pv;
    thc::ChessRules cr = pv_base_position;
    pv.variation.clear();

    // The PV calculation algorithm is derived in the executable documentation within
    //  std::string pv_algorithm_txt in src/sargon-minimax.cpp
    // 
    // Summarising the PV algorithm is;
    // Collect a list of nodes representing all positive 'best move so far' decisions
    // Define an initially empty list PV
    // Set N = 1
    // Loop over the best so far node list once in reverse order
    //   Skip over nodes of all levels looking for the next of level N
    //   If a scanned node has level equal to N, append it to PV and increment N
    int nbr = (int)nodes_pv.size();
    uint8_t target = 1;
    uint8_t plymax = z.PLYMAX;
    uint8_t last_move_to = SQ_a1;
    for( int i=nbr-1; i>=0; i-- )
    {
        NODE *p = &nodes_pv[i];
        if( p->level == target )
        {

            // We only go above target if we are in check
            if( target > plymax )
            {
                thc::Square sq = (cr.WhiteToPlay() ? cr.wking_square : cr.bking_square);
                bool in_check = cr.AttackedPiece(sq);
                //printf( "target=%u, p->level=%u; in_check = %s\n", target, p->level, in_check ? "true" : "false" );
                if( !in_check )
                    break;  // Presumambly this node is not in the PV tree
            }
            target++;
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
    return util::sprintf( "max length of build PV vector=%lu\n", max_len_so_far );
}
