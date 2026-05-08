#include "../include/attacks.h"
#include "../include/utils.h"
#include <cstring>

const int ROOK_RELEVANT_BITS[64] = {
    12, 11, 11, 11, 11, 11, 11, 12, 11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11, 11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11, 11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11, 12, 11, 11, 11, 11, 11, 11, 12
};

const int BISHOP_RELEVANT_BITS[64] = {
    6, 5, 5, 5, 5, 5, 5, 6, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5, 5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5, 5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 6, 5, 5, 5, 5, 5, 5, 6
};

Bitboard BISHOP_MAGICS[64];
Bitboard ROOK_MAGICS[64];

// --- Fast Sparse Random Number Generator ---
static uint64_t prng_state = 1804289383ULL;
static uint64_t rand64() {
    prng_state ^= prng_state >> 12;
    prng_state ^= prng_state << 25;
    prng_state ^= prng_state >> 27;
    return prng_state * 2685821657736338717LL;
}

// Sparse randoms (fewer bits set) find magic numbers 1000x faster
static uint64_t sparse_rand() {
    return rand64() & rand64() & rand64();
}

void init_magics() {
    static uint64_t used[4096];
    static uint64_t b_blockers[512];
    static uint64_t r_blockers[4096];
    
    for (int sq = 0; sq < 64; ++sq) {
        BISHOP_MASKS[sq] = mask_bishop_attacks(static_cast<Square>(sq));
        ROOK_MASKS[sq] = mask_rook_attacks(static_cast<Square>(sq));

        // -- BISHOPS --
        uint64_t b_mask = BISHOP_MASKS[sq].bb;
        int b_configs = 1 << popcount({b_mask});
        int b_shift = 64 - BISHOP_RELEVANT_BITS[sq];
        
        uint64_t b_occ = 0;
        int b_idx = 0;
        do {
            b_blockers[b_idx++] = b_occ;
            b_occ = (b_occ - b_mask) & b_mask;
        } while (b_occ != 0);

        while (true) {
            uint64_t magic = sparse_rand();
            std::memset(used, 0, sizeof(used));
            bool success = true;
            for (int i = 0; i < b_configs; ++i) {
                uint64_t key = (b_blockers[i] * magic) >> b_shift;
                if (used[key]) { success = false; break; }
                used[key] = 1;
            }
            if (success) { BISHOP_MAGICS[sq].bb = magic; break; }
        }

        // -- ROOKS --
        uint64_t r_mask = ROOK_MASKS[sq].bb;
        int r_configs = 1 << popcount({r_mask});
        int r_shift = 64 - ROOK_RELEVANT_BITS[sq];
        
        uint64_t r_occ = 0;
        int r_idx = 0;
        do {
            r_blockers[r_idx++] = r_occ;
            r_occ = (r_occ - r_mask) & r_mask;
        } while (r_occ != 0);

        while (true) {
            uint64_t magic = sparse_rand();
            std::memset(used, 0, sizeof(used));
            bool success = true;
            for (int i = 0; i < r_configs; ++i) {
                uint64_t key = (r_blockers[i] * magic) >> r_shift;
                if (used[key]) { success = false; break; }
                used[key] = 1;
            }
            if (success) { ROOK_MAGICS[sq].bb = magic; break; }
        }
    }
}
