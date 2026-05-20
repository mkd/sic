#include "../include/tt.h"
#include <cstring>
#include <new>

TTEntry* TT = nullptr;
size_t TT_SIZE = 0;

void init_tt(size_t mb_size) {
    if (TT != nullptr) {
        operator delete(TT, std::align_val_t(16));
    }
    if (mb_size < 1) mb_size = 1;

    TT_SIZE = mb_size * (1 << 20) / sizeof(TTEntry);
    TT = static_cast<TTEntry*>(operator new(TT_SIZE * sizeof(TTEntry), std::align_val_t(16)));
    clear_tt();
}

void clear_tt() {
    std::memset(TT, 0, TT_SIZE * sizeof(TTEntry));
}

void record_tt(uint64_t key, int depth, Value score, TTFlag flag, Move best_move) {
    TTEntry& entry = TT[key & (TT_SIZE - 1)];
    entry.key       = key;
    entry.best_move = best_move;
    // Clamp non-exact scores to prevent VALUE_INFINITE from leaking as a mate score
    if (flag != TT_EXACT) {
        if (score > VALUE_MATE_IN_1) score = VALUE_MATE_IN_1;
        else if (score < -VALUE_MATE_IN_1) score = -VALUE_MATE_IN_1;
    }
    entry.score     = score;
    entry.depth     = static_cast<int8_t>(depth);
    entry.flag      = flag;
}

bool probe_tt(uint64_t key, int depth, int alpha, int beta, Value& return_score, Move& tt_move, TTFlag& return_flag) {
    TTEntry& entry = TT[key & (TT_SIZE - 1)];

    if (entry.key == key) {
        tt_move = entry.best_move;

        if (entry.depth >= depth) {
            return_score = entry.score;
            return_flag = entry.flag;
            return true;
        }
    }

    return false;
}

int get_hashfull() {
    int count = 0;
    int max_samples = TT_SIZE < 1000 ? TT_SIZE : 1000;
    for (int i = 0; i < max_samples; ++i) {
        if (TT[i].key != 0) count++;
    }
    return (count * 1000) / max_samples;
}
