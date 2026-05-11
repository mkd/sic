#include "../include/timeman.h"

namespace TimeManager {

uint64_t start_time     = 0;
uint64_t allocated_time = 0;
bool stop_search        = false;

uint64_t get_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

void init_timer(int time_left_ms, int increment_ms) {
    allocated_time = (time_left_ms + increment_ms) / 25;
    if (allocated_time > static_cast<uint64_t>(time_left_ms) / 2) {
        allocated_time = time_left_ms / 2;
    }
    start_time = get_time_ms();
    stop_search = false;
}

void check_time() {
    if (get_time_ms() - start_time >= allocated_time) {
        stop_search = true;
    }
}

} // namespace TimeManager
