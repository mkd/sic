#include <iostream>
#include "src/position.h"
#include "src/zobrist.h"
#include "src/movegen.h"
#include "src/uci.h"
#include "src/search.h"
#include "src/stockfish_probe/probe.h"

int main() {
    Stockfish::Probe::init("nn-71d6d32cb962.nnue", "nn-71d6d32cb962.nnue");
    Zobrist::init();
    Bitboards::init();
    TT.resize(128 * 1024 * 1024);

    Position pos;
    pos.set_fen("1k6/1p2bR2/p3p2p/1q6/4B1P1/1PQ4P/K1P5/3r4 b - - 7 44");
    
    MoveList list;
    MoveGen::generate_legal_moves(pos, list);

    SearchWorker sw;
    sw.clear();

    for (int i = 0; i < list.size(); i++) {
        Move m = list.moves[i];
        if (move_to_str(m) == "d1a1") {
            pos.make_move(m);
            Stockfish::Incremental::setup_reset(pos.fen());
            Value score = evaluate(pos, false);
            std::cout << "Static eval of d1a1: " << score << "\n";
            pos.unmake_move(m);
        }
    }
    return 0;
}
