#include "../include/attacks.h"
#include "../include/position.h"
#include "../include/move.h"
#include "../include/movegen.h"
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
    Move castle = make_move(Square::SQ_E1, Square::SQ_G1, MOVE_FLAG_CASTLING);
    assert(move_from(castle) == Square::SQ_E1);
    assert(move_to(castle) == Square::SQ_G1);
    assert(move_flag(castle) == MOVE_FLAG_CASTLING);
    assert(move_prom(castle) == PieceType::NONE);

    Move promo = make_move(Square::SQ_E7, Square::SQ_E8, MOVE_FLAG_NORMAL, PieceType::QUEEN);
    assert(move_from(promo) == Square::SQ_E7);
    assert(move_to(promo) == Square::SQ_E8);
    assert(move_flag(promo) == MOVE_FLAG_NORMAL);
    assert(move_prom(promo) == PieceType::QUEEN);

    {
        Move ep = make_move(Square::SQ_E5, Square::SQ_D6, MOVE_FLAG_ENPASSANT);
        assert(move_from(ep) == Square::SQ_E5);
        assert(move_to(ep) == Square::SQ_D6);
        assert(move_flag(ep) == MOVE_FLAG_ENPASSANT);
        assert(move_prom(ep) == PieceType::NONE);
        (void)ep;
    }

    Move normal = make_move(Square::SQ_D2, Square::SQ_D4);
    assert(move_from(normal) == Square::SQ_D2);
    assert(move_to(normal) == Square::SQ_D4);
    assert(move_flag(normal) == MOVE_FLAG_NORMAL);
    assert(move_prom(normal) == PieceType::NONE);

    MoveList ml;
    ml.push(normal);
    ml.push(castle);
    ml.push(promo);
    assert(ml.size() == 3);
    assert(ml[0] == normal);
    assert(ml[1] == castle);
    assert(ml[2] == promo);

    int iterated = 0;
    for (Move m : ml) {
        (void)m;
        ++iterated;
    }
    assert(iterated == 3);

    ml.clear();
    assert(ml.size() == 0);

    std::cout << "All Move Encoding & MoveList tests passed! Phase 2.5 is complete." << std::endl;
}

void test_movegen() {
    init_attacks();
    init_zobrist();

    Position pos;
    pos.set_fen("8/8/3k4/8/3N4/8/3K4/8 w - - 0 1");

    MoveList list;
    MoveGen::generate_legal_moves(pos, list);

    // Knight on D4 has 8 moves, King on D2 has 8 moves = 16 total
    assert(list.size() == 16);

    std::cout << "All MoveGen leaper tests passed! Phase 3.1 is complete." << std::endl;
}

int main() {
    test_position();
    test_move_encoding();
    test_movegen();
    return 0;
}
