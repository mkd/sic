#include "../include/position.h"
#include "../include/utils.h"
#include "../include/attacks.h"
#include "../include/move.h"
#include <cstdlib>
#include <random>
#include <sstream>
#include <cassert>
#include <cstring>
#include <iostream>
#include <iomanip>

// ---------------------------------------------------------------------------
//  Sic-to-NNUE piece index mapping for HalfKP accumulator updates
// ---------------------------------------------------------------------------
constexpr int SicToNnuePiece[13] = {
    6,  // 0: WHITE_PAWN   -> wpawn
    5,  // 1: WHITE_KNIGHT -> wknight
    4,  // 2: WHITE_BISHOP -> wbishop
    3,  // 3: WHITE_ROOK   -> wrook
    2,  // 4: WHITE_QUEEN  -> wqueen
    1,  // 5: WHITE_KING   -> wking
    12, // 6: BLACK_PAWN   -> bpawn
    11, // 7: BLACK_KNIGHT -> bknight
    10, // 8: BLACK_BISHOP -> bbishop
    9,  // 9: BLACK_ROOK   -> brook
    8,  // 10: BLACK_QUEEN  -> bqueen
    7,  // 11: BLACK_KING   -> bking
    0   // 12: PIECE_NONE   -> blank
};

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
//  Slider Blockers / Pinners
// ---------------------------------------------------------------------------
Bitboard Position::slider_blockers(Bitboard sliders, Square s, Bitboard& pinners) const {
    Bitboard blockers = {0};
    pinners = {0};

    const Bitboard occ = occupied();

    // --- Rook-type sliders (Rooks + Queens) ---
    {
        const Bitboard rookSliders = sliders & (pieces(PieceType::ROOK) | pieces(PieceType::QUEEN));
        const Bitboard rookLines = get_rook_attacks(s, {0});
        Bitboard rAttackers = rookSliders & rookLines;

        while (rAttackers.bb) {
            const Square sliderSq = pop_lsb(rAttackers);
            const Bitboard ray = get_rook_attacks(s, occ) & get_rook_attacks(sliderSq, occ);
            const Bitboard between = ray & occ;

            if (between.bb && !(between.bb & (between.bb - 1))) {
                blockers |= between;
                pinners.bb |= (1ULL << static_cast<int>(sliderSq));
            }
        }
    }

    // --- Bishop-type sliders (Bishops + Queens) ---
    {
        const Bitboard bishopSliders = sliders & (pieces(PieceType::BISHOP) | pieces(PieceType::QUEEN));
        const Bitboard bishopLines = get_bishop_attacks(s, {0});
        Bitboard bAttackers = bishopSliders & bishopLines;

        while (bAttackers.bb) {
            const Square sliderSq = pop_lsb(bAttackers);
            const Bitboard ray = get_bishop_attacks(s, occ) & get_bishop_attacks(sliderSq, occ);
            const Bitboard between = ray & occ;

            if (between.bb && !(between.bb & (between.bb - 1))) {
                blockers |= between;
                pinners.bb |= (1ULL << static_cast<int>(sliderSq));
            }
        }
    }

    return blockers;
}

// ---------------------------------------------------------------------------
//  Set Check Info
// ---------------------------------------------------------------------------
void Position::set_check_info() {
    // --- Populate blockers and pinners for each side ---
    blockersForKing[static_cast<int>(Color::WHITE)] = slider_blockers(
        pieces(Color::BLACK), get_king_square(Color::WHITE), pinners[static_cast<int>(Color::WHITE)]);

    blockersForKing[static_cast<int>(Color::BLACK)] = slider_blockers(
        pieces(Color::WHITE), get_king_square(Color::BLACK), pinners[static_cast<int>(Color::BLACK)]);

    // --- Compute checkers for the side to move ---
    const Color us = sideToMove;
    const Color them = ~us;
    const Square kingSq = get_king_square(us);
    const Bitboard occ = occupied();

    Bitboard checkersAttackers = {0};

    // Knight checkers
    {
        const Bitboard knights = pieces(them) & pieces(PieceType::KNIGHT);
        if (knights.bb) {
            checkersAttackers |= (KNIGHT_ATTACKS[static_cast<int>(kingSq)] & knights);
        }
    }

    // Pawn checkers
    {
        const Bitboard pawns = pieces(them) & pieces(PieceType::PAWN);
        if (pawns.bb) {
            checkersAttackers |= (PAWN_ATTACKS[static_cast<int>(us)][static_cast<int>(kingSq)] & pawns);
        }
    }

    // King checkers
    {
        const Bitboard kings = pieces(them) & pieces(PieceType::KING);
        if (kings.bb) {
            checkersAttackers |= (KING_ATTACKS[static_cast<int>(kingSq)] & kings);
        }
    }

    // Bishop/Queen checkers (diagonal sliders that have a clear line)
    {
        const Bitboard diagSliders = pieces(them) & (pieces(PieceType::BISHOP) | pieces(PieceType::QUEEN));
        if (diagSliders.bb) {
            checkersAttackers |= (get_bishop_attacks(kingSq, occ) & diagSliders);
        }
    }

    // Rook/Queen checkers (orthogonal sliders that have a clear line)
    {
        const Bitboard orthSliders = pieces(them) & (pieces(PieceType::ROOK) | pieces(PieceType::QUEEN));
        if (orthSliders.bb) {
            checkersAttackers |= (get_rook_attacks(kingSq, occ) & orthSliders);
        }
    }

    checkers = checkersAttackers;
}

// ---------------------------------------------------------------------------
//  Make Move (with legality check)
// ---------------------------------------------------------------------------
bool Position::make_move(Move m) {
    const Square from = move_from(m);
    const Square to = move_to(m);
    int flag = move_flag(m);
    const PieceType prom = move_prom(m);

    const int from_int = static_cast<int>(from);
    const int to_int = static_cast<int>(to);

    const Piece moving_piece = board[from_int];
    const Piece captured_piece = board[to_int];
    const Color us = sideToMove;
    const Color them = ~us;

    // --- Auto-detect missing flags from naive UCI parsers ---
    if (piece_type(moving_piece) == PieceType::KING && std::abs(from_int - to_int) == 2) {
        flag = MOVE_FLAG_CASTLING;
    } else if (piece_type(moving_piece) == PieceType::PAWN &&
               captured_piece == Piece::PIECE_NONE &&
               (from_int % 8 != to_int % 8)) {
        flag = MOVE_FLAG_ENPASSANT;
    }

    // --- NNUE: mark stale on complex moves ---
    if (piece_type(moving_piece) == PieceType::KING ||
        flag == MOVE_FLAG_CASTLING ||
        flag == MOVE_FLAG_ENPASSANT ||
        prom != PieceType::NONE) {
        nnueStale = true;
    } else {
        nnueStale = false;
    }

    // --- NNUE: track dirty pieces for incremental update ---
    // nnue-probe piece codes: wking=1..wpawn=6, bking=7..bpawn=12
    // Sic piece codes: 0..11. Conversion: nnue_code = sic_code + 1
    int dirtyNum = 0;
    if (!nnueStale) {
        // Removing piece from 'from'
        nnueState.dirtyPiece.pc[dirtyNum] = SicToNnuePiece[static_cast<int>(moving_piece)];
        nnueState.dirtyPiece.from[dirtyNum] = from_int;
        nnueState.dirtyPiece.to[dirtyNum] = 64;
        dirtyNum++;

        // If capture, removing piece from 'to'
        if (captured_piece != Piece::PIECE_NONE) {
            nnueState.dirtyPiece.pc[dirtyNum] = SicToNnuePiece[static_cast<int>(captured_piece)];
            nnueState.dirtyPiece.from[dirtyNum] = to_int;
            nnueState.dirtyPiece.to[dirtyNum] = 64;
            dirtyNum++;
        }

        // Placing piece on 'to'
        Piece placed_piece = (prom != PieceType::NONE) ? make_piece(us, prom) : moving_piece;
        nnueState.dirtyPiece.pc[dirtyNum] = SicToNnuePiece[static_cast<int>(placed_piece)];
        nnueState.dirtyPiece.from[dirtyNum] = 64;
        nnueState.dirtyPiece.to[dirtyNum] = to_int;
        dirtyNum++;
    }

    // --- Update Zobrist: remove moving piece ---
    zobristKey ^= ZobristPiece[static_cast<int>(moving_piece)][from_int];

    // --- Move the piece on the board ---
    board[from_int] = Piece::PIECE_NONE;
    byTypeBB[static_cast<int>(moving_piece) % 6 + 1].bb &= ~(1ULL << from_int);
    byColorBB[static_cast<int>(us)].bb &= ~(1ULL << from_int);

    // --- Handle capture ---
    if (captured_piece != Piece::PIECE_NONE) {
        zobristKey ^= ZobristPiece[static_cast<int>(captured_piece)][to_int];
        byTypeBB[static_cast<int>(captured_piece) % 6 + 1].bb &= ~(1ULL << to_int);
        byColorBB[static_cast<int>(them)].bb &= ~(1ULL << to_int);
    }

    // --- Place moving piece on destination ---
    Piece placed_piece = (prom != PieceType::NONE) ? make_piece(us, prom) : moving_piece;

    board[to_int] = placed_piece;
    byTypeBB[static_cast<int>(placed_piece) % 6 + 1].bb |= (1ULL << to_int);
    byColorBB[static_cast<int>(us)].bb |= (1ULL << to_int);
    zobristKey ^= ZobristPiece[static_cast<int>(placed_piece)][to_int];

    // --- En Passant capture ---
    if (flag == MOVE_FLAG_ENPASSANT) {
        const int captured_sq = to_int + (us == Color::WHITE ? -8 : 8);
        const Piece ep_pawn = board[captured_sq];
        zobristKey ^= ZobristPiece[static_cast<int>(ep_pawn)][captured_sq];
        board[captured_sq] = Piece::PIECE_NONE;
        byTypeBB[static_cast<int>(ep_pawn) % 6 + 1].bb &= ~(1ULL << captured_sq);
        byColorBB[static_cast<int>(them)].bb &= ~(1ULL << captured_sq);

        if (!nnueStale) {
            nnueState.dirtyPiece.pc[dirtyNum] = SicToNnuePiece[static_cast<int>(ep_pawn)];
            nnueState.dirtyPiece.from[dirtyNum] = captured_sq;
            nnueState.dirtyPiece.to[dirtyNum] = 64;
            dirtyNum++;
        }
    }

    // --- Castling: move the rook ---
    if (flag == MOVE_FLAG_CASTLING) {
        int rookFrom = -1, rookTo = -1;
        Piece rookPiece = Piece::PIECE_NONE;
        if (to == Square::SQ_G1) { // White kingside: rook H1(7) -> F1(5)
            rookFrom = 7; rookTo = 5; rookPiece = Piece::WHITE_ROOK;
        } else if (to == Square::SQ_C1) { // White queenside: rook A1(0) -> D1(3)
            rookFrom = 0; rookTo = 3; rookPiece = Piece::WHITE_ROOK;
        } else if (to == Square::SQ_G8) { // Black kingside: rook H8(63) -> F8(61)
            rookFrom = 63; rookTo = 61; rookPiece = Piece::BLACK_ROOK;
        } else if (to == Square::SQ_C8) { // Black queenside: rook A8(56) -> D8(59)
            rookFrom = 56; rookTo = 59; rookPiece = Piece::BLACK_ROOK;
        }

        if (rookFrom >= 0) {
            board[rookFrom] = Piece::PIECE_NONE;
            board[rookTo] = rookPiece;
            byTypeBB[4].bb &= ~(1ULL << rookFrom);
            byColorBB[static_cast<int>(us)].bb &= ~(1ULL << rookFrom);
            byTypeBB[4].bb |= (1ULL << rookTo);
            byColorBB[static_cast<int>(us)].bb |= (1ULL << rookTo);
            zobristKey ^= ZobristPiece[static_cast<int>(rookPiece)][rookFrom];
            zobristKey ^= ZobristPiece[static_cast<int>(rookPiece)][rookTo];

            if (!nnueStale) {
                nnueState.dirtyPiece.pc[dirtyNum] = SicToNnuePiece[static_cast<int>(rookPiece)];
                nnueState.dirtyPiece.from[dirtyNum] = rookFrom;
                nnueState.dirtyPiece.to[dirtyNum] = 64;
                dirtyNum++;
                nnueState.dirtyPiece.pc[dirtyNum] = SicToNnuePiece[static_cast<int>(rookPiece)];
                nnueState.dirtyPiece.from[dirtyNum] = 64;
                nnueState.dirtyPiece.to[dirtyNum] = rookTo;
                dirtyNum++;
            }
        }
    }

    // --- NNUE: finalize dirty piece count ---
    nnueState.dirtyPiece.dirtyNum = dirtyNum;

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

    // --- Update check/pin info for the new position ---
    set_check_info();

    // --- Update halfmove clock: reset on capture or pawn push, increment otherwise ---
    if (piece_type(moving_piece) == PieceType::PAWN ||
        captured_piece != Piece::PIECE_NONE ||
        flag == MOVE_FLAG_ENPASSANT) {
        halfmoveClock = 0;
    } else {
        halfmoveClock++;
    }

    // --- Update fullmove number: increment when Black's move completes ---
    if (us == Color::BLACK) {
        fullmoveNumber++;
    }

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
    sideToMove = (sideToMove == Color::BLACK) ? Color::WHITE : Color::BLACK;
    zobristKey ^= ZobristSide;
    nnueStale = true;
    nnueState.dirtyPiece.dirtyNum = 0;
    halfmoveClock++;
}

// ---------------------------------------------------------------------------
//  FEN Parsing
// ---------------------------------------------------------------------------
void Position::set_fen(const std::string& fen) {
    // --- Full state reset ---
    for (int i = 0; i < 7; ++i) byTypeBB[i] = {0};
    for (int i = 0; i < 2; ++i) byColorBB[i] = {0};
    for (int i = 0; i < 64; ++i) board[i] = Piece::PIECE_NONE;

    sideToMove = Color::WHITE;
    castlingRights = CASTLING_NONE;
    epSquare = Square::SQ_NONE;
    halfmoveClock = 0;
    fullmoveNumber = 1;
    zobristKey = 0;

    // --- Reset NNUE state ---
    std::memset(&nnueState, 0, sizeof(NNUEState));
    nnueState.accumulator.computedAccumulation = 0;
    nnueStatePlyMinus1 = nullptr;
    nnueStatePlyMinus2 = nullptr;
    nnueStale = true;

    std::istringstream iss(fen);
    std::string token;

    // --- Field 1: Piece placement ---
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
            if (p != Piece::PIECE_NONE && sq >= 0 && sq < 64) {
                board[sq] = p;
                const int pt_idx = static_cast<int>(p) % 6 + 1;
                byTypeBB[pt_idx].bb |= (1ULL << sq);
                byColorBB[static_cast<int>(color_of(p))].bb |= (1ULL << sq);
                zobristKey ^= ZobristPiece[static_cast<int>(p)][sq];
            }
            ++sq;
        }
    }

    // --- Field 2: Active color ---
    iss >> token;
    sideToMove = (token == "w") ? Color::WHITE : Color::BLACK;
    if (sideToMove == Color::BLACK) {
        zobristKey ^= ZobristSide;
    }

    // --- Field 3: Castling availability ---
    iss >> token;
    if (token != "-") {
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

    // --- Field 4: En passant target square ---
    iss >> token;
    if (token != "-" && token.size() == 2) {
        int file = token[0] - 'a';
        int rank = token[1] - '1';
        if (file >= 0 && file < 8 && rank >= 0 && rank < 8) {
            epSquare = static_cast<Square>(rank * 8 + file);
            zobristKey ^= ZobristEpFile[file];
        }
    }

    // --- Field 5: Halfmove clock ---
    iss >> token;
    if (!iss.fail()) {
        try {
            halfmoveClock = std::stoi(token);
        } catch (...) {
            halfmoveClock = 0;
        }
    }

    // --- Field 6: Fullmove number ---
    iss >> token;
    if (!iss.fail()) {
        try {
            fullmoveNumber = std::stoi(token);
        } catch (...) {
            fullmoveNumber = 1;
        }
    }

    nnueStale = true;
    set_check_info();
}

// ---------------------------------------------------------------------------
//  FEN Generation
// ---------------------------------------------------------------------------
std::string Position::get_fen() const {
    std::ostringstream oss;

    // --- Piece placement (ranks 8 to 1) ---
    for (int r = 7; r >= 0; --r) {
        int empty = 0;
        for (int f = 0; f < 8; ++f) {
            int sq = r * 8 + f;
            Piece p = board[sq];
            if (p == Piece::PIECE_NONE) {
                ++empty;
            } else {
                if (empty > 0) {
                    oss << empty;
                    empty = 0;
                }
                oss << piece_to_char(p);
            }
        }
        if (empty > 0) oss << empty;
        if (r > 0) oss << '/';
    }

    // --- Active color ---
    oss << (sideToMove == Color::WHITE ? " w" : " b");

    // --- Castling rights ---
    oss << ' ';
    if (castlingRights == CASTLING_NONE) {
        oss << '-';
    } else {
        if (castlingRights & WHITE_OO)  oss << 'K';
        if (castlingRights & WHITE_OOO) oss << 'Q';
        if (castlingRights & BLACK_OO)  oss << 'k';
        if (castlingRights & BLACK_OOO) oss << 'q';
    }

    // --- En passant square ---
    oss << ' ';
    if (epSquare == Square::SQ_NONE) {
        oss << '-';
    } else {
        int sq_int = static_cast<int>(epSquare);
        oss << static_cast<char>('a' + (sq_int % 8))
            << static_cast<char>('1' + (sq_int / 8));
    }

    // --- Halfmove clock & fullmove number ---
    oss << ' ' << halfmoveClock << ' ' << fullmoveNumber;

    return oss.str();
}

// ---------------------------------------------------------------------------
//  Print Board (diagnostic command "d")
// ---------------------------------------------------------------------------
void Position::print() const {
    const char piece_chars[] = "PNBRQKpnbrqk";
    std::cout << "\n +---+---+---+---+---+---+---+---+\n";
    for (int r = 7; r >= 0; --r) {
        std::cout << (r + 1) << " |";
        for (int f = 0; f < 8; ++f) {
            Square sq = static_cast<Square>(r * 8 + f);
            Piece p = piece_on(sq);
            char c = (p == Piece::PIECE_NONE) ? ' ' : piece_chars[static_cast<int>(p)];
            std::cout << " " << c << " |";
        }
        std::cout << "\n +---+---+---+---+---+---+---+---+\n";
    }
    std::cout << "   a   b   c   d   e   f   g   h\n\n";
    std::cout << "Key: " << std::hex << zobristKey << std::dec << "\n";
    std::cout << "Checkers: " << checkers.bb << "\n";
    std::cout << std::flush;
}

// ---------------------------------------------------------------------------
//  Insufficient Material Detection
// ---------------------------------------------------------------------------
bool Position::is_insufficient_material() const {
    Bitboard occ = occupied();
    int occCount = popcount(occ);

    // KvK
    if (occCount == 2) return true;

    // KvKN or KvKB
    if (occCount == 3) {
        Bitboard nonKings = {occ.bb & ~pieces(PieceType::KING).bb};
        if (nonKings.bb) {
            Square sq = lsb(nonKings);
            PieceType pt = piece_type(board[static_cast<int>(sq)]);
            if (pt == PieceType::KNIGHT || pt == PieceType::BISHOP) return true;
        }
    }

    // KvKB opposite-colored bishops
    if (occCount == 4) {
        Bitboard wPieces = pieces(Color::WHITE);
        Bitboard bPieces = pieces(Color::BLACK);
        Bitboard bishops = pieces(PieceType::BISHOP);
        Bitboard kings = pieces(PieceType::KING);

        if (popcount(wPieces) == 2 && popcount(bPieces) == 2 &&
            popcount(bishops) == 2 && popcount(kings) == 2) {
            Square wb = lsb({wPieces.bb & bishops.bb});
            Square bb = lsb({bPieces.bb & bishops.bb});
            int wColor = (static_cast<int>(wb) / 8 + static_cast<int>(wb) % 8) & 1;
            int bColor = (static_cast<int>(bb) / 8 + static_cast<int>(bb) % 8) & 1;
            if (wColor != bColor) return true;
        }
    }

    return false;
}
