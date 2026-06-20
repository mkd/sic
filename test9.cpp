#include <iostream>
#include "include/position.h"
#include "include/search.h"

int main() {
    init_magics();
    Position pos;
    pos.set("r2rb1k1/1p2bppp/p3p3/q3P3/P1n1N3/1PB2N2/4QPPP/3R2K1 w - - 0 21");
    Move m = make_move(SQ_C3, SQ_A5);
    bool see = see_ge(pos, m, 0);
    std::cout << "see_ge for c3a5: " << (see ? "true" : "false") << std::endl;
    return 0;
}
