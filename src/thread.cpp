#include "../include/thread.h"
#include "../include/search.h"
#include "stockfish_probe/nnue_incremental.h"

void Thread::search(int max_depth) {
    best_move = search_position(rootPos, max_depth, id);
}

namespace ThreadPool {

std::vector<Thread*> threads;

void init() {
    set_thread_count(1);
}

void set_thread_count(int count) {
    for (Thread* t : threads) {
        delete t;
    }
    threads.clear();

    for (int i = 0; i < count; ++i) {
        threads.push_back(new Thread(i));
    }
}

Move start_search(Position& pos, int max_depth) {
    auto pos_ptr = std::make_shared<Stockfish::Position>();
    std::memcpy(pos_ptr.get(), &Stockfish::Incremental::get_global_pos(), sizeof(Stockfish::Position));
    auto setup_ptr = std::make_shared<std::deque<Stockfish::StateInfo>>(Stockfish::Incremental::get_setup_states());

    for (Thread* t : threads) {
        t->rootPos = pos;
        t->best_move = MOVE_NONE;
        
        // Spawn thread using a lambda to initialize thread-local NNUE state first
        t->stdThread = std::thread([t, max_depth, pos_ptr, setup_ptr]() {
            Stockfish::Incremental::sync_from_main_thread(*pos_ptr, *setup_ptr);
            t->search(max_depth);
        });
    }

    for (Thread* t : threads) {
        if (t->stdThread.joinable()) {
            t->stdThread.join();
        }
    }

    return threads[0]->best_move;
}

} // namespace ThreadPool
