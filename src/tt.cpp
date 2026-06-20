#include "../include/tt.h"
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
    TTCluster& cluster = TT[key & (TT_CLUSTER_COUNT - 1)];
    
    if (flag != TT_EXACT) {
        if (score > VALUE_MATE_IN_1) score = VALUE_MATE_IN_1;
        else if (score < -VALUE_MATE_IN_1) score = -VALUE_MATE_IN_1;
    }

    int replace_idx = 0;
    int min_depth = 999;
    
    // 1. Exact match
    for (int i = 0; i < 4; ++i) {
        if (cluster.entries[i].key == key) {
            // Preserve best move if the new move is NONE
            if (best_move == MOVE_NONE) {
                best_move = cluster.entries[i].best_move;
            }
            
            // Depth protection: don't overwrite a deeper entry with a shallower non-exact entry
            if (depth < cluster.entries[i].depth && flag != TT_EXACT) {
                // We still update the best move and age
                cluster.entries[i].best_move = best_move;
                cluster.entries[i].age = TT_AGE;
                return;
            }
            
            replace_idx = i;
            goto write;
        }
    }

    // 1.5 Empty slot
    for (int i = 0; i < 4; ++i) {
        if (cluster.entries[i].key == 0) {
            replace_idx = i;
            goto write;
        }
    }
    
    // 2. Older generation
    for (int i = 0; i < 4; ++i) {
        if (cluster.entries[i].age != TT_AGE) {
            replace_idx = i;
            goto write;
        }
    }
    
    // 3. Lowest depth
    for (int i = 0; i < 4; ++i) {
        if (cluster.entries[i].depth < min_depth) {
            min_depth = cluster.entries[i].depth;
            replace_idx = i;
        }
    }

write:
    cluster.entries[replace_idx].key = key;
    cluster.entries[replace_idx].best_move = best_move;
    cluster.entries[replace_idx].score = score;
    cluster.entries[replace_idx].depth = static_cast<int8_t>(depth);
    cluster.entries[replace_idx].flag = flag;
    cluster.entries[replace_idx].age = TT_AGE;
}

bool probe_tt(uint64_t key, int depth, int /*alpha*/, int /*beta*/, Value& return_score, Move& tt_move, TTFlag& return_flag) {
    TTCluster& cluster = TT[key & (TT_CLUSTER_COUNT - 1)];

    for (int i = 0; i < 4; ++i) {
        if (cluster.entries[i].key == key) {
            cluster.entries[i].age = TT_AGE; // Refresh age
            tt_move = cluster.entries[i].best_move;

            if (cluster.entries[i].depth >= depth) {
                return_score = cluster.entries[i].score;
                return_flag = cluster.entries[i].flag;
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
        for (int j = 0; j < 4; ++j) {
            if (TT[i * step].entries[j].key != 0) {
                count++;
            }
        }
    }
    return (count * 1000) / (max_samples * 4);
}
