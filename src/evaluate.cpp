#include "../include/evaluate.h"
#include "../include/utils.h"

// ---------------------------------------------------------------------------
//  NNUE Initialization Flag
// ---------------------------------------------------------------------------
bool nnue_initialized = false;

// ---------------------------------------------------------------------------
//  Piece Values (centipawns) — fallback when NNUE not loaded
// ---------------------------------------------------------------------------
const int PieceValues[7] = {
    0,    // NONE
    100,  // PAWN
    300,  // KNIGHT
    300,  // BISHOP
    500,  // ROOK
    900,  // QUEEN
    0     // KING
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

// ---------------------------------------------------------------------------
//  Sic Piece -> nnue-probe Piece Code Mapping
//  Sic:   WP=0, WN=1, WB=2, WR=3, WQ=4, WK=5,
//         BP=6, BN=7, BB=8, BR=9, BQ=10, BK=11, NONE=12
//  nnue:  wpawn=6, wknight=5, wbishop=4, wrook=3, wqueen=2, wking=1,
//         bpawn=12, bknight=11, bbishop=10, brook=9, bqueen=8, bking=7, blank=0
// ---------------------------------------------------------------------------
constexpr int SicToNnuePiece[13] = {
    6,    // 0:  WHITE_PAWN   -> wpawn
    5,    // 1:  WHITE_KNIGHT -> wknight
    4,    // 2:  WHITE_BISHOP -> wbishop
    3,    // 3:  WHITE_ROOK   -> wrook
    2,    // 4:  WHITE_QUEEN  -> wqueen
    1,    // 5:  WHITE_KING   -> wking
    12,   // 6:  BLACK_PAWN   -> bpawn
    11,   // 7:  BLACK_KNIGHT -> bknight
    10,   // 8:  BLACK_BISHOP -> bbishop
    9,    // 9:  BLACK_ROOK   -> brook
    8,    // 10: BLACK_QUEEN  -> bqueen
    7,    // 11: BLACK_KING   -> bking
    0,    // 12: PIECE_NONE   -> blank
};

// ---------------------------------------------------------------------------
//  Extract pieces[] and squares[] arrays from Sic's Position bitboards
//  nnue-probe requires:
//    pieces[0] = white king, squares[0] = its square
//    pieces[1] = black king, squares[1] = its square
//    pieces[2..n] = remaining pieces in any order
//    pieces[n+1] = 0 (terminator)
//  Max 32 pieces on board + 1 terminator = 33 entries
// ---------------------------------------------------------------------------
static FORCE_INLINE void extract_nnue_piece_lists(
    const Position& pos,
    int pieces[33],
    int squares[33]
) {
    int idx = 0;

    // White king (must be pieces[0])
    {
        Bitboard wk = pos.pieces(Color::WHITE) & pos.pieces(PieceType::KING);
        pieces[idx] = SicToNnuePiece[static_cast<int>(Piece::WHITE_KING)];
        squares[idx] = static_cast<int>(lsb(wk));
        idx++;
    }

    // Black king (must be pieces[1])
    {
        Bitboard bk = pos.pieces(Color::BLACK) & pos.pieces(PieceType::KING);
        pieces[idx] = SicToNnuePiece[static_cast<int>(Piece::BLACK_KING)];
        squares[idx] = static_cast<int>(lsb(bk));
        idx++;
    }

    // All other pieces (any order)
    {
        Bitboard all = pos.occupied();

        // Remove kings from the bitboard
        all.bb &= ~(1ULL << static_cast<int>(lsb(pos.pieces(Color::WHITE) & pos.pieces(PieceType::KING))));
        all.bb &= ~(1ULL << static_cast<int>(lsb(pos.pieces(Color::BLACK) & pos.pieces(PieceType::KING))));

        while (all.bb) {
            Square sq = pop_lsb(all);
            Piece pc = pos.piece_on(sq);
            pieces[idx] = SicToNnuePiece[static_cast<int>(pc)];
            squares[idx] = static_cast<int>(sq);
            idx++;
        }
    }

    // Terminator
    pieces[idx] = 0;
    squares[idx] = 0;
}

// ---------------------------------------------------------------------------
//  Main Evaluation Function
// ---------------------------------------------------------------------------
Value evaluate(const Position& pos) {
    if (!nnue_initialized) {
        // Fallback: simple material + PST evaluation
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

    // Extract piece/square arrays for nnue-probe
    int pieces[33];
    int squares[33];
    extract_nnue_piece_lists(pos, pieces, squares);

    // Determine player (side to move)
    int player = static_cast<int>(pos.sideToMove);

    // Call nnue-probe via bridge (incremental if accumulator is fresh,
    // otherwise falls back to full refresh internally)
    int score = nnue_bridge_evaluate(
        pieces, squares, player,
        &pos.nnueState,
        pos.nnueStatePlyMinus1,
        pos.nnueStatePlyMinus2
    );

    return score;
}
