#include "../include/attacks.h"
#include "../include/utils.h"
#include <iostream>
#include <cassert>

constexpr uint64_t sq_mask(Square sq) {
    return 1ULL << static_cast<int>(sq);
}

void test_magic_bitboards() {
    init_attacks(); // This should now internally call init_magic_bitboards()
    
    // --- Verify Magic Hashing vs Slow Raycasting ---
    
    // Square D4 with an empty board
    Bitboard empty_occ = {0};
    assert(get_bishop_attacks(Square::SQ_D4, empty_occ).bb == bishop_attacks_on_the_fly(Square::SQ_D4, empty_occ).bb);
    assert(get_rook_attacks(Square::SQ_D4, empty_occ).bb == rook_attacks_on_the_fly(Square::SQ_D4, empty_occ).bb);

    // Square D4 with blockers on D5 and F6
    Bitboard blockers = {sq_mask(Square::SQ_D5) | sq_mask(Square::SQ_F6)};
    assert(get_bishop_attacks(Square::SQ_D4, blockers).bb == bishop_attacks_on_the_fly(Square::SQ_D4, blockers).bb);
    assert(get_rook_attacks(Square::SQ_D4, blockers).bb == rook_attacks_on_the_fly(Square::SQ_D4, blockers).bb);
    
    std::cout << "All Magic Bitboard tests passed! Phase 2.3 is officially complete." << std::endl;
}

int main() {
    test_magic_bitboards();
    return 0;
}
