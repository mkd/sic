#include "../include/geometry.h" // Make sure the path is correct
#include <iostream>
#include <cassert>

void test_geometry() {
    // file_of / rank_of
    assert(file_of(Square::SQ_A1) == File::FILE_A);
    assert(rank_of(Square::SQ_A1) == Rank::RANK_1);
    assert(file_of(Square::SQ_H8) == File::FILE_H);
    assert(rank_of(Square::SQ_H8) == Rank::RANK_8);
    assert(file_of(Square::SQ_D4) == File::FILE_D);
    assert(rank_of(Square::SQ_D4) == Rank::RANK_4);
    
    // make_square
    assert(make_square(File::FILE_A, Rank::RANK_1) == Square::SQ_A1);
    assert(make_square(File::FILE_H, Rank::RANK_8) == Square::SQ_H8);
    assert(make_square(File::FILE_D, Rank::RANK_4) == Square::SQ_D4);
    
    // flip_vertical
    assert(flip_vertical(Square::SQ_A1) == Square::SQ_A8);
    assert(flip_vertical(Square::SQ_H8) == Square::SQ_H1);
    assert(flip_vertical(flip_vertical(Square::SQ_E4)) == Square::SQ_E4);
    
    // relative_square
    assert(relative_square(Color::WHITE, Square::SQ_A1) == Square::SQ_A1);
    assert(relative_square(Color::BLACK, Square::SQ_A1) == Square::SQ_A8);
    assert(relative_square(Color::BLACK, Square::SQ_E4) == Square::SQ_E5);
    
    // sq_distance (Chebyshev)
    assert(sq_distance(Square::SQ_A1, Square::SQ_A1) == 0);
    assert(sq_distance(Square::SQ_A1, Square::SQ_H8) == 7);
    assert(sq_distance(Square::SQ_A1, Square::SQ_C3) == 2);
    assert(sq_distance(Square::SQ_D4, Square::SQ_E6) == 2);
    
    // same_color (diagonal check) - FIXED THE B2 BUG HERE
    assert(same_color(Square::SQ_A1, Square::SQ_C3));
    assert(!same_color(Square::SQ_A1, Square::SQ_B1)); 
    
    // on_same_diagonal
    assert(on_same_diagonal(Square::SQ_A1, Square::SQ_H8));
    assert(on_same_diagonal(Square::SQ_A1, Square::SQ_C3));
    assert(!on_same_diagonal(Square::SQ_A1, Square::SQ_B1));
    
    // on_same_anti_diagonal
    assert(on_same_anti_diagonal(Square::SQ_A8, Square::SQ_H1));
    assert(on_same_anti_diagonal(Square::SQ_A3, Square::SQ_C1));
    assert(!on_same_anti_diagonal(Square::SQ_A1, Square::SQ_B1));
    
    // is_valid_square
    assert(is_valid_square(Square::SQ_A1));
    assert(is_valid_square(Square::SQ_H8));
    assert(!is_valid_square(Square::SQ_NONE));
    
    std::cout << "All geometry tests passed." << std::endl;
}

int main() {
    test_geometry();
    return 0;
}
