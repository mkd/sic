#pragma once

#include <cstdint>
#include <string>
#include <algorithm>
#include "types.h"


// ---------------------------------------------------------------------------
//  HalfKP Dimensions
// ---------------------------------------------------------------------------
constexpr int NNUE_PIECES = 7;
constexpr int NNUE_SQUARES = 64;
constexpr int TRANSFORMER_INPUTS = NNUE_PIECES * NNUE_SQUARES * 2;
constexpr int TRANSFORMER_NEURONS = 256;

// ---------------------------------------------------------------------------
//  Accumulator (64-byte aligned for cache-line safety)
// ---------------------------------------------------------------------------
struct alignas(64) Accumulator {
    int16_t white[TRANSFORMER_NEURONS];
    int16_t black[TRANSFORMER_NEURONS];
};

// ---------------------------------------------------------------------------
//  Global Network Weights
// ---------------------------------------------------------------------------
extern int16_t ft_weights[TRANSFORMER_INPUTS][TRANSFORMER_NEURONS];
extern int16_t ft_biases[TRANSFORMER_NEURONS];

extern int8_t output_weights[TRANSFORMER_NEURONS * 2];
extern int32_t output_bias;

extern bool nnue_initialized;

// ---------------------------------------------------------------------------
//  Loader
// ---------------------------------------------------------------------------
bool load_nnue(const std::string& filepath);

// ---------------------------------------------------------------------------
//  Activation
// ---------------------------------------------------------------------------
constexpr int32_t clipped_relu(int16_t x) {
    return std::max(int32_t(0), std::min(int32_t(127), static_cast<int32_t>(x)));
}

// ---------------------------------------------------------------------------
//  Accumulator Refresh
// ---------------------------------------------------------------------------
class Position;
void refresh_accumulator(Position& pos);

// ---------------------------------------------------------------------------
//  Incremental Accumulator Update
// ---------------------------------------------------------------------------
void update_accumulator_piece(Position& pos, Piece pc, Square sq, bool is_add);
