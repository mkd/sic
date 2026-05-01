#pragma once

#include "types.h"

// ---------------------------------------------------------------------------
//  Piece & Color Operations
//  Note: piece_type(), color_of(), make_piece(), and operator~(Color)
//  are already defined in types.h.  This header provides the remaining
//  operational helpers and string conversions.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
//  Color Predicates
// ---------------------------------------------------------------------------
inline constexpr bool is_white_piece(Piece p) {
    return color_of(p) == Color::WHITE;
}

inline constexpr bool is_black_piece(Piece p) {
    return color_of(p) == Color::BLACK;
}

// ---------------------------------------------------------------------------
//  PieceType Predicates
// ---------------------------------------------------------------------------
inline constexpr bool type_is_slider(PieceType pt) {
    return pt == PieceType::BISHOP || pt == PieceType::ROOK || pt == PieceType::QUEEN;
}

inline constexpr bool type_is_non_slider(PieceType pt) {
    return pt == PieceType::PAWN || pt == PieceType::KNIGHT || pt == PieceType::KING;
}

// ---------------------------------------------------------------------------
//  Piece-to-Char  (UCI / FEN output)
// ---------------------------------------------------------------------------
inline constexpr char piece_to_char(Piece p) {
    if (p == Piece::PIECE_NONE) return ' ';

    constexpr char Map[12] = {
        'P', 'N', 'B', 'R', 'Q', 'K',
        'p', 'n', 'b', 'r', 'q', 'k',
    };
    return Map[static_cast<int>(p)];
}

// ---------------------------------------------------------------------------
//  Char-to-Piece  (FEN / UCI input)
// ---------------------------------------------------------------------------
inline constexpr Piece char_to_piece(char c) {
    switch (c) {
        case 'P': return Piece::WHITE_PAWN;
        case 'N': return Piece::WHITE_KNIGHT;
        case 'B': return Piece::WHITE_BISHOP;
        case 'R': return Piece::WHITE_ROOK;
        case 'Q': return Piece::WHITE_QUEEN;
        case 'K': return Piece::WHITE_KING;
        case 'p': return Piece::BLACK_PAWN;
        case 'n': return Piece::BLACK_KNIGHT;
        case 'b': return Piece::BLACK_BISHOP;
        case 'r': return Piece::BLACK_ROOK;
        case 'q': return Piece::BLACK_QUEEN;
        case 'k': return Piece::BLACK_KING;
        default:  return Piece::PIECE_NONE;
    }
}

// ---------------------------------------------------------------------------
//  Color String Helpers
// ---------------------------------------------------------------------------
inline constexpr const char* color_to_str(Color c) {
    return (c == Color::WHITE) ? "w" : "b";
}

// ---------------------------------------------------------------------------
//  PieceType String Helpers
// ---------------------------------------------------------------------------
inline constexpr char type_to_char(PieceType pt) {
    constexpr char Map[7] = { ' ', 'P', 'N', 'B', 'R', 'Q', 'K' };
    return Map[static_cast<int>(pt)];
}
