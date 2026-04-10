#include <Core/FxDefines.hpp>


#ifdef FX_USE_AVX

#include "FxSSE.hpp"

#include <Math/FxMathConsts.hpp>
#include <Math/FxSSEUtil.hpp>

static constexpr uint32 sConstSignMask = 0x80000000;

static constexpr float32 sConstHalfPi = FX_PI_2;
static constexpr float32 sConstPi = M_PI;
static constexpr float32 sConst3HalfPi = sConstHalfPi * 3.0f;

static constexpr float32 sConst2Pi = M_PI * 2.0;
static constexpr float32 sConstInv2Pi = 1.0f / sConst2Pi;

namespace FxSSE {

// Implementation based on and DirectXMath: https://github.com/microsoft/DirectXMath/
// Minimax approximation for sine/cosine
void SinCos4(__m128 in_values, __m128* ysin, __m128* ycos)
{
    const __m128 cvOne = _mm_set1_ps(1.0);
    const __m128 cvSignMask = _mm_set1_ps(FxSSE::scSignMask32);

    const __m128 cvSineCoeff1 = _mm_set1_ps(-2.3889859e-08f);
    const __m128 cvSineCoeff0 = _mm_setr_ps(-0.16666667f, +0.0083333310f, -0.00019840874f, +2.7525562e-06f);

    const __m128 cvCosineCoeff1 = _mm_set1_ps(-2.6051615e-07f);
    const __m128 cvCosineCoeff0 = _mm_setr_ps(-0.5f, +0.041666638f, -0.0013888378f, +2.4760495e-05f);

    const __m128 cvPi = _mm_set1_ps(FX_PI);
    const __m128 cvHalfPi = _mm_set1_ps(FX_HALF_PI);

    __m128 sign = _mm_and_ps(in_values, cvSignMask);
    __m128 pi_or_neg_pi = _mm_or_ps(cvPi, sign);
    __m128 le_result = _mm_cmple_ps(_mm_andnot_ps(sign, in_values), cvHalfPi);

    __m128 sel0 = _mm_and_ps(le_result, in_values);
    __m128 sel1 = _mm_andnot_ps(le_result, _mm_sub_ps(pi_or_neg_pi, in_values));

    in_values = _mm_or_ps(sel0, sel1);

    sel0 = _mm_and_ps(le_result, cvOne);
    sel1 = _mm_andnot_ps(le_result, _mm_or_ps(cvOne, cvSignMask));
    sign = _mm_or_ps(sel0, sel1);

    __m128 values_sq = _mm_mul_ps(in_values, in_values);

    // Sine approximation
    __m128 scoeff0 = _mm_permute_ps(cvSineCoeff0, _MM_SHUFFLE(3, 3, 3, 3));
    __m128 result = _mm_fmadd_ps(cvSineCoeff1, values_sq, scoeff0);

    scoeff0 = _mm_permute_ps(cvSineCoeff0, _MM_SHUFFLE(2, 2, 2, 2));
    result = _mm_fmadd_ps(result, values_sq, scoeff0);

    scoeff0 = _mm_permute_ps(cvSineCoeff0, _MM_SHUFFLE(1, 1, 1, 1));
    result = _mm_fmadd_ps(result, values_sq, scoeff0);

    scoeff0 = _mm_permute_ps(cvSineCoeff0, _MM_SHUFFLE(0, 0, 0, 0));
    result = _mm_fmadd_ps(result, values_sq, scoeff0);

    result = _mm_fmadd_ps(result, values_sq, cvOne);
    result = _mm_mul_ps(result, in_values);

    (*ysin) = result;

    // Cosine approximation

    __m128 ccoeff0 = _mm_permute_ps(cvCosineCoeff0, _MM_SHUFFLE(3, 3, 3, 3));
    result = _mm_fmadd_ps(cvCosineCoeff1, values_sq, ccoeff0);

    ccoeff0 = _mm_permute_ps(cvCosineCoeff0, _MM_SHUFFLE(2, 2, 2, 2));
    result = _mm_fmadd_ps(result, values_sq, ccoeff0);

    ccoeff0 = _mm_permute_ps(cvCosineCoeff0, _MM_SHUFFLE(1, 1, 1, 1));
    result = _mm_fmadd_ps(result, values_sq, ccoeff0);

    ccoeff0 = _mm_permute_ps(cvCosineCoeff0, _MM_SHUFFLE(0, 0, 0, 0));
    result = _mm_fmadd_ps(result, values_sq, ccoeff0);

    result = _mm_fmadd_ps(result, values_sq, cvOne);
    result = _mm_mul_ps(result, sign);

    (*ycos) = result;
}

} // namespace FxSSE

#endif
