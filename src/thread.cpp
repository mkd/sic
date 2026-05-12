#include "../include/thread.h"
#include "../include/search.h"

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
    for (Thread* t : threads) {
        t->rootPos = pos;
        t->best_move = MOVE_NONE;
        t->stdThread = std::thread(&Thread::search, t, max_depth);
    }

    for (Thread* t : threads) {
        if (t->stdThread.joinable()) {
            t->stdThread.join();
        }
    }

    return threads[0]->best_move;
}

} // namespace ThreadPool
