//
// Created by emd22 on 11/10/22.
//

#ifndef HYPERION_INTRINSICS_HPP
#define HYPERION_INTRINSICS_HPP

#include <immintrin.h>
#include <Types.hpp>
#include <util/Defines.hpp>

namespace hyperion::intrinsics {


#if HYP_FEATURES_INTRINSICS && !defined(HYP_ARM)
/* set of 4 32-bit floats */
using Float128  = __m128;
using Float128U = __m128_u;

#else
using Float128 = float[4];
#define Float128Zero() { 0.0f, 0.0f, 0.0f, 0.0f };
#endif

#ifdef HYP_AVX_SUPPORTED
#define Float128Permute(vec, c) (_mm_permute_ps((v), c))
#else
#define Float128Permute(vec, c) (_mm_shuffle_ps((vec), (vec), c))
#endif

#if HYP_FEATURES_INTRINSICS && !defined(HYP_ARM)

/* Float128 Methods */

static const HYP_FORCE_INLINE Float128 Float128Set(Float32 x, Float32 y, Float32 z, Float32 w)
{
    // WTF? the docs for _mm_set_ps says zyxw but the implementation takes wzyx?
    // Needs more testing...
    const Float128 vec = _mm_set_ps(w, z, y, x);
    return vec;
}
static const HYP_FORCE_INLINE Float128 Float128Set(Float32 value)
{
    const Float128 vec = _mm_set_ps1(value);
    return vec;
}

#define Float128Load(src) (_mm_loadu_ps(src))

/*static const HYP_FORCE_INLINE Float128 Float128Load(const Float32 *src)
{
    //const Float128 vec = _mm_loadu_ps(src);
    //return vec;
    return _mm_loadu_ps(src);
}*/
static const HYP_FORCE_INLINE void Float128Store(Float32 *dest, Float128 vec)
{
    _mm_storeu_ps(dest, vec);
}


#define Float128Add(left, right) (_mm_add_ps(left, right))
#define Float128Sub(left, right) (_mm_sub_ps(left, right))
#define Float128Mul(left, right) (_mm_mul_ps(left, right))
#define Float128Div(left, right) (_mm_div_ps(left, right))
#define Float128Set1(value) (_mm_set1_ps(value))
#define Float128Zero() (_mm_setzero_ps())
#define Float128ShuffleMask(z, y, x, w) _MM_SHUFFLE(z, y, x, w)
#define Float128Shuffle(a, b, imm8) (_mm_shuffle_ps(a, b, imm8))

static const HYP_FORCE_INLINE Float128 Float128HAdd(Float128 vec)
{
    const __m128 temp = _mm_add_ps(vec, _mm_movehl_ps(vec, vec));
    const __m128 sum  = _mm_add_ss(temp, Float128Shuffle(temp, temp, 1));
    return sum;
}

static const HYP_FORCE_INLINE Float32 Float128Sum(Float128 vec)
{
    return _mm_cvtss_f32(Float128HAdd(vec));
}

static const HYP_FORCE_INLINE void Float128StoreVector3(Float32 *data, Float128 vec)
{
    // Store first two low floats to our data buffer
    _mm_storel_pi((__m64 *)data, vec);
    // Store the Z axis
    _MM_EXTRACT_FLOAT(data[2], vec, 2);
}

#endif

};

#endif //HYPERION_INTRINSICS_HPP
