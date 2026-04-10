#include "FxMathUtil.hpp"

#include "FxMathConsts.hpp"

#include <cmath>
#include <bit>

// Should we warn if FP_FAST_FMAF is not defined?
//
// #ifndef FP_FAST_FMAF
// #endif

namespace FxMath {

static constexpr uint32 scSignMask = 0x80000000;

// Basic reinterpret casts
#define AsUInt(value_)  std::bit_cast<uint32>(value_)
#define AsFloat(value_) std::bit_cast<float32>(value_)

static constexpr float32 scSineCoeff1 = -2.3889859e-08f;

static constexpr float32 scSineCoeff0_0 = +2.7525562e-06f;
static constexpr float32 scSineCoeff0_1 = -0.00019840874f;
static constexpr float32 scSineCoeff0_2 = +0.0083333310f;
static constexpr float32 scSineCoeff0_3 = -0.16666667f;

static constexpr float32 scCosineCoeff1 = -2.6051615e-07f;

static constexpr float32 scCosineCoeff0_0 = +2.4760495e-05f;
static constexpr float32 scCosineCoeff0_1 = -0.0013888378f;
static constexpr float32 scCosineCoeff0_2 = +0.041666638f;
static constexpr float32 scCosineCoeff0_3 = -0.5f;

void SinCos(float32 in_angle, float32* out_sine, float32* out_cosine)
{
    static constexpr float32 scPi = FX_PI;
    static constexpr float32 scHalfPi = FX_HALF_PI;

    if (in_angle > FX_2PI) {
        in_angle = -FX_2PI;
    }
    else if (in_angle < -FX_2PI) {
        in_angle = FX_2PI;
    }

    uint32 angle_sign = AsUInt(in_angle) & scSignMask;

    // Pi if the sign is positive, -Pi if the sign is negative.
    float32 pi_or_neg_pi = AsFloat(AsUInt(scPi) | angle_sign);

    uint32 cmp_result = (float32((~angle_sign) & AsUInt(in_angle)) <= float32(scHalfPi)) ? 0xFFFFFFFF : 0x00000000;

    uint32 sel0 = (cmp_result & AsUInt(in_angle));
    uint32 sel1 = ((~cmp_result) & AsUInt((pi_or_neg_pi - in_angle)));

    in_angle = AsFloat(sel0 | sel1);

    sel0 = cmp_result & AsUInt(1.0f);
    sel1 = ((~cmp_result) & AsUInt(-1.0f));
    angle_sign = (sel0 | sel1);

    float32 angle_sq = (in_angle * in_angle);

    // Sine approximation
    float32 result = std::fmaf(scSineCoeff1, angle_sq, scSineCoeff0_0);
    result = std::fmaf(result, angle_sq, scSineCoeff0_1);
    result = std::fmaf(result, angle_sq, scSineCoeff0_2);
    result = std::fmaf(result, angle_sq, scSineCoeff0_3);
    result = std::fmaf(result, angle_sq, 1.0f);
    result *= in_angle;

    (*out_sine) = result;

    // Cosine approximation
    result = std::fmaf(scCosineCoeff1, angle_sq, scCosineCoeff0_0);
    result = std::fmaf(result, angle_sq, scCosineCoeff0_1);
    result = std::fmaf(result, angle_sq, scCosineCoeff0_2);
    result = std::fmaf(result, angle_sq, scCosineCoeff0_3);
    result = std::fmaf(result, angle_sq, 1.0f);
    result = result * AsFloat(angle_sign);

    (*out_cosine) = result;
}

} // namespace FxMath
