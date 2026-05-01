#pragma once

#include "types.h"

// ---------------------------------------------------------------------------
//  Attack Table Declarations
// ---------------------------------------------------------------------------
extern Bitboard PAWN_ATTACKS[2][64];
extern Bitboard KNIGHT_ATTACKS[64];
extern Bitboard KING_ATTACKS[64];

// ---------------------------------------------------------------------------
//  Initialization
// ---------------------------------------------------------------------------
void init_attacks();
