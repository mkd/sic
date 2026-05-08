#include "../include/position.h"
#include "../include/utils.h"
#include <iostream>
#include <cassert>

void test_position() {
    init_zobrist();

    Position pos;
    pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    // --- Piece counts ---
    assert(popcount(pos.pieces(Color::WHITE)) == 16);
    assert(popcount(pos.pieces(Color::BLACK)) == 16);
    assert(popcount(pos.pieces(PieceType::PAWN)) == 16);
    assert(popcount(pos.pieces(PieceType::KNIGHT)) == 4);

    // --- Specific piece placement ---
    assert(pos.piece_on(Square::SQ_E1) == Piece::WHITE_KING);
    assert(pos.piece_on(Square::SQ_E8) == Piece::BLACK_KING);
    assert(pos.piece_on(Square::SQ_A1) == Piece::WHITE_ROOK);
    assert(pos.piece_on(Square::SQ_H1) == Piece::WHITE_ROOK);
    assert(pos.piece_on(Square::SQ_E2) == Piece::WHITE_PAWN);
    assert(pos.piece_on(Square::SQ_E7) == Piece::BLACK_PAWN);

    // --- Board state ---
    assert(popcount(pos.occupied()) == 32);
    assert(popcount(pos.empty_squares()) == 32);
    assert(pos.sideToMove == Color::WHITE);
    assert(pos.castlingRights == (WHITE_OO | WHITE_OOO | BLACK_OO | BLACK_OOO));
    assert(pos.epSquare == Square::SQ_NONE);
    assert(pos.halfmoveClock == 0);
    assert(pos.fullmoveNumber == 1);

    // --- Zobrist key is non-zero ---
    assert(pos.zobristKey != 0);

    std::cout << "All Position & FEN tests passed! Phase 2.4 is complete." << std::endl;
}

int main() {
    test_position();
    return 0;
}
