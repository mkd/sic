#include "../include/timeman.h"
#include <algorithm>

namespace TimeManager {

uint64_t start_time     = 0;
uint64_t base_optimum_time = 0;
uint64_t optimum_time   = 0;
uint64_t maximum_time   = 0;
double time_factor      = 1.0;
std::atomic<bool> stop_search{false};

uint64_t get_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

void init_timer(int time_left_ms, int increment_ms) {
    // 1. Calculate a safe maximum time (leave 50ms for network/GUI overhead)
    uint64_t safe_max = std::max(0, time_left_ms - 50);

    // 2. Soft Time: Target time we want to spend (usually time_left / 25)
    base_optimum_time = (time_left_ms / 25) + (increment_ms * 3 / 4);
    optimum_time = base_optimum_time;
    
    // 3. Hard Time: Absolute maximum we can spend (approx time_left / 4 + increment)
    maximum_time = std::min(static_cast<uint64_t>(safe_max), static_cast<uint64_t>(time_left_ms / 4) + increment_ms);
    
    // Fallback: Ensure maximum_time doesn't exceed safe_max
    if (maximum_time > safe_max) {
        maximum_time = safe_max;
    }
    
    // Ensure optimum_time doesn't exceed maximum_time
    if (optimum_time > maximum_time) {
        optimum_time = maximum_time;
    }
    if (base_optimum_time > maximum_time) {
        base_optimum_time = maximum_time;
    }

    start_time = get_time_ms();
    time_factor = 1.0;
    stop_search = false;
}

void check_time() {
    // Only abort if we hit the HARD limit during standard nodes
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
    // Best move changed! Extend the soft limit (up to 3.0x)
    time_factor += 0.3;
    if (time_factor > 3.0) time_factor = 3.0;
    optimum_time = std::min(maximum_time, static_cast<uint64_t>(base_optimum_time * time_factor));
}

void extend_time_for_score_drop() {
    // Score dropped significantly. Extend time (up to 4.0x)
    time_factor += 0.5;
    if (time_factor > 4.0) time_factor = 4.0;
    optimum_time = std::min(maximum_time, static_cast<uint64_t>(base_optimum_time * time_factor));
}

void scale_time_for_nodes(uint64_t best_move_nodes, uint64_t total_nodes) {
    if (total_nodes < 10000 || base_optimum_time == 999999999) return;
    double fraction = static_cast<double>(best_move_nodes) / static_cast<double>(total_nodes);
    // Multiply by up to 2.5x if the best move has very few nodes (low confidence)
    double multiplier = std::max(0.6, 2.5 - 2.0 * fraction);
    optimum_time = std::min(maximum_time, static_cast<uint64_t>(base_optimum_time * time_factor * multiplier));
}

} // namespace TimeManager
