#pragma once

#include "types.h"
#include <cstdlib>
#include <iostream>

// ---------------------------------------------------------------------------
//  Compiler Hints
// ---------------------------------------------------------------------------
#define FORCE_INLINE inline __attribute__((always_inline))
#define NOINLINE     __attribute__((noinline))
#define ALIGN(x)     __attribute__((aligned(x)))

// ---------------------------------------------------------------------------
//  Custom Assert
// ---------------------------------------------------------------------------
#ifndef NDEBUG
#define ASSERT(cond)                                                     \
    do {                                                                 \
        if (!(cond)) {                                                   \
            std::cerr << "Assertion failed: " << #cond                   \
                      << "\n  file: " << __FILE__                        \
                      << "\n  line: " << __LINE__ << "\n"                \
                      << std::endl;                                      \
            std::abort();                                                \
        }                                                                \
    } while (0)
#else
#define ASSERT(cond)
#endif

// ---------------------------------------------------------------------------
//  Bit Operations  (critical-path wrappers around compiler built-ins)
// ---------------------------------------------------------------------------
FORCE_INLINE int popcount(Bitboard b) {
    return __builtin_popcountll(b.bb);
}

FORCE_INLINE Square lsb(Bitboard b) {
    return static_cast<Square>(__builtin_ctzll(b.bb));
}

FORCE_INLINE Square msb(Bitboard b) {
    return static_cast<Square>(63 ^ __builtin_clzll(b.bb));
}

FORCE_INLINE Square pop_lsb(Bitboard& b) {
    const Square s = lsb(b);
    b.bb &= b.bb - 1;
    return s;
}

// ---------------------------------------------------------------------------
//  Platform Sanity
// ---------------------------------------------------------------------------
static_assert(sizeof(void*) == 8, "SIC requires a 64-bit platform");
