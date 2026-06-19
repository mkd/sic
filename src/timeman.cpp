#include "../include/timeman.h"
#include <algorithm>

namespace TimeManager {

uint64_t start_time     = 0;
uint64_t optimum_time   = 0;
uint64_t maximum_time   = 0;
bool stop_search        = false;

uint64_t get_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

void init_timer(int time_left_ms, int increment_ms) {
    // Soft Time: Target time we want to spend (usually time_left / 40)
    optimum_time = (time_left_ms / 40) + (increment_ms * 3 / 4);
    
    // Hard Time: Absolute maximum we can spend (approx time_left / 5)
    maximum_time = (time_left_ms / 5);

    if (optimum_time > static_cast<uint64_t>(time_left_ms) / 2) {
        optimum_time = time_left_ms / 2;
    }
    if (maximum_time > static_cast<uint64_t>(time_left_ms) - 100) { // Safety margin
        maximum_time = std::max(0, time_left_ms - 100);
    }
    
    // If maximum_time is somehow smaller than optimum, cap optimum
    if (optimum_time > maximum_time) {
        optimum_time = maximum_time;
    }

    start_time = get_time_ms();
    stop_search = false;
}

void check_time() {
    // Only abort if we hit the HARD limit during standard nodes
    // The soft limit is evaluated at the root!
    if (get_time_ms() - start_time >= maximum_time) {
        stop_search = true;
    }
}

void check_time_at_root() {
    // At the root between depth iterations, abort if we exceed the SOFT limit
    if (get_time_ms() - start_time >= optimum_time) {
        stop_search = true;
    }
}

void extend_time_for_instability() {
    // Best move changed! Extend the soft limit by 1.5x, up to maximum_time
    optimum_time = std::min(maximum_time, static_cast<uint64_t>(optimum_time * 1.5));
}

void extend_time_for_score_drop() {
    // Score crashed! Extend the soft limit by 2.0x, up to maximum_time
    optimum_time = std::min(maximum_time, static_cast<uint64_t>(optimum_time * 2.0));
}

} // namespace TimeManager
