#include "../include/timeman.h"

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
    optimum_time = (time_left_ms / 40) + (increment_ms * 3 / 4);
    maximum_time = time_left_ms / 3;
    if (optimum_time > maximum_time) {
        optimum_time = maximum_time;
    }
    start_time = get_time_ms();
    stop_search = false;
}

void check_time() {
    if (maximum_time != 999999999) {
        if (get_time_ms() - start_time >= maximum_time) {
            stop_search = true;
        }
    }
}

} // namespace TimeManager
