#pragma once

#include "FxMathUtil.hpp"

#ifdef FX_USE_NEON
#include <arm_neon.h>
#endif

namespace FxMath {

FX_FORCE_INLINE float32 RSqrt(float32 x)
{
#ifdef FX_USE_NEON
    // Approximate 1/sqrt(x)
    float32_t apx_1_sqrt = vrsqrtes_f32(x);

    // Do sqrt steps to improve precision.
    apx_1_sqrt = vrsqrtss_f32(x, apx_1_sqrt);
    apx_1_sqrt = vrsqrtss_f32(x, apx_1_sqrt);

    return apx_1_sqrt;

#endif
    return 1.0f / sqrtf(x);
}

}; // namespace FxMath
