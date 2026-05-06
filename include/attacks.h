#pragma once

#include "types.h"

// ---------------------------------------------------------------------------
//  Attack Table Declarations
// ---------------------------------------------------------------------------
extern Bitboard PAWN_ATTACKS[2][64];
extern Bitboard KNIGHT_ATTACKS[64];
extern Bitboard KING_ATTACKS[64];

// ---------------------------------------------------------------------------
//  Slider Mask Functions
// ---------------------------------------------------------------------------
Bitboard mask_bishop_attacks(Square sq);
Bitboard mask_rook_attacks(Square sq);

// ---------------------------------------------------------------------------
//  On-The-Fly Slider Attack Generation
// ---------------------------------------------------------------------------
Bitboard bishop_attacks_on_the_fly(Square sq, Bitboard block);
Bitboard rook_attacks_on_the_fly(Square sq, Bitboard block);

// ---------------------------------------------------------------------------
//  Magic Bitboard Constants (extern; linked from separate data file)
// ---------------------------------------------------------------------------
extern const Bitboard BISHOP_MAGICS[64];
extern const Bitboard ROOK_MAGICS[64];
extern const int BISHOP_RELEVANT_BITS[64];
extern const int ROOK_RELEVANT_BITS[64];

// ---------------------------------------------------------------------------
//  Precomputed Blocker Masks (filled at init, used in fast lookups)
// ---------------------------------------------------------------------------
extern Bitboard BISHOP_MASKS[64];
extern Bitboard ROOK_MASKS[64];

// ---------------------------------------------------------------------------
//  Magic Bitboard Lookup Tables (extern)
// ---------------------------------------------------------------------------
extern Bitboard BISHOP_ATTACKS_TABLE[64][512];
extern Bitboard ROOK_ATTACKS_TABLE[64][4096];

#include "utils.h"

// ---------------------------------------------------------------------------
//  Vertical Bitboard Flip (SIC a1=0 <-> Gargantua a8=0)
// ---------------------------------------------------------------------------
FORCE_INLINE Bitboard flip_bb(Bitboard b) {
    uint64_t x = b.bb;
    const uint64_t r0 = x & 0x0000000000FF;
    const uint64_t r1 = x & 0x00000000FF00;
    const uint64_t r2 = x & 0x000000FF0000;
    const uint64_t r3 = x & 0x0000FF000000;
    const uint64_t r4 = x & 0x00FF00000000;
    const uint64_t r5 = x & 0x0F0000000000;
    const uint64_t r6 = x & 0xF00000000000;
    const uint64_t r7 = x & 0xFF0000000000;
    return { (r0 >> 56) | (r1 >> 40) | (r2 >> 24) | (r3 >> 8) |
             (r4 << 8)  | (r5 << 24) | (r6 << 40) | (r7 << 56) };
}

// ---------------------------------------------------------------------------
//  Magic Bitboard Fast Lookups (FORCE_INLINE)
// ---------------------------------------------------------------------------
FORCE_INLINE Bitboard get_bishop_attacks(Square sq, Bitboard occupied) {
    const int idx = static_cast<int>(sq);
    const int garg_sq = idx ^ 56;
    Bitboard flipped = flip_bb(occupied);
    const uint64_t key = ((flipped.bb & BISHOP_MASKS[idx].bb) * BISHOP_MAGICS[garg_sq].bb) >> (64 - BISHOP_RELEVANT_BITS[garg_sq]);
    return flip_bb(BISHOP_ATTACKS_TABLE[idx][key]);
}

FORCE_INLINE Bitboard get_rook_attacks(Square sq, Bitboard occupied) {
    const int idx = static_cast<int>(sq);
    const int garg_sq = idx ^ 56;
    Bitboard flipped = flip_bb(occupied);
    const uint64_t key = ((flipped.bb & ROOK_MASKS[idx].bb) * ROOK_MAGICS[garg_sq].bb) >> (64 - ROOK_RELEVANT_BITS[garg_sq]);
    return flip_bb(ROOK_ATTACKS_TABLE[idx][key]);
}

// ---------------------------------------------------------------------------
//  Initialization
// ---------------------------------------------------------------------------
void init_attacks();
