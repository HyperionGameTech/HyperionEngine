//
// Created by emd22 on 11/10/22.
//

#ifndef HYPERION_INTRINSICS_HPP
#define HYPERION_INTRINSICS_HPP

#include <immintrin.h>
#include <util/Defines.hpp>

namespace hyperion::intrinsics {


#if HYP_FEATURES_INTRINSICS && !defined(HYP_ARM)
/* set of 4 32-bit floats */
using Float128 = __m128;
#else
using Float128 = float[4];
#endif

#if HYP_FEATURES_INTRINSICS && !defined(HYP_ARM)

/* Float128 Methods */

static const HYP_FORCE_INLINE Float128 Float128Load(const float *src)
{
    const Float128 vec = _mm_loadu_ps(src);
    return vec;
}
static const HYP_FORCE_INLINE void Float128Store(float *dest, Float128 vec)
{
    _mm_storeu_ps(dest, vec);
}

#define Float128Add(left, right) (_mm_add_ps(left, right))
#define Float128Sub(left, right) (_mm_sub_ps(left, right))
#define Float128Mul(left, right) (_mm_mul_ps(left, right))
#define Float128Div(left, right) (_mm_div_ps(left, right))

#endif

};

#endif //HYPERION_INTRINSICS_HPP
