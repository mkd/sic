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
//  Initialization
// ---------------------------------------------------------------------------
void init_attacks();
