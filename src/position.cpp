#include "position.h"
#include "utils.h"
#include <random>
#include <sstream>
#include <cassert>

// ---------------------------------------------------------------------------
//  Zobrist Hashing Tables
// ---------------------------------------------------------------------------
uint64_t ZobristPiece[12][64];
uint64_t ZobristCastling[16];
uint64_t ZobristEpFile[8];
uint64_t ZobristSide;

void init_zobrist() {
    std::mt19937_64 rng(1337);

    for (int p = 0; p < 12; ++p) {
        for (int sq = 0; sq < 64; ++sq) {
            ZobristPiece[p][sq] = rng();
        }
    }

    for (int i = 0; i < 16; ++i) {
        ZobristCastling[i] = rng();
    }

    for (int f = 0; f < 8; ++f) {
        ZobristEpFile[f] = rng();
    }

    ZobristSide = rng();
}

// ---------------------------------------------------------------------------
//  FEN Parsing
// ---------------------------------------------------------------------------
void Position::set_fen(const std::string& fen) {
    // Clear all state
    for (int i = 0; i < 7; ++i) byTypeBB[i] = {0};
    for (int i = 0; i < 2; ++i) byColorBB[i] = {0};
    for (int i = 0; i < 64; ++i) board[i] = Piece::PIECE_NONE;

    sideToMove = Color::WHITE;
    castlingRights = CASTLING_NONE;
    epSquare = Square::SQ_NONE;
    halfmoveClock = 0;
    fullmoveNumber = 1;
    zobristKey = 0;

    std::istringstream iss(fen);

    // --- Piece placement ---
    std::string token;
    iss >> token;

    int sq = 56;
    for (char c : token) {
        if (c == '/') {
            sq -= 16;
            continue;
        } else if (c >= '1' && c <= '8') {
            sq += (c - '0');
        } else {
            Piece p = char_to_piece(c);
            if (p != Piece::PIECE_NONE) {
                board[sq] = p;
                const int pt_idx = static_cast<int>(p) % 6 + 1;
                byTypeBB[pt_idx].bb |= (1ULL << sq);
                byColorBB[static_cast<int>(color_of(p))].bb |= (1ULL << sq);
                zobristKey ^= ZobristPiece[static_cast<int>(p)][sq];
            }
            ++sq;
        }
    }

    // --- Side to move ---
    iss >> token;
    sideToMove = (token == "w") ? Color::WHITE : Color::BLACK;
    if (sideToMove == Color::BLACK) {
        zobristKey ^= ZobristSide;
    }

    // --- Castling rights ---
    iss >> token;
    if (token == "-") {
        castlingRights = CASTLING_NONE;
    } else {
        for (char c : token) {
            switch (c) {
                case 'K': castlingRights |= WHITE_OO;  break;
                case 'Q': castlingRights |= WHITE_OOO; break;
                case 'k': castlingRights |= BLACK_OO;  break;
                case 'q': castlingRights |= BLACK_OOO; break;
                default: break;
            }
        }
    }
    zobristKey ^= ZobristCastling[castlingRights];

    // --- En passant square ---
    iss >> token;
    if (token != "-") {
        int file = token[0] - 'a';
        int rank = token[1] - '1';
        epSquare = static_cast<Square>(rank * 8 + file);
        zobristKey ^= ZobristEpFile[file];
    }

    // --- Halfmove clock ---
    iss >> token;
    halfmoveClock = std::stoi(token);

    // --- Fullmove number ---
    iss >> token;
    fullmoveNumber = std::stoi(token);
}
