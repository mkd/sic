#include "../include/tt.h"
#include "../include/timeman.h"
#include <cstring>
#include <new>

uint8_t TT_AGE = 0;
TTCluster* TT = nullptr;
size_t TT_CLUSTER_COUNT = 0;

void init_tt(size_t mb_size) {
    if (TT != nullptr) {
        operator delete(TT, std::align_val_t(64));
    }
    if (mb_size < 1) mb_size = 1;

    size_t target_count = mb_size * (1 << 20) / sizeof(TTCluster);
    TT_CLUSTER_COUNT = 1;
    while (TT_CLUSTER_COUNT * 2 <= target_count) {
        TT_CLUSTER_COUNT *= 2;
    }
    
    TT = static_cast<TTCluster*>(operator new(TT_CLUSTER_COUNT * sizeof(TTCluster), std::align_val_t(64)));
    clear_tt();
}

void clear_tt() {
    std::memset(TT, 0, TT_CLUSTER_COUNT * sizeof(TTCluster));
    TT_AGE = 0;
}

void inc_tt_age() {
    TT_AGE++;
}

void record_tt(uint64_t key, int depth, Value score, TTFlag flag, Move best_move) {
    if (TimeManager::stop_search) return;

    TTCluster& cluster = TT[key & (TT_CLUSTER_COUNT - 1)];
    uint16_t key16 = static_cast<uint16_t>(key >> 48);
    
    // Clamp to int16_t range (TT stores scores as int16_t)
    if (score > 32000) score = 32000;
    else if (score < -32000) score = -32000;

    int replace_idx = 0;
    int min_depth = 999;
    
    uint16_t e_key16; Move e_move; Value e_score; int8_t e_depth; TTFlag e_flag; uint8_t e_age;

    // 1. Exact match
    for (int i = 0; i < 8; ++i) {
        uint64_t d = cluster.entries[i].data.load(std::memory_order_relaxed);
        unpack_tt(d, e_key16, e_move, e_score, e_depth, e_flag, e_age);
        
        if (d != 0 && e_key16 == key16) {
            if (best_move == MOVE_NONE) {
                best_move = e_move;
            }
            if (depth < e_depth && flag != TT_EXACT) {
                cluster.entries[i].data.store(pack_tt(key16, best_move, e_score, e_depth, e_flag, TT_AGE), std::memory_order_relaxed);
                return;
            }
            replace_idx = i;
            goto write;
        }
    }

    // 1.5 Empty slot
    for (int i = 0; i < 8; ++i) {
        if (cluster.entries[i].data.load(std::memory_order_relaxed) == 0) {
            replace_idx = i;
            goto write;
        }
    }
    
    // 2. Older generation
    for (int i = 0; i < 8; ++i) {
        uint64_t d = cluster.entries[i].data.load(std::memory_order_relaxed);
        unpack_tt(d, e_key16, e_move, e_score, e_depth, e_flag, e_age);
        if (e_age != TT_AGE) {
            replace_idx = i;
            goto write;
        }
    }
    
    // 3. Lowest depth
    for (int i = 0; i < 8; ++i) {
        uint64_t d = cluster.entries[i].data.load(std::memory_order_relaxed);
        unpack_tt(d, e_key16, e_move, e_score, e_depth, e_flag, e_age);
        if (e_depth < min_depth) {
            min_depth = e_depth;
            replace_idx = i;
        }
    }

write:
    cluster.entries[replace_idx].data.store(pack_tt(key16, best_move, score, static_cast<int8_t>(depth), flag, TT_AGE), std::memory_order_relaxed);
}

bool probe_tt(uint64_t key, int depth, int /*alpha*/, int /*beta*/, Value& return_score, Move& tt_move, TTFlag& return_flag, int& tt_depth) {
    TTCluster& cluster = TT[key & (TT_CLUSTER_COUNT - 1)];
    uint16_t key16 = static_cast<uint16_t>(key >> 48);

    uint16_t e_key16; Move e_move; Value e_score; int8_t e_depth; TTFlag e_flag; uint8_t e_age;

    for (int i = 0; i < 8; ++i) {
        uint64_t d = cluster.entries[i].data.load(std::memory_order_relaxed);
        if (d == 0) continue;
        
        unpack_tt(d, e_key16, e_move, e_score, e_depth, e_flag, e_age);
        
        if (e_key16 == key16) {
            if (e_age != TT_AGE) {
                cluster.entries[i].data.store(pack_tt(e_key16, e_move, e_score, e_depth, e_flag, TT_AGE), std::memory_order_relaxed);
            }
            tt_move = e_move;
            return_score = e_score;
            return_flag = e_flag;
            tt_depth = e_depth;

            if (e_depth >= depth) {
                return true;
            }
            return false;
        }
    }
    return false;
}

int get_hashfull() {
    int count = 0;
    int max_samples = TT_CLUSTER_COUNT < 1000 ? TT_CLUSTER_COUNT : 1000;
    int step = TT_CLUSTER_COUNT / max_samples;
    for (int i = 0; i < max_samples; ++i) {
        for (int j = 0; j < 8; ++j) {
            if (TT[i * step].entries[j].data.load(std::memory_order_relaxed) != 0) {
                count++;
            }
        }
    }
    return (count * 1000) / (max_samples * 8);
}
