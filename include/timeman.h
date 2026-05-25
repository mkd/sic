#pragma once

#include <cstdint>
#include <chrono>

namespace TimeManager {

uint64_t get_time_ms();

void init_timer(int time_left_ms, int increment_ms);

void check_time();

extern uint64_t start_time;
extern uint64_t allocated_time;
extern bool stop_search;

} // namespace TimeManager
