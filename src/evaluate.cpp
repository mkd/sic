#include "evaluate.h"

// ---------------------------------------------------------------------------
//  Piece Square Values (centipawns)
// ---------------------------------------------------------------------------
constexpr int PieceValues[7] = {
    0,   // NONE
    100, // PAWN
    300, // KNIGHT
    300, // BISHOP
    500, // ROOK
    900, // QUEEN
    0    // KING
};

Value evaluate(const Position& pos) {
    int white_material = 0;
    int black_material = 0;

    for (int pt = 1; pt <= 6; ++pt) {
        const int val = PieceValues[pt];
        white_material += popcount(pos.pieces(Color::WHITE) & pos.pieces(static_cast<PieceType>(pt))) * val;
        black_material += popcount(pos.pieces(Color::BLACK) & pos.pieces(static_cast<PieceType>(pt))) * val;
    }

    const int score = white_material - black_material;
    return (pos.sideToMove == Color::WHITE) ? score : -score;
}
