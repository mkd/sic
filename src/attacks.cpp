#include "attacks.h"
#include "utils.h"

// ---------------------------------------------------------------------------
//  Attack Table Storage
// ---------------------------------------------------------------------------
Bitboard PAWN_ATTACKS[2][64];
Bitboard KNIGHT_ATTACKS[64];
Bitboard KING_ATTACKS[64];

// ---------------------------------------------------------------------------
//  File Masks (prevent wrap-around on left/right shifts)
// ---------------------------------------------------------------------------
constexpr Bitboard FILE_A_MASK = { 0x0101010101010101ULL };
constexpr Bitboard FILE_H_MASK = { 0x8080808080808080ULL };

// ---------------------------------------------------------------------------
//  Square to Bitboard Helper
// ---------------------------------------------------------------------------
constexpr Bitboard sq_bb(Square sq) {
    return { 1ULL << static_cast<int>(sq) };
}

// ---------------------------------------------------------------------------
//  Pawn Attacks
// ---------------------------------------------------------------------------
void init_pawn_attacks() {
    for (int sq = 0; sq < 64; ++sq) {
        const Bitboard b = sq_bb(static_cast<Square>(sq));

        // White pawns attack "up" the board$a
        PAWN_ATTACKS[0][sq] = { ((b.bb << 7) & ~FILE_A_MASK.bb) | ((b.bb << 9) & ~FILE_H_MASK.bb) };

        // Black pawns attack "down" the board
        PAWN_ATTACKS[1][sq] = { ((b.bb >> 7) & ~FILE_H_MASK.bb) | ((b.bb >> 9) & ~FILE_A_MASK.bb) };
    }
}

// ---------------------------------------------------------------------------
//  Knight Attacks
// ---------------------------------------------------------------------------
void init_knight_attacks() {
    for (int sq = 0; sq < 64; ++sq) {
        const Bitboard b = sq_bb(static_cast<Square>(sq));
        Bitboard attacks = { 0 };

        // Knight offsets: all 8 L-shape moves
        constexpr int Offsets[8] = { 17, 15, 10, 6, -6, -10, -15, -17 };

        for (int i = 0; i < 8; ++i) {
            const int target = sq + Offsets[i];
            if (target >= 0 && target < 64) {
                // Verify the move doesn't wrap around the board
                // A knight move changes file by 1 or 2. If source and target
                // file distance exceeds 2, the move wrapped.
                const int src_file = sq % 8;
                const int tgt_file = target % 8;
                const int file_diff = (src_file > tgt_file) ? (src_file - tgt_file) : (tgt_file - src_file);
                if (file_diff <= 2) {
                    attacks.bb |= 1ULL << target;
                }
            }
        }

        KNIGHT_ATTACKS[sq] = attacks;
    }
}

// ---------------------------------------------------------------------------
//  King Attacks
// ---------------------------------------------------------------------------
void init_king_attacks() {
    for (int sq = 0; sq < 64; ++sq) {
        const Bitboard b = sq_bb(static_cast<Square>(sq));
        Bitboard attacks = { 0 };

        // King offsets: all 8 surrounding squares
        constexpr int Offsets[8] = { 9, 8, 7, 1, -1, -7, -8, -9 };

        for (int i = 0; i < 8; ++i) {
            const int target = sq + Offsets[i];
            if (target >= 0 && target < 64) {
                const int src_file = sq % 8;
                const int tgt_file = target % 8;
                const int file_diff = (src_file > tgt_file) ? (src_file - tgt_file) : (tgt_file - src_file);
                if (file_diff <= 1) {
                    attacks.bb |= 1ULL << target;
                }
            }
        }

        KING_ATTACKS[sq] = attacks;
    }
}

// ---------------------------------------------------------------------------
//  Slider Blocker Masks
// ---------------------------------------------------------------------------
Bitboard mask_bishop_attacks(Square sq) {
    Bitboard mask = { 0 };
    const int f = static_cast<int>(sq) % 8;
    const int r = static_cast<int>(sq) / 8;

    // Iterate all 4 diagonal directions
    for (int dir = -1; dir <= 1; dir += 2) {
        for (int d = -1; d <= 1; d += 2) {
            for (int step = 1; step < 8; ++step) {
                const int nf = f + dir * step;
                const int nr = r + d * step;
                if (nf < 0 || nf > 7 || nr < 0 || nr > 7) break;
                // Skip the outermost square in each direction (edge can't block)
                if (nf == 0 || nf == 7 || nr == 0 || nr == 7) break;
                mask.bb |= 1ULL << (nr * 8 + nf);
            }
        }
    }

    return mask;
}

Bitboard mask_rook_attacks(Square sq) {
    Bitboard mask = { 0 };
    const int f = static_cast<int>(sq) % 8;
    const int r = static_cast<int>(sq) / 8;

    // Iterate all 4 orthogonal directions
    constexpr int DirF[4] = { 0, 0, -1, 1 };
    constexpr int DirR[4] = { -1, 1, 0, 0 };

    for (int i = 0; i < 4; ++i) {
        for (int step = 1; step < 8; ++step) {
            const int nf = f + DirF[i] * step;
            const int nr = r + DirR[i] * step;
            if (nf < 0 || nf > 7 || nr < 0 || nr > 7) break;
            // Skip the outermost square in each direction (edge can't block)
            if (nf == 0 || nf == 7 || nr == 0 || nr == 7) break;
            mask.bb |= 1ULL << (nr * 8 + nf);
        }
    }

    return mask;
}

// ---------------------------------------------------------------------------
//  On-The-Fly Slider Attack Generation (Raycasting)
// ---------------------------------------------------------------------------
Bitboard bishop_attacks_on_the_fly(Square sq, Bitboard block) {
    Bitboard attacks = { 0 };
    const int f = static_cast<int>(sq) % 8;
    const int r = static_cast<int>(sq) / 8;

    // 4 diagonal directions
    constexpr int DirF[4] = { -1, 1, 1, -1 };
    constexpr int DirR[4] = { -1, -1, 1, 1 };

    for (int i = 0; i < 4; ++i) {
        for (int step = 1; step < 8; ++step) {
            const int nf = f + DirF[i] * step;
            const int nr = r + DirR[i] * step;
            if (nf < 0 || nf > 7 || nr < 0 || nr > 7) break;
            const Bitboard target = { 1ULL << (nr * 8 + nf) };
            attacks.bb |= target.bb;
            if (block.bb & target.bb) break;
        }
    }

    return attacks;
}

Bitboard rook_attacks_on_the_fly(Square sq, Bitboard block) {
    Bitboard attacks = { 0 };
    const int f = static_cast<int>(sq) % 8;
    const int r = static_cast<int>(sq) / 8;

    // 4 orthogonal directions
    constexpr int DirF[4] = { 0, 0, -1, 1 };
    constexpr int DirR[4] = { -1, 1, 0, 0 };

    for (int i = 0; i < 4; ++i) {
        for (int step = 1; step < 8; ++step) {
            const int nf = f + DirF[i] * step;
            const int nr = r + DirR[i] * step;
            if (nf < 0 || nf > 7 || nr < 0 || nr > 7) break;
            const Bitboard target = { 1ULL << (nr * 8 + nf) };
            attacks.bb |= target.bb;
            if (block.bb & target.bb) break;
        }
    }

    return attacks;
}

// ---------------------------------------------------------------------------
//  Public Initialization Entry Point
// ---------------------------------------------------------------------------
void init_attacks() {
    init_pawn_attacks();
    init_knight_attacks();
    init_king_attacks();
}
