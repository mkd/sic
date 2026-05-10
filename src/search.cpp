#include "search.h"
#include "evaluate.h"
#include "movegen.h"
#include <algorithm>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------
// VALUE_INFINITE, VALUE_MATE, MOVE_NONE already defined in types.h

// ---------------------------------------------------------------------------
//  Negamax Search
// ---------------------------------------------------------------------------
static Value negamax(Position& pos, int depth, Value alpha, Value beta) {
    if (depth == 0) {
        return evaluate(pos);
    }

    MoveList list;
    MoveGen::generate_legal_moves(pos, list);

    Value best_value = -VALUE_INFINITE;

    for (int i = 0; i < list.size(); ++i) {
        Position next_pos = pos;
        if (!next_pos.make_move(list.moves[i])) continue;

        Value val = -negamax(next_pos, depth - 1, -beta, -alpha);

        if (val > best_value) best_value = val;
        if (val > alpha) alpha = val;
        if (alpha >= beta) break;
    }

    return best_value;
}

// ---------------------------------------------------------------------------
//  Root Search
// ---------------------------------------------------------------------------
Move search_position(Position& pos, int depth) {
    MoveList list;
    MoveGen::generate_legal_moves(pos, list);

    Move best_move = MOVE_NONE;
    Value best_value = -VALUE_INFINITE;
    Value alpha = -VALUE_INFINITE;
    Value beta = VALUE_INFINITE;

    for (int i = 0; i < list.size(); ++i) {
        Position next_pos = pos;
        if (!next_pos.make_move(list.moves[i])) continue;

        Value val = -negamax(next_pos, depth - 1, -beta, -alpha);

        if (val > best_value) {
            best_value = val;
            best_move = list.moves[i];
        }
        if (val > alpha) alpha = val;
    }

    return best_move;
}
