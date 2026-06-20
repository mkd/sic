#pragma once

#include <cstdint>
#include <chrono>

namespace TimeManager {

uint64_t get_time_ms();

void init_timer(int time_left_ms, int increment_ms);

void check_time();
void check_time_at_root();

void extend_time_for_instability();
void extend_time_for_score_drop();

extern uint64_t start_time;
extern uint64_t base_optimum_time;
extern uint64_t optimum_time;
extern uint64_t maximum_time;
extern double time_factor;
extern bool stop_search;

} // namespace TimeManager
