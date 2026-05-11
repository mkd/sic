#include "../include/nnue.h"
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
