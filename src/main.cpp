#include "../include/piece.h"
#include <iostream>
#include <cassert>

void test_piece() {
    // make_piece / piece_type / color_of roundtrip
    assert(piece_type(make_piece(Color::WHITE, PieceType::KNIGHT)) == PieceType::KNIGHT);
    assert(color_of(make_piece(Color::BLACK, PieceType::ROOK)) == Color::BLACK);
    assert(make_piece(Color::WHITE, PieceType::QUEEN) == Piece::WHITE_QUEEN);
    assert(make_piece(Color::BLACK, PieceType::PAWN) == Piece::BLACK_PAWN);
    
    // Color flip
    assert(~Color::WHITE == Color::BLACK);
    assert(~Color::BLACK == Color::WHITE);
    
    // Predicates
    assert(is_white_piece(Piece::WHITE_KNIGHT));
    assert(is_black_piece(Piece::BLACK_ROOK));
    assert(type_is_slider(PieceType::BISHOP));
    assert(type_is_slider(PieceType::ROOK));
    assert(type_is_slider(PieceType::QUEEN));
    assert(!type_is_slider(PieceType::KNIGHT));
    assert(!type_is_slider(PieceType::PAWN));
    assert(!type_is_slider(PieceType::KING));
    
    // piece_to_char / char_to_piece roundtrip
    assert(piece_to_char(char_to_piece('R')) == 'R');
    assert(piece_to_char(char_to_piece('q')) == 'q');
    assert(piece_to_char(char_to_piece('K')) == 'K');
    assert(piece_to_char(char_to_piece('p')) == 'p');
    assert(char_to_piece('X') == Piece::PIECE_NONE);
    
    // PIECE_NONE
    assert(piece_to_char(Piece::PIECE_NONE) == ' ');
    
    std::cout << "All piece tests passed." << std::endl;
}

int main() {
    test_piece(); // Make sure this says test_piece!
    return 0;
}
