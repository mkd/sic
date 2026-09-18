#pragma once

#include <cstdint>
#include <chrono>

namespace TimeManager {

uint64_t get_time_ms();

void init_timer(int time_left_ms, int increment_ms, int moves_to_go);

void check_time();
void check_time_at_root();

void extend_time_for_instability();
void extend_time_for_score_drop();
void scale_time_for_nodes(uint64_t best_move_nodes, uint64_t total_nodes);

extern uint64_t start_time;
extern uint64_t base_optimum_time;
extern uint64_t optimum_time;
extern uint64_t maximum_time;
extern double time_factor;
#include <atomic>

extern std::atomic<bool> stop_search;

} // namespace TimeManager
