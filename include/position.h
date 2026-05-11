#pragma once

#include "types.h"
#include "utils.h"
#include "piece.h"
#include "geometry.h"
#include <string>

// ---------------------------------------------------------------------------
//  Zobrist Hashing Tables (extern; populated by init_zobrist())
// ---------------------------------------------------------------------------
extern uint64_t ZobristPiece[12][64];
extern uint64_t ZobristCastling[16];
extern uint64_t ZobristEpFile[8];
extern uint64_t ZobristSide;

void init_zobrist();

// ---------------------------------------------------------------------------
//  Castling Rights Encoding
// ---------------------------------------------------------------------------
enum CastlingSide : int {
    CASTLING_NONE   = 0,
    WHITE_OO        = 1,
    WHITE_OOO       = 2,
    BLACK_OO        = 4,
    BLACK_OOO       = 8,
};

// ---------------------------------------------------------------------------
//  Position Class
// ---------------------------------------------------------------------------
class Position {
public:
    Bitboard byTypeBB[7];
    Bitboard byColorBB[2];
    Piece board[64];
    Color sideToMove;
    int castlingRights;
    Square epSquare;
    int halfmoveClock;
    int fullmoveNumber;
    uint64_t zobristKey;

    Position() = default;

    // -----------------------------------------------------------------------
    //  FEN Parsing
    // -----------------------------------------------------------------------
    void set_fen(const std::string& fen);

    // -----------------------------------------------------------------------
    //  Basic Getters (FORCE_INLINE)
    // -----------------------------------------------------------------------
    FORCE_INLINE Bitboard pieces(PieceType pt) const {
        return byTypeBB[static_cast<int>(pt)];
    }

    FORCE_INLINE Bitboard pieces(Color c) const {
        return byColorBB[static_cast<int>(c)];
    }

    FORCE_INLINE Piece piece_on(Square sq) const {
        return board[static_cast<int>(sq)];
    }

    FORCE_INLINE Bitboard occupied() const {
        return byColorBB[0] | byColorBB[1];
    }

    FORCE_INLINE Bitboard empty_squares() const {
        return ~occupied();
    }

    // -----------------------------------------------------------------------
    //  King Location
    // -----------------------------------------------------------------------
    FORCE_INLINE Square get_king_square(Color c) const {
        return lsb(pieces(c) & pieces(PieceType::KING));
    }

    // -----------------------------------------------------------------------
    //  Attack Detection
    // -----------------------------------------------------------------------
    bool is_attacked(Square sq, Color attacker) const;

    // -----------------------------------------------------------------------
    //  Move Execution
    // -----------------------------------------------------------------------
    bool make_move(Move m);
    void make_null_move();
};
