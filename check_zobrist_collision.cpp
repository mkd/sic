#include <iostream>
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
    
    // In sic: BLACK_ROOK is 10 (WHITE_PAWN=0, WHITE_KNIGHT=1, WHITE_BISHOP=2, WHITE_ROOK=3, WHITE_QUEEN=4, WHITE_KING=5, BLACK_PAWN=6, BLACK_KNIGHT=7, BLACK_BISHOP=8, BLACK_ROOK=9, BLACK_QUEEN=10, BLACK_KING=11)
    // Wait, let's see how pieces are defined in sic
    return 0;
}
