#pragma once

#include "../include/types.h"
#include <cstddef>

enum TTFlag : uint8_t {
    TT_EXACT = 0,
    TT_ALPHA = 1,
    TT_BETA  = 2
};

struct alignas(16) TTEntry {
    uint64_t key;
    Move     best_move;
    Value    score;
    int8_t   depth;
    TTFlag   flag;
    uint8_t  age;
    uint8_t  padding;
};

struct alignas(64) TTCluster {
    TTEntry entries[4];
};

extern TTCluster* TT;
extern size_t TT_CLUSTER_COUNT;
extern uint8_t TT_AGE;

void inc_tt_age();

void init_tt(size_t mb_size);
void clear_tt();
void record_tt(uint64_t key, int depth, Value score, TTFlag flag, Move best_move);
bool probe_tt(uint64_t key, int depth, int alpha, int beta, Value& return_score, Move& tt_move, TTFlag& return_flag);
int get_hashfull();
