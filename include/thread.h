#pragma once

#include <thread>
#include <vector>
#include <atomic>
#include "position.h"

struct SearchWorker {
    Move pv_array[64][64];
    int  pv_length[64];
    uint64_t node_count;
    Move killer_moves[128][2];
    int history[2][64][64];

    SearchWorker() : node_count(0) {
        for (int i = 0; i < 64; ++i) {
            pv_length[i] = 0;
            for (int j = 0; j < 64; ++j) {
                pv_array[i][j] = MOVE_NONE;
            }
        }
        for (int i = 0; i < 128; ++i) {
            killer_moves[i][0] = MOVE_NONE;
            killer_moves[i][1] = MOVE_NONE;
        }
        for (int s = 0; s < 2; ++s) {
            for (int f = 0; f < 64; ++f) {
                for (int t = 0; t < 64; ++t) {
                    history[s][f][t] = 0;
                }
            }
        }
    }
};

struct Thread {
    std::thread stdThread;
    Position rootPos;
    int id;
    SearchWorker sw;
    Move best_move;

    Thread(int thread_id) : id(thread_id), best_move(MOVE_NONE) {}

    void search(int max_depth);
};

namespace ThreadPool {

extern std::vector<Thread*> threads;

void init();
void set_thread_count(int count);
Move start_search(Position& pos, int max_depth);

} // namespace ThreadPool
