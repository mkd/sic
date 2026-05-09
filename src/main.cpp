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
        assert(list.size() == 32);
    }

    // --- Pawn test: White pawn on D7, promotion capture on C8 ---
    {
        Position pos;
        pos.set_fen("rnbq1bnr/pp1Pp1pp/8/2p5/8/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 1");
        MoveList list;
        MoveGen::generate_legal_moves(pos, list);

        int total_c8 = 0;
        for (const auto& m : list) {
            if (move_from(m) == Square::SQ_D7 && move_to(m) == Square::SQ_C8) {
                total_c8++;
            }
        }
        assert(total_c8 == 4);
        std::cout << "Pawn promotion capture test passed! (D7->C8: " << total_c8 << " moves)" << std::endl;
    }

    // --- En Passant test: White pawn on E5 captures d5 en passant ---
    {
        Position pos;
        pos.set_fen("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
        MoveList list;
        MoveGen::generate_legal_moves(pos, list);

        int ep_count = 0;
        for (const auto& m : list) {
            if (move_flag(m) == MOVE_FLAG_ENPASSANT) ep_count++;
        }
        assert(ep_count == 1);
        std::cout << "En Passant test passed! (EP moves: " << ep_count << ")" << std::endl;
    }

    // --- Castling test: Kiwipete position (both sides can castle) ---
    {
        Position pos;
        pos.set_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
        MoveList list;
        MoveGen::generate_legal_moves(pos, list);

        int castle_count = 0;
        for (const auto& m : list) {
            if (move_flag(m) == MOVE_FLAG_CASTLING) castle_count++;
        }
        assert(castle_count >= 2);
        std::cout << "Castling test passed! (Castling moves: " << castle_count << ")" << std::endl;
    }

    std::cout << "All MoveGen tests passed! Phase 3.4 is complete." << std::endl;
}

int main() {
    test_position();
    test_move_encoding();
    test_movegen();
    return 0;
}
