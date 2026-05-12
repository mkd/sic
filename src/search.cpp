#include "../include/search.h"
#include "../include/evaluate.h"
#include "../include/movegen.h"
#include "../include/timeman.h"
#include "../include/tt.h"
#include "../include/thread.h"
#include <iostream>

// ---------------------------------------------------------------------------
//  Move Ordering (MVV-LVA + TT move)
// ---------------------------------------------------------------------------
static int score_move(const Position& pos, Move m, Move tt_move) {
    if (m == tt_move) return 2000000;

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

static void sort_moves(const Position& pos, MoveList& list, Move tt_move) {
    int scores[MAX_MOVES];
    for (int i = 0; i < list.size(); ++i) {
        scores[i] = score_move(pos, list.moves[i], tt_move);
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
static Value quiescence(Position& pos, Value alpha, Value beta, SearchWorker& sw) {
    if (TimeManager::stop_search) return 0;

    sw.node_count++;
    if (!(sw.node_count & 2047)) {
        TimeManager::check_time();
        if (TimeManager::stop_search) return 0;
    }

    Value stand_pat = evaluate(pos);

    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;

    MoveList list;
    MoveGen::generate_legal_moves(pos, list);
    sort_moves(pos, list, MOVE_NONE);

    for (int i = 0; i < list.size(); ++i) {
        if (pos.piece_on(move_to(list.moves[i])) == Piece::PIECE_NONE
         && move_prom(list.moves[i]) == PieceType::NONE) continue;

        Position next_pos = pos;
        if (!next_pos.make_move(list.moves[i])) continue;

        Value val = -quiescence(next_pos, -beta, -alpha, sw);

        if (val >= beta) return beta;
        if (val > alpha) alpha = val;
    }

    return alpha;
}

// ---------------------------------------------------------------------------
//  Negamax Search
// ---------------------------------------------------------------------------
static Value negamax(Position& pos, int depth, int ply, Value alpha, Value beta, bool is_null, SearchWorker& sw) {
    sw.pv_length[ply] = ply;

    if (TimeManager::stop_search) return 0;

    sw.node_count++;
    if (!(sw.node_count & 2047)) {
        TimeManager::check_time();
        if (TimeManager::stop_search) return 0;
    }

    if (depth == 0) {
        return quiescence(pos, alpha, beta, sw);
    }

    Move tt_move = MOVE_NONE;
    Value tt_score;
    if (ply > 0 && probe_tt(pos.zobristKey, depth, alpha, beta, tt_score, tt_move)) {
        return tt_score;
    }

    // Null Move Pruning
    if (!is_null && depth >= 3 && ply > 0
     && evaluate(pos) >= beta
     && !pos.is_attacked(pos.get_king_square(pos.sideToMove), ~pos.sideToMove)) {
        Position null_pos = pos;
        null_pos.make_null_move();
        Value null_val = -negamax(null_pos, depth - 3, ply + 1, -beta, -beta + 1, true, sw);
        if (null_val >= beta) return beta;
    }

    MoveList list;
    MoveGen::generate_legal_moves(pos, list);
    sort_moves(pos, list, tt_move);

    Value best_value = -VALUE_INFINITE;
    Move best_move = MOVE_NONE;
    TTFlag flag = TT_ALPHA;

    for (int i = 0; i < list.size(); ++i) {
        Position next_pos = pos;
        if (!next_pos.make_move(list.moves[i])) continue;

        bool is_quiet = (pos.piece_on(move_to(list.moves[i])) == Piece::PIECE_NONE
                      && move_prom(list.moves[i]) == PieceType::NONE);

        Value val;

        if (depth >= 3 && i >= 4 && is_quiet) {
            val = -negamax(next_pos, depth - 2, ply + 1, -alpha - 1, -alpha, false, sw);

            if (val > alpha) {
                val = -negamax(next_pos, depth - 1, ply + 1, -beta, -alpha, false, sw);
            }
        } else {
            val = -negamax(next_pos, depth - 1, ply + 1, -beta, -alpha, false, sw);
        }

        if (val > best_value) {
            best_value = val;
            best_move = list.moves[i];
            sw.pv_array[ply][ply] = list.moves[i];
            for (int j = ply + 1; j < sw.pv_length[ply + 1]; ++j) {
                sw.pv_array[ply][j] = sw.pv_array[ply + 1][j];
            }
            sw.pv_length[ply] = sw.pv_length[ply + 1];
        }

        if (val > alpha) {
            alpha = val;
            flag = TT_EXACT;
        }

        if (alpha >= beta) {
            flag = TT_BETA;
            best_move = list.moves[i];
            break;
        }
    }

    record_tt(pos.zobristKey, depth, best_value, flag, best_move);
    return best_value;
}

// ---------------------------------------------------------------------------
//  Root Search (Iterative Deepening)
// ---------------------------------------------------------------------------
Move search_position(Position& pos, int max_depth, int thread_id) {
    Move best_root_move = MOVE_NONE;
    SearchWorker sw;

    for (int d = 1; d <= max_depth; ++d) {
        if (TimeManager::stop_search) break;

        sw.node_count = 0;

        Value alpha = -VALUE_INFINITE;
        Value beta = VALUE_INFINITE;

        MoveList list;
        MoveGen::generate_legal_moves(pos, list);
        sort_moves(pos, list, MOVE_NONE);

        Value best_value = -VALUE_INFINITE;

        for (int i = 0; i < list.size(); ++i) {
            Position next_pos = pos;
            if (!next_pos.make_move(list.moves[i])) continue;

            Value val = -negamax(next_pos, d - 1, 1, -beta, -alpha, false, sw);

            if (val > best_value) {
                best_value = val;
                sw.pv_array[0][0] = list.moves[i];
                for (int j = 1; j < sw.pv_length[1]; ++j) {
                    sw.pv_array[0][j] = sw.pv_array[1][j];
                }
                sw.pv_length[0] = sw.pv_length[1];
            }
            if (val > alpha) alpha = val;
        }

        if (TimeManager::stop_search) {
            break;
        }

        if (thread_id == 0) {
            std::string pv_str;
            for (int j = 0; j < sw.pv_length[0]; ++j) {
                pv_str += " " + move_to_str(sw.pv_array[0][j]);
            }

            uint64_t elapsed = TimeManager::get_time_ms() - TimeManager::start_time;
            uint64_t nps = (elapsed > 0) ? (sw.node_count * 1000) / elapsed : 0;

            std::cout << "info depth " << d
                      << " score cp " << best_value
                      << " time " << elapsed
                      << " nodes " << sw.node_count
                      << " nps " << nps
                      << " pv" << pv_str << std::endl;
        }

        best_root_move = sw.pv_array[0][0];
    }

    if (thread_id == 0) {
        std::cout.flush();
    }

    return best_root_move;
}
