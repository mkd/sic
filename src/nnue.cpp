#include "../include/nnue.h"
#include "../include/position.h"
#include "../include/utils.h"
#include <cstring>
#include <fstream>
#include <iostream>

// ---------------------------------------------------------------------------
//  Global Network Weights
// ---------------------------------------------------------------------------
int16_t ft_weights[TRANSFORMER_INPUTS][TRANSFORMER_NEURONS];
int16_t ft_biases[TRANSFORMER_NEURONS];

int8_t output_weights[TRANSFORMER_NEURONS * 2];
int32_t output_bias = 0;

bool nnue_initialized = false;

// ---------------------------------------------------------------------------
//  NNUE Binary Loader
// ---------------------------------------------------------------------------
bool load_nnue(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);

    if (!file.is_open()) {
        std::cout << "info string Failed to load NNUE" << std::endl;
        return false;
    }

    std::cout << "info string Successfully opened NNUE file" << std::endl;
    nnue_initialized = true;
    return true;
}

// ---------------------------------------------------------------------------
//  HalfKP Feature Index
// ---------------------------------------------------------------------------
static int make_halfkp_index(bool is_white_king, Square king_sq, Piece pc, Square sq) {
    Square transformed_king = is_white_king ? king_sq : static_cast<Square>(static_cast<int>(king_sq) ^ 56);
    Square transformed_sq = is_white_king ? sq : static_cast<Square>(static_cast<int>(sq) ^ 56);

    int piece_index;
    if (is_white_king) {
        piece_index = static_cast<int>(pc);
        if (piece_index > 4) piece_index -= 6;
    } else {
        piece_index = static_cast<int>(pc);
        if (piece_index <= 4) piece_index += 6;
        piece_index -= 6;
    }

    return piece_index * 64 + static_cast<int>(transformed_sq) + static_cast<int>(transformed_king) * 11 * 64;
}

// ---------------------------------------------------------------------------
//  Full Refresh
// ---------------------------------------------------------------------------
void refresh_accumulator(Position& pos) {
    std::memcpy(pos.accumulator.white, ft_biases, sizeof(pos.accumulator.white));
    std::memcpy(pos.accumulator.black, ft_biases, sizeof(pos.accumulator.black));

    Square w_king = pos.get_king_square(Color::WHITE);
    Square b_king = pos.get_king_square(Color::BLACK);

    Bitboard occ = pos.occupied();
    while (occ.bb) {
        Square sq = pop_lsb(occ);
        Piece pc = pos.piece_on(sq);

        if (pc == Piece::WHITE_KING || pc == Piece::BLACK_KING) continue;

        int w_idx = make_halfkp_index(true, w_king, pc, sq);
        int b_idx = make_halfkp_index(false, b_king, pc, sq);

        for (int i = 0; i < TRANSFORMER_NEURONS; ++i) {
            pos.accumulator.white[i] += ft_weights[w_idx][i];
            pos.accumulator.black[i] += ft_weights[b_idx][i];
        }
    }

    pos.accumulator_stale = false;
}
