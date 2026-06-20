#include <iostream>
#include <cstdint>
#include <random>

uint64_t ZobristPiece[12][64];
uint64_t ZobristCastling[16];
uint64_t ZobristEpFile[8];
uint64_t ZobristSide;

int main() {
    std::mt19937_64 rng(1337);

    for (int p = 0; p < 12; ++p) {
        for (int sq = 0; sq < 64; ++sq) {
            ZobristPiece[p][sq] = rng();
        }
    }
    for (int i = 0; i < 16; ++i) {
        ZobristCastling[i] = rng();
    }
    for (int f = 0; f < 8; ++f) {
        ZobristEpFile[f] = rng();
    }
    ZobristSide = rng();

    uint64_t* ptr = &ZobristPiece[0][0];
    for (int sq = 0; sq < 64; ++sq) {
        std::cout << "sq " << sq << ": " << ptr[12 * 64 + sq] << "\n";
    }
    return 0;
}
