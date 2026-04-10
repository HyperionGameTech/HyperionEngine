#pragma once

#include <Math/FxQuat.hpp>

#ifdef FX_USE_NEON

FX_FORCE_INLINE FxQuat::FxQuat(const float32* buffer)
{
    const float aligned_buffer[] = { buffer[0], buffer[1], buffer[2], buffer[2] };
    mIntrin = vld1q_f32(aligned_buffer);
}

FX_FORCE_INLINE FxQuat& FxQuat::operator=(const float32x4_t& other)
{
    mIntrin = other;
    return *this;
}

FX_FORCE_INLINE bool FxQuat::IsCloseTo(const FxQuat& other, const float32 tolerance) const
{
    return IsCloseTo(other.mIntrin, tolerance);
}

FX_FORCE_INLINE bool FxQuat::IsCloseTo(const float32x4_t& other, const float32 tolerance) const
{
    // Get the absolute difference between the vectors
    const float32x4_t diff = vabdq_f32(mIntrin, other);

    // Is the difference greater than our threshhold?
    const uint32x4_t lt = vcgtq_f32(diff, vdupq_n_f32(tolerance));

    // If any components are true, return false.
    return vmaxvq_u32(lt) == 0;
}

FX_FORCE_INLINE void FxQuat::NLerpIP(const FxQuat& dest, float32 time)
{
    // We can change (A + (B - A) * time) to ((1 - time) * A + time * B) to reduce floating point errors.

    float32x4_t dest_v = dest.mIntrin;

    if (FxNeon::Dot(mIntrin, dest.mIntrin) < 0.0f) {
        dest_v = FxNeon::SetSigns<-1>(dest_v);
    }

    const float32x4_t inv_time_v = vdupq_n_f32(1.0 - time);
    const float32x4_t time_v = vdupq_n_f32(time);

    // (1 - time) * A
    mIntrin = vmulq_f32(inv_time_v, mIntrin);
    // ... + time * B -> vfmaq takes in (D, N, M); N and M are multiplied, and accumulated to D(destination).
    mIntrin = vfmaq_f32(mIntrin, time_v, dest_v);

    // Quaternion lerp is always normalized!
    mIntrin = FxNeon::Normalize(mIntrin);
}

// Based off of https://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/slerp/index.htm and optimized
// for Neon.
FX_FORCE_INLINE FxQuat FxQuat::SLerp(const FxQuat& dest, const float32 step)
{
    // Note: there are so many different ways to implement this and a lot of them online just straight up pro
    float32x4_t a_v = mIntrin;
    float32x4_t b_v = dest.mIntrin;

    float32x4_t result;

    // Calculate angle between them.
    float32 cos_half_theta = FxNeon::Dot(a_v, b_v);
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
        float32x4_t half = vdupq_n_f32(0.5f);

        result = vmulq_f32(a_v, half);
        result = vfmaq_f32(result, b_v, half);

        return FxQuat(result);
    }

    const float32 sht_recip = 1.0f / sin_half_theta;
    float32 ratioA = sinf((1.0f - step) * half_theta) * sht_recip;
    float32 ratioB = sinf(step * half_theta) * sht_recip;

    result = vmulq_f32(a_v, vdupq_n_f32(ratioA));
    result = vfmaq_f32(result, b_v, vdupq_n_f32(ratioB));

    return FxQuat(result);
}

#endif
