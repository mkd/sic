#include "../include/evaluate.h"
#include "stockfish_probe/nnue_incremental.h"

// Piece Values for move ordering/SEE
const int PieceValues[7] = {
    0,    // NONE
    322,  // PAWN
    1048, // KNIGHT
    1048, // BISHOP
    1650, // ROOK
    3150, // QUEEN
    0     // KING
};

Value evaluate(const Position& pos, bool force_small) {
    (void)pos; // Suppress unused parameter warning
    int eval = Stockfish::Incremental::evaluate(force_small);
    
    // NNUE intrinsically understands 50-move rule progression, do not taper manually.
    return eval;
}
