#pragma once

#include "types.h"

// ---------------------------------------------------------------------------
//  File & Rank
// ---------------------------------------------------------------------------
enum class File : int {
    FILE_A = 0, FILE_B = 1, FILE_C = 2, FILE_D = 3,
    FILE_E = 4, FILE_F = 5, FILE_G = 6, FILE_H = 7,
    FILE_NONE = 8,
};

enum class Rank : int {
    RANK_1 = 0, RANK_2 = 1, RANK_3 = 2, RANK_4 = 3,
    RANK_5 = 4, RANK_6 = 5, RANK_7 = 6, RANK_8 = 7,
    RANK_NONE = 8,
};

// ---------------------------------------------------------------------------
//  Square Decomposition
// ---------------------------------------------------------------------------
inline constexpr File file_of(Square sq) {
    return static_cast<File>(static_cast<int>(sq) % 8);
}

inline constexpr Rank rank_of(Square sq) {
    return static_cast<Rank>(static_cast<int>(sq) / 8);
}

inline constexpr Square make_square(File f, Rank r) {
    return static_cast<Square>(static_cast<int>(r) * 8 + static_cast<int>(f));
}

// ---------------------------------------------------------------------------
//  Square Transposition
// ---------------------------------------------------------------------------
inline constexpr Square flip_vertical(Square sq) {
    return static_cast<Square>(static_cast<int>(sq) ^ 56);
}

inline constexpr Square relative_square(Color c, Square sq) {
    return static_cast<Square>(static_cast<int>(sq) ^ (static_cast<int>(c) * 56));
}

// ---------------------------------------------------------------------------
//  Distance & Relationships
// ---------------------------------------------------------------------------
inline constexpr int sq_distance(Square sq1, Square sq2) {
    const int f1 = static_cast<int>(sq1) % 8;
    const int r1 = static_cast<int>(sq1) / 8;
    const int f2 = static_cast<int>(sq2) % 8;
    const int r2 = static_cast<int>(sq2) / 8;
    const int df = (f1 > f2) ? (f1 - f2) : (f2 - f1);
    const int dr = (r1 > r2) ? (r1 - r2) : (r2 - r1);
    return (df > dr) ? df : dr;
}

inline constexpr bool on_same_diagonal(Square sq1, Square sq2) {
    const int f1 = static_cast<int>(sq1) % 8, r1 = static_cast<int>(sq1) / 8;
    const int f2 = static_cast<int>(sq2) % 8, r2 = static_cast<int>(sq2) / 8;
    return (f1 - r1) == (f2 - r2);
}

inline constexpr bool on_same_anti_diagonal(Square sq1, Square sq2) {
    const int f1 = static_cast<int>(sq1) % 8, r1 = static_cast<int>(sq1) / 8;
    const int f2 = static_cast<int>(sq2) % 8, r2 = static_cast<int>(sq2) / 8;
    return (f1 + r1) == (f2 + r2);
}

inline constexpr bool same_color(Square sq1, Square sq2) {
    return ((static_cast<int>(sq1) ^ static_cast<int>(sq2)) & 1) == 0;
}

// ---------------------------------------------------------------------------
//  Square Validation
// ---------------------------------------------------------------------------
inline constexpr bool is_valid_square(Square sq) {
    return static_cast<int>(sq) >= 0 && static_cast<int>(sq) < 64;
}
