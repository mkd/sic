#pragma once

#include "types.h"
#include "utils.h"

// ---------------------------------------------------------------------------
//  Move Encoding Bit Layout (16 bits, Stockfish style)
//
//  Bits  0- 5 : from square   (6 bits)
//  Bits  6-11 : to square     (6 bits)
//  Bits 12-13 : promotion     (2 bits: 0=KNIGHT, 1=BISHOP, 2=ROOK, 3=QUEEN)
//  Bits 14-15 : flag          (2 bits: 0=Normal, 1=EnPassant, 2=Castling)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
//  Move Flags
// ---------------------------------------------------------------------------
enum MoveFlag : int {
    MOVE_FLAG_NORMAL   = 0,
    MOVE_FLAG_ENPASSANT = 1,
    MOVE_FLAG_CASTLING  = 2,
};

// ---------------------------------------------------------------------------
//  Move Construction & Extraction (FORCE_INLINE constexpr)
// ---------------------------------------------------------------------------
FORCE_INLINE constexpr Move make_move(Square from, Square to, int flag = MOVE_FLAG_NORMAL, PieceType prom = PieceType::NONE) {
    const int prom_offset = (prom == PieceType::NONE) ? 0 : (static_cast<int>(prom) - 2);
    return static_cast<Move>(
        static_cast<int>(from)
        | (static_cast<int>(to) << 6)
        | (prom_offset << 12)
        | (flag << 14)
    );
}

FORCE_INLINE constexpr Square move_from(Move m) {
    return static_cast<Square>(m & 0x3F);
}

FORCE_INLINE constexpr Square move_to(Move m) {
    return static_cast<Square>((m >> 6) & 0x3F);
}

FORCE_INLINE constexpr PieceType move_prom(Move m) {
    const int val = (m >> 12) & 0x3;
    return (val == 0) ? PieceType::NONE : static_cast<PieceType>(val + 2);
}

FORCE_INLINE constexpr int move_flag(Move m) {
    return (m >> 14) & 0x3;
}

// ---------------------------------------------------------------------------
//  MoveList: Fixed-size, allocation-free move container
// ---------------------------------------------------------------------------
struct MoveList {
    Move moves[MAX_MOVES];
    int count = 0;

    FORCE_INLINE void push(Move m) {
        moves[count++] = m;
    }

    FORCE_INLINE int size() const {
        return count;
    }

    FORCE_INLINE void clear() {
        count = 0;
    }

    Move* begin() { return moves; }
    Move* end()   { return moves + count; }
    const Move* begin() const { return moves; }
    const Move* end() const   { return moves + count; }

    FORCE_INLINE Move& operator[](int i) {
        return moves[i];
    }

    FORCE_INLINE const Move& operator[](int i) const {
        return moves[i];
    }
};
