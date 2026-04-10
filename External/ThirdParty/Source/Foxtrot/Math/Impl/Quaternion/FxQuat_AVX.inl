#pragma once

#include <Core/FxDefines.hpp>

#ifdef FX_USE_AVX

#include <Math/FxQuat.hpp>
#include <Math/FxSSE.hpp>
#include <Math/FxSSEUtil.hpp>

FX_FORCE_INLINE FxQuat& FxQuat::operator=(const __m128 other)
{
    mIntrin = other;
    return *this;
}

FX_FORCE_INLINE FxQuat::FxQuat(const float32* buffer)
{
    const float32 aligned_buffer[] = { buffer[0], buffer[1], buffer[2], buffer[2] };
    mIntrin = _mm_load_ps(aligned_buffer);
}

FX_FORCE_INLINE bool FxQuat::IsCloseTo(const FxQuat& other, const float32 tolerance) const
{
    return IsCloseTo(other.mIntrin, tolerance);
}

FX_FORCE_INLINE bool FxQuat::IsCloseTo(const __m128 other, const float32 tolerance) const
{
    // Get the absolute difference
    const __m128 diff = FxSSE::RemoveSign(_mm_sub_ps(mIntrin, other));

    // Check if any absolute difference is > tolerance
    __m128i cmp_v = _mm_castps_si128(_mm_cmpgt_ps(diff, _mm_set1_ps(tolerance)));

    // If any components are, return true
    return static_cast<bool>(_mm_testz_si128(cmp_v, cmp_v));
}

// FX_FORCE_INLINE void FxQuat::LerpIP(const FxQuat& dest, float32 step)
//{
//     const __m128 inv_step_v = _mm_set1_ps(1.0 - step);
//     const __m128 step_v = _mm_set1_ps(step);
//
//     mIntrin = _mm_add_ps(_mm_mul_ps(inv_step_v, mIntrin), _mm_mul_ps(step_v, dest.mIntrin));
// }

FX_FORCE_INLINE void FxQuat::NLerpIP(const FxQuat& dest, float32 time)
{
    __m128 dest_v = dest.mIntrin;

    if (FxSSE::Dot(mIntrin, dest.mIntrin) < 0.0f) {
        dest_v = FxSSE::SetSigns<-1>(dest_v);
    }

    const __m128 inv_time_v = _mm_set1_ps(1.0 - time);
    const __m128 time_v = _mm_set1_ps(time);

    // (1 - time) * A
    mIntrin = _mm_mul_ps(inv_time_v, mIntrin);

    //  ... + time * B  ->  time * dest + mIntrin
    mIntrin = _mm_fmadd_ps(time_v, dest_v, mIntrin);

    mIntrin = FxSSE::Normalize(mIntrin);
}


// Based off of https://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/slerp/index.htm and optimized
// for Neon.
FX_FORCE_INLINE FxQuat FxQuat::SLerp(const FxQuat& dest, const float32 step)
{
    // Note: there are so many different ways to implement this and a lot of them online just straight up pro
    __m128 a_v = mIntrin;
    __m128 b_v = dest.mIntrin;

    __m128 result;

    // Calculate angle between them.
    float32 cos_half_theta = FxSSE::Dot(a_v, b_v);
    // if qa=qb or qa=-qb then theta = 0 and we can return qa
    if (abs(cos_half_theta) >= 1.0) {
        return FxQuat(mIntrin);
    }

    // Calculate temporary values.
    float32 half_theta = acosf(cos_half_theta);
    float32 sin_half_theta = sqrtf(1.0f - cos_half_theta * cos_half_theta);

    // if theta = 180 degrees then result is not fully defined
    // we could rotate around any axis normal to qa or qb
    if (sin_half_theta < 0.001) {
        __m128 half = _mm_set1_ps(0.5f);

        result = _mm_mul_ps(a_v, half);
        result = _mm_fmadd_ps(b_v, half, result);

        return FxQuat(result);
    }

    const float32 sht_recip = 1.0f / sin_half_theta;
    float32 ratioA = sinf((1.0f - step) * half_theta) * sht_recip;
    float32 ratioB = sinf(step * half_theta) * sht_recip;

    result = _mm_mul_ps(a_v, _mm_set1_ps(ratioA));
    result = _mm_fmadd_ps(b_v, _mm_set1_ps(ratioB), result);

    return FxQuat(result);
}


#endif
