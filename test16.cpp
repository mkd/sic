#include <iostream>
#include "include/evaluate.h"
#include "include/position.h"

int main() {
    init_magics();
    Position pos;
    pos.set("r3b1k1/1p2bppp/p3p3/B7/P1n1N3/1P6/4QPPP/3R2K1 b - - 0 21");
    Value e = evaluate(pos, false);
    std::cout << "e=" << e << std::endl;
    return 0;
}
