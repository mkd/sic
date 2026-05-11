#include "../include/attacks.h"
#include "../include/position.h"
#include "../include/uci.h"
#include "../include/tt.h"

int main() {
    init_attacks();
    init_zobrist();
    init_tt(16);
    uci_loop();
    return 0;
}
