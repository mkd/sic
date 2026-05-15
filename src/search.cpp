#include "../include/search.h"
#include "../include/evaluate.h"
#include "../include/movegen.h"
#include "../include/timeman.h"
#include "../include/tt.h"
#include "../include/thread.h"
#include "../include/attacks.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <iostream>

// ---------------------------------------------------------------------------
//  LMR Table
// ---------------------------------------------------------------------------
int LMRTable[64][64];

void init_lmr() {
    for (int d = 0; d < 64; ++d) {
        for (int m = 0; m < 64; ++m) {
            if (d >= 3 && m >= 4) {
                double reduction = 0.75 + std::log(d) * std::log(m) / 2.25;
                LMRTable[d][m] = static_cast<int>(reduction);
            } else {
                LMRTable[d][m] = 0;
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  Move Ordering (MVV-LVA + TT move + Killer Moves)
// ---------------------------------------------------------------------------
static int score_move(const Position& pos, Move m, Move tt_move, const SearchWorker& sw, int ply, Move prev_move) {
    if (m == tt_move) return 2000000;

    Piece victim = pos.piece_on(move_to(m));
    Piece attacker = pos.piece_on(move_from(m));

    if (victim != Piece::PIECE_NONE) {
        int v = static_cast<int>(piece_type(victim));
        int a = static_cast<int>(piece_type(attacker));
        return 1000000 + 10 * PieceValues[v] - PieceValues[a];
    }

    if (m == sw.killer_moves[ply][0]) return 900000;
    if (m == sw.killer_moves[ply][1]) return 800000;

    if (prev_move != MOVE_NONE) {
        int prev_from = static_cast<int>(move_from(prev_move));
        int prev_to = static_cast<int>(move_to(prev_move));
        if (m == sw.counter_moves[prev_from][prev_to]) {
            return 750000;
        }
    }

    if (move_prom(m) != PieceType::NONE) {
        return PieceValues[static_cast<int>(move_prom(m))];
    }

    int hist_score = sw.history[static_cast<int>(pos.sideToMove)][static_cast<int>(move_from(m))][static_cast<int>(move_to(m))];
    return hist_score < 700000 ? hist_score : 700000;
}

static void sort_moves(const Position& pos, MoveList& list, Move tt_move, const SearchWorker& sw, int ply, Move prev_move) {
    int scores[MAX_MOVES];
    for (int i = 0; i < list.size(); ++i) {
        scores[i] = score_move(pos, list.moves[i], tt_move, sw, ply, prev_move);
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
//  SEE Helpers
// ---------------------------------------------------------------------------
static Bitboard get_attackers(const Position& pos, Square sq, Bitboard occupied) {
    int sq_idx = static_cast<int>(sq);
    Bitboard attackers = {0};

    attackers.bb |= (PAWN_ATTACKS[static_cast<int>(Color::BLACK)][sq_idx].bb & pos.pieces(Color::WHITE).bb & pos.pieces(PieceType::PAWN).bb);
    attackers.bb |= (PAWN_ATTACKS[static_cast<int>(Color::WHITE)][sq_idx].bb & pos.pieces(Color::BLACK).bb & pos.pieces(PieceType::PAWN).bb);
    attackers.bb |= (KNIGHT_ATTACKS[sq_idx].bb & pos.pieces(PieceType::KNIGHT).bb);
    attackers.bb |= (KING_ATTACKS[sq_idx].bb & pos.pieces(PieceType::KING).bb);
    attackers.bb |= (get_bishop_attacks(sq, occupied).bb & (pos.pieces(PieceType::BISHOP).bb | pos.pieces(PieceType::QUEEN).bb));
    attackers.bb |= (get_rook_attacks(sq, occupied).bb & (pos.pieces(PieceType::ROOK).bb | pos.pieces(PieceType::QUEEN).bb));

    return {attackers.bb & occupied.bb};
}

static bool see_ge(const Position& pos, Move m, int threshold) {
    Square from = move_from(m);
    Square to = move_to(m);
    int swap_val = PieceValues[static_cast<int>(piece_type(pos.piece_on(to)))] - threshold;
    if (swap_val < 0) return false;

    PieceType attacker_type = piece_type(pos.piece_on(from));
    if (move_prom(m) != PieceType::NONE) attacker_type = move_prom(m);
    swap_val -= PieceValues[static_cast<int>(attacker_type)];
    if (swap_val >= 0) return true;

    Bitboard occupied = pos.occupied();
    occupied.bb ^= (1ULL << static_cast<int>(from));

    if (piece_type(pos.piece_on(to)) == PieceType::NONE && attacker_type == PieceType::PAWN) {
        Square ep_sq = pos.sideToMove == Color::WHITE ? static_cast<Square>(static_cast<int>(to) - 8) : static_cast<Square>(static_cast<int>(to) + 8);
        occupied.bb ^= (1ULL << static_cast<int>(ep_sq));
        swap_val += PieceValues[static_cast<int>(PieceType::PAWN)];
    }

    Bitboard attackers = get_attackers(pos, to, occupied);
    Color us = ~pos.sideToMove;

    while (true) {
        Bitboard our_attackers = {attackers.bb & pos.pieces(us).bb};
        if (our_attackers.bb == 0) break;

        PieceType pt = PieceType::NONE;
        for (int i = 1; i <= 6; ++i) {
            if (our_attackers.bb & pos.pieces(static_cast<PieceType>(i)).bb) {
                pt = static_cast<PieceType>(i);
                break;
            }
        }

        Square atk_sq = lsb({our_attackers.bb & pos.pieces(pt).bb});
        occupied.bb ^= (1ULL << static_cast<int>(atk_sq));
        attackers.bb &= ~(1ULL << static_cast<int>(atk_sq));

        if (pt == PieceType::PAWN || pt == PieceType::BISHOP || pt == PieceType::ROOK || pt == PieceType::QUEEN) {
            attackers.bb |= get_attackers(pos, to, occupied).bb;
        }

        swap_val = -swap_val - 1 - PieceValues[static_cast<int>(pt)];
        us = ~us;
        if (swap_val >= 0) return true;
    }
    return false;
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
    sort_moves(pos, list, MOVE_NONE, sw, 0, MOVE_NONE);

    for (int i = 0; i < list.size(); ++i) {
        if (pos.piece_on(move_to(list.moves[i])) == Piece::PIECE_NONE
         && move_prom(list.moves[i]) == PieceType::NONE) continue;

        if (!see_ge(pos, list.moves[i], 0)) continue;

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
static Value negamax(Position& pos, int depth, int ply, Value alpha, Value beta, bool is_null, SearchWorker& sw, Move prev_move = MOVE_NONE) {
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

    bool in_check = pos.is_attacked(pos.get_king_square(pos.sideToMove), ~pos.sideToMove);
    Value static_eval = evaluate(pos);
    sw.static_evals[ply] = static_eval;

    bool improving = false;
    if (ply >= 2 && !in_check) {
        improving = (static_eval >= sw.static_evals[ply - 2]);
    }

    Move tt_move = MOVE_NONE;
    Value tt_score;
    if (ply > 0 && probe_tt(pos.zobristKey, depth, alpha, beta, tt_score, tt_move)) {
        return tt_score;
    }

    // Reverse Futility Pruning (Static NMP)
    if (!is_null && depth <= 5 && !in_check && abs(beta) < VALUE_MATE - 1000) {
        int rfp_margin = improving ? depth * 75 : depth * 100;
        if (static_eval - rfp_margin >= beta) {
            return static_eval;
        }
    }

    // Null Move Pruning
    if (!is_null && depth >= 3 && ply > 0 && static_eval >= beta && !in_check) {
        Position null_pos = pos;
        null_pos.make_null_move();
        Value null_val = -negamax(null_pos, depth - 3, ply + 1, -beta, -beta + 1, true, sw, MOVE_NONE);
        if (null_val >= beta) return beta;
    }

    // Internal Iterative Reductions (IIR)
    if (depth >= 4 && tt_move == MOVE_NONE) {
        depth--;
    }

    MoveList list;
    MoveGen::generate_legal_moves(pos, list);
    sort_moves(pos, list, tt_move, sw, ply, prev_move);

    Value best_value = -VALUE_INFINITE;
    Move best_move = MOVE_NONE;
    TTFlag flag = TT_ALPHA;

    int legal_moves = 0;
    Move quiets_searched[MAX_MOVES];
    int quiet_count = 0;

    for (int i = 0; i < list.size(); ++i) {
        bool is_quiet = (pos.piece_on(move_to(list.moves[i])) == Piece::PIECE_NONE
                      && move_prom(list.moves[i]) == PieceType::NONE);

        bool is_killer = (list.moves[i] == sw.killer_moves[ply][0] || list.moves[i] == sw.killer_moves[ply][1]);

        // Late Move Pruning (LMP)
        if (depth <= 4 && !in_check && is_quiet && !is_killer) {
            int lmp_threshold = 3 + 2 * depth * depth;
            if (legal_moves > lmp_threshold) {
                continue;
            }
        }

        // History Pruning
        if (depth <= 3 && legal_moves > 0 && is_quiet && !is_killer) {
            int us = static_cast<int>(pos.sideToMove);
            int hist = sw.history[us][static_cast<int>(move_from(list.moves[i]))][static_cast<int>(move_to(list.moves[i]))];
            if (hist < -4000 * depth) {
                continue;
            }
        }

        // PVS SEE Pruning
        if (depth <= 4 && !in_check && !is_killer) {
            int see_threshold = is_quiet ? -50 : -200 * depth;
            if (!see_ge(pos, list.moves[i], see_threshold)) {
                continue;
            }
        }

        Position next_pos = pos;
        if (!next_pos.make_move(list.moves[i])) continue;

        // Futility Pruning
        if (depth <= 4 && is_quiet && !is_killer && !in_check && abs(alpha) < VALUE_MATE - 1000) {
            int fp_margin = depth * 100 + 100;
            if (static_eval + fp_margin <= alpha) {
                continue;
            }
        }

        if (is_quiet) {
            quiets_searched[quiet_count++] = list.moves[i];
        }

        Value val;

        if (legal_moves == 0) {
            val = -negamax(next_pos, depth - 1, ply + 1, -beta, -alpha, false, sw, list.moves[i]);
        } else {
            if (depth >= 3 && legal_moves >= 4 && is_quiet && !is_killer) {
                int reduction = LMRTable[std::min(depth, 63)][std::min(legal_moves, 63)];
                if (!improving) reduction++;
                int reduced_depth = std::max(1, depth - 1 - reduction);
                val = -negamax(next_pos, reduced_depth, ply + 1, -alpha - 1, -alpha, false, sw, list.moves[i]);
                if (val > alpha && reduced_depth < depth - 1) {
                    val = -negamax(next_pos, depth - 1, ply + 1, -alpha - 1, -alpha, false, sw, list.moves[i]);
                }
            } else {
                val = -negamax(next_pos, depth - 1, ply + 1, -alpha - 1, -alpha, false, sw, list.moves[i]);
            }

            if (val > alpha && val < beta) {
                val = -negamax(next_pos, depth - 1, ply + 1, -beta, -alpha, false, sw, list.moves[i]);
            }
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
            if (is_quiet) {
                if (list.moves[i] != sw.killer_moves[ply][0]) {
                    sw.killer_moves[ply][1] = sw.killer_moves[ply][0];
                    sw.killer_moves[ply][0] = list.moves[i];
                }
                int bonus = depth * depth;
                int us = static_cast<int>(pos.sideToMove);
                int from = static_cast<int>(move_from(list.moves[i]));
                int to = static_cast<int>(move_to(list.moves[i]));
                sw.history[us][from][to] += bonus - sw.history[us][from][to] * abs(bonus) / 16384;
                for (int q = 0; q < quiet_count - 1; ++q) {
                    int q_from = static_cast<int>(move_from(quiets_searched[q]));
                    int q_to = static_cast<int>(move_to(quiets_searched[q]));
                    sw.history[us][q_from][q_to] -= bonus + sw.history[us][q_from][q_to] * abs(bonus) / 16384;
                }
                if (prev_move != MOVE_NONE) {
                    sw.counter_moves[static_cast<int>(move_from(prev_move))][static_cast<int>(move_to(prev_move))] = list.moves[i];
                }
            }
            best_move = list.moves[i];
            break;
        }

        legal_moves++;
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
        sort_moves(pos, list, MOVE_NONE, sw, 0, MOVE_NONE);

        Value best_value = -VALUE_INFINITE;

        int legal_moves = 0;

        for (int i = 0; i < list.size(); ++i) {
            Position next_pos = pos;
            if (!next_pos.make_move(list.moves[i])) continue;

            Value val;
            if (legal_moves == 0) {
                val = -negamax(next_pos, d - 1, 1, -beta, -alpha, false, sw, list.moves[i]);
            } else {
                val = -negamax(next_pos, d - 1, 1, -alpha - 1, -alpha, false, sw, list.moves[i]);
                if (val > alpha) {
                    val = -negamax(next_pos, d - 1, 1, -beta, -alpha, false, sw, list.moves[i]);
                }
            }

            if (val > best_value) {
                best_value = val;
                sw.pv_array[0][0] = list.moves[i];
                for (int j = 1; j < sw.pv_length[1]; ++j) {
                    sw.pv_array[0][j] = sw.pv_array[1][j];
                }
                sw.pv_length[0] = sw.pv_length[1];
            }
            if (val > alpha) {
                alpha = val;
            }

            legal_moves++;
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
                       << " hashfull " << get_hashfull()
                       << " pv" << pv_str << std::endl;
        }

        best_root_move = sw.pv_array[0][0];
    }

    if (thread_id == 0) {
        std::cout.flush();
    }

    return best_root_move;
}
