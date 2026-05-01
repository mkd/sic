#include "../include/attacks.h"
#include "../include/utils.h"
#include <iostream>
#include <cassert>

// Quick test helper to convert Square to a 64-bit mask
constexpr uint64_t sq_mask(Square sq) {
    return 1ULL << static_cast<int>(sq);
}

void test_slider_attacks() {
    init_attacks(); 
    
    // --- Blocker mask tests ---
    assert(popcount(mask_rook_attacks(Square::SQ_D4)) == 10);
    assert(popcount(mask_bishop_attacks(Square::SQ_D4)) == 9);
    assert(popcount(mask_rook_attacks(Square::SQ_A1)) > 0);
    
    // --- On-the-fly attack tests (empty board) ---
    assert(popcount(rook_attacks_on_the_fly(Square::SQ_D4, Bitboard{0})) == 14);
    assert(popcount(rook_attacks_on_the_fly(Square::SQ_A1, Bitboard{0})) == 14);
    assert(popcount(bishop_attacks_on_the_fly(Square::SQ_D4, Bitboard{0})) == 13);
    assert(popcount(bishop_attacks_on_the_fly(Square::SQ_A1, Bitboard{0})) == 7);
    
    // --- Blocked attacks ---
    // Rook on D4 blocked by piece on D5
    {
        const Bitboard block = {sq_mask(Square::SQ_D5)};
        const Bitboard atk = rook_attacks_on_the_fly(Square::SQ_D4, block);
        // Should still attack D5 (capture), but not D6/D7/D8
        assert(atk.bb & sq_mask(Square::SQ_D5));
        assert(!(atk.bb & sq_mask(Square::SQ_D6)));
        assert(!(atk.bb & sq_mask(Square::SQ_D7)));
        assert(!(atk.bb & sq_mask(Square::SQ_D8)));
        // Unblocked directions should still work
        assert(atk.bb & sq_mask(Square::SQ_D3));
        assert(atk.bb & sq_mask(Square::SQ_D2));
        assert(atk.bb & sq_mask(Square::SQ_D1));
    }
    
    // Bishop on E4 blocked by piece on F5
    {
        const Bitboard block = {sq_mask(Square::SQ_F5)};
        const Bitboard atk = bishop_attacks_on_the_fly(Square::SQ_E4, block);
        assert(atk.bb & sq_mask(Square::SQ_F5));
        assert(!(atk.bb & sq_mask(Square::SQ_G6)));
        assert(!(atk.bb & sq_mask(Square::SQ_H7)));
    }
    
    std::cout << "All slider attack tests passed." << std::endl;
}

int main() {
    test_slider_attacks();
    return 0;
}
