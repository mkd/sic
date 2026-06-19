#include "../include/search.h"
#include "../include/evaluate.h"
#include "../include/movegen.h"
#include "../include/timeman.h"
#include "../include/tt.h"
#include "../include/thread.h"
#include "../include/attacks.h"
#include "../include/timeman.h"
#include "../include/tbprobe.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <vector>
#include <string>
#include "stockfish_probe/nnue_incremental.h"

struct NnueGuard {
    int move;
    bool is_null;
    NnueGuard(int m, bool null_move = false) : move(m), is_null(null_move) {
        if (is_null) Stockfish::Incremental::push_null_state();
        else Stockfish::Incremental::push_state(move);
    }
    ~NnueGuard() {
        if (is_null) Stockfish::Incremental::pop_state(0);
        else Stockfish::Incremental::pop_state(move);
    }
};

// ---------------------------------------------------------------------------
//  Game History (from uci.cpp)
// ---------------------------------------------------------------------------
extern std::vector<uint64_t> g_gameHistory;

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
static bool see_ge(const Position& pos, Move m, int threshold);

static int get_stat_score(const Position& pos, Move m, const SearchWorker& sw, int ply) {
    int us = static_cast<int>(pos.sideToMove);
    int from = static_cast<int>(move_from(m));
    int to = static_cast<int>(move_to(m));
    int p = static_cast<int>(pos.piece_on(static_cast<Square>(from)));
    
    int score = sw.history[us][from][to];

    if (ply >= 1 && sw.played_moves[ply - 1] != MOVE_NONE) {
        int prev_p = static_cast<int>(sw.played_pieces[ply - 1]);
        int prev_to = static_cast<int>(move_to(sw.played_moves[ply - 1]));
        score += sw.continuation_history[0][prev_p][prev_to][p][to];
    }
    if (ply >= 2 && sw.played_moves[ply - 2] != MOVE_NONE) {
        int prev2_p = static_cast<int>(sw.played_pieces[ply - 2]);
        int prev2_to = static_cast<int>(move_to(sw.played_moves[ply - 2]));
        score += sw.continuation_history[1][prev2_p][prev2_to][p][to];
    }
    return score;
}

static int score_move(const Position& pos, Move m, Move tt_move, const SearchWorker& sw, int ply, Move prev_move) {
    if (m == tt_move) return 2000000;

    Piece victim = pos.piece_on(move_to(m));
    Piece attacker = pos.piece_on(move_from(m));

    if (victim != Piece::PIECE_NONE) {
        if (!see_ge(pos, m, 0)) return 100000; // Bad capture
        int v = static_cast<int>(victim);
        int a = static_cast<int>(attacker);
        int to = static_cast<int>(move_to(m));
        int cap_hist = sw.capture_history[a][to][v];
        return 1000000 + 10 * PieceValues[static_cast<int>(piece_type(victim))] - PieceValues[static_cast<int>(piece_type(attacker))] + cap_hist;
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

    int hist_score = get_stat_score(pos, m, sw, ply);
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

// ---------------------------------------------------------------------------
//  WDL Normalization
// ---------------------------------------------------------------------------
struct WinRateParams {
    double a;
    double b;
};

WinRateParams win_rate_params(const Position& pos) {
    int material = popcount(pos.pieces(PieceType::PAWN))
                 + 3 * popcount(pos.pieces(PieceType::KNIGHT))
                 + 3 * popcount(pos.pieces(PieceType::BISHOP))
                 + 5 * popcount(pos.pieces(PieceType::ROOK))
                 + 9 * popcount(pos.pieces(PieceType::QUEEN));

    double m = std::clamp(material, 17, 78) / 58.0;

    constexpr double as[] = {-72.32565836, 185.93832038, -144.58862193, 416.44950446};
    constexpr double bs[] = {83.86794042, -136.06112997, 69.98820887, 47.62901433};

    double a = (((as[0] * m + as[1]) * m + as[2]) * m) + as[3];
    double b = (((bs[0] * m + bs[1]) * m + bs[2]) * m) + bs[3];

    return {a, b};
}

int to_cp(int v, const Position& pos) {
    auto params = win_rate_params(pos);
    return static_cast<int>(std::round(100 * v / params.a));
}

// ---------------------------------------------------------------------------
//  Mate Value Checks
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

    int swap = PieceValues[static_cast<int>(piece_type(pos.piece_on(to)))] - threshold;
    if (swap < 0) return false;

    PieceType attacker_type = piece_type(pos.piece_on(from));
    if (move_prom(m) != PieceType::NONE) attacker_type = move_prom(m);
    swap = PieceValues[static_cast<int>(attacker_type)] - swap;
    if (swap <= 0) return true;

    Bitboard occupied = pos.occupied();
    occupied.bb ^= (1ULL << static_cast<int>(from));
    occupied.bb ^= (1ULL << static_cast<int>(to)); // Xoring 'to' is important for pin logic

    if (piece_type(pos.piece_on(to)) == PieceType::NONE && attacker_type == PieceType::PAWN) {
        Square ep_sq = pos.sideToMove == Color::WHITE ? static_cast<Square>(static_cast<int>(to) - 8) : static_cast<Square>(static_cast<int>(to) + 8);
        occupied.bb ^= (1ULL << static_cast<int>(ep_sq));
        swap += PieceValues[static_cast<int>(PieceType::PAWN)];
    }

    Bitboard attackers = get_attackers(pos, to, occupied);
    Color stm = pos.sideToMove;
    int res = 1;

    while (true) {
        stm = ~stm;
        attackers.bb &= occupied.bb;

        Bitboard stmAttackers = {attackers.bb & pos.pieces(stm).bb};
        if (stmAttackers.bb == 0) break;

        // Don't allow pinned pieces to attack as long as there are pinners
        if (pos.pinners[static_cast<int>(~stm)].bb & occupied.bb) {
            stmAttackers.bb &= ~pos.blockersForKing[static_cast<int>(stm)].bb;
            if (stmAttackers.bb == 0) break;
        }

        res ^= 1;

        Bitboard bb;
        if ((bb.bb = stmAttackers.bb & pos.pieces(PieceType::PAWN).bb)) {
            if ((swap = PieceValues[static_cast<int>(PieceType::PAWN)] - swap) < res) break;
            Square lss = lsb(bb);
            occupied.bb ^= (1ULL << static_cast<int>(lss));
            attackers.bb |= (get_bishop_attacks(to, occupied).bb & (pos.pieces(PieceType::BISHOP).bb | pos.pieces(PieceType::QUEEN).bb));
        } else if ((bb.bb = stmAttackers.bb & pos.pieces(PieceType::KNIGHT).bb)) {
            if ((swap = PieceValues[static_cast<int>(PieceType::KNIGHT)] - swap) < res) break;
            Square lss = lsb(bb);
            occupied.bb ^= (1ULL << static_cast<int>(lss));
        } else if ((bb.bb = stmAttackers.bb & pos.pieces(PieceType::BISHOP).bb)) {
            if ((swap = PieceValues[static_cast<int>(PieceType::BISHOP)] - swap) < res) break;
            Square lss = lsb(bb);
            occupied.bb ^= (1ULL << static_cast<int>(lss));
            attackers.bb |= (get_bishop_attacks(to, occupied).bb & (pos.pieces(PieceType::BISHOP).bb | pos.pieces(PieceType::QUEEN).bb));
        } else if ((bb.bb = stmAttackers.bb & pos.pieces(PieceType::ROOK).bb)) {
            if ((swap = PieceValues[static_cast<int>(PieceType::ROOK)] - swap) < res) break;
            Square lss = lsb(bb);
            occupied.bb ^= (1ULL << static_cast<int>(lss));
            attackers.bb |= (get_rook_attacks(to, occupied).bb & (pos.pieces(PieceType::ROOK).bb | pos.pieces(PieceType::QUEEN).bb));
        } else if ((bb.bb = stmAttackers.bb & pos.pieces(PieceType::QUEEN).bb)) {
            if ((swap = PieceValues[static_cast<int>(PieceType::QUEEN)] - swap) < res) break;
            Square lss = lsb(bb);
            occupied.bb ^= (1ULL << static_cast<int>(lss));
            attackers.bb |= (get_bishop_attacks(to, occupied).bb & (pos.pieces(PieceType::BISHOP).bb | pos.pieces(PieceType::QUEEN).bb)) |
                            (get_rook_attacks(to, occupied).bb & (pos.pieces(PieceType::ROOK).bb | pos.pieces(PieceType::QUEEN).bb));
        } else { // KING
            return (attackers.bb & ~pos.pieces(stm).bb) ? res ^ 1 : res;
        }
    }
    return static_cast<bool>(res);
}

// ---------------------------------------------------------------------------
//  Repetition Detection (2-fold, aggressive)
// ---------------------------------------------------------------------------
static bool is_repetition(const Position& pos, int ply, const SearchWorker& sw) {
    int hmr = pos.halfmoveClock;
    int checks = 0;

    // Search backward through in-search history (step by 2: same side-to-move)
    for (int i = ply - 2; i >= 0; i -= 2) {
        if (--hmr <= 0) return false;
        if (sw.search_history[i] == pos.zobristKey) {
            checks++;
            if (checks >= 1) return true;
        }
    }

    // Search backward through UCI game history (step by 2)
    for (int i = static_cast<int>(g_gameHistory.size()) - 2; i >= 0; i -= 2) {
        if (--hmr <= 0) break;
        if (g_gameHistory[i] == pos.zobristKey) {
            checks++;
            if (checks >= 1) return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
//  Quiescence Search
// ---------------------------------------------------------------------------
static Value quiescence(Position& pos, Value alpha, Value beta, int ply, SearchWorker& sw) {
    if (TimeManager::stop_search) return 0;

    sw.node_count++;
    if (!(sw.node_count & 2047)) {
        TimeManager::check_time();
        if (TimeManager::stop_search) return 0;
    }

    // Draw detection: 50-move rule and insufficient material
    if (pos.halfmoveClock >= 100) return 0;
    if (pos.is_insufficient_material()) return 0;

    bool in_check = pos.is_attacked(pos.get_king_square(pos.sideToMove), ~pos.sideToMove);

    Value tt_score;
    Move tt_move = MOVE_NONE;
    TTFlag tt_flag;
    if (probe_tt(pos.zobristKey, 0, alpha, beta, tt_score, tt_move, tt_flag)) {
        if (tt_flag == TT_EXACT) return tt_score;
        if (tt_flag == TT_ALPHA && tt_score <= alpha) return tt_score;
        if (tt_flag == TT_BETA && tt_score >= beta) return tt_score;
    }

    Value stand_pat = -VALUE_INFINITE;
    if (!in_check) {
        stand_pat = evaluate(pos, false);
        if (stand_pat >= beta) {
            record_tt(pos.zobristKey, 0, stand_pat, TT_BETA, MOVE_NONE);
            return beta;
        }
        if (stand_pat > alpha) alpha = stand_pat;
    }

    MoveList list;
    MoveGen::generate_legal_moves(pos, list);

    // Terminal-node detection in QS: if in check and no evasions => checkmate
    if (in_check && list.size() == 0) {
        return -(VALUE_MATE - ply);
    }

    sort_moves(pos, list, tt_move, sw, 0, MOVE_NONE);

    Value best_value = stand_pat;
    Move best_move = MOVE_NONE;
    TTFlag flag = TT_ALPHA;
    int legal_moves = 0;

    for (int i = 0; i < list.size(); ++i) {
        if (!in_check && pos.piece_on(move_to(list.moves[i])) == Piece::PIECE_NONE
         && move_prom(list.moves[i]) == PieceType::NONE) continue;

        // Delta Pruning
        if (!in_check && move_prom(list.moves[i]) == PieceType::NONE) {
            int captured_val = PieceValues[static_cast<int>(piece_type(pos.piece_on(move_to(list.moves[i]))))];
            if (stand_pat + captured_val + 200 < alpha) {
                continue;
            }
        }

        if (!in_check && !see_ge(pos, list.moves[i], 0)) continue;

        Position next_pos = pos;
        if (!next_pos.make_move(list.moves[i])) continue;

        legal_moves++;

        NnueGuard guard(list.moves[i]);
        Value val = -quiescence(next_pos, -beta, -alpha, ply + 1, sw);

        if (val > best_value) {
            best_value = val;
            best_move = list.moves[i];
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

    if (in_check && legal_moves == 0) {
        return -(VALUE_MATE - ply);
    }

    record_tt(pos.zobristKey, 0, best_value, flag, best_move);
    return best_value;
}

// ---------------------------------------------------------------------------
//  Negamax Search
// ---------------------------------------------------------------------------
static Value negamax(Position& pos, int depth, int ply, Value alpha, Value beta, bool is_null, SearchWorker& sw, Move prev_move = MOVE_NONE, Move excluded_move = MOVE_NONE) {
    __builtin_prefetch(&TT[pos.zobristKey & (TT_CLUSTER_COUNT - 1)]);
    sw.pv_length[ply] = ply;
    sw.search_history[ply] = pos.zobristKey;

    if (TimeManager::stop_search) return 0;

    sw.node_count++;
    if (!(sw.node_count & 2047)) {
        TimeManager::check_time();
        if (TimeManager::stop_search) return 0;
    }

    bool pv_node = (beta - alpha) > 1;

    if (depth == 0) {
        return quiescence(pos, alpha, beta, ply, sw);
    }

    // Draw detection: 50-move rule, insufficient material, repetition
    if (ply > 0) {
        if (pos.halfmoveClock >= 100) return 0;
        if (pos.is_insufficient_material()) return 0;
        if (is_repetition(pos, ply, sw)) return 0;

        if (TB_LARGEST > 0) {
            int pieces = __builtin_popcountll(pos.byColorBB[0].bb) + __builtin_popcountll(pos.byColorBB[1].bb);
            if (pieces <= (int)TB_LARGEST && pos.halfmoveClock == 0 && pos.castlingRights == 0) {
                unsigned wdl = tb_probe_wdl(
                    pos.byColorBB[0].bb, pos.byColorBB[1].bb,
                    pos.byTypeBB[6].bb, pos.byTypeBB[5].bb, pos.byTypeBB[4].bb,
                    pos.byTypeBB[3].bb, pos.byTypeBB[2].bb, pos.byTypeBB[1].bb,
                    0, 0, pos.epSquare == Square::SQ_NONE ? 0 : static_cast<unsigned>(pos.epSquare),
                    pos.sideToMove == Color::WHITE
                );

                if (wdl != TB_RESULT_FAILED) {
                    sw.node_count++;
                    Value v = 0;
                    if (wdl == TB_WIN) v = VALUE_MATE_IN_1 - ply;
                    else if (wdl == TB_LOSS) v = -VALUE_MATE_IN_1 + ply;
                    else if (wdl == TB_DRAW) v = VALUE_DRAW;

                    if (v != 0) {
                        record_tt(pos.zobristKey, depth, v, TT_EXACT, MOVE_NONE);
                        return v;
                    }
                }
            }
        }
    }

    bool in_check = pos.is_attacked(pos.get_king_square(pos.sideToMove), ~pos.sideToMove);
    Value static_eval = evaluate(pos, false);
    sw.static_evals[ply] = static_eval;

    bool improving = false;
    if (ply >= 2 && !in_check) {
        improving = (static_eval >= sw.static_evals[ply - 2]);
    }

    Move tt_move = MOVE_NONE;
    Value tt_score = VALUE_ZERO;
    TTFlag tt_flag = TT_EXACT;
    bool singular_extension = false;

    if (excluded_move == MOVE_NONE && ply > 0 && probe_tt(pos.zobristKey, depth, alpha, beta, tt_score, tt_move, tt_flag)) {
        // 1. Ply-correct first
        if (tt_score >= VALUE_MATE - 500) tt_score -= ply;
        else if (tt_score <= -VALUE_MATE + 500) tt_score += ply;

        // 2. Evaluate bounds strictly
        bool cutoff = false;
        if (tt_flag == TT_EXACT) cutoff = true;
        else if (tt_flag == TT_ALPHA && tt_score <= alpha) cutoff = true;
        else if (tt_flag == TT_BETA && tt_score >= beta) cutoff = true;

        if (cutoff) {
            if (tt_move != MOVE_NONE) {
                sw.pv_array[ply][ply] = tt_move;
                sw.pv_length[ply] = ply + 1;
            } else {
                sw.pv_length[ply] = ply;
            }
            return tt_score;
        }
    }

    // Singular Extension (SE)
    if (depth >= 8 && tt_move != MOVE_NONE && excluded_move == MOVE_NONE && tt_flag != TT_ALPHA && value_abs(tt_score) < VALUE_MATE_IN_2) {
        int se_depth = (depth - 1) / 2;
        Value se_beta = tt_score - depth * 2;
        Value se_score = -negamax(pos, se_depth, ply, -se_beta - 1, -se_beta, true, sw, MOVE_NONE, tt_move);
        singular_extension = (se_score < se_beta);
    }

    // Reverse Futility Pruning (Static NMP)
    if (!pv_node && !is_null && depth <= 5 && !in_check && abs(beta) < VALUE_MATE - 500) {
        int rfp_margin = improving ? depth * 75 : depth * 100;
        if (static_eval - rfp_margin >= beta) {
            return static_eval;
        }
    }

    // ProbCut
    if (!pv_node && !is_null && depth >= 5 && !in_check && abs(beta) < VALUE_MATE - 500) {
        Value prob_beta = beta + 200;
        MoveList pc_list;
        MoveGen::generate_legal_moves(pos, pc_list);
        sort_moves(pos, pc_list, tt_move, sw, ply, prev_move);
        bool prob_cut = false;
        for (int i = 0; i < pc_list.size(); ++i) {
            Move m = pc_list.moves[i];
            if (pos.piece_on(move_to(m)) == Piece::PIECE_NONE && move_prom(m) == PieceType::NONE) continue;
            if (!see_ge(pos, m, 1)) continue;
            
            Position next_pos = pos;
            if (!next_pos.make_move(m)) continue;
            NnueGuard guard(m);
            Value pc_score = -negamax(next_pos, depth - 4, ply + 1, -prob_beta, -prob_beta + 1, false, sw, m);
            if (pc_score >= prob_beta) {
                prob_cut = true;
                break;
            }
        }
        if (prob_cut) return prob_beta;
    }

    // Razoring
    if (!pv_node && !is_null && depth <= 3 && !in_check && abs(beta) < VALUE_MATE - 500) {
        int razor_margin = depth * 300;
        if (static_eval + razor_margin <= alpha) {
            Value qval = quiescence(pos, alpha, beta, ply, sw);
            if (qval <= alpha) return qval;
        }
    }

    // Dynamic Null Move Pruning
    if (!pv_node && !is_null && depth >= 2 && ply > 0 && static_eval >= beta && !in_check) {
        if ((pos.pieces(pos.sideToMove) & ~(pos.pieces(PieceType::PAWN) | pos.pieces(PieceType::KING))).bb != 0) {
            int r = 3 + depth / 6;
            int nmp_depth = depth - r - 1;
            if (nmp_depth < 0) nmp_depth = 0;
            
            Position null_pos = pos;
            null_pos.make_null_move();
            NnueGuard guard(0, true);
            Value null_val = -negamax(null_pos, nmp_depth, ply + 1, -beta, -beta + 1, true, sw, MOVE_NONE);
            if (null_val >= beta) return beta;
        }
    }

    // Internal Iterative Deepening (IID)
    if (pv_node && depth >= 6 && tt_move == MOVE_NONE && !is_null) {
        int iid_depth = depth - 2;
        negamax(pos, iid_depth, ply, alpha, beta, is_null, sw, prev_move, excluded_move);
        Value dummy_score;
        TTFlag dummy_flag;
        probe_tt(pos.zobristKey, 0, alpha, beta, dummy_score, tt_move, dummy_flag);
    } else if (pv_node && depth >= 3 && tt_move == MOVE_NONE) {
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
        if (list.moves[i] == excluded_move) continue;

        Position next_pos = pos;
        if (!next_pos.make_move(list.moves[i])) continue;

        NnueGuard guard(list.moves[i]);
        legal_moves++;

        sw.played_moves[ply] = list.moves[i];
        sw.played_pieces[ply] = pos.piece_on(move_from(list.moves[i]));

        bool is_quiet = (pos.piece_on(move_to(list.moves[i])) == Piece::PIECE_NONE
                      && move_prom(list.moves[i]) == PieceType::NONE);

        bool is_killer = (list.moves[i] == sw.killer_moves[ply][0] || list.moves[i] == sw.killer_moves[ply][1]);

        // Late Move Pruning (LMP)
        if (!pv_node && depth <= 3 && !in_check && is_quiet && !is_killer) {
            int lmp_thresholds[] = {0, 8, 12, 24};
            if (legal_moves > lmp_thresholds[depth]) continue;
        }

        // History Pruning
        if (!pv_node && depth <= 3 && is_quiet && !is_killer) {
            int hist = get_stat_score(pos, list.moves[i], sw, ply);
            if (hist < -4000 * depth) continue;
        }

        // PVS SEE Pruning
        if (!pv_node && depth <= 4 && !in_check && !is_killer && !is_quiet) {
            int see_threshold = -200 * depth;
            if (!see_ge(pos, list.moves[i], see_threshold)) continue;
        }

        // Futility Pruning
        if (!pv_node && depth <= 8 && is_quiet && !is_killer && !in_check && abs(alpha) < VALUE_MATE - 500) {
            int fp_margin = depth * 100;
            if (static_eval + fp_margin <= alpha) continue;
        }

        if (is_quiet) {
            quiets_searched[quiet_count++] = list.moves[i];
        }

        int current_extension = (singular_extension && list.moves[i] == tt_move) ? 1 : 0;

        Value val;
        if (legal_moves == 1) {
            val = -negamax(next_pos, depth - 1 + current_extension, ply + 1, -beta, -alpha, false, sw, list.moves[i]);
        } else {
            if (depth >= 3 && legal_moves >= 2 && is_quiet) {
                int reduction = LMRTable[std::min(depth, 63)][std::min(legal_moves, 63)];
                if (pv_node) reduction--;
                if (is_killer) reduction--;
                if (!improving) reduction++;
                
                int hist = get_stat_score(pos, list.moves[i], sw, ply);
                reduction -= hist / 4000;
                
                reduction = std::max(0, reduction);
                int reduced_depth = std::max(1, depth - 1 + current_extension - reduction);
                val = -negamax(next_pos, reduced_depth, ply + 1, -alpha - 1, -alpha, false, sw, list.moves[i]);
                if (val > alpha && reduced_depth < depth - 1 + current_extension) {
                    val = -negamax(next_pos, depth - 1 + current_extension, ply + 1, -alpha - 1, -alpha, false, sw, list.moves[i]);
                }
            } else {
                val = -negamax(next_pos, depth - 1 + current_extension, ply + 1, -alpha - 1, -alpha, false, sw, list.moves[i]);
            }

            if (val > alpha && val < beta) {
                val = -negamax(next_pos, depth - 1 + current_extension, ply + 1, -beta, -alpha, false, sw, list.moves[i]);
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
            if (excluded_move == MOVE_NONE) {
                if (is_quiet) {
                    if (list.moves[i] != sw.killer_moves[ply][0]) {
                        sw.killer_moves[ply][1] = sw.killer_moves[ply][0];
                        sw.killer_moves[ply][0] = list.moves[i];
                    }
                    int bonus = depth * depth;
                    if (bonus > 400) bonus = 400; // Cap bonus
                    
                    int us = static_cast<int>(pos.sideToMove);
                    int from = static_cast<int>(move_from(list.moves[i]));
                    int to = static_cast<int>(move_to(list.moves[i]));
                    int p = static_cast<int>(pos.piece_on(static_cast<Square>(from)));
                    
                    sw.history[us][from][to] += bonus - sw.history[us][from][to] * abs(bonus) / 16384;
                    if (ply >= 1 && sw.played_moves[ply - 1] != MOVE_NONE) {
                        int prev_p = static_cast<int>(sw.played_pieces[ply - 1]);
                        int prev_to = static_cast<int>(move_to(sw.played_moves[ply - 1]));
                        sw.continuation_history[0][prev_p][prev_to][p][to] += bonus - sw.continuation_history[0][prev_p][prev_to][p][to] * abs(bonus) / 16384;
                    }
                    if (ply >= 2 && sw.played_moves[ply - 2] != MOVE_NONE) {
                        int prev2_p = static_cast<int>(sw.played_pieces[ply - 2]);
                        int prev2_to = static_cast<int>(move_to(sw.played_moves[ply - 2]));
                        sw.continuation_history[1][prev2_p][prev2_to][p][to] += bonus - sw.continuation_history[1][prev2_p][prev2_to][p][to] * abs(bonus) / 16384;
                    }

                    for (int q = 0; q < quiet_count - 1; ++q) {
                        int q_from = static_cast<int>(move_from(quiets_searched[q]));
                        int q_to = static_cast<int>(move_to(quiets_searched[q]));
                        int q_p = static_cast<int>(pos.piece_on(static_cast<Square>(q_from)));
                        sw.history[us][q_from][q_to] -= bonus + sw.history[us][q_from][q_to] * abs(bonus) / 16384;
                        
                        if (ply >= 1 && sw.played_moves[ply - 1] != MOVE_NONE) {
                            int prev_p = static_cast<int>(sw.played_pieces[ply - 1]);
                            int prev_to = static_cast<int>(move_to(sw.played_moves[ply - 1]));
                            sw.continuation_history[0][prev_p][prev_to][q_p][q_to] -= bonus + sw.continuation_history[0][prev_p][prev_to][q_p][q_to] * abs(bonus) / 16384;
                        }
                        if (ply >= 2 && sw.played_moves[ply - 2] != MOVE_NONE) {
                            int prev2_p = static_cast<int>(sw.played_pieces[ply - 2]);
                            int prev2_to = static_cast<int>(move_to(sw.played_moves[ply - 2]));
                            sw.continuation_history[1][prev2_p][prev2_to][q_p][q_to] -= bonus + sw.continuation_history[1][prev2_p][prev2_to][q_p][q_to] * abs(bonus) / 16384;
                        }
                    }
                    if (prev_move != MOVE_NONE) {
                        sw.counter_moves[static_cast<int>(move_from(prev_move))][static_cast<int>(move_to(prev_move))] = list.moves[i];
                    }
                } else {
                    int bonus = depth * depth;
                    if (bonus > 400) bonus = 400; // Cap bonus
                    int a = static_cast<int>(pos.piece_on(move_from(list.moves[i])));
                    int to = static_cast<int>(move_to(list.moves[i]));
                    int v = static_cast<int>(pos.piece_on(static_cast<Square>(to)));
                    sw.capture_history[a][to][v] += bonus - sw.capture_history[a][to][v] * abs(bonus) / 16384;
                }
                Value tt_store_value = best_value;
                if (tt_store_value >= VALUE_MATE - 500) tt_store_value += ply;
                else if (tt_store_value <= -VALUE_MATE + 500) tt_store_value -= ply;
                record_tt(pos.zobristKey, depth, tt_store_value, flag, list.moves[i]);
            }
            best_move = list.moves[i];
            break;
        }
    }

    // Terminal-node detection: checkmate or stalemate
    if (legal_moves == 0) {
        if (in_check) {
            best_value = -(VALUE_MATE - ply);
            flag = TT_EXACT;
        } else {
            best_value = VALUE_DRAW;
            flag = TT_EXACT;
        }
    } else if (best_value == -VALUE_INFINITE) {
        // All legal moves were pruned. Return fail-low.
        best_value = alpha;
    }

    // Ply-correct mate scores before storing in TT
    Value tt_store_value = best_value;
    if (tt_store_value >= VALUE_MATE - 500) tt_store_value += ply;
    else if (tt_store_value <= -VALUE_MATE + 500) tt_store_value -= ply;

    if (excluded_move == MOVE_NONE) {
        record_tt(pos.zobristKey, depth, tt_store_value, flag, best_move);
    }
    return best_value;
}

// ---------------------------------------------------------------------------
//  Root Search (Iterative Deepening)
// ---------------------------------------------------------------------------
Move search_position(Position& pos, int max_depth, int thread_id) {
    Move best_root_move = MOVE_NONE;
    Move last_best_move = MOVE_NONE;
    Value last_depth_score = -VALUE_INFINITE;
    SearchWorker& sw = ThreadPool::threads[thread_id]->sw;
    sw.node_count = 0;
    Value prev_score = 0;

    for (int d = 1; d <= max_depth; ++d) {
        if (TimeManager::stop_search) break;

        Value alpha = -VALUE_MATE_IN_1;
        Value beta = VALUE_MATE_IN_1;
        int delta = 50;

        if (d >= 5) {
            alpha = std::max(static_cast<Value>(-VALUE_MATE_IN_1), static_cast<Value>(prev_score - delta));
            beta = std::min(static_cast<Value>(VALUE_MATE_IN_1), static_cast<Value>(prev_score + delta));
        }

        Value best_value = -VALUE_INFINITE;

        while (true) {
            Value alpha_orig = alpha;
            Value beta_orig = beta;

            MoveList list;
            MoveGen::generate_legal_moves(pos, list);
            sort_moves(pos, list, best_root_move, sw, 0, MOVE_NONE); // Use best_root_move instead of MOVE_NONE for TT move

            // Lazy SMP: perturb root move order for helper threads to avoid TT lock contention
            if (thread_id > 0 && list.size() > 1) {
                int shift = thread_id % list.size();
                std::rotate(list.moves, list.moves + shift, list.moves + list.size());
            }

            best_value = -VALUE_INFINITE;
            int legal_moves = 0;

            for (int i = 0; i < list.size(); ++i) {
                Position next_pos = pos;
                if (!next_pos.make_move(list.moves[i])) continue;

                NnueGuard guard(list.moves[i]);
                Value val;
                if (legal_moves == 0) {
                    val = -negamax(next_pos, d - 1, 1, -beta, -alpha, false, sw, list.moves[i]);
                } else {
                    val = -negamax(next_pos, d - 1, 1, -alpha - 1, -alpha, false, sw, list.moves[i]);
                    if (val > alpha && val < beta) {
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

            if (best_value <= alpha_orig && alpha_orig != -VALUE_MATE_IN_1) {
                alpha = std::max(static_cast<Value>(-VALUE_MATE_IN_1), static_cast<Value>(alpha_orig - delta));
                delta += delta / 2;
                continue;
            }
            if (best_value >= beta_orig && beta_orig != VALUE_MATE_IN_1) {
                beta = std::min(static_cast<Value>(VALUE_MATE_IN_1), static_cast<Value>(beta_orig + delta));
                delta += delta / 2;
                continue;
            }

            prev_score = best_value;
            break;
        }

        if (TimeManager::stop_search) {
            break;
        }

        if (thread_id == 0) {
            std::string pv_str;
            for (int j = 0; j < sw.pv_length[0]; ++j) {
                pv_str += " " + move_to_str(sw.pv_array[0][j]);
            }

            uint64_t total_nodes = 0;
            for (auto* t : ThreadPool::threads) {
                total_nodes += t->sw.node_count;
            }

            uint64_t elapsed = TimeManager::get_time_ms() - TimeManager::start_time;
            uint64_t nps = (elapsed > 0) ? (total_nodes * 1000) / elapsed : 0;
            
            std::string score_str;
            if (is_mate(best_value)) {
                int plies = mate_distance(best_value);
                int moves = (plies + 1) / 2;
                score_str = "score mate " + std::to_string(best_value < 0 ? -moves : moves);
            } else {
                score_str = "score cp " + std::to_string(to_cp(best_value, pos));
            }

            std::cout << "info depth " << d
                       << " " << score_str
                       << " time " << elapsed
                       << " nodes " << total_nodes
                       << " nps " << nps
                       << " hashfull " << get_hashfull()
                       << " pv" << pv_str << std::endl;

            best_root_move = sw.pv_array[0][0];

            if (d >= 5 && thread_id == 0) {
                if (best_root_move != last_best_move) {
                    TimeManager::extend_time_for_instability();
                }
                if (last_depth_score != -VALUE_INFINITE && best_value < last_depth_score - 50) {
                    TimeManager::extend_time_for_score_drop();
                }
            }
            last_best_move = best_root_move;
            last_depth_score = best_value;
            
            // Time check for normal moves (evaluate SOFT limit at root)
            if (thread_id == 0 && TimeManager::optimum_time != 999999999) {
                TimeManager::check_time_at_root();
                if (TimeManager::stop_search) {
                    break;
                }
            }
        } else {
            best_root_move = sw.pv_array[0][0];
        }

        // Early termination: Only break if it's a guaranteed Mate-in-1
        if (best_value == VALUE_MATE_IN_1 || best_value == -VALUE_MATE_IN_1) {
            break;
        }
    }

    if (thread_id == 0) {
        TimeManager::stop_search = true;
        std::cout.flush();
    }

    return best_root_move;
}
