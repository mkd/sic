#include "sf_wrapper.h"

#include "position.h"
#include "evaluate.h"
#include "thread.h"
#include "nnue/network.h"
#include "nnue/nnue_accumulator.h"

#include <vector>
#include <string>

namespace StockfishWrapper {

    thread_local Stockfish::Position g_pos;
    thread_local std::vector<Stockfish::StateInfo> g_states;
    thread_local ThreadState g_thread_state;

    Stockfish::Eval::NNUE::Network* g_network_ptr = nullptr;
    thread_local Stockfish::Eval::NNUE::AccumulatorStack* g_accumulators_ptr = nullptr;
    thread_local Stockfish::Eval::NNUE::AccumulatorCaches* g_caches_ptr = nullptr;

    void init() {
        Stockfish::Bitboards::init();
        Stockfish::Position::init();
        
        Stockfish::Eval::NNUE::EvalFile ef;
        ef.defaultName = EvalFileDefaultName;
        ef.current = "None";
        g_network_ptr = new Stockfish::Eval::NNUE::Network(ef);
        
        // Use load_internal since load_user_net is private or just call verify?
        // Wait, Stockfish does networks[0] = std::make_unique<...>();
        // Actually, just calling load() or initialize() is enough if it's default
        g_network_ptr->load("", EvalFileDefaultName);
        std::cout << "Network loaded.\n";
    }
    
    void init_thread() {
        g_states.reserve(2048);
        g_caches_ptr = new Stockfish::Eval::NNUE::AccumulatorCaches(*g_network_ptr);
        g_accumulators_ptr = new Stockfish::Eval::NNUE::AccumulatorStack();
    }

        void set_fen(const std::string& fen) {
        g_states.clear();
        g_states.emplace_back();
        g_pos.set(fen, false, &g_states.back());
        
        g_thread_state.fen = fen;
        g_thread_state.moves.clear();
        g_accumulators_ptr->reset();
    }
    
    Stockfish::Move convert_move(uint16_t sic_move) {
        int from = sic_move & 0x3F;
        int to   = (sic_move >> 6) & 0x3F;
        int prom = (sic_move >> 12) & 0x3;
        int flag = (sic_move >> 14) & 0x3;
        
        int sf_type = Stockfish::NORMAL;
        if (flag == 1) sf_type = Stockfish::PROMOTION;
        else if (flag == 2) sf_type = Stockfish::EN_PASSANT;
        else if (flag == 3) {
            sf_type = Stockfish::CASTLING;
            // Stockfish encodes castling as King to Rook
            if (from == 4) { // E1
                if (to == 6) to = 7; // G1 -> H1
                else if (to == 2) to = 0; // C1 -> A1
            } else if (from == 60) { // E8
                if (to == 62) to = 63; // G8 -> H8
                else if (to == 58) to = 56; // C8 -> A8
            }
        }
        
        int sf_prom = 0;
        if (sf_type == Stockfish::PROMOTION) {
            if (prom == 0) sf_prom = Stockfish::KNIGHT - Stockfish::KNIGHT;
            else if (prom == 1) sf_prom = Stockfish::BISHOP - Stockfish::KNIGHT;
            else if (prom == 2) sf_prom = Stockfish::ROOK - Stockfish::KNIGHT;
            else if (prom == 3) sf_prom = Stockfish::QUEEN - Stockfish::KNIGHT;
        }
        
        return static_cast<Stockfish::Move>((from << 6) | to | sf_type | (sf_prom << 12));
    }

        void do_move(uint16_t move) {
        Stockfish::Move sf_move = convert_move(move);
            g_states.emplace_back();
        auto accum = g_accumulators_ptr->push();
        g_pos.do_move(sf_move, g_states.back(), g_pos.gives_check(sf_move), accum.first, accum.second, nullptr, nullptr);
        g_thread_state.moves.push_back(move);
    }
    
    void undo_move(uint16_t move) {
            Stockfish::Move sf_move = convert_move(move);
            g_pos.undo_move(sf_move);
        g_states.pop_back();
        g_accumulators_ptr->pop();
        g_thread_state.moves.pop_back();
    }

    void do_null_move() {
            g_states.emplace_back();
        g_accumulators_ptr->push();
        g_pos.do_null_move(g_states.back());
        g_thread_state.moves.push_back(0); // 0 acts as null move
    }

    void undo_null_move() {
                g_pos.undo_null_move();
        g_states.pop_back();
        g_accumulators_ptr->pop();
        g_thread_state.moves.pop_back();
    }
    
    int evaluate() {
        return Stockfish::Eval::evaluate(*g_network_ptr, g_pos, *g_accumulators_ptr, *g_caches_ptr, 0);
    }
    
    ThreadState get_thread_state() {
        return g_thread_state;
    }
    
    void sync_thread_state(const ThreadState& state) {
        set_fen(state.fen);
        for (uint16_t m : state.moves) {
            if (m == 0) do_null_move();
            else do_move(m);
        }
    }

}
