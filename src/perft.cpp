#include "../include/perft.h"
#include "../include/movegen.h"
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

#include <chrono>

// ---------------------------------------------------------------------------
//  Perft Divide
// ---------------------------------------------------------------------------
void perft_divide(const Position& pos, int depth) {
    MoveList list;
    MoveGen::generate_legal_moves(pos, list);

    auto start_time = std::chrono::high_resolution_clock::now();
    uint64_t total = 0;
    
    for (int i = 0; i < list.size(); ++i) {
        Position next_pos = pos;
        if (next_pos.make_move(list.moves[i])) {
            uint64_t nodes = perft(next_pos, depth - 1);
            std::cout << move_to_str(list.moves[i]) << ": " << nodes << std::endl;
            total += nodes;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed_ms = end_time - start_time;
    
    double ms = elapsed_ms.count();
    uint64_t knps = (ms > 0) ? static_cast<uint64_t>((total / ms) * 1000.0 / 1000.0) : 0;
    
    std::cout << "\n    Depth: " << depth << "\n"
              << "    Nodes: " << total << "\n"
              << "    Time:  " << ms << "ms\n"
              << "   Speed:  " << knps << " Knps\n\n";
}
