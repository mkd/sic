#include "../include/movegen.h"
#include "../include/attacks.h"
#include "../include/geometry.h"

namespace MoveGen {

// ---------------------------------------------------------------------------
//  Leaper Move Generation (Knights & Kings)
// ---------------------------------------------------------------------------
template <Color Us>
void generate_leaper_moves(const Position& pos, MoveList& list) {
    const Bitboard our_pieces = pos.pieces(Us);
    const Bitboard target = ~our_pieces;

    // --- Knights ---
    Bitboard knights = our_pieces & pos.pieces(PieceType::KNIGHT);
    while (knights.bb) {
        const Square sq = pop_lsb(knights);
        Bitboard attacks = KNIGHT_ATTACKS[static_cast<int>(sq)] & target;
        while (attacks.bb) {
            const Square to = pop_lsb(attacks);
            list.push(make_move(sq, to));
        }
    }

    // --- Kings ---
    Bitboard kings = our_pieces & pos.pieces(PieceType::KING);
    while (kings.bb) {
        const Square sq = pop_lsb(kings);
        Bitboard attacks = KING_ATTACKS[static_cast<int>(sq)] & target;
        while (attacks.bb) {
            const Square to = pop_lsb(attacks);
            list.push(make_move(sq, to));
        }
    }
}

// ---------------------------------------------------------------------------
//  Slider Move Generation (Bishops, Rooks, Queens)
// ---------------------------------------------------------------------------
template <Color Us>
void generate_slider_moves(const Position& pos, MoveList& list) {
    const Bitboard our_pieces = pos.pieces(Us);
    const Bitboard occ = pos.occupied();
    const Bitboard target = ~our_pieces;

    // --- Bishops ---
    Bitboard bishops = our_pieces & pos.pieces(PieceType::BISHOP);
    while (bishops.bb) {
        const Square sq = pop_lsb(bishops);
        Bitboard attacks = get_bishop_attacks(sq, occ) & target;
        while (attacks.bb) {
            const Square to = pop_lsb(attacks);
            list.push(make_move(sq, to));
        }
    }

    // --- Rooks ---
    Bitboard rooks = our_pieces & pos.pieces(PieceType::ROOK);
    while (rooks.bb) {
        const Square sq = pop_lsb(rooks);
        Bitboard attacks = get_rook_attacks(sq, occ) & target;
        while (attacks.bb) {
            const Square to = pop_lsb(attacks);
            list.push(make_move(sq, to));
        }
    }

    // --- Queens (Bishop + Rook) ---
    Bitboard queens = our_pieces & pos.pieces(PieceType::QUEEN);
    while (queens.bb) {
        const Square sq = pop_lsb(queens);
        Bitboard attacks = (get_bishop_attacks(sq, occ) | get_rook_attacks(sq, occ)) & target;
        while (attacks.bb) {
            const Square to = pop_lsb(attacks);
            list.push(make_move(sq, to));
        }
    }
}

// ---------------------------------------------------------------------------
//  Rank Masks
// ---------------------------------------------------------------------------
constexpr Bitboard RANK_1_MASK = {0x00000000000000FFULL};
constexpr Bitboard RANK_3_MASK = {0x0000000000FF0000ULL};
constexpr Bitboard RANK_6_MASK = {0x0000FF0000000000ULL};
constexpr Bitboard RANK_8_MASK = {0xFF00000000000000ULL};

template <Color Us>
void generate_pawn_moves(const Position& pos, MoveList& list) {
    constexpr int Up = (Us == Color::WHITE) ? 8 : -8;
    constexpr Bitboard PromRankMask = (Us == Color::WHITE) ? RANK_8_MASK : RANK_1_MASK;
    constexpr Bitboard ThirdRankMask = (Us == Color::WHITE) ? RANK_3_MASK : RANK_6_MASK;

    const Bitboard pawns = pos.pieces(Us) & pos.pieces(PieceType::PAWN);
    const Bitboard empty = pos.empty_squares();
    const Bitboard enemies = pos.pieces(~Us);

    // --- Single pushes ---
    Bitboard single_push = (Us == Color::WHITE)
        ? ((pawns.bb << 8) & empty.bb)
        : ((pawns.bb >> 8) & empty.bb);

    Bitboard promo_push = {single_push.bb & PromRankMask.bb};
    Bitboard normal_push = {single_push.bb & ~PromRankMask.bb};

    while (normal_push.bb) {
        const Square to = pop_lsb(normal_push);
        const Square from = static_cast<Square>(static_cast<int>(to) - Up);
        list.push(make_move(from, to));
    }

    while (promo_push.bb) {
        const Square to = pop_lsb(promo_push);
        const Square from = static_cast<Square>(static_cast<int>(to) - Up);
        list.push(make_move(from, to, MOVE_FLAG_NORMAL, PieceType::QUEEN));
        list.push(make_move(from, to, MOVE_FLAG_NORMAL, PieceType::ROOK));
        list.push(make_move(from, to, MOVE_FLAG_NORMAL, PieceType::BISHOP));
        list.push(make_move(from, to, MOVE_FLAG_NORMAL, PieceType::KNIGHT));
    }

    // --- Double pushes ---
    // A double push is just a single push that happens to land on Rank 3 (White) or Rank 6 (Black)
    // shifted forward one more time into an empty square.
    Bitboard valid_for_double = {single_push.bb & ThirdRankMask.bb};
    Bitboard double_push = (Us == Color::WHITE)
        ? ((valid_for_double.bb << 8) & empty.bb)
        : ((valid_for_double.bb >> 8) & empty.bb);

    while (double_push.bb) {
        const Square to = pop_lsb(double_push);
        const Square from = static_cast<Square>(static_cast<int>(to) - Up - Up);
        list.push(make_move(from, to)); // We can add a double-push flag later if desired
    }

    // --- Captures ---
    Bitboard pawn_copy = pawns;
    while (pawn_copy.bb) {
        const Square sq = pop_lsb(pawn_copy);
        Bitboard attacks = {PAWN_ATTACKS[static_cast<int>(Us)][static_cast<int>(sq)].bb & enemies.bb};
        Bitboard promo_cap = {attacks.bb & PromRankMask.bb};
        Bitboard normal_cap = {attacks.bb & ~PromRankMask.bb};

        while (normal_cap.bb) {
            const Square to = pop_lsb(normal_cap);
            list.push(make_move(sq, to));
        }

        while (promo_cap.bb) {
            const Square to = pop_lsb(promo_cap);
            list.push(make_move(sq, to, MOVE_FLAG_NORMAL, PieceType::QUEEN));
            list.push(make_move(sq, to, MOVE_FLAG_NORMAL, PieceType::ROOK));
            list.push(make_move(sq, to, MOVE_FLAG_NORMAL, PieceType::BISHOP));
            list.push(make_move(sq, to, MOVE_FLAG_NORMAL, PieceType::KNIGHT));
        }
    }
}

// ---------------------------------------------------------------------------
//  Primary Entry Point
// ---------------------------------------------------------------------------
void generate_legal_moves(const Position& pos, MoveList& list) {
    list.clear();

    if (pos.sideToMove == Color::WHITE) {
        generate_leaper_moves<Color::WHITE>(pos, list);
        generate_slider_moves<Color::WHITE>(pos, list);
        generate_pawn_moves<Color::WHITE>(pos, list);
    } else {
        generate_leaper_moves<Color::BLACK>(pos, list);
        generate_slider_moves<Color::BLACK>(pos, list);
        generate_pawn_moves<Color::BLACK>(pos, list);
    }
}

} // namespace MoveGen
