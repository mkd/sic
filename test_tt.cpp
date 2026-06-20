#include <iostream>
#include <cstdint>

struct alignas(64) TTCluster {
    uint64_t entries[4][2]; // 64 bytes
};

int main() {
    size_t mb_size = 8000;
    size_t target_count = mb_size * (1 << 20) / sizeof(TTCluster);
    size_t TT_CLUSTER_COUNT = 1;
    while (TT_CLUSTER_COUNT * 2 <= target_count) {
        TT_CLUSTER_COUNT *= 2;
    }
    std::cout << "target_count: " << target_count << "\n";
    std::cout << "TT_CLUSTER_COUNT: " << TT_CLUSTER_COUNT << "\n";
    return 0;
}
