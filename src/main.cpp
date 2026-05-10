#include "../include/attacks.h"
#include "../include/position.h"
#include "../include/uci.h"

int main() {
    init_attacks();
    init_zobrist();
    uci_loop();
    return 0;
}
