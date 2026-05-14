#include "../include/attacks.h"
#include "../include/position.h"
#include "../include/search.h"
#include "../include/uci.h"
#include "../include/tt.h"
#include "../include/thread.h"

int main() {
    init_attacks();
    init_zobrist();
    init_tt(1024);
    init_lmr();
    ThreadPool::init();
    uci_loop();
    return 0;
}
