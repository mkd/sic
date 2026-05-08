#include "../include/movegen.h"
#include "../include/attacks.h"

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
//  Primary Entry Point
// ---------------------------------------------------------------------------
void generate_legal_moves(const Position& pos, MoveList& list) {
    list.clear();

    if (pos.sideToMove == Color::WHITE) {
        generate_leaper_moves<Color::WHITE>(pos, list);
        generate_slider_moves<Color::WHITE>(pos, list);
    } else {
        generate_leaper_moves<Color::BLACK>(pos, list);
        generate_slider_moves<Color::BLACK>(pos, list);
    }
}

} // namespace MoveGen
