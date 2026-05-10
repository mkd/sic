#include "perft.h"
#include "movegen.h"
#include <iostream>

// ---------------------------------------------------------------------------
//  Perft
// ---------------------------------------------------------------------------
uint64_t perft(const Position& pos, int depth) {
    if (depth == 0) return 1ULL;

    MoveList list;
    MoveGen::generate_legal_moves(pos, list);

    uint64_t nodes = 0;
    for (int i = 0; i < list.size(); ++i) {
        Position next_pos = pos;
        if (next_pos.make_move(list.moves[i])) {
            nodes += perft(next_pos, depth - 1);
        }
    }
    return nodes;
}

// ---------------------------------------------------------------------------
//  Perft Divide
// ---------------------------------------------------------------------------
void perft_divide(const Position& pos, int depth) {
    MoveList list;
    MoveGen::generate_legal_moves(pos, list);

    uint64_t total = 0;
    for (int i = 0; i < list.size(); ++i) {
        Position next_pos = pos;
        if (next_pos.make_move(list.moves[i])) {
            uint64_t nodes = perft(next_pos, depth - 1);
            std::cout << move_to_str(list.moves[i]) << ": " << nodes << std::endl;
            total += nodes;
        }
    }
    std::cout << "Total nodes: " << total << std::endl;
}
