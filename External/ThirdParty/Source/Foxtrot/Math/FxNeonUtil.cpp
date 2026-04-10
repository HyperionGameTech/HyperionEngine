#include <Core/FxDefines.hpp>

#ifdef FX_USE_NEON

#include "FxMathConsts.hpp"
#include "FxNeonUtil.hpp"

namespace FxNeon {

#define AsFloat(value_) vreinterpretq_u32_f32(value_)
#define AsUInt(value_)  vreinterpretq_f32_u32(value_)

// Modified algorithm based off of the SSE version at FxSSEUtil, which is originally based off of DirectXMath's
// implementation. Uses minimax algorithm for accuracy at small angles, with angles above 2pi(or below -2pi) being
// truncated.
void SinCos4(float32x4_t in_values, float32x4_t* ysin, float32x4_t* ycos)
{
    const float32x4_t cvPi = vdupq_n_f32(FX_PI);
    const float32x4_t cvHalfPi = vdupq_n_f32(FX_HALF_PI);
    const float32x4_t cvOne = vdupq_n_f32(1.0f);

    // Ensure that in_values are between -2pi and 2pi.

    // x mod y = x - (round(x / y) * y)
    // Since we know the limit (2pi), we can replace (x / y) with (x * (1 / y)), or (x * (1 / 2pi)).
    {
        const float32x4_t cvOneOver2Pi = vdupq_n_f32(FX_1_OVER_2PI);

        float32x4_t r0 = vrndnq_f32(vmulq_f32(in_values, cvOneOver2Pi));

        // Note that vld/vdup incur latency (pipeline stalls) as the values need to be transferred into the NEON
        // registers, so for one-time use things like this it can be faster(or more reliably fast) to calculate the same
        // value over loading.
        in_values = vfmsq_f32(in_values, r0, vmulq_n_f32(cvPi, 2.0f));
    }

    const uint32x4_t cvSignMask = vdupq_n_u32(FxNeon::scSignMask32);

    const float32x4_t cvSineCoeff1 = vdupq_n_f32(-2.3889859e-08f);
    const float32x4_t cvSineCoeff0 = { -0.16666667f, +0.0083333310f, -0.00019840874f, +2.7525562e-06f };

    const float32x4_t cvCosineCoeff1 = vdupq_n_f32(-2.6051615e-07f);
    const float32x4_t cvCosineCoeff0 = { -0.5f, +0.041666638f, -0.0013888378f, +2.4760495e-05f };

    // Extract sign bit
    uint32x4_t sign = vandq_u32(in_values, cvSignMask);

    float32x4_t pi_or_neg_pi = AsFloat(vorrq_u32(cvPi, sign));

    // Compare |in_values| <= pi / 2.
    // If the result is true, the component will be 0xFFFFFFFF, and 0x00000000 otherwise.
    // We use this as a mask and use bitwise select to select values based on this mask.
    uint32x4_t cmp_result = vcleq_f32(vabsq_f32(in_values), cvHalfPi);

    in_values = vbslq_f32(cmp_result, in_values, vsubq_f32(pi_or_neg_pi, in_values));

    float32x4_t one_or_sign = AsFloat(vorrq_u32(AsUInt(cvOne), cvSignMask));
    sign = vbslq_f32(cmp_result, cvOne, one_or_sign);

    float32x4_t values_sq = vmulq_f32(in_values, in_values);

    float32x4_t result;

    // Sine approximation (evaluated using Horner's method)
    result = vfmaq_f32(vdupq_laneq_f32(cvSineCoeff0, 3), cvSineCoeff1, values_sq);
    result = vfmaq_f32(vdupq_laneq_f32(cvSineCoeff0, 2), result, values_sq);
    result = vfmaq_f32(vdupq_laneq_f32(cvSineCoeff0, 1), result, values_sq);
    result = vfmaq_f32(vdupq_laneq_f32(cvSineCoeff0, 0), result, values_sq);
    result = vfmaq_f32(cvOne, result, values_sq);

    *ysin = vmulq_f32(result, in_values);

    // Cosine approximation
    result = vfmaq_f32(vdupq_laneq_f32(cvCosineCoeff0, 3), cvCosineCoeff1, values_sq);
    result = vfmaq_f32(vdupq_laneq_f32(cvCosineCoeff0, 2), result, values_sq);
    result = vfmaq_f32(vdupq_laneq_f32(cvCosineCoeff0, 1), result, values_sq);
    result = vfmaq_f32(vdupq_laneq_f32(cvCosineCoeff0, 0), result, values_sq);
    result = vfmaq_f32(cvOne, result, values_sq);

    *ycos = vmulq_f32(result, sign);
}


}; // namespace FxNeon

#endif // #ifdef FX_USE_NEON
