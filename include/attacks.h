#pragma once

#include "types.h"
#include "utils.h"

// ---------------------------------------------------------------------------
//  Attack Table Declarations
// ---------------------------------------------------------------------------
extern Bitboard PAWN_ATTACKS[2][64];
extern Bitboard KNIGHT_ATTACKS[64];
extern Bitboard KING_ATTACKS[64];

// ---------------------------------------------------------------------------
//  Magic Bitboard Constants (extern; linked from src/magics.cpp)
// ---------------------------------------------------------------------------
extern const Bitboard BISHOP_MAGICS[64];
extern const Bitboard ROOK_MAGICS[64];
extern const int BISHOP_RELEVANT_BITS[64];
extern const int ROOK_RELEVANT_BITS[64];

// ---------------------------------------------------------------------------
//  Slider Mask Functions & Precomputed Masks
// ---------------------------------------------------------------------------
Bitboard mask_bishop_attacks(Square sq);
Bitboard mask_rook_attacks(Square sq);

extern Bitboard BISHOP_MASKS[64];
extern Bitboard ROOK_MASKS[64];
extern Bitboard BISHOP_ATTACKS_TABLE[64][512];
extern Bitboard ROOK_ATTACKS_TABLE[64][4096];

// ---------------------------------------------------------------------------
//  On-The-Fly Slider Attack Generation
// ---------------------------------------------------------------------------
Bitboard bishop_attacks_on_the_fly(Square sq, Bitboard block);
Bitboard rook_attacks_on_the_fly(Square sq, Bitboard block);

// ---------------------------------------------------------------------------
//  Magic Bitboard Fast Lookups (FORCE_INLINE)
// ---------------------------------------------------------------------------
FORCE_INLINE Bitboard get_bishop_attacks(Square sq, Bitboard occupied) {
    const int idx = static_cast<int>(sq);
    const uint64_t key = ((occupied.bb & BISHOP_MASKS[idx].bb) * BISHOP_MAGICS[idx].bb) >> (64 - BISHOP_RELEVANT_BITS[idx]);
    return {BISHOP_ATTACKS_TABLE[idx][key]};
}

FORCE_INLINE Bitboard get_rook_attacks(Square sq, Bitboard occupied) {
    const int idx = static_cast<int>(sq);
    const uint64_t key = ((occupied.bb & ROOK_MASKS[idx].bb) * ROOK_MAGICS[idx].bb) >> (64 - ROOK_RELEVANT_BITS[idx]);
    return {ROOK_ATTACKS_TABLE[idx][key]};
}

// ---------------------------------------------------------------------------
//  Initialization
// ---------------------------------------------------------------------------
void init_magic_bitboards();
void init_attacks();
void init_pawn_attacks();
void init_knight_attacks();
void init_king_attacks();
