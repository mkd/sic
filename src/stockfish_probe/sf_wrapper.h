#pragma once
#include <string>
#include <vector>

namespace StockfishWrapper {

    void init();
    void init_thread();
    
    void set_fen(const std::string& fen);
    
    void do_move(uint16_t move);
    void undo_move(uint16_t move);
    void do_null_move();
    void undo_null_move();
    
    int evaluate();

    struct ThreadState {
        std::string fen;
        std::vector<uint16_t> moves;
    };
    
    ThreadState get_thread_state();
    void sync_thread_state(const ThreadState& state);
}
