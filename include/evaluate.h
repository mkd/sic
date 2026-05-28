#pragma once

#include "position.h"

extern const int PieceValues[7];
extern bool nnue_initialized;

Value evaluate(const Position& pos, bool force_small = false);
