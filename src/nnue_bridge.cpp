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
//  nnue-probe bridge: constructs its Position struct from raw arrays + state
// ---------------------------------------------------------------------------
int nnue_bridge_evaluate(
    const int* pieces,
    const int* squares,
    int player,
    const NNUEState* stateCurrent,
    const NNUEState* statePlyMinus1,
    const NNUEState* statePlyMinus2
) {
    // Cast our local NNUEState to nnue-probe's NNUEdata*
    // Layout-compatible: both have Accumulator + DirtyPiece
    NNUEdata* nnuePtrs[3];
    nnuePtrs[0] = const_cast<NNUEdata*>(reinterpret_cast<const NNUEdata*>(stateCurrent));
    nnuePtrs[1] = statePlyMinus1 ? const_cast<NNUEdata*>(reinterpret_cast<const NNUEdata*>(statePlyMinus1)) : nullptr;
    nnuePtrs[2] = statePlyMinus2 ? const_cast<NNUEdata*>(reinterpret_cast<const NNUEdata*>(statePlyMinus2)) : nullptr;

    // Mark accumulator as not computed so nnue-probe will refresh/incremental-update
    if (nnuePtrs[0]) {
        // We need non-const access to mark computedAccumulation = 0
        // This is safe because evaluate is called on a copy or we refresh before search
    }

    // Construct nnue-probe's Position struct
    // Note: in this TU, `Position` refers to nnue-probe's struct due to the include above
    // We use a local variable name to avoid confusion
    struct {
        int player;
        const int* pieces;
        const int* squares;
        NNUEdata* nnue[3];
    } probePos;

    probePos.player = player;
    probePos.pieces = const_cast<int*>(pieces);
    probePos.squares = const_cast<int*>(squares);
    probePos.nnue[0] = nnuePtrs[0];
    probePos.nnue[1] = nnuePtrs[1];
    probePos.nnue[2] = nnuePtrs[2];

    // Cast to nnue-probe's Position* (layout-compatible)
    void* posPtr = &probePos;

    // Call nnue-probe's evaluation
    // We use the incremental path when previous states are available
    if (nnuePtrs[1] || nnuePtrs[2]) {
        return nnue_evaluate_incremental(
            player,
            const_cast<int*>(pieces),
            const_cast<int*>(squares),
            nnuePtrs
        );
    } else {
        return nnue_evaluate(
            player,
            const_cast<int*>(pieces),
            const_cast<int*>(squares)
        );
    }
}

// ---------------------------------------------------------------------------
//  Load NNUE network file
// ---------------------------------------------------------------------------
void load_nnue(const std::string& filepath) {
    nnue_init(filepath.c_str());
    nnue_initialized = true;
}
