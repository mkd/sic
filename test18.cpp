#include <iostream>
#include "include/search.h"
#include "include/position.h"
#include "include/movegen.h"
#include "include/evaluate.h"

int main() {
    init_magics();
    init_tt(16);
    Position pos;
    pos.set("r2rb1k1/1p1nbppp/p1n1p3/q3P3/P1B1N3/1P3N2/1B2QPPP/2RR2K1 b - - 4 17");
    
    // Play d7e5
    pos.make_move(make_move(Square::SQ_D7, Square::SQ_E5));
    // Play f3e5
    pos.make_move(make_move(Square::SQ_F3, Square::SQ_E5));
    // Play d8d1
    pos.make_move(make_move(Square::SQ_D8, Square::SQ_D1));
    // Play c1d1
    pos.make_move(make_move(Square::SQ_C1, Square::SQ_D1));
    // Play c6e5
    pos.make_move(make_move(Square::SQ_C6, Square::SQ_E5));
    // Play b2c3
    pos.make_move(make_move(Square::SQ_B2, Square::SQ_C3));
    // Play e5c4
    pos.make_move(make_move(Square::SQ_E5, Square::SQ_C4));
    
    SearchWorker sw;
    sw.clear();
    
    Value qs = quiescence(pos, -VALUE_MATE, VALUE_MATE, 0, sw);
    std::cout << "QS after e5c4: " << qs << std::endl;
    
    return 0;
}
