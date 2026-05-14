#include "nnue_bridge.h"
#include "../include/evaluate.h"
#include <cstring>

// Include nnue-probe library header AFTER position.h is already included.
// nnue.h defines struct Position which conflicts with class Position.
// Rename it via macro so only the function/NNUEdata declarations are usable.
#define DLL_EXPORT
#define Position NNUEProbePosition
#include "nnue.h"
#undef Position
#undef DLL_EXPORT

// ---------------------------------------------------------------------------
//  nnue-probe bridge: incremental accumulator routing
// ---------------------------------------------------------------------------
int nnue_bridge_evaluate(
    const int* pieces,
    const int* squares,
    int player,
    const NNUEState* stateCurrent,
    const NNUEState* statePlyMinus1,
    const NNUEState* statePlyMinus2
) {
    NNUEdata* nnuePtrs[3] = { nullptr, nullptr, nullptr };
    nnuePtrs[0] = const_cast<NNUEdata*>(reinterpret_cast<const NNUEdata*>(stateCurrent));
    if (statePlyMinus1)
        nnuePtrs[1] = const_cast<NNUEdata*>(reinterpret_cast<const NNUEdata*>(statePlyMinus1));
    if (statePlyMinus2)
        nnuePtrs[2] = const_cast<NNUEdata*>(reinterpret_cast<const NNUEdata*>(statePlyMinus2));

    if (!stateCurrent->dirtyPiece.dirtyNum) {
        return nnue_evaluate(player, const_cast<int*>(pieces), const_cast<int*>(squares));
    } else if (nnuePtrs[1] || nnuePtrs[2]) {
        return nnue_evaluate_incremental(player, const_cast<int*>(pieces),
                                         const_cast<int*>(squares), nnuePtrs);
    } else {
        return nnue_evaluate(player, const_cast<int*>(pieces), const_cast<int*>(squares));
    }
}

// ---------------------------------------------------------------------------
//  Load single HalfKP NNUE network file
// ---------------------------------------------------------------------------
void load_nnue(const std::string& path) {
    nnue_init(path.c_str());
    nnue_initialized = true;
}
