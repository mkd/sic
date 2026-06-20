#include "include/position.h"
#include "include/evaluate.h"
#include "src/stockfish_probe/nnue_incremental.h"
#include "include/uci.h"
#include <iostream>

int main() {
    Stockfish::Probe::init("nn-b1a57edbea57.nnue", "nn-baff1ede1f90.nnue");
    Position pos;
    pos.set_start_position();
    std::string moves = "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7 f1e1 b7b5 a4b3 e8g1 c2c3 d7d6 h2h3 c6b8 d2d4 b8d7 a2a4 c8b7 b1d2 c7c5 d4d5 c5c4 b3c2 d7c5 d1e2 f6d7 d2f1 f7f5 e4f5 b7d5 c1e3 a6a4 c2a4 b5a4 e1d1 d5f7 f5f6 e7f6 d1d6 e5e4 f3g5 f6g5 e3g5 d8g5 d6d7 a8b8 d7d2 g5b5 e2d1 b5b3 d1b3 a4b3 a1a6 f8d8 g1h2 g8f8 e1e2 h7h6 f1e3 b8b5 a6c6 b5b7 e3c4 b7d7 h2g3 d7d3 g3f4 d3d5 c6c5 d5e6 f4e4 e6c4 c5c4 d3d2 e4e2 d2d2 b4b4 d2b2 b4b8 f8f7 b8b7 f7e6 b7b6 e6f7 g2g3 b2f2 b6b3 f2e2 f3f3 e2d2 h3h4 d2d3 f3f4 g7g5 h4g5 h6g5 f4g4 f7f6 b3a3 f6e6 a3a6 e6e7 c3c4 e7f7 a6a7 f7g6 a7c7 d3d4 g4h3 g6h5 c7c5 h5g6 g3g4 d4d3 h3h2 d3d4 h2g3 d4d3 g3f2 d3d4 c5c6 g6f7 c6c7 f7e6 c7f3 d4d3 f3e4 d3g3 c7c8 g3g4 e4f3 g4h4 c4c5 h4f4 f3e3 f4d7 c8h8 d7g4 c5c6 g4c7 h8h7 c7c6 f2f2 c6h4 h7g7 h4f4 e3e2 f4f6 g7g5 f6e6 g5d4 e6e7 d4c3 e7e6 c3b4";
    // wait, I need to parse moves correctly. I'll just use the engine's uci_execute_line
    return 0;
}
