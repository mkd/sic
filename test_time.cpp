#include "src/timeman.cpp"
#include <iostream>

int main() {
    uint64_t start = TimeManager::get_time_ms();
    std::cout << "start: " << start << std::endl;
    return 0;
}
