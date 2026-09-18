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
#include "stockfish_probe/sf_wrapper.h"

struct NnueGuard {
    int move;
    bool is_null;
    NnueGuard(int m, bool null_move = false) : move(m), is_null(null_move) {
        if (is_null) StockfishWrapper::do_null_move();
        else StockfishWrapper::do_move(move);
    }
    ~NnueGuard() {
        if (is_null) StockfishWrapper::undo_null_move();
        else StockfishWrapper::undo_move(move);
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
            if (d >= 2 && m >= 2) {
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
    if (ply >= 4 && sw.played_moves[ply - 4] != MOVE_NONE) {
        int prev4_p = static_cast<int>(sw.played_pieces[ply - 4]);
        int prev4_to = static_cast<int>(move_to(sw.played_moves[ply - 4]));
        score += sw.continuation_history[2][prev4_p][prev4_to][p][to];
    }
    if (ply >= 6 && sw.played_moves[ply - 6] != MOVE_NONE) {
        int prev6_p = static_cast<int>(sw.played_pieces[ply - 6]);
        int prev6_to = static_cast<int>(move_to(sw.played_moves[ply - 6]));
        score += sw.continuation_history[3][prev6_p][prev6_to][p][to];
    }
    return score;
}

static int score_move(const Position& pos, Move m, Move tt_move, const SearchWorker& sw, int ply, Move prev_move) {
    if (m == tt_move) return 2000000;

    Piece victim = pos.piece_on(move_to(m));
    if (move_flag(m) == MOVE_FLAG_ENPASSANT) {
        victim = Piece::WHITE_PAWN; // exact color doesn't matter for piece_type(victim)
    }
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

class MovePicker {
    const Position& pos;
    const SearchWorker& sw;
    int ply;
    Move prev_move;
    Move tt_move;
    
    MoveList list;
    int scores[MAX_MOVES];
    int current_idx;
    
public:
    MovePicker(const Position& p, const SearchWorker& s, int pl, Move prev, Move tt, bool qsearch = false)
      : pos(p), sw(s), ply(pl), prev_move(prev), tt_move(tt), current_idx(0) {
        if (qsearch && !pos.is_attacked(pos.get_king_square(pos.sideToMove), ~pos.sideToMove)) {
            MoveGen::generate_noisy_moves(pos, list);
        } else {
            MoveGen::generate_legal_moves(pos, list);
        }
        for (int i = 0; i < list.size(); ++i) scores[i] = -2000000;
    }
    
    Move next() {
        if (current_idx >= list.size()) return MOVE_NONE;
        
        int best_score = -3000000;
        int best_idx = current_idx;
        
        for (int i = current_idx; i < list.size(); ++i) {
            if (scores[i] == -2000000) {
                scores[i] = score_move(pos, list.moves[i], tt_move, sw, ply, prev_move);
            }
            if (scores[i] > best_score) {
                best_score = scores[i];
                best_idx = i;
            }
        }
        
        Move best_move = list.moves[best_idx];
        list.moves[best_idx] = list.moves[current_idx];
        scores[best_idx] = scores[current_idx];
        
        current_idx++;
        return best_move;
    }
    
    int size() const { return list.size(); }
};


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

    PieceType cap_type = piece_type(pos.piece_on(to));
    if (cap_type == PieceType::NONE && move_flag(m) == MOVE_FLAG_ENPASSANT) cap_type = PieceType::PAWN;
    int swap = PieceValues[static_cast<int>(cap_type)] - threshold;
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
    if (ply >= 127) return evaluate(pos, true);
    if (ply > 0 && is_repetition(pos, ply, sw)) return 0;
    
    sw.search_history[ply] = pos.zobristKey;

    bool in_check = pos.is_attacked(pos.get_king_square(pos.sideToMove), ~pos.sideToMove);

    Value tt_score;
    Move tt_move = MOVE_NONE;
    TTFlag tt_flag;
    int tt_depth = 0;
    if (probe_tt(pos.zobristKey, 0, alpha, beta, tt_score, tt_move, tt_flag, tt_depth)) {
        // Ply-adjust mate scores from TT
        if (tt_score >= VALUE_MATE - 500) tt_score -= ply;
        else if (tt_score <= -VALUE_MATE + 500) tt_score += ply;

        if (tt_flag == TT_EXACT) return tt_score;
        if (tt_flag == TT_ALPHA && tt_score <= alpha) return tt_score;
        if (tt_flag == TT_BETA && tt_score >= beta) return tt_score;
    }

    Value stand_pat = -VALUE_INFINITE;
    if (!in_check) {
        stand_pat = evaluate(pos, true); // Force Small NNUE for speed in QS
        if (stand_pat >= beta) {
            record_tt(pos.zobristKey, 0, stand_pat, TT_BETA, MOVE_NONE);
            return beta;
        }
        if (stand_pat > alpha) alpha = stand_pat;
    }

    MovePicker picker(pos, sw, ply, MOVE_NONE, tt_move, true);

    // Terminal-node detection in QS: if in check and no evasions => checkmate
    if (in_check && picker.size() == 0) {
        return -(VALUE_MATE - ply);
    }

    Value best_value = stand_pat;
    Move best_move = MOVE_NONE;
    TTFlag flag = TT_ALPHA;
    int legal_moves = 0;
    Move move;

    bool pruned_evasions = false;
    while ((move = picker.next()) != MOVE_NONE) {
        if (!in_check && pos.piece_on(move_to(move)) == Piece::PIECE_NONE
         && move_prom(move) == PieceType::NONE && move_flag(move) != MOVE_FLAG_ENPASSANT) continue;

        // Delta Pruning
        if (!in_check && move_prom(move) == PieceType::NONE) {
            PieceType cap_type = piece_type(pos.piece_on(move_to(move)));
            if (cap_type == PieceType::NONE && move_flag(move) == MOVE_FLAG_ENPASSANT) cap_type = PieceType::PAWN;
            int captured_val = PieceValues[static_cast<int>(cap_type)];
            // Margin is 200 centipawns, which is ~656 internal units (200 * 3.28)
            if (stand_pat + captured_val + 656 < alpha) {
                continue;
            }
        }

        // Prune bad captures and bad evasions
        if (!see_ge(pos, move, 0)) {
            pruned_evasions = true;
            continue;
        }

        Position next_pos = pos;
        if (!next_pos.make_move(move)) continue;

        legal_moves++;

        NnueGuard guard(move);
        Value val = -quiescence(next_pos, -beta, -alpha, ply + 1, sw);
        if (TimeManager::stop_search) return 0;

        if (val > best_value) {
            best_value = val;
            best_move = move;
        }
        if (val > alpha) {
            alpha = val;
            flag = TT_EXACT;
        }
        if (alpha >= beta) {
            flag = TT_BETA;
            best_move = move;
            break;
        }
    }

    if (in_check && legal_moves == 0) {
        if (!pruned_evasions) {
            return -(VALUE_MATE - ply);
        } else {
            return alpha; // All evasions were bad (SEE < 0), return alpha to fail low
        }
    }

    // Ply-adjust mate scores before storing in TT
    Value qs_store = best_value;
    if (qs_store >= VALUE_MATE - 500) qs_store += ply;
    else if (qs_store <= -VALUE_MATE + 500) qs_store -= ply;
    record_tt(pos.zobristKey, 0, qs_store, flag, best_move);
    return best_value;
}

// ---------------------------------------------------------------------------
//  Negamax Search
// ---------------------------------------------------------------------------
static Value negamax(Position& pos, int depth, int ply, Value alpha, Value beta, bool is_null, SearchWorker& sw, Move prev_move = MOVE_NONE, Move excluded_move = MOVE_NONE) {
    if (ply >= 127) {
        return evaluate(pos, false);
    }

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
    Value raw_static_eval = evaluate(pos, false);
    Value static_eval = raw_static_eval;
    if (!in_check) {
        int ch_idx = pos.pawnKey & 16383;
        int offset = sw.correction_history[static_cast<int>(pos.sideToMove)][ch_idx];
        static_eval = std::clamp(static_eval + offset / 256, -VALUE_MATE + 1, VALUE_MATE - 1);
    }
    sw.static_evals[ply] = static_eval;

    bool improving = false;
    if (ply >= 2 && !in_check) {
        improving = (static_eval >= sw.static_evals[ply - 2]);
    }

    Move tt_move = MOVE_NONE;
    Value tt_score = VALUE_ZERO;
    TTFlag tt_flag = TT_EXACT;
    int tt_depth = 0;
    bool singular_extension = false;
    int double_extension = 0;

    bool tt_hit = probe_tt(pos.zobristKey, depth, alpha, beta, tt_score, tt_move, tt_flag, tt_depth);
    if (excluded_move == MOVE_NONE && ply > 0 && tt_hit) {
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
    // Ensure we have a valid lower bound from a sufficient depth
    if (depth >= 6 && tt_move != MOVE_NONE && excluded_move == MOVE_NONE 
        && tt_depth >= depth - 3 && tt_flag != TT_ALPHA && value_abs(tt_score) < VALUE_MATE_IN_2) {
        
        int se_depth = (depth - 1) / 2;
        Value se_beta = tt_score - depth * 2;
        // Search SAME position but excluded_move=tt_move. 
        // We do NOT negate the result, because it's still from our perspective!
        // We use window [se_beta - 1, se_beta]
        Value se_score = negamax(pos, se_depth, ply, se_beta - 1, se_beta, true, sw, MOVE_NONE, tt_move);
        
        if (TimeManager::stop_search) return 0;
        if (se_score < se_beta) {
            singular_extension = true;
            if (se_score < se_beta - 20) double_extension = 1;
        }
    }

    // Reverse Futility Pruning (Static NMP)
    if (!pv_node && !is_null && depth <= 8 && !in_check && abs(beta) < VALUE_MATE - 500) {
        int rfp_margin = improving ? depth * 75 : depth * 120;
        if (static_eval - rfp_margin >= beta) {
            return static_eval;
        }
    }






    // Razoring
    if (!pv_node && !is_null && depth <= 3 && !in_check && abs(beta) < VALUE_MATE - 500) {
        int razor_margin = 300 + (depth - 1) * 100;
        if (static_eval + razor_margin <= alpha) {
            Value qval = quiescence(pos, alpha, beta, ply, sw);
            if (qval <= alpha) return qval;
        }
    }

    // Dynamic Null Move Pruning
    if (!pv_node && !is_null && depth >= 2 && ply > 0 && static_eval >= beta && !in_check) {
        if ((pos.pieces(pos.sideToMove) & ~(pos.pieces(PieceType::PAWN) | pos.pieces(PieceType::KING))).bb != 0) {
            int eval_margin = (static_eval - beta) / 200;
            if (eval_margin > 3) eval_margin = 3;
            int r = 3 + depth / 4 + eval_margin;
            int nmp_depth = depth - r - 1;
            if (nmp_depth < 0) nmp_depth = 0;
            
            Position null_pos = pos;
            null_pos.make_null_move();
            NnueGuard guard(0, true);
            Value null_val = -negamax(null_pos, nmp_depth, ply + 1, -beta, -beta + 1, true, sw, MOVE_NONE);
            if (TimeManager::stop_search) return 0;
            if (null_val >= beta) return beta;
        }
    }

    // Internal Iterative Reduction (IIR) - replaces wasteful IID
    // When we have no TT move, reduce depth instead of doing a full sub-search.
    // This saves enormous amounts of nodes compared to IID.
    if (depth >= 4 && tt_move == MOVE_NONE && !is_null) {
        depth -= (pv_node ? 1 : 2);
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
    Move captures_searched[MAX_MOVES];
    int capture_count = 0;

    for (int i = 0; i < list.size(); ++i) {
        if (list.moves[i] == excluded_move) continue;

        Position next_pos = pos;
        if (!next_pos.make_move(list.moves[i])) continue;

        NnueGuard guard(list.moves[i]);
        legal_moves++;

        sw.played_moves[ply] = list.moves[i];
        sw.played_pieces[ply] = pos.piece_on(move_from(list.moves[i]));

        // Compute gives_check EARLY so pruning can use it
        bool gives_check = next_pos.is_attacked(
            next_pos.get_king_square(next_pos.sideToMove), ~next_pos.sideToMove);

        bool is_quiet = (pos.piece_on(move_to(list.moves[i])) == Piece::PIECE_NONE
                      && move_prom(list.moves[i]) == PieceType::NONE);

        bool is_killer = (list.moves[i] == sw.killer_moves[ply][0] || list.moves[i] == sw.killer_moves[ply][1]);

        // Late Move Pruning (LMP) — extended to depth 8 with Stockfish-style thresholds
        if (!pv_node && depth <= 8 && !in_check && is_quiet && !is_killer && !gives_check
            && abs(best_value) < VALUE_MATE - 500) {
            int lmp_threshold = (3 + depth * depth) / (2 - improving);
            if (legal_moves > lmp_threshold) continue;
        }

        // History Pruning — extended to depth 8
        if (!pv_node && depth <= 8 && is_quiet && !is_killer && !gives_check) {
            int hist = get_stat_score(pos, list.moves[i], sw, ply);
            if (hist < -2000 * depth) continue;
        }

        // SEE Pruning for captures — extended to depth 9
        if (!pv_node && depth <= 9 && !in_check && !is_killer && !is_quiet) {
            int see_threshold = -80 * depth * depth;
            if (!see_ge(pos, list.moves[i], see_threshold)) continue;
        }

        // SEE Pruning for quiet moves — prune moves where piece is hanging
        if (!pv_node && depth <= 8 && !in_check && !is_killer && is_quiet && !gives_check) {
            if (!see_ge(pos, list.moves[i], -50 * depth)) continue;
        }

        // Futility Pruning — tightened margins
        if (!pv_node && depth <= 9 && is_quiet && !is_killer && !in_check && !gives_check
            && abs(alpha) < VALUE_MATE - 500) {
            int hist = get_stat_score(pos, list.moves[i], sw, ply);
            int fp_margin = depth * 90 + std::max(0, hist / 256);
            if (static_eval + fp_margin <= alpha) continue;
        }

        if (is_quiet) {
            quiets_searched[quiet_count++] = list.moves[i];
        } else {
            captures_searched[capture_count++] = list.moves[i];
        }

        // === Extensions ===
        int current_extension = 0;

        // Check extension
        if (gives_check) {
            current_extension = 1;
        }

        // Passed pawn extension (pawn push to 6th or 7th rank)
        if (current_extension == 0) {
            PieceType moving_pt = piece_type(pos.piece_on(move_from(list.moves[i])));
            if (moving_pt == PieceType::PAWN) {
                int to_rank = static_cast<int>(move_to(list.moves[i])) / 8;
                bool advanced = (pos.sideToMove == Color::WHITE && to_rank >= 5) ||
                                (pos.sideToMove == Color::BLACK && to_rank <= 2);
                if (advanced) current_extension = 1;
            }
        }

        // Singular extension
        if (singular_extension) {
            if (list.moves[i] == tt_move) {
                current_extension = std::max(current_extension, 1 + double_extension);
            } else if (!gives_check) {
                current_extension = -1;
            }
        }

        Value val;
        if (legal_moves == 1) {
            val = -negamax(next_pos, depth - 1 + current_extension, ply + 1, -beta, -alpha, false, sw, list.moves[i]);
            if (TimeManager::stop_search) return 0;
        } else {
            // LMR: Late Move Reductions
            int reduction = 0;
            bool do_lmr = false;

            if (depth >= 2 && legal_moves >= 2) {
                if (is_quiet) {
                    // Quiet move LMR
                    reduction = LMRTable[std::min(depth, 63)][std::min(legal_moves, 63)];

                    // Adjustments
                    if (pv_node) reduction -= 1;
                    if (is_killer) reduction -= 2;
                    if (!improving) reduction += 1;
                    if (gives_check) reduction -= 1;

                    // History-based adjustment
                    int hist = get_stat_score(pos, list.moves[i], sw, ply);
                    int hist_reduction = hist / 8192;
                    reduction -= hist_reduction;
                    
                    if (hist < -10000) reduction += 1;
                    if (hist > 10000) reduction -= 1;

                    // Clamp: never reduce to depth < 1
                    reduction = std::max(0, std::min(reduction, depth - 2));
                    do_lmr = (reduction > 0);

                } else if (legal_moves >= 4 && !gives_check) {
                    // Capture LMR: reduce captures with bad capture history
                    int a = static_cast<int>(pos.piece_on(move_from(list.moves[i])));
                    int to = static_cast<int>(move_to(list.moves[i]));
                    int v = static_cast<int>(pos.piece_on(static_cast<Square>(to)));
                    if (v != static_cast<int>(Piece::PIECE_NONE)) {
                        int cap_hist = sw.capture_history[a][to][v];
                        if (cap_hist < -2000) {
                            reduction = 1;
                            do_lmr = true;
                        }
                    }
                }
            }

            if (do_lmr) {
                int reduced_depth = std::max(1, depth - 1 + current_extension - reduction);
                val = -negamax(next_pos, reduced_depth, ply + 1, -alpha - 1, -alpha, false, sw, list.moves[i]);
                if (TimeManager::stop_search) return 0;
                if (val > alpha && reduced_depth < depth - 1 + current_extension) {
                    val = -negamax(next_pos, depth - 1 + current_extension, ply + 1, -alpha - 1, -alpha, false, sw, list.moves[i]);
                    if (TimeManager::stop_search) return 0;
                }
            } else {
                val = -negamax(next_pos, depth - 1 + current_extension, ply + 1, -alpha - 1, -alpha, false, sw, list.moves[i]);
                if (TimeManager::stop_search) return 0;
            }

            if (val > alpha && val < beta) {
                val = -negamax(next_pos, depth - 1 + current_extension, ply + 1, -beta, -alpha, false, sw, list.moves[i]);
                if (TimeManager::stop_search) return 0;
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
                    int bonus = std::min(16 * depth * depth + 120, 1800);
                    
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
                    if (ply >= 4 && sw.played_moves[ply - 4] != MOVE_NONE) {
                        int prev4_p = static_cast<int>(sw.played_pieces[ply - 4]);
                        int prev4_to = static_cast<int>(move_to(sw.played_moves[ply - 4]));
                        sw.continuation_history[2][prev4_p][prev4_to][p][to] += bonus - sw.continuation_history[2][prev4_p][prev4_to][p][to] * abs(bonus) / 16384;
                    }
                    if (ply >= 6 && sw.played_moves[ply - 6] != MOVE_NONE) {
                        int prev6_p = static_cast<int>(sw.played_pieces[ply - 6]);
                        int prev6_to = static_cast<int>(move_to(sw.played_moves[ply - 6]));
                        sw.continuation_history[3][prev6_p][prev6_to][p][to] += bonus - sw.continuation_history[3][prev6_p][prev6_to][p][to] * abs(bonus) / 16384;
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
                        if (ply >= 4 && sw.played_moves[ply - 4] != MOVE_NONE) {
                            int prev4_p = static_cast<int>(sw.played_pieces[ply - 4]);
                            int prev4_to = static_cast<int>(move_to(sw.played_moves[ply - 4]));
                            sw.continuation_history[2][prev4_p][prev4_to][q_p][q_to] -= bonus + sw.continuation_history[2][prev4_p][prev4_to][q_p][q_to] * abs(bonus) / 16384;
                        }
                        if (ply >= 6 && sw.played_moves[ply - 6] != MOVE_NONE) {
                            int prev6_p = static_cast<int>(sw.played_pieces[ply - 6]);
                            int prev6_to = static_cast<int>(move_to(sw.played_moves[ply - 6]));
                            sw.continuation_history[3][prev6_p][prev6_to][q_p][q_to] -= bonus + sw.continuation_history[3][prev6_p][prev6_to][q_p][q_to] * abs(bonus) / 16384;
                        }
                    }
                    for (int c = 0; c < capture_count; ++c) {
                        int c_from = static_cast<int>(move_from(captures_searched[c]));
                        int c_to = static_cast<int>(move_to(captures_searched[c]));
                        int c_a = static_cast<int>(pos.piece_on(static_cast<Square>(c_from)));
                        int c_v = static_cast<int>(pos.piece_on(static_cast<Square>(c_to)));
                        if (c_v == static_cast<int>(Piece::PIECE_NONE) && move_flag(captures_searched[c]) == MOVE_FLAG_ENPASSANT) {
                            c_v = static_cast<int>(pos.sideToMove == Color::WHITE ? Piece::BLACK_PAWN : Piece::WHITE_PAWN);
                        }
                        sw.capture_history[c_a][c_to][c_v] -= bonus + sw.capture_history[c_a][c_to][c_v] * abs(bonus) / 16384;
                    }

                    if (prev_move != MOVE_NONE) {
                        sw.counter_moves[static_cast<int>(move_from(prev_move))][static_cast<int>(move_to(prev_move))] = list.moves[i];
                    }
                } else {
                    int bonus = std::min(16 * depth * depth + 120, 1800);
                    int a = static_cast<int>(pos.piece_on(move_from(list.moves[i])));
                    int to = static_cast<int>(move_to(list.moves[i]));
                    int v = static_cast<int>(pos.piece_on(static_cast<Square>(to)));
                    if (v == static_cast<int>(Piece::PIECE_NONE) && move_flag(list.moves[i]) == MOVE_FLAG_ENPASSANT) {
                        v = static_cast<int>(pos.sideToMove == Color::WHITE ? Piece::BLACK_PAWN : Piece::WHITE_PAWN);
                    }
                    sw.capture_history[a][to][v] += bonus - sw.capture_history[a][to][v] * abs(bonus) / 16384;
                    
                    for (int c = 0; c < capture_count - 1; ++c) {
                        int c_from = static_cast<int>(move_from(captures_searched[c]));
                        int c_to = static_cast<int>(move_to(captures_searched[c]));
                        int c_a = static_cast<int>(pos.piece_on(static_cast<Square>(c_from)));
                        int c_v = static_cast<int>(pos.piece_on(static_cast<Square>(c_to)));
                        if (c_v == static_cast<int>(Piece::PIECE_NONE) && move_flag(captures_searched[c]) == MOVE_FLAG_ENPASSANT) {
                            c_v = static_cast<int>(pos.sideToMove == Color::WHITE ? Piece::BLACK_PAWN : Piece::WHITE_PAWN);
                        }
                        sw.capture_history[c_a][c_to][c_v] -= bonus + sw.capture_history[c_a][c_to][c_v] * abs(bonus) / 16384;
                    }
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
        if (!in_check && abs(best_value) < VALUE_MATE - 500 && depth >= 0) {
            int bonus = best_value - raw_static_eval;
            bonus = std::clamp(bonus, -1000, 1000); // cap diff
            int weight = depth < 16 ? depth : 16;
            int ch_idx = pos.pawnKey & 16383;
            int& ch = sw.correction_history[static_cast<int>(pos.sideToMove)][ch_idx];
            ch = (ch * (256 - weight) + bonus * 256 * weight) / 256;
            ch = std::clamp(ch, -32000, 32000);
        }
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

    if (thread_id == 0 && TB_LARGEST > 0) {
        int pieces = __builtin_popcountll(pos.byColorBB[0].bb) + __builtin_popcountll(pos.byColorBB[1].bb);
        if (pieces <= (int)TB_LARGEST && pos.castlingRights == 0 && pos.halfmoveClock == 0) {
            unsigned results[TB_MAX_MOVES];
            unsigned res = tb_probe_root(
                pos.byColorBB[0].bb, pos.byColorBB[1].bb,
                pos.byTypeBB[6].bb, pos.byTypeBB[5].bb, pos.byTypeBB[4].bb,
                pos.byTypeBB[3].bb, pos.byTypeBB[2].bb, pos.byTypeBB[1].bb,
                pos.halfmoveClock, pos.castlingRights, pos.epSquare == Square::SQ_NONE ? 0 : static_cast<unsigned>(pos.epSquare),
                pos.sideToMove == Color::WHITE, results
            );

            if (res != TB_RESULT_FAILED) {
                Square from = static_cast<Square>(TB_GET_FROM(res));
                Square to = static_cast<Square>(TB_GET_TO(res));
                unsigned prom = TB_GET_PROMOTES(res);
                PieceType prom_piece = PieceType::NONE;
                if (prom == TB_PROMOTES_QUEEN) prom_piece = PieceType::QUEEN;
                else if (prom == TB_PROMOTES_ROOK) prom_piece = PieceType::ROOK;
                else if (prom == TB_PROMOTES_BISHOP) prom_piece = PieceType::BISHOP;
                else if (prom == TB_PROMOTES_KNIGHT) prom_piece = PieceType::KNIGHT;
                
                MoveList list;
                MoveGen::generate_legal_moves(pos, list);
                for (int i = 0; i < list.size(); ++i) {
                    if (move_from(list.moves[i]) == from && move_to(list.moves[i]) == to && move_prom(list.moves[i]) == prom_piece) {
                        best_root_move = list.moves[i];
                        break;
                    }
                }
                
                if (best_root_move != MOVE_NONE) {
                    unsigned wdl = TB_GET_WDL(res);
                    Value score = 0;
                    if (wdl == TB_WIN) score = VALUE_MATE_IN_1 - 1;
                    else if (wdl == TB_LOSS) score = -VALUE_MATE_IN_1 + 1;
                    
                    std::cout << "info depth 1 score cp " << score << " time 0 nodes 0 nps 0 hashfull 0 pv " << move_to_str(best_root_move) << std::endl;
                    TimeManager::stop_search = true;
                    return best_root_move;
                }
            }
        }
    }


    uint64_t root_move_nodes[MAX_MOVES] = {0};
    Move root_moves[MAX_MOVES] = {MOVE_NONE};
    int root_move_count = 0;

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
                if (d == 1 && root_move_count < MAX_MOVES) {
                    root_moves[root_move_count++] = list.moves[i];
                }
                
                Position next_pos = pos;
                if (!next_pos.make_move(list.moves[i])) continue;

                uint64_t nodes_before = sw.node_count;
                NnueGuard guard(list.moves[i]);
                Value val;
                if (legal_moves == 0) {
                    val = -negamax(next_pos, d - 1, 1, -beta, -alpha, false, sw, list.moves[i]);
                    if (TimeManager::stop_search) break;
                } else {
                    val = -negamax(next_pos, d - 1, 1, -alpha - 1, -alpha, false, sw, list.moves[i]);
                    if (TimeManager::stop_search) break;
                    if (val > alpha && val < beta) {
                        val = -negamax(next_pos, d - 1, 1, -beta, -alpha, false, sw, list.moves[i]);
                        if (TimeManager::stop_search) break;
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


                uint64_t nodes_spent = sw.node_count - nodes_before;
                for (int m_idx = 0; m_idx < root_move_count; ++m_idx) {
                    if (root_moves[m_idx] == list.moves[i]) {
                        root_move_nodes[m_idx] += nodes_spent;
                        break;
                    }
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
                uint64_t best_nodes = 0;
                for (int m_idx = 0; m_idx < root_move_count; ++m_idx) {
                    if (root_moves[m_idx] == best_root_move) {
                        best_nodes = root_move_nodes[m_idx];
                        break;
                    }
                }
                TimeManager::scale_time_for_nodes(best_nodes, sw.node_count);
                TimeManager::check_time_at_root();
                if (TimeManager::stop_search) {
                    break;
                }
            }
        } else {
            best_root_move = sw.pv_array[0][0];
        }

        // Early termination: break if we found a forced mate in 1
        if (value_abs(best_value) >= VALUE_MATE - 1) {
            break;
        }
    }

    if (thread_id == 0) {
        TimeManager::stop_search = true;
        std::cout.flush();
    }

    return best_root_move;
}
