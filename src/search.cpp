#include "search.h"
#include "evaluate.h"
#include "movegen.h"
#include "timeman.h"
#include <iostream>

// ---------------------------------------------------------------------------
//  PV Table (triangular)
// ---------------------------------------------------------------------------
static Move pv_array[64][64];
static int pv_length[64];
static uint64_t node_count = 0;

// ---------------------------------------------------------------------------
//  Move Ordering (MVV-LVA)
// ---------------------------------------------------------------------------
static int score_move(const Position& pos, Move m) {
    Piece victim = pos.piece_on(move_to(m));
    Piece attacker = pos.piece_on(move_from(m));

    if (victim != Piece::PIECE_NONE) {
        int v = static_cast<int>(piece_type(victim));
        int a = static_cast<int>(piece_type(attacker));
        return 10 * PieceValues[v] - PieceValues[a];
    }

    if (move_prom(m) != PieceType::NONE) {
        return PieceValues[static_cast<int>(move_prom(m))];
    }

    return 0;
}

static void sort_moves(const Position& pos, MoveList& list) {
    int scores[MAX_MOVES];
    for (int i = 0; i < list.size(); ++i) {
        scores[i] = score_move(pos, list.moves[i]);
    }

    for (int i = 1; i < list.size(); ++i) {
        int key_score = scores[i];
        Move key_move = list.moves[i];
        int j = i - 1;
        while (j >= 0 && scores[j] < key_score) {
            list.moves[j + 1] = list.moves[j];
            scores[j + 1] = scores[j];
            --j;
        }
        list.moves[j + 1] = key_move;
        scores[j + 1] = key_score;
    }
}

// ---------------------------------------------------------------------------
//  Quiescence Search
// ---------------------------------------------------------------------------
static Value quiescence(Position& pos, Value alpha, Value beta) {
    if (TimeManager::stop_search) return 0;

    node_count++;
    if (node_count & 2047) return 0;
    TimeManager::check_time();
    if (TimeManager::stop_search) return 0;

    Value stand_pat = evaluate(pos);

    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;

    MoveList list;
    MoveGen::generate_legal_moves(pos, list);
    sort_moves(pos, list);

    for (int i = 0; i < list.size(); ++i) {
        if (pos.piece_on(move_to(list.moves[i])) == Piece::PIECE_NONE
         && move_prom(list.moves[i]) == PieceType::NONE) continue;

        Position next_pos = pos;
        if (!next_pos.make_move(list.moves[i])) continue;

        Value val = -quiescence(next_pos, -beta, -alpha);

        if (val >= beta) return beta;
        if (val > alpha) alpha = val;
    }

    return alpha;
}

// ---------------------------------------------------------------------------
//  Negamax Search
// ---------------------------------------------------------------------------
static Value negamax(Position& pos, int depth, int ply, Value alpha, Value beta) {
    pv_length[ply] = ply;

    if (TimeManager::stop_search) return 0;

    node_count++;
    if (!(node_count & 2047)) {
        TimeManager::check_time();
        if (TimeManager::stop_search) return 0;
    }

    if (depth == 0) {
        return quiescence(pos, alpha, beta);
    }

    MoveList list;
    MoveGen::generate_legal_moves(pos, list);
    sort_moves(pos, list);

    Value best_value = -VALUE_INFINITE;

    for (int i = 0; i < list.size(); ++i) {
        Position next_pos = pos;
        if (!next_pos.make_move(list.moves[i])) continue;

        Value val = -negamax(next_pos, depth - 1, ply + 1, -beta, -alpha);

        if (val > best_value) {
            best_value = val;
            pv_array[ply][ply] = list.moves[i];
            for (int j = ply + 1; j < pv_length[ply + 1]; ++j) {
                pv_array[ply][j] = pv_array[ply + 1][j];
            }
            pv_length[ply] = pv_length[ply + 1];
        }

        if (val > alpha) alpha = val;
        if (alpha >= beta) break;
    }

    return best_value;
}

// ---------------------------------------------------------------------------
//  Root Search (Iterative Deepening)
// ---------------------------------------------------------------------------
Move search_position(Position& pos, int max_depth) {
    Move best_root_move = MOVE_NONE;
    bool search_aborted = false;

    for (int d = 1; d <= max_depth; ++d) {
        node_count = 0;

        Value alpha = -VALUE_INFINITE;
        Value beta = VALUE_INFINITE;

        MoveList list;
        MoveGen::generate_legal_moves(pos, list);
        sort_moves(pos, list);

        Value best_value = -VALUE_INFINITE;

        for (int i = 0; i < list.size(); ++i) {
            Position next_pos = pos;
            if (!next_pos.make_move(list.moves[i])) continue;

            Value val = -negamax(next_pos, d - 1, 1, -beta, -alpha);

            if (val > best_value) {
                best_value = val;
                pv_array[0][0] = list.moves[i];
                // Copy the rest of the PV line from ply 1 to ply 0
                for (int j = 1; j < pv_length[1]; ++j) {
                    pv_array[0][j] = pv_array[1][j];
                }
                pv_length[0] = pv_length[1];
            }
            if (val > alpha) alpha = val;
        }

        if (TimeManager::stop_search) {
            search_aborted = true;
            break;
        }

        std::string pv_str;
        for (int j = 0; j < pv_length[0]; ++j) {
            pv_str += " " + move_to_str(pv_array[0][j]);
        }

        uint64_t elapsed = TimeManager::get_time_ms() - TimeManager::start_time;
        uint64_t nps = (elapsed > 0) ? (node_count * 1000) / elapsed : 0;

        std::cout << "info depth " << d
                  << " score cp " << best_value
                  << " time " << elapsed
                  << " nodes " << node_count
                  << " nps " << nps
                  << " pv" << pv_str << std::endl;

        best_root_move = pv_array[0][0];
    }

    (void)search_aborted;

    std::cout.flush();
    return best_root_move;
}
