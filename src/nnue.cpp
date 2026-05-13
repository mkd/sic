#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

//--------------------
#ifdef _MSC_VER
#  define USE_AVX2   1
#  define USE_SSE41  1
#  define USE_SSE3   1
#  define USE_SSE2   1
#  define USE_SSE    1
#  define IS_64BIT   1
#endif
//-------------------

#if defined(USE_AVX2)
#include <immintrin.h>

#elif defined(USE_SSE41)
#include <smmintrin.h>

#elif defined(USE_SSSE3)
#include <tmmintrin.h>

#elif defined(USE_SSE2)
#include <emmintrin.h>

#elif defined(USE_SSE)
#include <xmmintrin.h>

#elif defined(USE_MMX)
#include <mmintrin.h>

#elif defined(USE_NEON)
#include <arm_neon.h>
#endif

//-------------------
#include "misc.h"
#define DLL_EXPORT
#include "nnue.h"
#undef DLL_EXPORT

#define KING(c)    ( (c) ? bking : wking )
#define IS_KING(p) ( ((p) == wking) || ((p) == bking) )
//-------------------

// Old gcc on Windows is unable to provide a 32-byte aligned stack.
// We need to hack around this when using AVX2 and AVX512.
#if     defined(__GNUC__ ) && (__GNUC__ < 9) && defined(_WIN32) \
    && !defined(__clang__) && !defined(__INTEL_COMPILER) \
    &&  defined(USE_AVX2)
#define ALIGNMENT_HACK
#endif

#if defined(USE_NEON) && !defined(IS_64BIT)
INLINE int16x8_t vmovl_high_s16(int8x16_t v)
{
  return vmovl_s16(vget_high_s16(v));
}
#endif

enum {
  PS_W_PAWN   =  1,
  PS_B_PAWN   =  1 * 64 + 1,
  PS_W_KNIGHT =  2 * 64 + 1,
  PS_B_KNIGHT =  3 * 64 + 1,
  PS_W_BISHOP =  4 * 64 + 1,
  PS_B_BISHOP =  5 * 64 + 1,
  PS_W_ROOK   =  6 * 64 + 1,
  PS_B_ROOK   =  7 * 64 + 1,
  PS_W_QUEEN  =  8 * 64 + 1,
  PS_B_QUEEN  =  9 * 64 + 1,
  PS_END      = 10 * 64 + 1
};

uint32_t PieceToIndex[2][14] = {
  { 0, 0, PS_W_QUEEN, PS_W_ROOK, PS_W_BISHOP, PS_W_KNIGHT, PS_W_PAWN,
       0, PS_B_QUEEN, PS_B_ROOK, PS_B_BISHOP, PS_B_KNIGHT, PS_B_PAWN, 0},
  { 0, 0, PS_B_QUEEN, PS_B_ROOK, PS_B_BISHOP, PS_B_KNIGHT, PS_B_PAWN,
       0, PS_W_QUEEN, PS_W_ROOK, PS_W_BISHOP, PS_W_KNIGHT, PS_W_PAWN, 0}
};

// Version of the evaluation file
static const uint32_t NnueVersion = 0x7AF32F16u;

// Constants used in evaluation value calculation
enum {
  FV_SCALE = 16,
  SHIFT = 6
};

enum {
  kHalfDimensions = 256,
  FtInDims = 64 * PS_END, // 64 * 641
  FtOutDims = kHalfDimensions * 2
};

// ---------------------------------------------------------------------------
//  SFNNv13 HalfKAv2 Architecture Constants
//  Modern Stockfish dual-network (Big + Small) with King Buckets
// ---------------------------------------------------------------------------

// Network file sizes (exact byte counts)
enum {
  SFNNv13_Big_Size   = 65429575,  // Big  network (~62MB)
  SFNNv13_Small_Size =  3480122,  // Small network (~3.3MB)
  HalfKP_Legacy_Size = 21022697   // Legacy HalfKP (~20MB)
};

// SFNNv13 Big network architecture
enum {
  SFNNv13_Big_HalfDims      = 3072,
  SFNNv13_Big_KingBuckets   = 8192,
  SFNNv13_Big_FtInDims      = 768,   // 12 piece types * 64 squares
  SFNNv13_Big_FtOutDims     = SFNNv13_Big_HalfDims * 2
};

// SFNNv13 Small network architecture
enum {
  SFNNv13_Small_HalfDims      = 1536,
  SFNNv13_Small_KingBuckets   = 1024,
  SFNNv13_Small_FtInDims      = 768,  // 12 piece types * 64 squares
  SFNNv13_Small_FtOutDims     = SFNNv13_Small_HalfDims * 2
};

// SFNNv13 magic numbers (HalfKAv2 header signatures)
enum {
  SFNNv13_Magic_Version   = 0x7AF32F16u,
  SFNNv13_Magic_HashLow   = 0x6153107Fu,  // HalfKAv2 hash low
  SFNNv13_Magic_HashHigh  = 0x1483D667u,  // HalfKAv2 hash high
  SFNNv13_Magic_Transformer = 0x8774dd14u, // HalfKAv2 transformer magic
  SFNNv13_Magic_Network   = 0x63337156u   // Network section magic (shared)
};

// Architecture identification
enum NetworkArchitecture {
  ARCH_UNKNOWN = 0,
  ARCH_HalfKP  = 1,   // Legacy HalfKP (256-dim, no king buckets)
  ARCH_SFNNv13_Big  = 2, // HalfKAv2 Big  (3072-dim, 8192 king buckets)
  ARCH_SFNNv13_Small = 3 // HalfKAv2 Small (1536-dim, 1024 king buckets)
};

static NetworkArchitecture g_NetworkArch = ARCH_UNKNOWN;

// USE_MMX generates _mm_empty() instructions, so undefine if not needed
#if defined(USE_SSE2)
#undef USE_MMX
#endif

static_assert(kHalfDimensions % 256 == 0, "kHalfDimensions should be a multiple of 256");

#define VECTOR

#ifdef USE_AVX512
#define SIMD_WIDTH 512
typedef __m512i vec16_t;
typedef __m512i vec8_t;
typedef __mmask64 mask_t;
#define vec_add_16(a,b) _mm512_add_epi16(a,b)
#define vec_sub_16(a,b) _mm512_sub_epi16(a,b)
#define vec_packs(a,b) _mm512_packs_epi16(a,b)
#define vec_mask_pos(a) _mm512_cmpgt_epi8_mask(a,_mm512_setzero_si512())
#define NUM_REGS 8 // only 8 are needed

#elif USE_AVX2
#define SIMD_WIDTH 256
typedef __m256i vec16_t;
typedef __m256i vec8_t;
typedef uint32_t mask_t;
#define vec_add_16(a,b) _mm256_add_epi16(a,b)
#define vec_sub_16(a,b) _mm256_sub_epi16(a,b)
#define vec_packs(a,b) _mm256_packs_epi16(a,b)
#define vec_mask_pos(a) _mm256_movemask_epi8(_mm256_cmpgt_epi8(a,_mm256_setzero_si256()))
#define NUM_REGS 16

#elif USE_SSE2
#define SIMD_WIDTH 128
typedef __m128i vec16_t;
typedef __m128i vec8_t;
typedef uint16_t mask_t;
#define vec_add_16(a,b) _mm_add_epi16(a,b)
#define vec_sub_16(a,b) _mm_sub_epi16(a,b)
#define vec_packs(a,b) _mm_packs_epi16(a,b)
#define vec_mask_pos(a) _mm_movemask_epi8(_mm_cmpgt_epi8(a,_mm_setzero_si128()))
#ifdef IS_64BIT
#define NUM_REGS 16
#else
#define NUM_REGS 8
#endif

#elif USE_MMX
#define SIMD_WIDTH 64
typedef __m64 vec16_t;
typedef __m64 vec8_t;
typedef uint8_t mask_t;
#define vec_add_16(a,b) _mm_add_pi16(a,b)
#define vec_sub_16(a,b) _mm_sub_pi16(a,b)
#define vec_packs(a,b) _mm_packs_pi16(a,b)
#define vec_mask_pos(a) _mm_movemask_pi8(_mm_cmpgt_pi8(a,_mm_setzero_si64()))
#define NUM_REGS 8

#elif USE_NEON
#define SIMD_WIDTH 128
typedef int16x8_t vec16_t;
typedef int8x16_t vec8_t;
typedef uint16_t mask_t;
#define vec_add_16(a,b) vaddq_s16(a,b)
#define vec_sub_16(a,b) vsubq_s16(a,b)
#define vec_packs(a,b) vcombine_s8(vqmovn_s16(a),vqmovn_s16(b))
#define vec_mask_pos(a) neon_movemask(vcgtq_s8(a,vdupq_n_u8(0)))
#ifdef IS_64BIT
#define NUM_REGS 16
#else
#define NUM_REGS 8
#endif

#else
#undef VECTOR
#define SIMD_WIDTH 16 // dummy
typedef uint8_t mask_t; // dummy

#endif

#ifdef IS_64BIT
typedef uint64_t mask2_t;
#else
typedef uint32_t mask2_t;
#endif

typedef int8_t clipped_t;
#if defined(USE_MMX) || (defined(USE_SSE2) && !defined(USE_AVX2))
typedef int16_t weight_t;
#else
typedef int8_t weight_t;
#endif

typedef struct {
  size_t size;
  unsigned values[30];
} IndexList;

INLINE int orient(int c, int s)
{
  return s ^ (c == white ? 0x00 : 0x3f);
}

INLINE unsigned make_index(int c, int s, int pc, int ksq)
{
  return orient(c, s) + PieceToIndex[c][pc] + PS_END * ksq;
}

static void half_kp_append_active_indices(const Position *pos, const int c,
    IndexList *active)
{
  int ksq = pos->squares[c];
  ksq = orient(c, ksq);
  for (int i = 2; pos->pieces[i]; i++) {
    int sq = pos->squares[i];
    int pc = pos->pieces[i];
    active->values[active->size++] = make_index(c, sq, pc, ksq);
  }
}

static void half_kp_append_changed_indices(const Position *pos, const int c,
    const DirtyPiece *dp, IndexList *removed, IndexList *added)
{
  int ksq = pos->squares[c];
  ksq = orient(c, ksq);
  for (int i = 0; i < dp->dirtyNum; i++) {
    int pc = dp->pc[i];
    if (IS_KING(pc)) continue;
    if (dp->from[i] != 64)
      removed->values[removed->size++] = make_index(c, dp->from[i], pc, ksq);
    if (dp->to[i] != 64)
      added->values[added->size++] = make_index(c, dp->to[i], pc, ksq);
  }
}

static void append_active_indices(const Position *pos, IndexList active[2])
{
  for (unsigned c = 0; c < 2; c++)
    half_kp_append_active_indices(pos, c, &active[c]);
}

static void append_changed_indices(const Position *pos, IndexList removed[2],
    IndexList added[2], bool reset[2])
{
  const DirtyPiece *dp = &(pos->nnue[0]->dirtyPiece);
  // assert(dp->dirtyNum != 0);

  if (pos->nnue[1]->accumulator.computedAccumulation) {
    for (unsigned c = 0; c < 2; c++) {
      reset[c] = dp->pc[0] == (int)KING(c);
      if (reset[c])
        half_kp_append_active_indices(pos, c, &added[c]);
      else
        half_kp_append_changed_indices(pos, c, dp, &removed[c], &added[c]);
    }
  } else {
    const DirtyPiece *dp2 = &(pos->nnue[1]->dirtyPiece);
    for (unsigned c = 0; c < 2; c++) {
      reset[c] =   dp->pc[0] == (int)KING(c)
                || dp2->pc[0] == (int)KING(c);
      if (reset[c])
        half_kp_append_active_indices(pos, c, &added[c]);
      else {
        half_kp_append_changed_indices(pos, c, dp, &removed[c], &added[c]);
        half_kp_append_changed_indices(pos, c, dp2, &removed[c], &added[c]);
      }
    }
  }
}

// InputLayer = InputSlice<256 * 2>
// out: 512 x clipped_t

// Hidden1Layer = ClippedReLu<AffineTransform<InputLayer, 32>>
// 512 x clipped_t -> 32 x int32_t -> 32 x clipped_t

// Hidden2Layer = ClippedReLu<AffineTransform<hidden1, 32>>
// 32 x clipped_t -> 32 x int32_t -> 32 x clipped_t

// OutputLayer = AffineTransform<HiddenLayer2, 1>
// 32 x clipped_t -> 1 x int32_t

#if !defined(USE_AVX512)
static weight_t hidden1_weights alignas(64) [32 * 512];
static weight_t hidden2_weights alignas(64) [32 * 32];
#else
static weight_t hidden1_weights alignas(64) [64 * 512];
static weight_t hidden2_weights alignas(64) [64 * 32];
#endif
static weight_t output_weights alignas(64) [1 * 32];

static int32_t hidden1_biases alignas(64) [32];
static int32_t hidden2_biases alignas(64) [32];
static int32_t output_biases[1];

INLINE int32_t affine_propagate(clipped_t *input, int32_t *biases,
    weight_t *weights)
{
#if defined(USE_AVX2)
  __m256i *iv = (__m256i *)input;
  __m256i *row = (__m256i *)weights;
#if defined(USE_VNNI)
  __m256i prod = _mm256_dpbusd_epi32(_mm256_setzero_si256(), iv[0], row[0]);
#else
  __m256i prod = _mm256_maddubs_epi16(iv[0], row[0]);
  prod = _mm256_madd_epi16(prod, _mm256_set1_epi16(1));
#endif
  __m128i sum = _mm_add_epi32(
      _mm256_castsi256_si128(prod), _mm256_extracti128_si256(prod, 1));
  sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x1b));
  return _mm_cvtsi128_si32(sum) + _mm_extract_epi32(sum, 1) + biases[0];

#elif defined(USE_SSE2)
  __m128i *iv = (__m128i *)input;
  __m128i *row = (__m128i *)weights;
#if defined(AVOID_USE_SSSE3)
  const __m128i kOnes = _mm_set1_epi16(1);
  __m128i p0 = _mm_madd_epi16(_mm_maddubs_epi16(iv[0], row[0]), kOnes);
  __m128i p1 = _mm_madd_epi16(_mm_maddubs_epi16(iv[1], row[1]), kOnes);
  __m128i sum = _mm_add_epi32(p0, p1);
#else
  __m128i p0 = _mm_madd_epi16(iv[0], row[0]);
  __m128i p1 = _mm_madd_epi16(iv[1], row[1]);
  __m128i p2 = _mm_madd_epi16(iv[2], row[2]);
  __m128i p3 = _mm_madd_epi16(iv[3], row[3]);
  __m128i sum = _mm_add_epi32(_mm_add_epi32(p0, p1), _mm_add_epi32(p2, p3));
#endif
  sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0xb));
#if defined(USE_SSE41)
  return _mm_cvtsi128_si32(sum) + _mm_extract_epi32(sum, 1) + biases[0];
#else
  sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x1));
  return _mm_cvtsi128_si32(sum) + biases[0];
#endif

#elif defined(USE_MMX)
  __m64 *iv = (__m64 *)input;
  __m64 s0 = _mm_setzero_si64(), s1 = s0;
  __m64 *row = (__m64 *)weights;
  for (unsigned j = 0; j < 4; j++) {
    s0 = _mm_add_pi32(s0, _mm_madd_pi16(row[2 * j], iv[2 * j]));
    s1 = _mm_add_pi32(s1, _mm_madd_pi16(row[2 * j + 1], iv[2 * j + 1]));
  }
  __m64 sum = _mm_add_pi32(s0, s1);
  sum = _mm_add_pi32(sum, _mm_unpackhi_pi32(sum, sum));
  return _mm_cvtsi64_si32(sum) + biases[0];

#elif defined(USE_NEON)
  int8x8_t *iv = (int8x8_t *)input;
  int32x4_t sum = {biases[0]};
  int8x8_t *row = (int8x8_t *)weights;
  int16x8_t p0 = vmull_s8(iv[0], row[0]);
  int16x8_t p1 = vmull_s8(iv[1], row[1]);
  p0 = vmlal_s8(p0, iv[2], row[2]);
  sum = vpadalq_s16(sum, p0);
  p1 = vmlal_s8(p1, iv[3], row[3]);
  sum = vpadalq_s16(sum, p1);
  return sum[0] + sum[1] + sum[2] + sum[3];

#else
  int32_t sum = biases[0];
  for (unsigned j = 0; j < 32; j++)
    sum += weights[j] * input[j];
  return sum;

#endif
}

static_assert(FtOutDims % 64 == 0, "FtOutDims not a multiple of 64");

#ifdef VECTOR
INLINE bool next_idx(unsigned *idx, unsigned *offset, mask2_t *v,
    mask_t *mask, unsigned inDims)
{
  while (*v == 0) {
    *offset += 8 * sizeof(mask2_t);
    if (*offset >= inDims) return false;
    memcpy(v, (char *)mask + (*offset / 8), sizeof(mask2_t));
  }
#ifdef IS_64BIT
  *idx = *offset + bsf(*v);
#else
  *idx = *offset + bsf(*v);
#endif
  *v &= *v - 1;
  return true;
}

#if defined(USE_MMX) && !defined(USE_SSE)
INLINE int _mm_movemask_pi8(__m64 v)
{
  const __m64 powers = _mm_set_pi8(-128, 64, 32, 16, 8, 4, 2, 1);
  __m64 m = _mm_and_si64(v, powers);
  m = _mm_or_si64(m, _mm_srli_si64(m, 32));
  m = _mm_or_si64(m, _mm_srli_pi32(m, 16));
  m = _mm_or_si64(m, _mm_srli_pi16(m, 8));
  return _mm_cvtsi64_si32(m) & 0xff;
}
#elif defined(USE_NEON)
INLINE int neon_movemask(uint8x16_t v)
{
  const uint8_t __attribute__((aligned(16))) powers[16] =
    { 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128 };
  const uint8x16_t kPowers = vld1q_u8(powers);

  uint64x2_t mask = vpaddlq_u32(vpaddlq_u16(vpaddlq_u8(vandq_u8(v, kPowers))));
  return   vgetq_lane_u8((uint8x16_t)mask, 0)
        | (vgetq_lane_u8((uint8x16_t)mask, 8) << 8);
}
#endif
#endif

#if defined(USE_AVX512)
INLINE void affine_txfm(int8_t *input, void *output, unsigned inDims,
    unsigned outDims, const int32_t *biases, const weight_t *weights,
    mask_t *inMask, mask_t *outMask, const bool pack8_and_calc_mask)
{
  assert(outDims == 32);

  (void)outDims;
  const __m512i kZero = _mm512_setzero_si512();
  __m512i out_0 = ((__m512i *)biases)[0];
  __m512i out_1 = ((__m512i *)biases)[1];
  __m512i first, second;
  mask2_t v;
  unsigned idx;

  memcpy(&v, inMask, sizeof(mask2_t));
  for (unsigned offset = 0; offset < inDims;) {
    if (!next_idx(&idx, &offset, &v, inMask, inDims))
      break;
    first = ((__m512i *)weights)[idx];
    uint16_t factor = input[idx];
    if (next_idx(&idx, &offset, &v, inMask, inDims)) {
      second = ((__m512i *)weights)[idx];
      factor |= input[idx] << 8;
    } else {
      second = kZero;
    }
    __m512i mul = _mm512_set1_epi16(factor), prod, signs;
    prod = _mm512_maddubs_epi16(mul, _mm512_unpacklo_epi8(first, second));
    signs = _mm512_srai_epi16(prod, 15);
    out_0 = _mm512_add_epi32(out_0, _mm512_unpacklo_epi16(prod, signs));
    out_1 = _mm512_add_epi32(out_1, _mm512_unpackhi_epi16(prod, signs));
  }

  __m512i out16 = _mm512_srai_epi16(_mm512_packs_epi32(out_0, out_1), SHIFT);

  __m256i *outVec = (__m256i *)output;
  const __m256i kZero256 = _mm256_setzero_si256();
  outVec[0] = _mm256_packs_epi16(
      _mm512_castsi512_si256(out16),_mm512_extracti64x4_epi64(out16, 1));
  if (pack8_and_calc_mask)
    outMask[0] = (uint32_t)_mm256_movemask_epi8(_mm256_cmpgt_epi8(outVec[0], kZero256));
  else
    outVec[0] = _mm256_max_epi8(outVec[0], kZero256);
}
#elif defined(USE_AVX2)
INLINE void affine_txfm(int8_t *input, void *output, unsigned inDims,
    unsigned outDims, const int32_t *biases, const weight_t *weights,
    mask_t *inMask, mask_t *outMask, const bool pack8_and_calc_mask)
{
  assert(outDims == 32);

  (void)outDims;
  const __m256i kZero = _mm256_setzero_si256();
  __m256i out_0 = ((__m256i *)biases)[0];
  __m256i out_1 = ((__m256i *)biases)[1];
  __m256i out_2 = ((__m256i *)biases)[2];
  __m256i out_3 = ((__m256i *)biases)[3];
  __m256i first, second;
  mask2_t v;
  unsigned idx;

  memcpy(&v, inMask, sizeof(mask2_t));
  for (unsigned offset = 0; offset < inDims;) {
    if (!next_idx(&idx, &offset, &v, inMask, inDims))
      break;
    first = ((__m256i *)weights)[idx];
    uint16_t factor = input[idx];
    if (next_idx(&idx, &offset, &v, inMask, inDims)) {
      second = ((__m256i *)weights)[idx];
      factor |= input[idx] << 8;
    } else {
      second = kZero;
    }
    __m256i mul = _mm256_set1_epi16(factor), prod, signs;
    prod = _mm256_maddubs_epi16(mul, _mm256_unpacklo_epi8(first, second));
    signs = _mm256_cmpgt_epi16(kZero, prod);
    out_0 = _mm256_add_epi32(out_0, _mm256_unpacklo_epi16(prod, signs));
    out_1 = _mm256_add_epi32(out_1, _mm256_unpackhi_epi16(prod, signs));
    prod = _mm256_maddubs_epi16(mul, _mm256_unpackhi_epi8(first, second));
    signs = _mm256_cmpgt_epi16(kZero, prod);
    out_2 = _mm256_add_epi32(out_2, _mm256_unpacklo_epi16(prod, signs));
    out_3 = _mm256_add_epi32(out_3, _mm256_unpackhi_epi16(prod, signs));
  }

  __m256i out16_0 = _mm256_srai_epi16(_mm256_packs_epi32(out_0, out_1), SHIFT);
  __m256i out16_1 = _mm256_srai_epi16(_mm256_packs_epi32(out_2, out_3), SHIFT);

  __m256i *outVec = (__m256i *)output;
  outVec[0] = _mm256_packs_epi16(out16_0, out16_1);
  if (pack8_and_calc_mask)
    outMask[0] = _mm256_movemask_epi8(_mm256_cmpgt_epi8(outVec[0], kZero));
  else
    outVec[0] = _mm256_max_epi8(outVec[0], kZero);
}
#elif AVOID_USE_SSSE3
INLINE void affine_txfm(int8_t *input, void *output, unsigned inDims,
    unsigned outDims, const int32_t *biases, const weight_t *weights,
    mask_t *inMask, mask_t *outMask, const bool pack8_and_calc_mask)
{
  assert(outDims == 32);

  const __m128i kZeros[2] = { 0 };
  __m128i out_0 = ((__m128i *)biases)[0];
  __m128i out_1 = ((__m128i *)biases)[1];
  __m128i out_2 = ((__m128i *)biases)[2];
  __m128i out_3 = ((__m128i *)biases)[3];
  __m128i out_4 = ((__m128i *)biases)[4];
  __m128i out_5 = ((__m128i *)biases)[5];
  __m128i out_6 = ((__m128i *)biases)[6];
  __m128i out_7 = ((__m128i *)biases)[7];
  const __m128i *first, *second;
  mask2_t v;
  unsigned idx;

  memcpy(&v, inMask, sizeof(mask2_t));
  for (unsigned offset = 0; offset < inDims;) {
    if (!next_idx(&idx, &offset, &v, inMask, inDims))
      break;
    first = (__m128i *)&weights[outDims * idx];
    uint16_t factor = input[idx];
    if (next_idx(&idx, &offset, &v, inMask, inDims)) {
      second = (__m128i *)&weights[outDims * idx];
      factor |= input[idx] << 8;
    } else {
      second = kZeros;
    }
    __m128i mul = _mm_set1_epi16(factor), prod, signs;
    prod = _mm_maddubs_epi16(mul, _mm_unpacklo_epi8(first[0], second[0]));
    signs = _mm_cmpgt_epi16(kZeros[0], prod);
    out_0 = _mm_add_epi32(out_0, _mm_unpacklo_epi16(prod, signs));
    out_1 = _mm_add_epi32(out_1, _mm_unpackhi_epi16(prod, signs));
    prod = _mm_maddubs_epi16(mul, _mm_unpackhi_epi8(first[0], second[0]));
    signs = _mm_cmpgt_epi16(kZeros[0], prod);
    out_2 = _mm_add_epi32(out_2, _mm_unpacklo_epi16(prod, signs));
    out_3 = _mm_add_epi32(out_3, _mm_unpackhi_epi16(prod, signs));
    prod = _mm_maddubs_epi16(mul, _mm_unpacklo_epi8(first[1], second[1]));
    signs = _mm_cmpgt_epi16(kZeros[0], prod);
    out_4 = _mm_add_epi32(out_4, _mm_unpacklo_epi16(prod, signs));
    out_5 = _mm_add_epi32(out_5, _mm_unpackhi_epi16(prod, signs));
    prod = _mm_maddubs_epi16(mul, _mm_unpackhi_epi8(first[1], second[1]));
    signs = _mm_cmpgt_epi16(kZeros[0], prod);
    out_6 = _mm_add_epi32(out_6, _mm_unpacklo_epi16(prod, signs));
    out_7 = _mm_add_epi32(out_7, _mm_unpackhi_epi16(prod, signs));
  }

  __m128i out16_0 = _mm_srai_epi16(_mm_packs_epi32(out_0, out_1), SHIFT);
  __m128i out16_1 = _mm_srai_epi16(_mm_packs_epi32(out_2, out_3), SHIFT);
  __m128i out16_2 = _mm_srai_epi16(_mm_packs_epi32(out_4, out_5), SHIFT);
  __m128i out16_3 = _mm_srai_epi16(_mm_packs_epi32(out_6, out_7), SHIFT);

  __m128i *outVec = (__m128i *)output;
  if (pack8_and_calc_mask) {
    outVec[0] = _mm_packs_epi16(out16_0, out16_1);
    outMask[0] = _mm_movemask_epi8(_mm_cmpgt_epi8(outVec[0], kZeros[0]));
    outVec[1] = _mm_packs_epi16(out16_2, out16_3);
    outMask[1] = _mm_movemask_epi8(_mm_cmpgt_epi8(outVec[1], kZeros[0]));
  } else {
#if defined(USE_SSE41)
    outVec[0] = _mm_max_epi8(_mm_packs_epi16(out16_0, out16_1), kZeros[0]);
    outVec[1] = _mm_max_epi8(_mm_packs_epi16(out16_2, out16_3), kZeros[0]);
#else
    outVec[0] = _mm_packs_epi16(
        _mm_max_epi16(out16_0, kZeros[0]), _mm_max_epi16(out16_1, kZeros[0]));
    outVec[1] = _mm_packs_epi16(
        _mm_max_epi16(out16_2, kZeros[0]), _mm_max_epi16(out16_3, kZeros[0]));
#endif
  }
}
#elif defined(USE_SSE2)
INLINE void affine_txfm(clipped_t *input, void *output, unsigned inDims,
    unsigned outDims, const int32_t *biases, const weight_t *weights,
    mask_t *inMask, mask_t *outMask, const bool pack8_and_calc_mask)
{
  assert(outDims == 32);

  const __m128i kZeros[4] = { 0 };
  __m128i out_0 = ((__m128i *)biases)[0];
  __m128i out_1 = ((__m128i *)biases)[1];
  __m128i out_2 = ((__m128i *)biases)[2];
  __m128i out_3 = ((__m128i *)biases)[3];
  __m128i out_4 = ((__m128i *)biases)[4];
  __m128i out_5 = ((__m128i *)biases)[5];
  __m128i out_6 = ((__m128i *)biases)[6];
  __m128i out_7 = ((__m128i *)biases)[7];
  const __m128i *first, *second;
  mask2_t v;
  unsigned idx;

  memcpy(&v, inMask, sizeof(mask2_t));
  for (unsigned offset = 0; offset < inDims;) {
    if (!next_idx(&idx, &offset, &v, inMask, inDims))
      break;
    first = (__m128i *)&weights[outDims * idx];
    uint32_t factor = input[idx];
    if (next_idx(&idx, &offset, &v, inMask, inDims)) {
      second = (__m128i *)&weights[outDims * idx];
      factor |= input[idx] << 16;
    } else {
      second = kZeros;
    }
    __m128i mul = _mm_set1_epi32(factor);
    out_0 = _mm_add_epi32(out_0, _mm_madd_epi16(mul, _mm_unpacklo_epi16(first[0],second[0])));
    out_1 = _mm_add_epi32(out_1, _mm_madd_epi16(mul, _mm_unpackhi_epi16(first[0],second[0])));
    out_2 = _mm_add_epi32(out_2, _mm_madd_epi16(mul, _mm_unpacklo_epi16(first[1],second[1])));
    out_3 = _mm_add_epi32(out_3, _mm_madd_epi16(mul, _mm_unpackhi_epi16(first[1],second[1])));
    out_4 = _mm_add_epi32(out_4, _mm_madd_epi16(mul, _mm_unpacklo_epi16(first[2],second[2])));
    out_5 = _mm_add_epi32(out_5, _mm_madd_epi16(mul, _mm_unpackhi_epi16(first[2],second[2])));
    out_6 = _mm_add_epi32(out_6, _mm_madd_epi16(mul, _mm_unpacklo_epi16(first[3],second[3])));
    out_7 = _mm_add_epi32(out_7, _mm_madd_epi16(mul, _mm_unpackhi_epi16(first[3],second[3])));
  }

  __m128i out16_0 = _mm_srai_epi16(_mm_packs_epi32(out_0, out_1), SHIFT);
  __m128i out16_1 = _mm_srai_epi16(_mm_packs_epi32(out_2, out_3), SHIFT);
  __m128i out16_2 = _mm_srai_epi16(_mm_packs_epi32(out_4, out_5), SHIFT);
  __m128i out16_3 = _mm_srai_epi16(_mm_packs_epi32(out_6, out_7), SHIFT);

  __m128i *outVec = (__m128i *)output;
  if (pack8_and_calc_mask) {
    outVec[0] = _mm_packs_epi16(out16_0, out16_1);
    outMask[0] = _mm_movemask_epi8(_mm_cmpgt_epi8(outVec[0], kZeros[0]));
    outVec[1] = _mm_packs_epi16(out16_2, out16_3);
    outMask[1] = _mm_movemask_epi8(_mm_cmpgt_epi8(outVec[1], kZeros[0]));
  } else {
    const __m128i kx07f = _mm_set1_epi16(127);
    outVec[0] = _mm_min_epi16(_mm_max_epi16(out16_0, kZeros[0]), kx07f);
    outVec[1] = _mm_min_epi16(_mm_max_epi16(out16_1, kZeros[0]), kx07f);
    outVec[2] = _mm_min_epi16(_mm_max_epi16(out16_2, kZeros[0]), kx07f);
    outVec[3] = _mm_min_epi16(_mm_max_epi16(out16_3, kZeros[0]), kx07f);
  }
}
#elif defined(USE_MMX)
INLINE void affine_txfm(clipped_t *input, void *output, unsigned inDims,
    unsigned outDims, const int32_t *biases, const weight_t *weights,
    mask_t *inMask, mask_t *outMask, const bool pack8_and_calc_mask)
{
  assert(outDims == 32);

#if 0
  const __m64 kZeros[2] = { 0 };
  for (unsigned t = 0; t < 4; t++) {
    __m64 out_0 = ((__m64 *)biases)[4 * t + 0];
    __m64 out_1 = ((__m64 *)biases)[4 * t + 1];
    __m64 out_2 = ((__m64 *)biases)[4 * t + 2];
    __m64 out_3 = ((__m64 *)biases)[4 * t + 3];
    const __m64 *first, *second;
    mask2_t v;
    unsigned idx;

    memcpy(&v, inMask, sizeof(mask2_t));
    for (unsigned offset = 0; offset < inDims;) {
      if (!next_idx(&idx, &offset, &v, inMask, inDims))
        break;
      first = &((__m64 *)&weights[outDims * idx])[2  * t];
      uint32_t factor = input[idx];
      if (next_idx(&idx, &offset, &v, inMask, inDims)) {
        second = &((__m64 *)&weights[outDims * idx])[2 * t];
        factor |= input[idx] << 16;
      } else {
        second = kZeros;
      }
      __m64 mul = _mm_set1_pi32(factor);
      out_0 = _mm_add_pi32(out_0, _mm_madd_pi16(mul, _mm_unpacklo_pi16(first[0],second[0])));
      out_1 = _mm_add_pi32(out_1, _mm_madd_pi16(mul, _mm_unpackhi_pi16(first[0],second[0])));
      out_2 = _mm_add_pi32(out_2, _mm_madd_pi16(mul, _mm_unpacklo_pi16(first[1],second[1])));
      out_3 = _mm_add_pi32(out_3, _mm_madd_pi16(mul, _mm_unpackhi_pi16(first[1],second[1])));
    }

    __m64 out16_0 = _mm_srai_pi16(_mm_packs_pi32(out_0, out_1), SHIFT);
    __m64 out16_1 = _mm_srai_pi16(_mm_packs_pi32(out_2, out_3), SHIFT);

    __m64 *outVec = (__m64 *)output;
    if (pack8_and_calc_mask) {
      outVec[t] = _mm_packs_pi16(out16_0, out16_1);
      outMask[t] = _mm_movemask_pi8(_mm_cmpgt_pi8(outVec[t], kZeros[0]));
    } else {
#ifdef USE_SSE
      const __m64 kx07f = _mm_set1_pi16(127);
      outVec[2 * t] = _mm_min_pi16(_mm_max_pi16(out16_0, kZeros[0]), kx07f);
      outVec[2 * t + 1] = _mm_min_pi16(_mm_max_pi16(out16_1, kZeros[0]), kx07f);
#else
      const __m64 k0x7f80 = _mm_set1_pi16(0x7f80);
      const __m64 k0x0080 = _mm_set1_pi16(0x0080);
      const __m64 k0x8000 = _mm_set1_pi16(-0x8000);
      outVec[2 * t] = _mm_subs_pu16(_mm_add_pi16(_mm_adds_pi16(out16_0, k0x7f80), k0x0080), k0x8000);
      outVec[2 * t + 1] = _mm_subs_pu16(_mm_add_pi16(_mm_adds_pi16(out16_1, k0x7f80), k0x0080), k0x8000);
#endif
    }
  }
#else
  const __m64 kZeros[8] = { 0 };
  __m64 out_0 = ((__m64 *)biases)[0];
  __m64 out_1 = ((__m64 *)biases)[1];
  __m64 out_2 = ((__m64 *)biases)[2];
  __m64 out_3 = ((__m64 *)biases)[3];
  __m64 out_4 = ((__m64 *)biases)[4];
  __m64 out_5 = ((__m64 *)biases)[5];
  __m64 out_6 = ((__m64 *)biases)[6];
  __m64 out_7 = ((__m64 *)biases)[7];
  __m64 out_8 = ((__m64 *)biases)[8];
  __m64 out_9 = ((__m64 *)biases)[9];
  __m64 out_10 = ((__m64 *)biases)[10];
  __m64 out_11 = ((__m64 *)biases)[11];
  __m64 out_12 = ((__m64 *)biases)[12];
  __m64 out_13 = ((__m64 *)biases)[13];
  __m64 out_14 = ((__m64 *)biases)[14];
  __m64 out_15 = ((__m64 *)biases)[15];
  const __m64 *first, *second;
  mask2_t v;
  unsigned idx;

  memcpy(&v, inMask, sizeof(mask2_t));
  for (unsigned offset = 0; offset < inDims;) {
    if (!next_idx(&idx, &offset, &v, inMask, inDims))
      break;
    first = (__m64 *)&weights[outDims * idx];
    uint32_t factor = input[idx];
    if (next_idx(&idx, &offset, &v, inMask, inDims)) {
      second = (__m64 *)&weights[outDims * idx];
      factor |= input[idx] << 16;
    } else {
      second = kZeros;
    }
    __m64 mul = _mm_set1_pi32(factor);
    out_0 = _mm_add_pi32(out_0, _mm_madd_pi16(mul, _mm_unpacklo_pi16(first[0],second[0])));
    out_1 = _mm_add_pi32(out_1, _mm_madd_pi16(mul, _mm_unpackhi_pi16(first[0],second[0])));
    out_2 = _mm_add_pi32(out_2, _mm_madd_pi16(mul, _mm_unpacklo_pi16(first[1],second[1])));
    out_3 = _mm_add_pi32(out_3, _mm_madd_pi16(mul, _mm_unpackhi_pi16(first[1],second[1])));
    out_4 = _mm_add_pi32(out_4, _mm_madd_pi16(mul, _mm_unpacklo_pi16(first[2],second[2])));
    out_5 = _mm_add_pi32(out_5, _mm_madd_pi16(mul, _mm_unpackhi_pi16(first[2],second[2])));
    out_6 = _mm_add_pi32(out_6, _mm_madd_pi16(mul, _mm_unpacklo_pi16(first[3],second[3])));
    out_7 = _mm_add_pi32(out_7, _mm_madd_pi16(mul, _mm_unpackhi_pi16(first[3],second[3])));
    out_8 = _mm_add_pi32(out_8, _mm_madd_pi16(mul, _mm_unpacklo_pi16(first[4],second[4])));
    out_9 = _mm_add_pi32(out_9, _mm_madd_pi16(mul, _mm_unpackhi_pi16(first[4],second[4])));
    out_10 = _mm_add_pi32(out_10, _mm_madd_pi16(mul, _mm_unpacklo_pi16(first[5],second[5])));
    out_11 = _mm_add_pi32(out_11, _mm_madd_pi16(mul, _mm_unpackhi_pi16(first[5],second[5])));
    out_12 = _mm_add_pi32(out_12, _mm_madd_pi16(mul, _mm_unpacklo_pi16(first[6],second[6])));
    out_13 = _mm_add_pi32(out_13, _mm_madd_pi16(mul, _mm_unpackhi_pi16(first[6],second[6])));
    out_14 = _mm_add_pi32(out_14, _mm_madd_pi16(mul, _mm_unpacklo_pi16(first[7],second[7])));
    out_15 = _mm_add_pi32(out_15, _mm_madd_pi16(mul, _mm_unpackhi_pi16(first[7],second[7])));
  }

  __m64 out16_0 = _mm_srai_pi16(_mm_packs_pi32(out_0, out_1), SHIFT);
  __m64 out16_1 = _mm_srai_pi16(_mm_packs_pi32(out_2, out_3), SHIFT);
  __m64 out16_2 = _mm_srai_pi16(_mm_packs_pi32(out_4, out_5), SHIFT);
  __m64 out16_3 = _mm_srai_pi16(_mm_packs_pi32(out_6, out_7), SHIFT);
  __m64 out16_4 = _mm_srai_pi16(_mm_packs_pi32(out_8, out_9), SHIFT);
  __m64 out16_5 = _mm_srai_pi16(_mm_packs_pi32(out_10, out_11), SHIFT);
  __m64 out16_6 = _mm_srai_pi16(_mm_packs_pi32(out_12, out_13), SHIFT);
  __m64 out16_7 = _mm_srai_pi16(_mm_packs_pi32(out_14, out_15), SHIFT);

  __m64 *outVec = (__m64 *)output;
  if (pack8_and_calc_mask) {
    outVec[0] = _mm_packs_pi16(out16_0, out16_1);
    outMask[0] = _mm_movemask_pi8(_mm_cmpgt_pi8(outVec[0], kZeros[0]));
    outVec[1] = _mm_packs_pi16(out16_2, out16_3);
    outMask[1] = _mm_movemask_pi8(_mm_cmpgt_pi8(outVec[1], kZeros[0]));
    outVec[2] = _mm_packs_pi16(out16_4, out16_5);
    outMask[2] = _mm_movemask_pi8(_mm_cmpgt_pi8(outVec[2], kZeros[0]));
    outVec[3] = _mm_packs_pi16(out16_6, out16_7);
    outMask[3] = _mm_movemask_pi8(_mm_cmpgt_pi8(outVec[3], kZeros[0]));
  } else {
#ifdef USE_SSE
    const __m64 kx07f = _mm_set1_pi16(127);
    outVec[0] = _mm_min_pi16(_mm_max_pi16(out16_0, kZeros[0]), kx07f);
    outVec[1] = _mm_min_pi16(_mm_max_pi16(out16_1, kZeros[0]), kx07f);
    outVec[2] = _mm_min_pi16(_mm_max_pi16(out16_2, kZeros[0]), kx07f);
    outVec[3] = _mm_min_pi16(_mm_max_pi16(out16_3, kZeros[0]), kx07f);
    outVec[4] = _mm_min_pi16(_mm_max_pi16(out16_4, kZeros[0]), kx07f);
    outVec[5] = _mm_min_pi16(_mm_max_pi16(out16_5, kZeros[0]), kx07f);
    outVec[6] = _mm_min_pi16(_mm_max_pi16(out16_6, kZeros[0]), kx07f);
    outVec[7] = _mm_min_pi16(_mm_max_pi16(out16_7, kZeros[0]), kx07f);
#else
    const __m64 k0x7f80 = _mm_set1_pi16(0x7f80);
    const __m64 k0x0080 = _mm_set1_pi16(0x0080);
    const __m64 k0x8000 = _mm_set1_pi16(-0x8000);
    outVec[0] = _mm_subs_pu16(_mm_add_pi16(_mm_adds_pi16(out16_0, k0x7f80), k0x0080), k0x8000);
    outVec[1] = _mm_subs_pu16(_mm_add_pi16(_mm_adds_pi16(out16_1, k0x7f80), k0x0080), k0x8000);
    outVec[2] = _mm_subs_pu16(_mm_add_pi16(_mm_adds_pi16(out16_2, k0x7f80), k0x0080), k0x8000);
    outVec[3] = _mm_subs_pu16(_mm_add_pi16(_mm_adds_pi16(out16_3, k0x7f80), k0x0080), k0x8000);
    outVec[4] = _mm_subs_pu16(_mm_add_pi16(_mm_adds_pi16(out16_4, k0x7f80), k0x0080), k0x8000);
    outVec[5] = _mm_subs_pu16(_mm_add_pi16(_mm_adds_pi16(out16_5, k0x7f80), k0x0080), k0x8000);
    outVec[6] = _mm_subs_pu16(_mm_add_pi16(_mm_adds_pi16(out16_6, k0x7f80), k0x0080), k0x8000);
    outVec[7] = _mm_subs_pu16(_mm_add_pi16(_mm_adds_pi16(out16_7, k0x7f80), k0x0080), k0x8000);
#endif
  }
#endif
}
#elif defined(USE_NEON)
INLINE void affine_txfm(clipped_t *input, void *output, unsigned inDims,
    unsigned outDims, const int32_t *biases, const weight_t *weights,
    mask_t *inMask, mask_t *outMask, const bool pack8_and_calc_mask)
{
  assert(outDims == 32);

  int32x4_t out_0 = ((int32x4_t *)biases)[0];
  int32x4_t out_1 = ((int32x4_t *)biases)[1];
  int32x4_t out_2 = ((int32x4_t *)biases)[2];
  int32x4_t out_3 = ((int32x4_t *)biases)[3];
  int32x4_t out_4 = ((int32x4_t *)biases)[4];
  int32x4_t out_5 = ((int32x4_t *)biases)[5];
  int32x4_t out_6 = ((int32x4_t *)biases)[6];
  int32x4_t out_7 = ((int32x4_t *)biases)[7];
  const int8x8_t *first;
  mask2_t v;
  unsigned idx;

  memcpy(&v, inMask, sizeof(mask2_t));
  for (unsigned offset = 0; offset < inDims;) {
    if (!next_idx(&idx, &offset, &v, inMask, inDims))
      break;
    first = (int8x8_t *)&weights[outDims * idx];
    int16_t factor = input[idx];

    int16x8_t prod;
    prod = vmulq_n_s16(vmovl_s8(first[0]), factor);
    out_0 = vaddq_s32(out_0, vmovl_s16(vget_low_s16(prod)));
    out_1 = vaddq_s32(out_1, vmovl_high_s16(prod));
    prod = vmulq_n_s16(vmovl_s8(first[1]), factor);
    out_2 = vaddq_s32(out_2, vmovl_s16(vget_low_s16(prod)));
    out_3 = vaddq_s32(out_3, vmovl_high_s16(prod));
    prod = vmulq_n_s16(vmovl_s8(first[2]), factor);
    out_4 = vaddq_s32(out_4, vmovl_s16(vget_low_s16(prod)));
    out_5 = vaddq_s32(out_5, vmovl_high_s16(prod));
    prod = vmulq_n_s16(vmovl_s8(first[3]), factor);
    out_6 = vaddq_s32(out_6, vmovl_s16(vget_low_s16(prod)));
    out_7 = vaddq_s32(out_7, vmovl_high_s16(prod));
  }

  int16x8_t out16_0 = vcombine_s16(vqshrn_n_s32(out_0, SHIFT), vqshrn_n_s32(out_1, SHIFT));
  int16x8_t out16_1 = vcombine_s16(vqshrn_n_s32(out_2, SHIFT), vqshrn_n_s32(out_3, SHIFT));
  int16x8_t out16_2 = vcombine_s16(vqshrn_n_s32(out_4, SHIFT), vqshrn_n_s32(out_5, SHIFT));
  int16x8_t out16_3 = vcombine_s16(vqshrn_n_s32(out_6, SHIFT), vqshrn_n_s32(out_7, SHIFT));

  if (pack8_and_calc_mask) {
    const int8x16_t kZero = { 0 };
    int8x16_t *outVec = (int8x16_t *)output;
    outVec[0] = vcombine_s8(vqmovn_s16(out16_0), vqmovn_s16(out16_1));
    outMask[0] = neon_movemask(vcgtq_s8(outVec[0], kZero));
    outVec[1] = vcombine_s8(vqmovn_s16(out16_2), vqmovn_s16(out16_3));
    outMask[1] = neon_movemask(vcgtq_s8(outVec[1], kZero));
  } else {
    // The next step takes int8x8_t as input, so store as int8x8_t
    const int8x8_t kZero = { 0 };
    int8x8_t *outVec = (int8x8_t *)output;
    outVec[0] = vmax_s8(vqmovn_s16(out16_0), kZero);
    outVec[1] = vmax_s8(vqmovn_s16(out16_1), kZero);
    outVec[2] = vmax_s8(vqmovn_s16(out16_2), kZero);
    outVec[3] = vmax_s8(vqmovn_s16(out16_3), kZero);
  }
}
#else /* generic fallback */
INLINE void affine_txfm(clipped_t *input, void *output, unsigned inDims,
    unsigned outDims, int32_t *biases, const weight_t *weights,
    mask_t *inMask, mask_t *outMask, const bool pack8_and_calc_mask)
{
  (void)inMask; (void)outMask; (void)pack8_and_calc_mask;

  int32_t tmp[outDims];

  for (unsigned i = 0; i < outDims; i++)
    tmp[i] = biases[i];

  for (unsigned idx = 0; idx < inDims; idx++)
    if (input[idx])
      for (unsigned i = 0; i < outDims; i++)
        tmp[i] += (int8_t)input[idx] * weights[outDims * idx + i];

  clipped_t *outVec = (clipped_t *)output;
  for (unsigned i = 0; i < outDims; i++)
    outVec[i] = clamp(tmp[i] >> SHIFT, 0, 127);
}
#endif

// Input feature converter
static int16_t ft_biases alignas(64) [kHalfDimensions];
static int16_t ft_weights alignas(64) [kHalfDimensions * FtInDims];

// ---------------------------------------------------------------------------
//  SFNNv13 HalfKAv2 Weight / Bias Arrays (Big + Small networks)
//  Dynamically allocated via pointers — parsed by init_weights_sfnnv13()
// ---------------------------------------------------------------------------

// Big network transformer
static int16_t* sfnnv13_big_ft_biases = nullptr;
static int16_t* sfnnv13_big_ft_weights = nullptr;

// Big network layers
static int32_t* sfnnv13_big_l1_biases = nullptr;
static int16_t* sfnnv13_big_l1_weights = nullptr;
static int32_t* sfnnv13_big_l2_biases = nullptr;
static int8_t*  sfnnv13_big_l2_weights = nullptr;
static int32_t* sfnnv13_big_out_biases = nullptr;
static int8_t*  sfnnv13_big_out_weights = nullptr;
static uint32_t sfnnv13_big_l1Size = 0;
static uint32_t sfnnv13_big_l2Size = 0;

// Small network transformer
static int16_t* sfnnv13_small_ft_biases = nullptr;
static int16_t* sfnnv13_small_ft_weights = nullptr;

// Small network layers
static int32_t* sfnnv13_small_l1_biases = nullptr;
static int16_t* sfnnv13_small_l1_weights = nullptr;
static int32_t* sfnnv13_small_l2_biases = nullptr;
static int8_t*  sfnnv13_small_l2_weights = nullptr;
static int32_t* sfnnv13_small_out_biases = nullptr;
static int8_t*  sfnnv13_small_out_weights = nullptr;
static uint32_t sfnnv13_small_l1Size = 0;
static uint32_t sfnnv13_small_l2Size = 0;

#ifdef VECTOR
#define TILE_HEIGHT (NUM_REGS * SIMD_WIDTH / 16)
#endif

// Calculate cumulative value without using difference calculation
INLINE void refresh_accumulator(Position *pos)
{
  Accumulator *accumulator = &(pos->nnue[0]->accumulator);

  IndexList activeIndices[2];
  activeIndices[0].size = activeIndices[1].size = 0;
  append_active_indices(pos, activeIndices);

  for (unsigned c = 0; c < 2; c++) {
#ifdef VECTOR
    for (unsigned i = 0; i < kHalfDimensions / TILE_HEIGHT; i++) {
      vec16_t *ft_biases_tile = (vec16_t *)&ft_biases[i * TILE_HEIGHT];
      vec16_t *accTile = (vec16_t *)&accumulator->accumulation[c][i * TILE_HEIGHT];
      vec16_t acc[NUM_REGS];

      for (unsigned j = 0; j < NUM_REGS; j++)
        acc[j] = ft_biases_tile[j];

      for (size_t k = 0; k < activeIndices[c].size; k++) {
        unsigned index = activeIndices[c].values[k];
        unsigned offset = kHalfDimensions * index + i * TILE_HEIGHT;
        vec16_t *column = (vec16_t *)&ft_weights[offset];

        for (unsigned j = 0; j < NUM_REGS; j++)
          acc[j] = vec_add_16(acc[j], column[j]);
      }

      for (unsigned j = 0; j < NUM_REGS; j++)
        accTile[j] = acc[j];
    }
#else
    memcpy(accumulator->accumulation[c], ft_biases,
        kHalfDimensions * sizeof(int16_t));

    for (size_t k = 0; k < activeIndices[c].size; k++) {
      unsigned index = activeIndices[c].values[k];
      unsigned offset = kHalfDimensions * index;

      for (unsigned j = 0; j < kHalfDimensions; j++)
        accumulator->accumulation[c][j] += ft_weights[offset + j];
    }
#endif
  }

  accumulator->computedAccumulation = 1;
}

// Calculate cumulative value using difference calculation if possible
INLINE bool update_accumulator(Position *pos)
{
  Accumulator *accumulator = &(pos->nnue[0]->accumulator);
  if (accumulator->computedAccumulation)
    return true;

  Accumulator *prevAcc;
  if (   (!pos->nnue[1] || !(prevAcc = &pos->nnue[1]->accumulator)->computedAccumulation)
      && (!pos->nnue[2] || !(prevAcc = &pos->nnue[2]->accumulator)->computedAccumulation) )
    return false;

  IndexList removed_indices[2], added_indices[2];
  removed_indices[0].size = removed_indices[1].size = 0;
  added_indices[0].size = added_indices[1].size = 0;
  bool reset[2];
  append_changed_indices(pos, removed_indices, added_indices, reset);

#ifdef VECTOR
  for (unsigned i = 0; i< kHalfDimensions / TILE_HEIGHT; i++) {
    for (unsigned c = 0; c < 2; c++) {
      vec16_t *accTile = (vec16_t *)&accumulator->accumulation[c][i * TILE_HEIGHT];
      vec16_t acc[NUM_REGS];

      if (reset[c]) {
        vec16_t *ft_b_tile = (vec16_t *)&ft_biases[i * TILE_HEIGHT];
        for (unsigned j = 0; j < NUM_REGS; j++)
          acc[j] = ft_b_tile[j];
      } else {
        vec16_t *prevAccTile = (vec16_t *)&prevAcc->accumulation[c][i * TILE_HEIGHT];
        for (unsigned j = 0; j < NUM_REGS; j++)
          acc[j] = prevAccTile[j];

        // Difference calculation for the deactivated features
        for (unsigned k = 0; k < removed_indices[c].size; k++) {
          unsigned index = removed_indices[c].values[k];
          const unsigned offset = kHalfDimensions * index + i * TILE_HEIGHT;

          vec16_t *column = (vec16_t *)&ft_weights[offset];
          for (unsigned j = 0; j < NUM_REGS; j++)
            acc[j] = vec_sub_16(acc[j], column[j]);
        }
      }

      // Difference calculation for the activated features
      for (unsigned k = 0; k < added_indices[c].size; k++) {
        unsigned index = added_indices[c].values[k];
        const unsigned offset = kHalfDimensions * index + i * TILE_HEIGHT;

        vec16_t *column = (vec16_t *)&ft_weights[offset];
        for (unsigned j = 0; j < NUM_REGS; j++)
          acc[j] = vec_add_16(acc[j], column[j]);
      }

      for (unsigned j = 0; j < NUM_REGS; j++)
        accTile[j] = acc[j];
    }
  }
#else
  for (unsigned c = 0; c < 2; c++) {
    if (reset[c]) {
      memcpy(accumulator->accumulation[c], ft_biases,
          kHalfDimensions * sizeof(int16_t));
    } else {
      memcpy(accumulator->accumulation[c], prevAcc->accumulation[c],
          kHalfDimensions * sizeof(int16_t));
      // Difference calculation for the deactivated features
      for (unsigned k = 0; k < removed_indices[c].size; k++) {
        unsigned index = removed_indices[c].values[k];
        const unsigned offset = kHalfDimensions * index;

        for (unsigned j = 0; j < kHalfDimensions; j++)
          accumulator->accumulation[c][j] -= ft_weights[offset + j];
      }
    }

    // Difference calculation for the activated features
    for (unsigned k = 0; k < added_indices[c].size; k++) {
      unsigned index = added_indices[c].values[k];
      const unsigned offset = kHalfDimensions * index;

      for (unsigned j = 0; j < kHalfDimensions; j++)
        accumulator->accumulation[c][j] += ft_weights[offset + j];
    }
  }
#endif

  accumulator->computedAccumulation = 1;
  return true;
}

// Convert input features
INLINE void transform(Position *pos, clipped_t *output, mask_t *outMask)
{
  if (!update_accumulator(pos))
    refresh_accumulator(pos);

  int16_t (*accumulation)[2][3072] = &pos->nnue[0]->accumulator.accumulation;
  (void)outMask; // avoid compiler warning

  const int perspectives[2] = { pos->player, !pos->player };
  for (unsigned p = 0; p < 2; p++) {
    const unsigned offset = kHalfDimensions * p;

#ifdef VECTOR
    const unsigned numChunks = (16 * kHalfDimensions) / SIMD_WIDTH;
    vec8_t *out = (vec8_t *)&output[offset];
    for (unsigned i = 0; i < numChunks / 2; i++) {
      vec16_t s0 = ((vec16_t *)(*accumulation)[perspectives[p]])[i * 2];
      vec16_t s1 = ((vec16_t *)(*accumulation)[perspectives[p]])[i * 2 + 1];
      out[i] = vec_packs(s0, s1);
      *outMask++ = vec_mask_pos(out[i]);
    }

#else
    for (unsigned i = 0; i < kHalfDimensions; i++) {
      int16_t sum = (*accumulation)[perspectives[p]][i];
      output[offset + i] = clamp(sum, 0, 127);
    }

#endif

  }
}

struct NetData {
  alignas(64) clipped_t input[FtOutDims];
  clipped_t hidden1_out[32];
#if (defined(USE_SSE2) || defined(USE_MMX)) && !defined(USE_AVX2)
  int16_t hidden2_out[32];
#else
  int8_t hidden2_out[32];
#endif
};

// ---------------------------------------------------------------------------
//  SFNNv13 HalfKAv2 Scalar Evaluation (Big network)
//  Pure C — no SIMD — for correctness verification
// ---------------------------------------------------------------------------

// Count set bits in a 64-bit integer (portable popcount)
static inline int popcount64(uint64_t x)
{
  int c = 0;
  while (x) { x &= x - 1; c++; }
  return c;
}

// Compute king bucket index for a given perspective.
// HalfKAv2 uses the king square and occupancy to select which
// transformer bias slice to use.
static inline uint32_t sfnnv13_king_bucket(int kingSquare, uint64_t occupiedBB, uint32_t numBuckets)
{
  // Orient king square from white's perspective
  int rank = kingSquare / 8;
  int file = kingSquare % 8;
  uint32_t bucket = (uint32_t)(kingSquare * 128 + (rank * 8 + file) * 64 + popcount64(occupiedBB));
  return bucket % numBuckets;
}

// Refresh accumulator for SFNNv13 Big network (scalar)
static void refresh_accumulator_sfnnv13_big(Position *pos)
{
  Accumulator *accumulator = &(pos->nnue[0]->accumulator);

  // Build occupied bitboard from piece list
  uint64_t occupiedBB = 0;
  for (int i = 0; pos->pieces[i]; i++)
    occupiedBB |= (1ULL << pos->squares[i]);

  // Find king squares for both colors
  int wkSquare = -1, bkSquare = -1;
  for (int i = 0; pos->pieces[i]; i++) {
    if (pos->pieces[i] == wking) wkSquare = pos->squares[i];
    if (pos->pieces[i] == bking) bkSquare = pos->squares[i];
  }

  if (wkSquare < 0 || bkSquare < 0) {
    accumulator->computedAccumulation = 0;
    return;
  }

  uint32_t halfDims = SFNNv13_Big_HalfDims;
  uint32_t kingBuckets = SFNNv13_Big_KingBuckets;

  // White perspective (c=0): king = white king
  {
    uint32_t bucket = sfnnv13_king_bucket(wkSquare, occupiedBB, kingBuckets);
    // Copy biases for this king bucket
    for (uint32_t d = 0; d < halfDims; d++)
      accumulator->accumulation[0][d] = sfnnv13_big_ft_biases[d * kingBuckets + bucket];

    // Accumulate weights for all non-king pieces
    for (int i = 0; pos->pieces[i]; i++) {
      int pc = pos->pieces[i];
      if (pc == wking || pc == bking) continue;

      // HalfKAv2 feature index: (pc - 1) * 64 + square
      uint32_t featureIdx = (uint32_t)((pc - 1) * 64 + pos->squares[i]);
      for (uint32_t d = 0; d < halfDims; d++)
        accumulator->accumulation[0][d] += sfnnv13_big_ft_weights[d * SFNNv13_Big_FtInDims + featureIdx];
    }
  }

  // Black perspective (c=1): king = black king
  // In HalfKAv2, black perspective mirrors squares vertically
  {
    uint32_t bucket = sfnnv13_king_bucket(bkSquare, occupiedBB, kingBuckets);
    for (uint32_t d = 0; d < halfDims; d++)
      accumulator->accumulation[1][d] = sfnnv13_big_ft_biases[d * kingBuckets + bucket];

    for (int i = 0; pos->pieces[i]; i++) {
      int pc = pos->pieces[i];
      if (pc == wking || pc == bking) continue;

      // Mirror square for black perspective
      int mirroredSq = pos->squares[i] ^ 0x38; // flip rank (sq ^ 56)
      // For black perspective, swap piece color: white pieces become black and vice versa
      int blackPC = (pc <= 6) ? (pc + 6) : (pc - 6);
      uint32_t featureIdx = (uint32_t)((blackPC - 1) * 64 + mirroredSq);
      for (uint32_t d = 0; d < halfDims; d++)
        accumulator->accumulation[1][d] += sfnnv13_big_ft_weights[d * SFNNv13_Big_FtInDims + featureIdx];
    }
  }

  accumulator->computedAccumulation = 1;
}

// Clipped ReLU: clamp(x >> SHIFT, 0, 127)
static inline int8_t clipped_relu(int32_t x)
{
  int32_t v = x >> SHIFT;
  if (v < 0) return 0;
  if (v > 127) return 127;
  return (int8_t)v;
}

// Full evaluation for SFNNv13 Big network (scalar)
static int nnue_evaluate_pos_sfnnv13_big(Position *pos)
{
  Accumulator *accumulator = &(pos->nnue[0]->accumulator);

  // Refresh if needed
  if (!accumulator->computedAccumulation)
    refresh_accumulator_sfnnv13_big(pos);

  if (!accumulator->computedAccumulation)
    return 0; // fallback

  uint32_t halfDims = sfnnv13_big_l1Size ? sfnnv13_big_l1Size : SFNNv13_Big_HalfDims;
  // Actually, halfDims for the accumulator is SFNNv13_Big_HalfDims (3072)
  // l1Size and l2Size come from the network header
  uint32_t l1Size = sfnnv13_big_l1Size;
  uint32_t l2Size = sfnnv13_big_l2Size;

  if (l1Size == 0 || l2Size == 0)
    return 0; // network not fully loaded

  // Use side-to-move perspective
  int perspective = pos->player;

  // Temporary buffers for layer outputs
  int8_t *l1_input = (int8_t*)malloc(halfDims * sizeof(int8_t));
  int32_t *l1_output = (int32_t*)malloc(l1Size * sizeof(int32_t));
  int8_t *l2_input = (int8_t*)malloc(l1Size * sizeof(int8_t));
  int32_t *l2_output = (int32_t*)malloc(l2Size * sizeof(int32_t));

  if (!l1_input || !l1_output || !l2_input || !l2_output) {
    free(l1_input); free(l1_output); free(l2_input); free(l2_output);
    return 0;
  }

  // Step 1: Clipped ReLU on accumulator → l1 input
  for (uint32_t i = 0; i < halfDims; i++)
    l1_input[i] = clipped_relu(accumulator->accumulation[perspective][i]);

  // Step 2: L1 affine transform (l1_weights is [halfDims][l1Size], column-major)
  for (uint32_t j = 0; j < l1Size; j++) {
    int32_t sum = sfnnv13_big_l1_biases[j];
    for (uint32_t i = 0; i < halfDims; i++)
      sum += (int32_t)l1_input[i] * sfnnv13_big_l1_weights[i * l1Size + j];
    l1_output[j] = sum;
  }

  // Step 3: Clipped ReLU on L1 → l2 input
  for (uint32_t i = 0; i < l1Size; i++)
    l2_input[i] = clipped_relu(l1_output[i]);

  // Step 4: L2 affine transform (l2_weights is [l2Size][l1Size], row-major)
  for (uint32_t j = 0; j < l2Size; j++) {
    int32_t sum = sfnnv13_big_l2_biases[j];
    for (uint32_t i = 0; i < l1Size; i++)
      sum += (int32_t)l2_input[i] * sfnnv13_big_l2_weights[j * l1Size + i];
    l2_output[j] = sum;
  }

  // Step 5: Clipped ReLU on L2
  for (uint32_t i = 0; i < l2Size; i++)
    l2_input[i] = clipped_relu(l2_output[i]);

  // Step 6: Output affine transform
  int32_t out_value = 0;
  for (uint32_t j = 0; j < l2Size; j++) {
    out_value += sfnnv13_big_out_biases[j];
    out_value += (int32_t)l2_input[j] * sfnnv13_big_out_weights[j];
  }

  free(l1_input); free(l1_output); free(l2_input); free(l2_output);

  return out_value / FV_SCALE;
}

// ---------------------------------------------------------------------------
//  SFNNv13 HalfKAv2 Scalar Evaluation (Small network)
// ---------------------------------------------------------------------------

static void refresh_accumulator_sfnnv13_small(Position *pos)
{
  Accumulator *accumulator = &(pos->nnue[0]->accumulator);

  uint64_t occupiedBB = 0;
  for (int i = 0; pos->pieces[i]; i++)
    occupiedBB |= (1ULL << pos->squares[i]);

  int wkSquare = -1, bkSquare = -1;
  for (int i = 0; pos->pieces[i]; i++) {
    if (pos->pieces[i] == wking) wkSquare = pos->squares[i];
    if (pos->pieces[i] == bking) bkSquare = pos->squares[i];
  }

  if (wkSquare < 0 || bkSquare < 0) {
    accumulator->computedAccumulation = 0;
    return;
  }

  uint32_t halfDims = SFNNv13_Small_HalfDims;
  uint32_t kingBuckets = SFNNv13_Small_KingBuckets;

  // White perspective
  {
    uint32_t bucket = sfnnv13_king_bucket(wkSquare, occupiedBB, kingBuckets);
    for (uint32_t d = 0; d < halfDims; d++)
      accumulator->accumulation[0][d] = sfnnv13_small_ft_biases[d * kingBuckets + bucket];

    for (int i = 0; pos->pieces[i]; i++) {
      int pc = pos->pieces[i];
      if (pc == wking || pc == bking) continue;
      uint32_t featureIdx = (uint32_t)((pc - 1) * 64 + pos->squares[i]);
      for (uint32_t d = 0; d < halfDims; d++)
        accumulator->accumulation[0][d] += sfnnv13_small_ft_weights[d * SFNNv13_Small_FtInDims + featureIdx];
    }
  }

  // Black perspective
  {
    uint32_t bucket = sfnnv13_king_bucket(bkSquare, occupiedBB, kingBuckets);
    for (uint32_t d = 0; d < halfDims; d++)
      accumulator->accumulation[1][d] = sfnnv13_small_ft_biases[d * kingBuckets + bucket];

    for (int i = 0; pos->pieces[i]; i++) {
      int pc = pos->pieces[i];
      if (pc == wking || pc == bking) continue;
      int mirroredSq = pos->squares[i] ^ 0x38;
      int blackPC = (pc <= 6) ? (pc + 6) : (pc - 6);
      uint32_t featureIdx = (uint32_t)((blackPC - 1) * 64 + mirroredSq);
      for (uint32_t d = 0; d < halfDims; d++)
        accumulator->accumulation[1][d] += sfnnv13_small_ft_weights[d * SFNNv13_Small_FtInDims + featureIdx];
    }
  }

  accumulator->computedAccumulation = 1;
}

static int nnue_evaluate_pos_sfnnv13_small(Position *pos)
{
  Accumulator *accumulator = &(pos->nnue[0]->accumulator);

  if (!accumulator->computedAccumulation)
    refresh_accumulator_sfnnv13_small(pos);

  if (!accumulator->computedAccumulation)
    return 0;

  uint32_t halfDims = SFNNv13_Small_HalfDims;
  uint32_t l1Size = sfnnv13_small_l1Size;
  uint32_t l2Size = sfnnv13_small_l2Size;

  if (l1Size == 0 || l2Size == 0)
    return 0;

  int perspective = pos->player;

  int8_t *l1_input = (int8_t*)malloc(halfDims * sizeof(int8_t));
  int32_t *l1_output = (int32_t*)malloc(l1Size * sizeof(int32_t));
  int8_t *l2_input = (int8_t*)malloc(l1Size * sizeof(int8_t));
  int32_t *l2_output = (int32_t*)malloc(l2Size * sizeof(int32_t));

  if (!l1_input || !l1_output || !l2_input || !l2_output) {
    free(l1_input); free(l1_output); free(l2_input); free(l2_output);
    return 0;
  }

  for (uint32_t i = 0; i < halfDims; i++)
    l1_input[i] = clipped_relu(accumulator->accumulation[perspective][i]);

  for (uint32_t j = 0; j < l1Size; j++) {
    int32_t sum = sfnnv13_small_l1_biases[j];
    for (uint32_t i = 0; i < halfDims; i++)
      sum += (int32_t)l1_input[i] * sfnnv13_small_l1_weights[i * l1Size + j];
    l1_output[j] = sum;
  }

  for (uint32_t i = 0; i < l1Size; i++)
    l2_input[i] = clipped_relu(l1_output[i]);

  for (uint32_t j = 0; j < l2Size; j++) {
    int32_t sum = sfnnv13_small_l2_biases[j];
    for (uint32_t i = 0; i < l1Size; i++)
      sum += (int32_t)l2_input[i] * sfnnv13_small_l2_weights[j * l1Size + i];
    l2_output[j] = sum;
  }

  for (uint32_t i = 0; i < l2Size; i++)
    l2_input[i] = clipped_relu(l2_output[i]);

  int32_t out_value = 0;
  for (uint32_t j = 0; j < l2Size; j++) {
    out_value += sfnnv13_small_out_biases[j];
    out_value += (int32_t)l2_input[j] * sfnnv13_small_out_weights[j];
  }

  free(l1_input); free(l1_output); free(l2_input); free(l2_output);

  return out_value / FV_SCALE;
}

// Evaluation function
int nnue_evaluate_pos(Position *pos)
{
  if (g_NetworkArch == ARCH_SFNNv13_Big) return nnue_evaluate_pos_sfnnv13_big(pos);
  if (g_NetworkArch == ARCH_SFNNv13_Small) return nnue_evaluate_pos_sfnnv13_small(pos);

  int32_t out_value;
  alignas(8) mask_t input_mask[FtOutDims / (8 * sizeof(mask_t))];
  alignas(8) mask_t hidden1_mask[8 / sizeof(mask_t)] = { 0 };
#ifdef ALIGNMENT_HACK // work around a bug in old gcc on Windows
  uint8_t buf[sizeof(struct NetData) + 63];
  struct NetData *b = (struct NetData *)(buf + ((((uintptr_t)buf-1) ^ 0x3f) & 0x3f));
#define B(x) (b->x)
#else
  struct NetData buf;
#define B(x) (buf.x)
#endif

  transform(pos, B(input), input_mask);

  affine_txfm(B(input), B(hidden1_out), FtOutDims, 32,
      hidden1_biases, hidden1_weights, input_mask, hidden1_mask, true);

  affine_txfm(B(hidden1_out), B(hidden2_out), 32, 32,
      hidden2_biases, hidden2_weights, hidden1_mask, NULL, false);

  out_value = affine_propagate((int8_t *)B(hidden2_out), output_biases,
      output_weights);

#if defined(USE_MMX)
  _mm_empty();
#endif

  return out_value / FV_SCALE;
}

static void read_output_weights(weight_t *w, const char *d)
{
  for (unsigned i = 0; i < 32; i++) {
    unsigned c = i;
#if defined(USE_AVX512)
    unsigned b = c & 0x18;
    b = (b << 1) | (b >> 1);
    c = (c & ~0x18) | (b & 0x18);
#endif
    w[c] = *d++;
  }
}

INLINE unsigned wt_idx(unsigned r, unsigned c, unsigned dims)
{
  (void)dims;

#if defined(USE_AVX512)
  if (dims > 32) {
    unsigned b = c & 0x38;
    b = (b << 1) | (b >> 2);
    c = (c & ~0x38) | (b & 0x38);
  }
  else if (dims == 32) {
    unsigned b = c & 0x18;
    b = (b << 1) | (b >> 1);
    c = (c & ~0x18) | (b & 0x18);
  }

#elif defined(USE_AVX2)
  if (dims > 32) {
    unsigned b = c & 0x18;
    b = (b << 1) | (b >> 1);
    c = (c & ~0x18) | (b & 0x18);
  }

#endif

#if defined(USE_AVX512)
  return c * 64 + r + (r & ~7);

#else
  return c * 32 + r;

#endif
}

static const char *read_hidden_weights(weight_t *w, unsigned dims,
    const char *d)
{
  for (unsigned r = 0; r < 32; r++)
    for (unsigned c = 0; c < dims; c++)
      w[wt_idx(r, c, dims)] = *d++;

  return d;
}

#ifdef USE_AVX2
static void permute_biases(int32_t *biases)
{
  __m128i *b = (__m128i *)biases;
  __m128i tmp[8];
#ifdef USE_AVX512
  tmp[0] = b[0];
  tmp[1] = b[2];
  tmp[2] = b[4];
  tmp[3] = b[6];
  tmp[4] = b[1];
  tmp[5] = b[3];
  tmp[6] = b[5];
  tmp[7] = b[7];
#elif USE_AVX2
  tmp[0] = b[0];
  tmp[1] = b[4];
  tmp[2] = b[1];
  tmp[3] = b[5];
  tmp[4] = b[2];
  tmp[5] = b[6];
  tmp[6] = b[3];
  tmp[7] = b[7];
#else
#error
#endif
  memcpy(b, tmp, 8 * sizeof(__m128i));
}
#endif

enum {
  TransformerStart = 3 * 4 + 177,
  NetworkStart = TransformerStart + 4 + 2 * 256 + 2 * 256 * 64 * 641
};

// ---------------------------------------------------------------------------
//  SFNNv13 HalfKAv2 Binary Header Structures
//  These describe the on-disk layout of modern Stockfish networks.
// ---------------------------------------------------------------------------

// SFNNv13 header: 4 bytes version + 4 bytes hashLow + 4 bytes hashHigh + N bytes features
typedef struct {
  uint32_t version;       // 0x7AF32F16
  uint32_t hashLow;       // HalfKAv2 signature
  uint32_t hashHigh;      // HalfKAv2 signature
  uint32_t featureSize;   // e.g. 768 for HalfKAv2
} SFNNv13_Header;

// SFNNv13 transformer section header
typedef struct {
  uint32_t magic;         // 0x8774dd14 for HalfKAv2
  uint32_t halfDims;      // 3072 (Big) or 1536 (Small)
  uint32_t kingBuckets;   // 8192 (Big) or 1024 (Small)
} SFNNv13_TransformerHeader;

// SFNNv13 network section header
typedef struct {
  uint32_t magic;         // 0x63337156
  uint32_t l1Size;        // hidden layer 1 size
  uint32_t l2Size;        // hidden layer 2 size
} SFNNv13_NetworkHeader;

static bool verify_net(const void *evalData, size_t size)
{
  const char *d = (const char*)evalData;

  // Legacy HalfKP network
  if (size == HalfKP_Legacy_Size) {
    if (readu_le_u32(d) != NnueVersion) return false;
    if (readu_le_u32(d + 4) != 0x3e5aa6eeU) return false;
    if (readu_le_u32(d + 8) != 177) return false;
    if (readu_le_u32(d + TransformerStart) != 0x5d69d7b8) return false;
    if (readu_le_u32(d + NetworkStart) != 0x63337156) return false;
    g_NetworkArch = ARCH_HalfKP;
    return true;
  }

  // SFNNv13 Big network (HalfKAv2)
  if (size == SFNNv13_Big_Size) {
    g_NetworkArch = ARCH_SFNNv13_Big;
    printf("  Detected: SFNNv13 Big (HalfKAv2, %d-dim, %d king buckets)\n",
           SFNNv13_Big_HalfDims, SFNNv13_Big_KingBuckets);
    fflush(stdout);
    return true;
  }

  // SFNNv13 Small network (HalfKAv2)
  if (size == SFNNv13_Small_Size) {
    g_NetworkArch = ARCH_SFNNv13_Small;
    printf("  Detected: SFNNv13 Small (HalfKAv2, %d-dim, %d king buckets)\n",
           SFNNv13_Small_HalfDims, SFNNv13_Small_KingBuckets);
    fflush(stdout);
    return true;
  }

  printf("Warning: Unknown network size (%zu bytes). Bypassing weight initialization.\n", size);
  fflush(stdout);
  g_NetworkArch = ARCH_UNKNOWN;
  return false;
}

static void init_weights(const void *evalData)
{
  const char *d = (const char *)evalData + TransformerStart + 4;

  // Read transformer
  for (unsigned i = 0; i < kHalfDimensions; i++, d += 2)
    ft_biases[i] = readu_le_u16(d);
  for (unsigned i = 0; i < kHalfDimensions * FtInDims; i++, d += 2)
    ft_weights[i] = readu_le_u16(d);

  // Read network
  d += 4;
  for (unsigned i = 0; i < 32; i++, d += 4)
    hidden1_biases[i] = readu_le_u32(d);
  d = read_hidden_weights(hidden1_weights, 512, d);
  for (unsigned i = 0; i < 32; i++, d += 4)
    hidden2_biases[i] = readu_le_u32(d);
  d = read_hidden_weights(hidden2_weights, 32, d);
  for (unsigned i = 0; i < 1; i++, d += 4)
    output_biases[i] = readu_le_u32(d);
  read_output_weights(output_weights, d);

#ifdef USE_AVX2
  permute_biases(hidden1_biases);
  permute_biases(hidden2_biases);
#endif
}

// ---------------------------------------------------------------------------
//  SFNNv13 HalfKAv2 Raw Binary Memory Mapping
//  Modern Stockfish networks have NO internal headers for Transformer/Network.
//  Layout: Version(4) + Hash(4) + DescLen(4) + DescriptionString(DescLen) + RAW WEIGHTS
//  Weights are read sequentially from offset (12 + descLen).
// ---------------------------------------------------------------------------

// SFNNv13 hidden layer sizes (not embedded in file — architecture-defined)
enum {
  SFNNv13_Big_l1Size   = 1536,
  SFNNv13_Big_l2Size   = 32,
  SFNNv13_Small_l1Size = 768,
  SFNNv13_Small_l2Size = 32
};

static bool init_weights_sfnnv13(const void *evalData)
{
  // Skip header: Version(4) + Hash(4) + DescLen(4) + DescriptionString(descLen)
  uint32_t descLen = readu_le_u32((const char*)evalData + 8);
  const char *d = (const char*)evalData + 12 + descLen;

  uint32_t halfDims, kingBuckets, ftInDims, l1Size, l2Size;

  switch (g_NetworkArch)
  {
    case ARCH_SFNNv13_Big:
      halfDims  = SFNNv13_Big_HalfDims;
      kingBuckets = SFNNv13_Big_KingBuckets;
      ftInDims  = SFNNv13_Big_FtInDims;
      l1Size    = SFNNv13_Big_l1Size;
      l2Size    = SFNNv13_Big_l2Size;

      // --- Allocate Big network arrays ---
      sfnnv13_big_ft_biases  = (int16_t*)malloc(halfDims  * kingBuckets * sizeof(int16_t));
      sfnnv13_big_ft_weights = (int16_t*)malloc(halfDims  * ftInDims    * sizeof(int16_t));
      sfnnv13_big_l1_biases  = (int32_t*)malloc(l1Size                   * sizeof(int32_t));
      sfnnv13_big_l1_weights = (int16_t*)malloc(l1Size    * halfDims    * sizeof(int16_t));
      sfnnv13_big_l2_biases  = (int32_t*)malloc(l2Size                   * sizeof(int32_t));
      sfnnv13_big_l2_weights = (int8_t*) malloc(l2Size    * l1Size      * sizeof(int8_t));
      sfnnv13_big_out_biases = (int32_t*)malloc(1                        * sizeof(int32_t));
      sfnnv13_big_out_weights= (int8_t*) malloc(l2Size                   * sizeof(int8_t));
      sfnnv13_big_l1Size = l1Size;
      sfnnv13_big_l2Size = l2Size;

      if (!sfnnv13_big_ft_biases  || !sfnnv13_big_ft_weights ||
          !sfnnv13_big_l1_biases  || !sfnnv13_big_l1_weights ||
          !sfnnv13_big_l2_biases  || !sfnnv13_big_l2_weights ||
          !sfnnv13_big_out_biases || !sfnnv13_big_out_weights)
      {
        printf("  Error: Failed to allocate Big network arrays.\n");
        fflush(stdout);
        return false;
      }

      // 1. FT Biases
      memcpy(sfnnv13_big_ft_biases, d, halfDims  * kingBuckets * sizeof(int16_t));
      d += halfDims  * kingBuckets * sizeof(int16_t);

      // 2. FT Weights
      memcpy(sfnnv13_big_ft_weights, d, halfDims  * ftInDims    * sizeof(int16_t));
      d += halfDims  * ftInDims    * sizeof(int16_t);

      // 3. L1 Biases
      memcpy(sfnnv13_big_l1_biases, d, l1Size                   * sizeof(int32_t));
      d += l1Size                   * sizeof(int32_t);

      // 4. L1 Weights
      memcpy(sfnnv13_big_l1_weights, d, l1Size    * halfDims    * sizeof(int16_t));
      d += l1Size    * halfDims    * sizeof(int16_t);

      // 5. L2 Biases
      memcpy(sfnnv13_big_l2_biases, d, l2Size                   * sizeof(int32_t));
      d += l2Size                   * sizeof(int32_t);

      // 6. L2 Weights
      memcpy(sfnnv13_big_l2_weights, d, l2Size    * l1Size      * sizeof(int8_t));
      d += l2Size    * l1Size      * sizeof(int8_t);

      // 7. Out Biases
      memcpy(sfnnv13_big_out_biases, d, 1                        * sizeof(int32_t));
      d += 1                        * sizeof(int32_t);

      // 8. Out Weights
      memcpy(sfnnv13_big_out_weights, d, l2Size                   * sizeof(int8_t));
      d += l2Size                   * sizeof(int8_t);

      break;

    case ARCH_SFNNv13_Small:
      halfDims  = SFNNv13_Small_HalfDims;
      kingBuckets = SFNNv13_Small_KingBuckets;
      ftInDims  = SFNNv13_Small_FtInDims;
      l1Size    = SFNNv13_Small_l1Size;
      l2Size    = SFNNv13_Small_l2Size;

      // --- Allocate Small network arrays ---
      sfnnv13_small_ft_biases  = (int16_t*)malloc(halfDims  * kingBuckets * sizeof(int16_t));
      sfnnv13_small_ft_weights = (int16_t*)malloc(halfDims  * ftInDims    * sizeof(int16_t));
      sfnnv13_small_l1_biases  = (int32_t*)malloc(l1Size                   * sizeof(int32_t));
      sfnnv13_small_l1_weights = (int16_t*)malloc(l1Size    * halfDims    * sizeof(int16_t));
      sfnnv13_small_l2_biases  = (int32_t*)malloc(l2Size                   * sizeof(int32_t));
      sfnnv13_small_l2_weights = (int8_t*) malloc(l2Size    * l1Size      * sizeof(int8_t));
      sfnnv13_small_out_biases = (int32_t*)malloc(1                        * sizeof(int32_t));
      sfnnv13_small_out_weights= (int8_t*) malloc(l2Size                   * sizeof(int8_t));
      sfnnv13_small_l1Size = l1Size;
      sfnnv13_small_l2Size = l2Size;

      if (!sfnnv13_small_ft_biases  || !sfnnv13_small_ft_weights ||
          !sfnnv13_small_l1_biases  || !sfnnv13_small_l1_weights ||
          !sfnnv13_small_l2_biases  || !sfnnv13_small_l2_weights ||
          !sfnnv13_small_out_biases || !sfnnv13_small_out_weights)
      {
        printf("  Error: Failed to allocate Small network arrays.\n");
        fflush(stdout);
        return false;
      }

      // 1. FT Biases
      memcpy(sfnnv13_small_ft_biases, d, halfDims  * kingBuckets * sizeof(int16_t));
      d += halfDims  * kingBuckets * sizeof(int16_t);

      // 2. FT Weights
      memcpy(sfnnv13_small_ft_weights, d, halfDims  * ftInDims    * sizeof(int16_t));
      d += halfDims  * ftInDims    * sizeof(int16_t);

      // 3. L1 Biases
      memcpy(sfnnv13_small_l1_biases, d, l1Size                   * sizeof(int32_t));
      d += l1Size                   * sizeof(int32_t);

      // 4. L1 Weights
      memcpy(sfnnv13_small_l1_weights, d, l1Size    * halfDims    * sizeof(int16_t));
      d += l1Size    * halfDims    * sizeof(int16_t);

      // 5. L2 Biases
      memcpy(sfnnv13_small_l2_biases, d, l2Size                   * sizeof(int32_t));
      d += l2Size                   * sizeof(int32_t);

      // 6. L2 Weights
      memcpy(sfnnv13_small_l2_weights, d, l2Size    * l1Size      * sizeof(int8_t));
      d += l2Size    * l1Size      * sizeof(int8_t);

      // 7. Out Biases
      memcpy(sfnnv13_small_out_biases, d, 1                        * sizeof(int32_t));
      d += 1                        * sizeof(int32_t);

      // 8. Out Weights
      memcpy(sfnnv13_small_out_weights, d, l2Size                   * sizeof(int8_t));
      d += l2Size                   * sizeof(int8_t);

      break;

    default:
      printf("  Error: Unknown SFNNv13 architecture (%d).\n", g_NetworkArch);
      fflush(stdout);
      return false;
  }

  return true;
}

static bool load_eval_file(const char *evalFile)
{
  const void *evalData;
  map_t mapping;
  size_t size;

  {
    FD fd = open_file(evalFile);
    if (fd == FD_ERR) return false;
    evalData = map_file(fd, &mapping);
    size = file_size(fd);
    close_file(fd);
  }

  bool success = verify_net(evalData, size);
  if (success && g_NetworkArch == ARCH_HalfKP)
    init_weights(evalData);
  else if (success && (g_NetworkArch == ARCH_SFNNv13_Big || g_NetworkArch == ARCH_SFNNv13_Small)) {
    if (!init_weights_sfnnv13(evalData))
      success = false;
  }
  if (mapping) unmap_file(evalData, mapping);
  return success;
}

/*
Interfaces
*/
DLLExport void _CDECL nnue_init(const char* evalFile)
{
  printf("Loading NNUE : %s\n", evalFile);
  fflush(stdout);

  if (load_eval_file(evalFile)) {
    printf("NNUE loaded !\n");
    fflush(stdout);
    return;
  }

  printf("NNUE file not found!\n");
  fflush(stdout);
}

DLLExport void _CDECL nnue_init_dual(const char* bigFile, const char* smallFile)
{
  bool bigOk = false, smallOk = false;

  printf("Loading NNUE Big  : %s\n", bigFile ? bigFile : "(null)");
  fflush(stdout);
  if (bigFile && load_eval_file(bigFile)) {
    printf("NNUE Big loaded !\n");
    fflush(stdout);
    bigOk = true;
  } else if (bigFile) {
    printf("NNUE Big file not found!\n");
    fflush(stdout);
  }

  printf("Loading NNUE Small: %s\n", smallFile ? smallFile : "(null)");
  fflush(stdout);
  if (smallFile && load_eval_file(smallFile)) {
    printf("NNUE Small loaded !\n");
    fflush(stdout);
    smallOk = true;
  } else if (smallFile) {
    printf("NNUE Small file not found!\n");
    fflush(stdout);
  }

  if (bigOk || smallOk) {
    printf("Dual NNUE ready (big=%d, small=%d)\n", bigOk, smallOk);
    fflush(stdout);
  } else {
    printf("No NNUE network loaded!\n");
    fflush(stdout);
  }
}

DLLExport int _CDECL nnue_evaluate(
  int player, int* pieces, int* squares)
{
  NNUEdata nnue;
  nnue.accumulator.computedAccumulation = 0;

  Position pos;
  pos.nnue[0] = &nnue;
  pos.nnue[1] = 0;
  pos.nnue[2] = 0;
  pos.player = player;
  pos.pieces = pieces;
  pos.squares = squares;
  return nnue_evaluate_pos(&pos);
}

DLLExport int _CDECL nnue_evaluate_incremental(
  int player, int* pieces, int* squares, NNUEdata** nnue)
{
  assert(nnue[0] && (uint64_t)(&nnue[0]->accumulator) % 64 == 0);

  Position pos;
  pos.nnue[0] = nnue[0];
  pos.nnue[1] = nnue[1];
  pos.nnue[2] = nnue[2];
  pos.player = player;
  pos.pieces = pieces;
  pos.squares = squares;
  return nnue_evaluate_pos(&pos);
}

DLLExport int _CDECL nnue_evaluate_fen(const char* fen)
{
  int pieces[33],squares[33],player,castle,fifty,move_number;
  decode_fen((char*)fen,&player,&castle,&fifty,&move_number,pieces,squares);;
  return nnue_evaluate(player,pieces,squares);
}
