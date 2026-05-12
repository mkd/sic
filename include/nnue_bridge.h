#pragma once

#include "position.h"

// ---------------------------------------------------------------------------
//  Bridge to nnue-probe library
//  Calls nnue-probe evaluation without exposing nnue.h's conflicting types
// ---------------------------------------------------------------------------
int nnue_bridge_evaluate(
    const int* pieces,
    const int* squares,
    int player,
    const NNUEState* stateCurrent,
    const NNUEState* statePlyMinus1,
    const NNUEState* statePlyMinus2
);

void load_nnue(const std::string& bigPath, const std::string& smallPath);
