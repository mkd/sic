#pragma once

#include "../include/types.h"
#include <cstddef>
#include <atomic>
#include <cstdint>

enum TTFlag : uint8_t {
    TT_EXACT = 0,
    TT_ALPHA = 1,
    TT_BETA  = 2,
    TT_NONE  = 3
};

// 8-byte TT entry (guarantees tear-free atomic SMP)
// Bit layout:
// 0-15:  key16 (upper 16 bits of Zobrist key)
// 16-31: move (16 bits)
// 32-47: score (16 bits)
// 48-55: depth (8 bits)
// 56-63: flag & age (8 bits: flag in lower 2 bits, age in upper 6 bits)
struct TTEntry {
    std::atomic<uint64_t> data;
};

// 8 entries per 64-byte cache line
struct alignas(64) TTCluster {
    TTEntry entries[8];
};

extern TTCluster* TT;
extern size_t TT_CLUSTER_COUNT;
extern uint8_t TT_AGE;

void inc_tt_age();
void init_tt(size_t mb_size);
void clear_tt();
void record_tt(uint64_t key, int depth, Value score, TTFlag flag, Move best_move);
bool probe_tt(uint64_t key, int depth, int alpha, int beta, Value& return_score, Move& tt_move, TTFlag& return_flag, int& tt_depth);
int get_hashfull();

inline uint64_t pack_tt(uint16_t key16, Move best_move, Value score, int8_t depth, TTFlag flag, uint8_t age) {
    uint64_t d = 0;
    d |= static_cast<uint64_t>(key16);
    d |= static_cast<uint64_t>(best_move) << 16;
    d |= static_cast<uint64_t>(static_cast<uint16_t>(score)) << 32;
    d |= static_cast<uint64_t>(static_cast<uint8_t>(depth)) << 48;
    uint8_t genBound = (age << 2) | (flag & 0x3);
    d |= static_cast<uint64_t>(genBound) << 56;
    return d;
}

inline void unpack_tt(uint64_t d, uint16_t& key16, Move& best_move, Value& score, int8_t& depth, TTFlag& flag, uint8_t& age) {
    key16 = static_cast<uint16_t>(d & 0xFFFF);
    best_move = static_cast<Move>((d >> 16) & 0xFFFF);
    score = static_cast<int16_t>((d >> 32) & 0xFFFF);
    depth = static_cast<int8_t>((d >> 48) & 0xFF);
    uint8_t genBound = static_cast<uint8_t>((d >> 56) & 0xFF);
    flag = static_cast<TTFlag>(genBound & 0x3);
    age = genBound >> 2;
}
