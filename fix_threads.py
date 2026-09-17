import sys

# Update thread.h
with open('include/thread.h', 'r') as f:
    content = f.read()

replacement = """#include <mutex>
#include <condition_variable>
#include <memory>
#include <deque>

namespace Stockfish {
    struct Position;
    struct StateInfo;
}

struct SearchWorker {
"""
content = content.replace("struct SearchWorker {", replacement)

thread_struct = """struct Thread {
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
    
    std::shared_ptr<Stockfish::Position> sync_pos;
    std::shared_ptr<std::deque<Stockfish::StateInfo>> sync_setup;

    Thread(int thread_id) : id(thread_id), best_move(MOVE_NONE), max_depth(0), is_searching(false), should_exit(false) {}

    void search();
    void loop();
};"""
import re
content = re.sub(r'struct Thread \{.*?\};', thread_struct, content, flags=re.DOTALL)

with open('include/thread.h', 'w') as f:
    f.write(content)

# Update thread.cpp
cpp_content = """#include "../include/thread.h"
#include "../include/search.h"
#include "stockfish_probe/nnue_incremental.h"

void Thread::search() {
    best_move = search_position(rootPos, max_depth, id);
}

void Thread::loop() {
    Stockfish::Incremental::init(); // Initialize thread-local NNUE once
    
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return is_searching || should_exit; });
        
        if (should_exit) {
            break;
        }
        
        // Sync NNUE state before search
        Stockfish::Incremental::sync_from_main_thread(*sync_pos, *sync_setup);
        
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
    auto pos_ptr = std::make_shared<Stockfish::Position>();
    std::memcpy(pos_ptr.get(), &Stockfish::Incremental::get_global_pos(), sizeof(Stockfish::Position));
    auto setup_ptr = std::make_shared<std::deque<Stockfish::StateInfo>>(Stockfish::Incremental::get_setup_states());

    // Wake up all threads
    for (Thread* t : threads) {
        {
            std::lock_guard<std::mutex> lock(t->mtx);
            t->rootPos = pos;
            t->best_move = MOVE_NONE;
            t->max_depth = max_depth;
            t->sync_pos = pos_ptr;
            t->sync_setup = setup_ptr;
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
"""

with open('src/thread.cpp', 'w') as f:
    f.write(cpp_content)
