#pragma once

#include "position.h"

extern const int PieceValues[7];

Value evaluate(const Position& pos);
