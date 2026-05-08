#include "../include/position.h"
#include "../include/move.h"
#include "../include/utils.h"
#include <iostream>
#include <cassert>

void test_position() {
    init_zobrist();

    Position pos;
    pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    assert(popcount(pos.pieces(Color::WHITE)) == 16);
    assert(popcount(pos.pieces(Color::BLACK)) == 16);
    assert(popcount(pos.pieces(PieceType::PAWN)) == 16);
    assert(popcount(pos.pieces(PieceType::KNIGHT)) == 4);

    assert(pos.piece_on(Square::SQ_E1) == Piece::WHITE_KING);
    assert(pos.piece_on(Square::SQ_E8) == Piece::BLACK_KING);
    assert(pos.piece_on(Square::SQ_A1) == Piece::WHITE_ROOK);
    assert(pos.piece_on(Square::SQ_H1) == Piece::WHITE_ROOK);
    assert(pos.piece_on(Square::SQ_E2) == Piece::WHITE_PAWN);
    assert(pos.piece_on(Square::SQ_E7) == Piece::BLACK_PAWN);

    assert(popcount(pos.occupied()) == 32);
    assert(popcount(pos.empty_squares()) == 32);
    assert(pos.sideToMove == Color::WHITE);
    assert(pos.castlingRights == (WHITE_OO | WHITE_OOO | BLACK_OO | BLACK_OOO));
    assert(pos.epSquare == Square::SQ_NONE);
    assert(pos.halfmoveClock == 0);
    assert(pos.fullmoveNumber == 1);
    assert(pos.zobristKey != 0);

    std::cout << "All Position & FEN tests passed! Phase 2.4 is complete." << std::endl;
}

void test_move_encoding() {
    // --- Castling: E1 -> G1, flag=Castling ---
    Move castle = make_move(Square::SQ_E1, Square::SQ_G1, MOVE_FLAG_CASTLING);
    assert(move_from(castle) == Square::SQ_E1);
    assert(move_to(castle) == Square::SQ_G1);
    assert(move_flag(castle) == MOVE_FLAG_CASTLING);
    assert(move_prom(castle) == PieceType::NONE);

    // --- Promotion: E7 -> E8, Queen ---
    Move promo = make_move(Square::SQ_E7, Square::SQ_E8, MOVE_FLAG_NORMAL, PieceType::QUEEN);
    assert(move_from(promo) == Square::SQ_E7);
    assert(move_to(promo) == Square::SQ_E8);
    assert(move_flag(promo) == MOVE_FLAG_NORMAL);
    assert(move_prom(promo) == PieceType::QUEEN);

    // --- En passant: E5 -> D6 ---
    {
        Move ep = make_move(Square::SQ_E5, Square::SQ_D6, MOVE_FLAG_ENPASSANT);
        assert(move_from(ep) == Square::SQ_E5);
        assert(move_to(ep) == Square::SQ_D6);
        assert(move_flag(ep) == MOVE_FLAG_ENPASSANT);
        assert(move_prom(ep) == PieceType::NONE);
        (void)ep;
    }

    // --- Normal move: D2 -> D4 ---
    Move normal = make_move(Square::SQ_D2, Square::SQ_D4);
    assert(move_from(normal) == Square::SQ_D2);
    assert(move_to(normal) == Square::SQ_D4);
    assert(move_flag(normal) == MOVE_FLAG_NORMAL);
    assert(move_prom(normal) == PieceType::NONE);

    // --- MoveList ---
    MoveList ml;
    ml.push(normal);
    ml.push(castle);
    ml.push(promo);
    assert(ml.size() == 3);
    assert(ml[0] == normal);
    assert(ml[1] == castle);
    assert(ml[2] == promo);

    // --- Ranged-for iteration ---
    int iterated = 0;
    for (Move m : ml) {
        (void)m;
        ++iterated;
    }
    assert(iterated == 3);

    // --- Clear ---
    ml.clear();
    assert(ml.size() == 0);

    std::cout << "All Move Encoding & MoveList tests passed! Phase 2.5 is complete." << std::endl;
}

int main() {
    test_position();
    test_move_encoding();
    return 0;
}
