#include <iostream>
#include "../include/types.h"
#include "../include/utils.h"

int main() {
    Bitboard b = {0b1001000ULL}; 

    std::cout << "--- Bitboard Intrinsics Test ---\n";
    std::cout << "Initial popcount: " << popcount(b) << " (Expected: 2)\n";
    std::cout << "LSB: " << (int)lsb(b) << " (Expected: 3)\n";
    std::cout << "MSB: " << (int)msb(b) << " (Expected: 6)\n";

    Square sq = pop_lsb(b);
    std::cout << "Popped LSB: " << (int)sq << " (Expected: 3)\n";
    std::cout << "Remaining popcount: " << popcount(b) << " (Expected: 1)\n";

    std::cout << "All tests executed.\n";
    return 0;
}
