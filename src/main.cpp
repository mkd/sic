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
    assert(popcount(pos.pieces(PieceType::KNIGHT)) == 4);
    assert(pos.piece_on(Square::SQ_E1) == Piece::WHITE_KING);
    std::cout << "All Position & FEN tests passed! Phase 2.4 is complete." << std::endl;
}

void test_move_encoding() {
    Move castle = make_move(Square::SQ_E1, Square::SQ_G1, MOVE_FLAG_CASTLING);
    assert(move_from(castle) == Square::SQ_E1);
    assert(move_flag(castle) == MOVE_FLAG_CASTLING);
    MoveList ml;
    ml.push(castle);
    assert(ml.size() == 1);
    std::cout << "All Move Encoding & MoveList tests passed! Phase 2.5 is complete." << std::endl;
}

void test_movegen() {
    init_attacks(); // Now this will run instantly thanks to our new Sparse PRNG!
    init_zobrist();

    // --- Leaper test: Knight + King ---
    {
        Position pos;
        pos.set_fen("8/8/3k4/8/3N4/8/3K4/8 w - - 0 1");
        MoveList list;
        MoveGen::generate_legal_moves(pos, list);
        assert(list.size() == 16);
    }

    // --- Slider test: Queen on D4 + King on E1 ---
    {
        Position pos;
        pos.set_fen("8/8/8/3Q4/8/8/8/4K3 w - - 0 1");
        MoveList list;
        MoveGen::generate_legal_moves(pos, list);
        // Queen on D4: 27 moves (14 orthogonal + 13 diagonal)
        // King on E1: 5 moves
        assert(list.size() == 32);
    }

    std::cout << "All MoveGen leaper & slider tests passed! Phase 3.2 is complete." << std::endl;
}

int main() {
    test_position();
    test_move_encoding();
    test_movegen();
    return 0;
}
