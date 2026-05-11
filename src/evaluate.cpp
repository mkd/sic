#include "../include/evaluate.h"
#include "../include/nnue.h"

// ---------------------------------------------------------------------------
//  Piece Values (centipawns)
// ---------------------------------------------------------------------------
const int PieceValues[7] = {
    0,   // NONE
    100, // PAWN
    300, // KNIGHT
    300, // BISHOP
    500, // ROOK
    900, // QUEEN
    0    // KING
};

// ---------------------------------------------------------------------------
//  Piece-Square Tables (White perspective; flip sq ^ 56 for Black)
// ---------------------------------------------------------------------------
constexpr int PawnPST[64] = {
     0,  0,  0, 30, 30,  0,  0,  0,
     0,  0, 10, 20, 20, 10,  0,  0,
    -5,  0,  5, 10, 10,  5,  0, -5,
    -5, -5,  0,  5,  5,  0, -5, -5,
   -10,-10, -5,  0,  0, -5,-10,-10,
   -10,-10, -5, -5,-10, -5,-10,-10,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0
};

constexpr int KnightPST[64] = {
    -10,-10,-10,-10,-10,-10,-10,-10,
    -10,  0,  0, -5, -5,  0,  0,-10,
    -10,  0, 10,  5,  5, 10,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  5,  0,-10,
    -10, -5, -5, -5, -5, -5, -5,-10,
    -10,-10,-10,-10,-10,-10,-10,-10
};

static FORCE_INLINE int pst_score(Bitboard pieces, const int table[64], bool is_white) {
    int score = 0;
    Bitboard bb = pieces;
    while (bb.bb) {
        int sq = static_cast<int>(pop_lsb(bb));
        score += is_white ? table[sq] : table[sq ^ 56];
    }
    return score;
}

Value evaluate(const Position& pos) {
    if (!nnue_initialized) {
        int white_material = 0;
        int black_material = 0;

        for (int pt = 1; pt <= 6; ++pt) {
            const int val = PieceValues[pt];
            white_material += popcount(pos.pieces(Color::WHITE) & pos.pieces(static_cast<PieceType>(pt))) * val;
            black_material += popcount(pos.pieces(Color::BLACK) & pos.pieces(static_cast<PieceType>(pt))) * val;
        }

        int score = white_material - black_material;

        Bitboard wp = pos.pieces(Color::WHITE) & pos.pieces(PieceType::PAWN);
        Bitboard bp = pos.pieces(Color::BLACK) & pos.pieces(PieceType::PAWN);
        score += pst_score(wp, PawnPST, true) - pst_score(bp, PawnPST, false);

        Bitboard wn = pos.pieces(Color::WHITE) & pos.pieces(PieceType::KNIGHT);
        Bitboard bn = pos.pieces(Color::BLACK) & pos.pieces(PieceType::KNIGHT);
        score += pst_score(wn, KnightPST, true) - pst_score(bn, KnightPST, false);

        return (pos.sideToMove == Color::WHITE) ? score : -score;
    }

    if (pos.accumulator_stale) {
        refresh_accumulator(const_cast<Position&>(pos));
    }

    const int16_t* us = (pos.sideToMove == Color::WHITE) ? pos.accumulator.white : pos.accumulator.black;
    const int16_t* them = (pos.sideToMove == Color::WHITE) ? pos.accumulator.black : pos.accumulator.white;

    int32_t sum = 0;
    for (int i = 0; i < TRANSFORMER_NEURONS; ++i) {
        sum += clipped_relu(us[i]) * output_weights[i];
        sum += clipped_relu(them[i]) * output_weights[i + TRANSFORMER_NEURONS];
    }

    return (sum + output_bias) / 16;
}
