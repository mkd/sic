#include "../include/evaluate.h"
#include "stockfish_probe/nnue_incremental.h"

// Piece Values for move ordering/SEE
const int PieceValues[7] = {
    0,    // NONE
    100,  // PAWN
    300,  // KNIGHT
    300,  // BISHOP
    500,  // ROOK
    900,  // QUEEN
    0     // KING
};

Value evaluate(const Position& pos, bool force_small) {
    int eval = Stockfish::Incremental::evaluate(force_small);
    // Tapering based on halfmoveClock for 50-move rule scaling
    return eval * (100 - pos.halfmoveClock) / 100;
}
