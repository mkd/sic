#include "../include/thread.h"
#include "../include/search.h"
#include "stockfish_probe/sf_wrapper.h"

void Thread::search() {
    best_move = search_position(rootPos, max_depth, id);
}

void Thread::loop() {
    StockfishWrapper::init_thread(); // Initialize thread-local NNUE once
    
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return is_searching || should_exit; });
        
        if (should_exit) {
            break;
        }
        
        // Sync NNUE state before search
        StockfishWrapper::sync_thread_state(*sync_state);
        
        // Run search
        search();
        
        is_searching = false;
        cv.notify_one();
    }
}

namespace ThreadPool {

std::vector<Thread*> threads;

void init() {
    unsigned int hw_threads = std::thread::hardware_concurrency();
    unsigned int default_threads = (hw_threads > 1) ? hw_threads / 2 : 1;
    set_thread_count(default_threads);
}

void set_thread_count(int count) {
    for (Thread* t : threads) {
        {
            std::lock_guard<std::mutex> lock(t->mtx);
            t->should_exit = true;
        }
        t->cv.notify_one();
        if (t->stdThread.joinable()) {
            t->stdThread.join();
        }
        delete t;
    }
    threads.clear();

    for (int i = 0; i < count; ++i) {
        Thread* t = new Thread(i);
        t->stdThread = std::thread(&Thread::loop, t);
        threads.push_back(t);
    }
}

Move start_search(Position& pos, int max_depth) {
    auto state_ptr = std::make_shared<StockfishWrapper::ThreadState>(StockfishWrapper::get_thread_state());

    // Wake up all threads
    for (Thread* t : threads) {
        {
            std::lock_guard<std::mutex> lock(t->mtx);
            t->rootPos = pos;
            t->best_move = MOVE_NONE;
            t->max_depth = max_depth;
            t->sync_state = state_ptr;
            t->is_searching = true;
        }
        t->cv.notify_one();
    }

    // Wait for all threads to finish
    for (Thread* t : threads) {
        std::unique_lock<std::mutex> lock(t->mtx);
        t->cv.wait(lock, [t]() { return !t->is_searching; });
    }

    return threads[0]->best_move;
}

} // namespace ThreadPool
