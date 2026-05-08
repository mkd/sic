#pragma once

#include "position.h"
#include "move.h"

namespace MoveGen {
    void generate_legal_moves(const Position& pos, MoveList& list);
}
