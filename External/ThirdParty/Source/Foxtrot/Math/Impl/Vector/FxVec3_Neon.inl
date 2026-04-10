#pragma once

#include <Core/FxDefines.hpp>

#ifdef FX_USE_NEON

#include <Math/FxVec3.hpp>


FX_FORCE_INLINE FxVec3f::FxVec3f(float32 x, float32 y, float32 z)
{
    const float32 values[4] = { x, y, z, 0 };
    mIntrin = vld1q_f32(values);
}

FX_FORCE_INLINE FxVec3f::FxVec3f(const float32* values)
{
    const float32 values4[4] = { values[0], values[1], values[2], 0 };
    mIntrin = vld1q_f32(values4);
}

FX_FORCE_INLINE FxVec3f::FxVec3f(float32 scalar) { mIntrin = vdupq_n_f32(scalar); }

FX_FORCE_INLINE bool FxVec3f::IsCloseTo(const FxVec3f& other, const float32 tolerance) const
{
    return IsCloseTo(other.mIntrin, tolerance);
}

FX_FORCE_INLINE bool FxVec3f::IsCloseTo(const FxVec3f::SimdType other, const float32 tolerance) const
{
    // Get the absolute difference between the vectors
    const float32x4_t diff = vabdq_f32(mIntrin, other);

    // Is the difference greater than our threshold?
    const uint32x4_t lt = vcgtq_f32(diff, vdupq_n_f32(tolerance));

    // If any components are true, return false.
    return vmaxvq_u32(lt) == 0;
}

FX_FORCE_INLINE bool FxVec3f::IsZero() const
{
    const uint32x4_t is_zero_v = vceqzq_f32(mIntrin);
    return vmaxvq_u32(is_zero_v) == 0;
}

FX_FORCE_INLINE bool FxVec3f::IsNearZero(const float32 tolerance) const { return IsCloseTo(sZero, tolerance); }


FX_FORCE_INLINE void FxVec3f::Set(float32 x, float32 y, float32 z)
{
    const float32 v[] = { x, y, z, 0 };
    mIntrin = vld1q_f32(v);
}

FX_FORCE_INLINE FxVec3f FxVec3f::Min(const FxVec3f& a, const FxVec3f& b)
{
    return FxVec3f(vminq_f32(a.mIntrin, b.mIntrin));
}

FX_FORCE_INLINE FxVec3f FxVec3f::Max(const FxVec3f& a, const FxVec3f& b)
{
    return FxVec3f(vmaxq_f32(a.mIntrin, b.mIntrin));
}

FX_FORCE_INLINE FxVec3f FxVec3f::Clamp(const FxVec3f& v, const FxVec3f& min, const FxVec3f& max)
{
    return FxVec3f(vminq_f32(vmaxq_f32(v, min.mIntrin), max.mIntrin));
}

FX_FORCE_INLINE FxVec3f FxVec3f::Lerp(const FxVec3f& a, const FxVec3f& b, const float f)
{
    // a + f * (b - a);
    float32x4_t d = vsubq_f32(b, a);

    return FxVec3f(vaddq_f32(a, vmulq_n_f32(d, f)));
}

FX_FORCE_INLINE FxVec3f& FxVec3f::LerpIP(const FxVec3f& dest, const float step)
{
    // a + f * (b - a);
    float32x4_t d = vsubq_f32(dest, mIntrin);

    mIntrin = vaddq_f32(mIntrin, vmulq_n_f32(d, step));

    return *this;
}


FX_FORCE_INLINE FxVec3f& FxVec3f::NormalizeIP()
{
    // Calculate length and splat to register
    const float32x4_t len_v = vdupq_n_f32(Length());

    // Divide vector by length
    mIntrin = vdivq_f32(mIntrin, len_v);

    return *this;
}

FX_FORCE_INLINE FxVec3f FxVec3f::Normalize() const
{
    // Calculate length and splat to register
    const float32x4_t len_v = vdupq_n_f32(Length());

    return FxVec3f(vdivq_f32(mIntrin, len_v));
}


FX_FORCE_INLINE float32 FxVec3f::Length() const
{
    float32x4_t v = mIntrin;
    return sqrtf(Dot(v));
}

FX_FORCE_INLINE float32 FxVec3f::Dot(const FxVec3f& other) const { return FxNeon::Dot(mIntrin, other.mIntrin); }

FX_FORCE_INLINE float32 FxVec3f::Dot(FxVec3f::SimdType other) const { return FxNeon::Dot(mIntrin, other); }

//////////////////////////////
// Operator Overloads
//////////////////////////////

FX_FORCE_INLINE bool FxVec3f::operator==(const FxVec3f& other) const
{
    return (vaddvq_u32(vceqq_f32(mIntrin, other.mIntrin))) != 0;
}

FX_FORCE_INLINE FxVec3f FxVec3f::operator+(const FxVec3f& other) const
{
    return FxVec3f(vaddq_f32(mIntrin, other.mIntrin));
}

FX_FORCE_INLINE FxVec3f FxVec3f::operator-(const FxVec3f& other) const
{
    return FxVec3f(vsubq_f32(mIntrin, other.mIntrin));
}

FX_FORCE_INLINE FxVec3f FxVec3f::operator*(const FxVec3f& other) const
{
    return FxVec3f(vmulq_f32(mIntrin, other.mIntrin));
}

FX_FORCE_INLINE FxVec3f FxVec3f::operator*(float32 scalar) const
{
    float32x4_t result = vmulq_n_f32(mIntrin, scalar);
    return FxVec3f(result);
}

FX_FORCE_INLINE FxVec3f FxVec3f::operator/(const FxVec3f& other) const
{
    return FxVec3f(vdivq_f32(mIntrin, other.mIntrin));
}

FX_FORCE_INLINE FxVec3f FxVec3f::operator/(float32 scalar) const
{
    float32x4_t result = vdivq_f32(mIntrin, vdupq_n_f32(scalar));
    return FxVec3f(result);
}

FX_FORCE_INLINE FxVec3f FxVec3f::operator-() const
{
    float32x4_t result = vnegq_f32(mIntrin);
    return FxVec3f(result);
}


FX_FORCE_INLINE FxVec3f& FxVec3f::operator+=(const FxVec3f& other)
{
    mIntrin = vaddq_f32(mIntrin, other);
    return *this;
}

FX_FORCE_INLINE FxVec3f& FxVec3f::operator-=(const FxVec3f& other)
{
    mIntrin = vsubq_f32(mIntrin, other);
    return *this;
}

FX_FORCE_INLINE FxVec3f& FxVec3f::operator-=(float32 scalar)
{
    mIntrin = vsubq_f32(mIntrin, vdupq_n_f32(scalar));
    return *this;
}

FX_FORCE_INLINE FxVec3f& FxVec3f::operator*=(const FxVec3f& other)
{
    mIntrin = vmulq_f32(mIntrin, other);
    return *this;
}


#endif
