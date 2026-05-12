#include "../include/position.h"
#include "../include/utils.h"
#include "../include/attacks.h"
#include "../include/move.h"
#include "../include/nnue.h"
#include <cstdlib>
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
//  Castling Rights Mask — removes rights when king/rook moves or is captured
//    A1=13(1101), H1=14(1110), E1=12(1100)
//    A8=7(0111),  H8=11(1011), E8=3(0011)
// ---------------------------------------------------------------------------
constexpr int CASTLING_RIGHTS_MASK[64] = {
    13, 15, 15, 15, 12, 15, 15, 14,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    7,  15, 15, 15, 3,  15, 15, 11
};

// ---------------------------------------------------------------------------
//  Attack Detection
// ---------------------------------------------------------------------------
bool Position::is_attacked(Square sq, Color attacker) const {
    const Bitboard attackerBB = pieces(attacker);
    const Bitboard occ = occupied();
    const int sq_int = static_cast<int>(sq);

    // Pawn attacks (vectorized: PAWN_ATTACKS[~attacker][sq] gives squares where
    // attacker's pawns would be if they attack sq)
    {
        const Bitboard pawns = attackerBB & pieces(PieceType::PAWN);
        if (pawns.bb && (PAWN_ATTACKS[static_cast<int>(~attacker)][sq_int].bb & pawns.bb))
            return true;
    }

    // Knight attacks
    {
        const Bitboard knights = attackerBB & pieces(PieceType::KNIGHT);
        if (knights.bb && (KNIGHT_ATTACKS[sq_int] & knights.bb))
            return true;
    }

    // King attacks
    {
        const Bitboard kings = attackerBB & pieces(PieceType::KING);
        if (kings.bb && (KING_ATTACKS[sq_int] & kings.bb))
            return true;
    }

    // Bishop/Queen diagonal attacks
    {
        const Bitboard diagonals = attackerBB & (pieces(PieceType::BISHOP) | pieces(PieceType::QUEEN));
        if (diagonals.bb && (get_bishop_attacks(sq, occ) & diagonals.bb))
            return true;
    }

    // Rook/Queen orthogonal attacks
    {
        const Bitboard orthogonals = attackerBB & (pieces(PieceType::ROOK) | pieces(PieceType::QUEEN));
        if (orthogonals.bb && (get_rook_attacks(sq, occ) & orthogonals.bb))
            return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
//  Make Move (with legality check)
// ---------------------------------------------------------------------------
bool Position::make_move(Move m) {
    const Square from = move_from(m);
    const Square to = move_to(m);
    const int flag = move_flag(m);
    const PieceType prom = move_prom(m);

    const int from_int = static_cast<int>(from);
    const int to_int = static_cast<int>(to);

    const Piece moving_piece = board[from_int];
    const Piece captured_piece = board[to_int];
    const Color us = sideToMove;
    const Color them = ~us;

    bool was_stale = accumulator_stale;

    if (piece_type(moving_piece) == PieceType::KING) {
        accumulator_stale = true;
    }

    // --- Update Zobrist: remove moving piece ---
    zobristKey ^= ZobristPiece[static_cast<int>(moving_piece)][from_int];

    // --- Move the piece on the board ---
    board[from_int] = Piece::PIECE_NONE;
    byTypeBB[static_cast<int>(moving_piece) % 6 + 1].bb &= ~(1ULL << from_int);
    byColorBB[static_cast<int>(us)].bb &= ~(1ULL << from_int);

    if (!accumulator_stale && !was_stale) {
        update_accumulator_piece(*this, moving_piece, from, false);
    }

    // --- Handle capture ---
    if (captured_piece != Piece::PIECE_NONE) {
        zobristKey ^= ZobristPiece[static_cast<int>(captured_piece)][to_int];
        byTypeBB[static_cast<int>(captured_piece) % 6 + 1].bb &= ~(1ULL << to_int);
        byColorBB[static_cast<int>(them)].bb &= ~(1ULL << to_int);

        if (!accumulator_stale && !was_stale) {
            update_accumulator_piece(*this, captured_piece, to, false);
        }
    }

    // --- Place moving piece on destination ---
    Piece placed_piece = moving_piece;

    // Promotion
    if (prom != PieceType::NONE) {
        placed_piece = make_piece(us, prom);
    }

    board[to_int] = placed_piece;
    byTypeBB[static_cast<int>(placed_piece) % 6 + 1].bb |= (1ULL << to_int);
    byColorBB[static_cast<int>(us)].bb |= (1ULL << to_int);
    zobristKey ^= ZobristPiece[static_cast<int>(placed_piece)][to_int];

    if (!accumulator_stale && !was_stale) {
        update_accumulator_piece(*this, placed_piece, to, true);
    }

    // --- En Passant capture ---
    if (flag == MOVE_FLAG_ENPASSANT) {
        const int captured_sq = to_int + (us == Color::WHITE ? -8 : 8);
        const Piece ep_pawn = board[captured_sq];
        zobristKey ^= ZobristPiece[static_cast<int>(ep_pawn)][captured_sq];
        board[captured_sq] = Piece::PIECE_NONE;
        byTypeBB[static_cast<int>(ep_pawn) % 6 + 1].bb &= ~(1ULL << captured_sq);
        byColorBB[static_cast<int>(them)].bb &= ~(1ULL << captured_sq);

        if (!accumulator_stale && !was_stale) {
            update_accumulator_piece(*this, ep_pawn, static_cast<Square>(captured_sq), false);
        }
    }

    // --- Castling: move the rook ---
    if (flag == MOVE_FLAG_CASTLING) {
        if (to == Square::SQ_G1) { // White kingside
            board[7] = Piece::WHITE_ROOK;
            board[5] = Piece::PIECE_NONE;
            byTypeBB[4].bb &= ~(1ULL << 7);
            byColorBB[0].bb &= ~(1ULL << 7);
            byTypeBB[4].bb |= (1ULL << 5);
            byColorBB[0].bb |= (1ULL << 5);
            zobristKey ^= ZobristPiece[3][7];
            zobristKey ^= ZobristPiece[3][5];
        } else if (to == Square::SQ_C1) { // White queenside
            board[0] = Piece::WHITE_ROOK;
            board[3] = Piece::PIECE_NONE;
            byTypeBB[4].bb &= ~(1ULL << 0);
            byColorBB[0].bb &= ~(1ULL << 0);
            byTypeBB[4].bb |= (1ULL << 3);
            byColorBB[0].bb |= (1ULL << 3);
            zobristKey ^= ZobristPiece[3][0];
            zobristKey ^= ZobristPiece[3][3];
        } else if (to == Square::SQ_G8) { // Black kingside
            board[63] = Piece::BLACK_ROOK;
            board[61] = Piece::PIECE_NONE;
            byTypeBB[4].bb &= ~(1ULL << 63);
            byColorBB[1].bb &= ~(1ULL << 63);
            byTypeBB[4].bb |= (1ULL << 61);
            byColorBB[1].bb |= (1ULL << 61);
            zobristKey ^= ZobristPiece[9][63];
            zobristKey ^= ZobristPiece[9][61];
        } else if (to == Square::SQ_C8) { // Black queenside
            board[56] = Piece::BLACK_ROOK;
            board[59] = Piece::PIECE_NONE;
            byTypeBB[4].bb &= ~(1ULL << 56);
            byColorBB[1].bb &= ~(1ULL << 56);
            byTypeBB[4].bb |= (1ULL << 59);
            byColorBB[1].bb |= (1ULL << 59);
            zobristKey ^= ZobristPiece[9][56];
            zobristKey ^= ZobristPiece[9][59];
        }
    }

    // --- Update castling rights ---
    zobristKey ^= ZobristCastling[castlingRights];
    castlingRights &= CASTLING_RIGHTS_MASK[from_int];
    castlingRights &= CASTLING_RIGHTS_MASK[to_int];
    zobristKey ^= ZobristCastling[castlingRights];

    // --- Update en passant square ---
    if (epSquare != Square::SQ_NONE) {
        zobristKey ^= ZobristEpFile[static_cast<int>(epSquare) % 8];
    }
    epSquare = Square::SQ_NONE;

    // Double pawn push
    if (piece_type(moving_piece) == PieceType::PAWN &&
        std::abs(static_cast<int>(to) - static_cast<int>(from)) == 16) {
        epSquare = static_cast<Square>((static_cast<int>(from) + static_cast<int>(to)) / 2);
        zobristKey ^= ZobristEpFile[static_cast<int>(epSquare) % 8];
    }

    // --- Swap side to move ---
    zobristKey ^= ZobristSide;
    sideToMove = them;

    // --- Legality check: is the king of the side that just moved in check? ---
    const Square our_king = get_king_square(us);
    if (is_attacked(our_king, sideToMove)) {
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
//  Null Move (for NMP)
// ---------------------------------------------------------------------------
void Position::make_null_move() {
    if (epSquare != Square::SQ_NONE) {
        zobristKey ^= ZobristEpFile[static_cast<int>(epSquare) % 8];
        epSquare = Square::SQ_NONE;
    }
    sideToMove = (sideToMove == Color::WHITE) ? Color::BLACK : Color::WHITE;
    zobristKey ^= ZobristSide;
}

// ---------------------------------------------------------------------------
//  FEN Parsing
// ---------------------------------------------------------------------------
void Position::set_fen(const std::string& fen) {
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

    iss >> token;
    sideToMove = (token == "w") ? Color::WHITE : Color::BLACK;
    if (sideToMove == Color::BLACK) {
        zobristKey ^= ZobristSide;
    }

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

    iss >> token;
    if (token != "-") {
        int file = token[0] - 'a';
        int rank = token[1] - '1';
        epSquare = static_cast<Square>(rank * 8 + file);
        zobristKey ^= ZobristEpFile[file];
    }

    iss >> token;
    halfmoveClock = std::stoi(token);

    iss >> token;
    fullmoveNumber = std::stoi(token);

    accumulator_stale = true;
}
