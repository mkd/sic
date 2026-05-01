#pragma once

#include <cstdint>
#include <limits>

// ---------------------------------------------------------------------------
// Color
// ---------------------------------------------------------------------------
inline constexpr int MAX_MOVES = 256;

enum class Color : int8_t {
    WHITE = 0,
    BLACK = 1,
};

inline constexpr Color operator^(Color c, int i) {
    return static_cast<Color>(static_cast<int8_t>(c) ^ i);
}

inline constexpr bool operator!=(Color c1, Color c2) {
    return static_cast<int8_t>(c1) != static_cast<int8_t>(c2);
}

inline constexpr bool operator==(Color c1, Color c2) {
    return static_cast<int8_t>(c1) == static_cast<int8_t>(c2);
}

inline constexpr Color operator~(Color c) {
    return c ^ 1;
}

// ---------------------------------------------------------------------------
// PieceType
// ---------------------------------------------------------------------------
enum class PieceType : int8_t {
    PAWN   = 1,
    KNIGHT = 2,
    BISHOP = 3,
    ROOK   = 4,
    QUEEN  = 5,
    KING   = 6,

    NONE = 0,
};

inline constexpr bool operator<(PieceType pt1, PieceType pt2) {
    return static_cast<int8_t>(pt1) < static_cast<int8_t>(pt2);
}

inline constexpr bool operator<=(PieceType pt1, PieceType pt2) {
    return static_cast<int8_t>(pt1) <= static_cast<int8_t>(pt2);
}

inline constexpr bool operator>(PieceType pt1, PieceType pt2) {
    return static_cast<int8_t>(pt1) > static_cast<int8_t>(pt2);
}

inline constexpr bool operator>=(PieceType pt1, PieceType pt2) {
    return static_cast<int8_t>(pt1) >= static_cast<int8_t>(pt2);
}

// ---------------------------------------------------------------------------
// Piece  (encoded as Color * 6 + PieceType; PIECE_NONE = 12)
// ---------------------------------------------------------------------------
enum class Piece : int8_t {
    WHITE_PAWN   = 0,
    WHITE_KNIGHT = 1,
    WHITE_BISHOP = 2,
    WHITE_ROOK   = 3,
    WHITE_QUEEN  = 4,
    WHITE_KING   = 5,

    BLACK_PAWN   = 6,
    BLACK_KNIGHT = 7,
    BLACK_BISHOP = 8,
    BLACK_ROOK   = 9,
    BLACK_QUEEN  = 10,
    BLACK_KING   = 11,

    PIECE_NONE = 12,
};

inline constexpr Piece make_piece(Color c, PieceType pt) {
    return static_cast<Piece>(static_cast<int8_t>(c) * 6 + static_cast<int8_t>(pt));
}

inline constexpr Color color_of(Piece pc) {
    if (pc == Piece::PIECE_NONE) return Color::WHITE;
    return static_cast<Color>(static_cast<int8_t>(pc) / 6);
}

inline constexpr PieceType piece_type(Piece pc) {
    if (pc == Piece::PIECE_NONE) return PieceType::NONE;
    return static_cast<PieceType>(static_cast<int8_t>(pc) % 6);
}

inline constexpr bool operator==(Piece pc1, Piece pc2) {
    return static_cast<int8_t>(pc1) == static_cast<int8_t>(pc2);
}

inline constexpr bool operator!=(Piece pc1, Piece pc2) {
    return static_cast<int8_t>(pc1) != static_cast<int8_t>(pc2);
}

// ---------------------------------------------------------------------------
// Square  (SQ_A1 = 0 .. SQ_H8 = 63)
// ---------------------------------------------------------------------------
enum class Square : int8_t {
    SQ_A1 = 0,  SQ_B1 = 1,  SQ_C1 = 2,  SQ_D1 = 3,
    SQ_E1 = 4,  SQ_F1 = 5,  SQ_G1 = 6,  SQ_H1 = 7,
    SQ_A2 = 8,  SQ_B2 = 9,  SQ_C2 = 10, SQ_D2 = 11,
    SQ_E2 = 12, SQ_F2 = 13, SQ_G2 = 14, SQ_H2 = 15,
    SQ_A3 = 16, SQ_B3 = 17, SQ_C3 = 18, SQ_D3 = 19,
    SQ_E3 = 20, SQ_F3 = 21, SQ_G3 = 22, SQ_H3 = 23,
    SQ_A4 = 24, SQ_B4 = 25, SQ_C4 = 26, SQ_D4 = 27,
    SQ_E4 = 28, SQ_F4 = 29, SQ_G4 = 30, SQ_H4 = 31,
    SQ_A5 = 32, SQ_B5 = 33, SQ_C5 = 34, SQ_D5 = 35,
    SQ_E5 = 36, SQ_F5 = 37, SQ_G5 = 38, SQ_H5 = 39,
    SQ_A6 = 40, SQ_B6 = 41, SQ_C6 = 42, SQ_D6 = 43,
    SQ_E6 = 44, SQ_F6 = 45, SQ_G6 = 46, SQ_H6 = 47,
    SQ_A7 = 48, SQ_B7 = 49, SQ_C7 = 50, SQ_D7 = 51,
    SQ_E7 = 52, SQ_F7 = 53, SQ_G7 = 54, SQ_H7 = 55,
    SQ_A8 = 56, SQ_B8 = 57, SQ_C8 = 58, SQ_D8 = 59,
    SQ_E8 = 60, SQ_F8 = 61, SQ_G8 = 62, SQ_H8 = 63,

    SQ_NONE = 64,
};

inline constexpr Square operator+(Square sq, int i) {
    return static_cast<Square>(static_cast<int8_t>(sq) + i);
}

inline constexpr Square operator-(Square sq, int i) {
    return static_cast<Square>(static_cast<int8_t>(sq) - i);
}

inline constexpr Square operator+=(Square& sq, int i) {
    sq = sq + i;
    return sq;
}

inline constexpr Square operator-=(Square& sq, int i) {
    sq = sq - i;
    return sq;
}

inline constexpr bool operator==(Square sq1, Square sq2) {
    return static_cast<int8_t>(sq1) == static_cast<int8_t>(sq2);
}

inline constexpr bool operator!=(Square sq1, Square sq2) {
    return static_cast<int8_t>(sq1) != static_cast<int8_t>(sq2);
}

inline constexpr bool operator<(Square sq1, Square sq2) {
    return static_cast<int8_t>(sq1) < static_cast<int8_t>(sq2);
}

inline constexpr bool operator>(Square sq1, Square sq2) {
    return static_cast<int8_t>(sq1) > static_cast<int8_t>(sq2);
}

inline constexpr bool operator<=(Square sq1, Square sq2) {
    return static_cast<int8_t>(sq1) <= static_cast<int8_t>(sq2);
}

inline constexpr bool operator>=(Square sq1, Square sq2) {
    return static_cast<int8_t>(sq1) >= static_cast<int8_t>(sq2);
}

// ---------------------------------------------------------------------------
// Bitboard  (struct-wrapped uint64_t with constexpr bitwise operators)
// ---------------------------------------------------------------------------
struct Bitboard {
    uint64_t bb;

    constexpr Bitboard() noexcept : bb(0) {}
    constexpr Bitboard(uint64_t v) noexcept : bb(v) {}

    // Implicit conversion to bool (truthiness: non-zero = true)
    constexpr explicit operator bool() const noexcept {
        return bb != 0;
    }
};

inline constexpr Bitboard operator&(Bitboard a, Bitboard b) noexcept {
    return a.bb & b.bb;
}

inline constexpr Bitboard operator|(Bitboard a, Bitboard b) noexcept {
    return a.bb | b.bb;
}

inline constexpr Bitboard operator^(Bitboard a, Bitboard b) noexcept {
    return a.bb ^ b.bb;
}

inline constexpr Bitboard operator~(Bitboard bb) noexcept {
    return ~bb.bb;
}

inline constexpr Bitboard operator<<(Bitboard bb, int shift) noexcept {
    return bb.bb << shift;
}

inline constexpr Bitboard operator>>(Bitboard bb, int shift) noexcept {
    return bb.bb >> shift;
}

inline constexpr Bitboard& operator&=(Bitboard& a, Bitboard b) noexcept {
    a.bb &= b.bb;
    return a;
}

inline constexpr Bitboard& operator|=(Bitboard& a, Bitboard b) noexcept {
    a.bb |= b.bb;
    return a;
}

inline constexpr Bitboard& operator^=(Bitboard& a, Bitboard b) noexcept {
    a.bb ^= b.bb;
    return a;
}

inline constexpr bool operator==(Bitboard a, Bitboard b) noexcept {
    return a.bb == b.bb;
}

inline constexpr bool operator!=(Bitboard a, Bitboard b) noexcept {
    return a.bb != b.bb;
}

// ---------------------------------------------------------------------------
// Value  (centipawn-scale, with mate encoding)
// ---------------------------------------------------------------------------
using Value = int;

inline constexpr Value VALUE_ZERO       = 0;
inline constexpr Value VALUE_DRAW       = 0;
inline constexpr Value VALUE_INFINITE   = 32000;
inline constexpr Value VALUE_NONE       = 32001;
inline constexpr Value VALUE_MATE       = 32000;
inline constexpr Value VALUE_MATE_IN_1  = VALUE_MATE - 1;
inline constexpr Value VALUE_MATE_IN_2  = VALUE_MATE - 3;

// Maximum possible evaluation (non-mate), used to clamp raw eval
inline constexpr Value VALUE_EVAL_MAX = 20000;

inline constexpr int value_abs(int v) {
    return v < 0 ? -v : v;
}

inline constexpr bool is_mate(Value v) {
    return value_abs(v) >= VALUE_MATE_IN_2;
}

inline constexpr bool is_valid(Value v) {
    return value_abs(v) <= VALUE_MATE;
}

inline constexpr int mate_distance(Value v) {
    return (VALUE_MATE + 1 - value_abs(v)) / 2;
}

// ---------------------------------------------------------------------------
// Move  (16-bit encoded move; encoding logic in Phase 3)
// ---------------------------------------------------------------------------
using Move = uint16_t;

inline constexpr Move MOVE_NONE = 0;
inline constexpr Move MOVE_NULL = 1;

// ---------------------------------------------------------------------------
// Depth  (plies, with special sentinels)
// ---------------------------------------------------------------------------
using Depth = int;

inline constexpr Depth DEPTH_ZERO    = 0;
inline constexpr Depth DEPTH_QS_CHECKS = -6;
inline constexpr Depth DEPTH_NONE    = -7;
inline constexpr Depth DEPTH_MAX     = 128;

// ---------------------------------------------------------------------------
// Platform safety assertions
// ---------------------------------------------------------------------------
static_assert(sizeof(Bitboard) == 8, "Bitboard must be exactly 64 bits");
static_assert(sizeof(Move) == 2, "Move must be exactly 16 bits");
static_assert(sizeof(Color) == 1, "Color must be exactly 1 byte");
static_assert(sizeof(PieceType) == 1, "PieceType must be exactly 1 byte");
static_assert(sizeof(Piece) == 1, "Piece must be exactly 1 byte");
static_assert(sizeof(Square) == 1, "Square must be exactly 1 byte");
static_assert(sizeof(Value) == 4, "Value must be exactly 32 bits");
