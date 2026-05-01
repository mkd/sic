#include "../include/attacks.h" // Fixed path
#include "../include/utils.h"   // Fixed path
#include <iostream>
#include <cassert>

void test_attacks() {
    // Note: If the compiler complains about this function name, 
    // check attacks.h to see what OpenCode actually named it!
    init_attacks(); 
    
    // Knight on D4 attacks all 8 squares
    assert(popcount(KNIGHT_ATTACKS[static_cast<int>(Square::SQ_D4)]) == 8);
    // Knight on A1 attacks 2 squares (B3, C2)
    assert(popcount(KNIGHT_ATTACKS[static_cast<int>(Square::SQ_A1)]) == 2);
    // Knight on A4 attacks 4 squares
    assert(popcount(KNIGHT_ATTACKS[static_cast<int>(Square::SQ_A4)]) == 4);
    // Knight on center-ish square B3 attacks 6 squares
    assert(popcount(KNIGHT_ATTACKS[static_cast<int>(Square::SQ_B3)]) == 6);
    // King on A1 attacks 3 squares
    assert(popcount(KING_ATTACKS[static_cast<int>(Square::SQ_A1)]) == 3);
    // King on D4 attacks all 8 squares
    assert(popcount(KING_ATTACKS[static_cast<int>(Square::SQ_D4)]) == 8);
    // King on A8 attacks 3 squares
    assert(popcount(KING_ATTACKS[static_cast<int>(Square::SQ_A8)]) == 3);
    // King on E5 attacks 8 squares
    assert(popcount(KING_ATTACKS[static_cast<int>(Square::SQ_E5)]) == 8);
    
    // White pawn on E2 attacks D3 and F3
    assert(popcount(PAWN_ATTACKS[0][static_cast<int>(Square::SQ_E2)]) == 2);
    // Black pawn on E7 attacks D6 and F6
    assert(popcount(PAWN_ATTACKS[1][static_cast<int>(Square::SQ_E7)]) == 2);
    // Black pawn on H7 only attacks G6
    assert(popcount(PAWN_ATTACKS[1][static_cast<int>(Square::SQ_H7)]) == 1);
    
    std::cout << "All attack table tests passed." << std::endl;
}

int main() {
    test_attacks();
    return 0;
}
