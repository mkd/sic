#pragma once

#include "position.h"
#include "nnue_bridge.h"

extern const int PieceValues[7];
extern bool nnue_initialized;

Value evaluate(const Position& pos);
