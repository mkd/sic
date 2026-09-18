#include "../src/stockfish_probe/sf_wrapper.h"
#pragma once

#include <thread>
#include <vector>
#include <atomic>
#include <cstring>
#include "position.h"

#include <mutex>
#include <condition_variable>
#include <memory>
#include <deque>

namespace Stockfish {
    struct Position;
    struct StateInfo;
}

struct SearchWorker {

    Move pv_array[128][128];
    int  pv_length[128];
    uint64_t node_count;
    Move killer_moves[128][2];
    int history[2][64][64];
    int capture_history[14][64][14];
    int continuation_history[4][14][64][14][64];
    int pawn_history[512][64];
    Move played_moves[128];
    Piece played_pieces[128];
    Value static_evals[128];
    Move counter_moves[64][64];
    int correction_history[2][16384];
    uint64_t search_history[128];

    SearchWorker() : node_count(0) {
        std::memset(search_history, 0, sizeof(search_history));
        std::memset(capture_history, 0, sizeof(capture_history));
        std::memset(continuation_history, 0, sizeof(continuation_history));
        std::memset(pawn_history, 0, sizeof(pawn_history));
        std::memset(history, 0, sizeof(history));
        std::memset(correction_history, 0, sizeof(correction_history));
        std::memset(killer_moves, 0, sizeof(killer_moves));
        std::memset(counter_moves, 0, sizeof(counter_moves));
        std::memset(static_evals, 0, sizeof(static_evals));
        for (int i = 0; i < 128; ++i) {
            played_moves[i] = MOVE_NONE;
            played_pieces[i] = Piece::PIECE_NONE;
        }
        for (int i = 0; i < 128; ++i) {
            pv_length[i] = 0;
            for (int j = 0; j < 128; ++j) {
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
        for (int s = 0; s < 2; ++s) {
            for (int i = 0; i < 16384; ++i) {
                correction_history[s][i] = 0;
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
    int max_depth;
    
    std::mutex mtx;
    std::condition_variable cv;
    bool is_searching;
    bool should_exit;
    
    std::shared_ptr<StockfishWrapper::ThreadState> sync_state;

    Thread(int thread_id) : id(thread_id), best_move(MOVE_NONE), max_depth(0), is_searching(false), should_exit(false) {}

    void search();
    void loop();
};

namespace ThreadPool {

extern std::vector<Thread*> threads;

void init();
void set_thread_count(int count);
Move start_search(Position& pos, int max_depth);

} // namespace ThreadPool
