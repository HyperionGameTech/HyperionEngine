#pragma once

#include <Core/FxDefines.hpp>

#ifdef FX_USE_NEON
#include <Math/FxVec4.hpp>


FX_FORCE_INLINE FxVec4f::FxVec4f(float32 x, float32 y, float32 z, float32 w)
{
    const float32 values[4] = { x, y, z, w };
    mIntrin = vld1q_f32(values);
}


FX_FORCE_INLINE FxVec4f::FxVec4f(const float32* values) { mIntrin = vld1q_f32(values); }


FX_FORCE_INLINE FxVec4f::FxVec4f(float32 scalar) { mIntrin = vdupq_n_f32(scalar); }


FX_FORCE_INLINE float32 FxVec4f::LengthSquared() const
{
    float32x4_t vec = mIntrin;
    vec = vmulq_f32(vec, vec);

    // Calculate the length squared by doing a horizontal add (add all components)
    return vaddvq_f32(vec);
}

FX_FORCE_INLINE bool FxVec4f::IsCloseTo(const FxVec4f& other, const float32 tolerance) const
{
    return IsCloseTo(other.mIntrin);
}

FX_FORCE_INLINE bool FxVec4f::IsCloseTo(const FxVec4f::SimdType other, const float32 tolerance) const
{
    // Get the absolute difference between the vectors
    const float32x4_t diff = vabdq_f32(mIntrin, other);

    // Is the difference greater than our threshold?
    const uint32x4_t lt = vcgtq_f32(diff, vdupq_n_f32(tolerance));

    // If any components are true, return false.
    return vmaxvq_u32(lt) == 0;
}

FX_FORCE_INLINE bool FxVec4f::IsZero() const
{
    const uint32x4_t is_zero_v = vceqzq_f32(mIntrin);
    return vmaxvq_u32(is_zero_v) == 0;
}

FX_FORCE_INLINE bool FxVec4f::IsNearZero(const float32 tolerance) const { return IsCloseTo(sZero, tolerance); }

FX_FORCE_INLINE void FxVec4f::Set(float32 x, float32 y, float32 z, float32 w)
{
    const float32 v[4] = { x, y, z, w };
    mIntrin = vld1q_f32(v);
}

FX_FORCE_INLINE void FxVec4f::Set(float32 scalar) { mIntrin = vdupq_n_f32(scalar); }


///////////////////////////////
// Vec + Vec Operators
///////////////////////////////

FxVec4f FxVec4f::operator+(const FxVec4f& other) const { return FxVec4f(vaddq_f32(mIntrin, other.mIntrin)); }

FxVec4f FxVec4f::operator-(const FxVec4f& other) const { return FxVec4f(vsubq_f32(mIntrin, other.mIntrin)); }

FxVec4f FxVec4f::operator*(const FxVec4f& other) const { return FxVec4f(vmulq_f32(mIntrin, other.mIntrin)); }

FxVec4f FxVec4f::operator/(const FxVec4f& other) const { return FxVec4f(vdivq_f32(mIntrin, other.mIntrin)); }


///////////////////////////////
// Vec + Scalar Operators
///////////////////////////////

FX_FORCE_INLINE FxVec4f FxVec4f::operator*(float scalar) const { return FxVec4f(vmulq_n_f32(mIntrin, scalar)); }
FX_FORCE_INLINE FxVec4f& FxVec4f::operator*=(float scalar)
{
    mIntrin = vmulq_n_f32(mIntrin, scalar);
    return *this;
}

FX_FORCE_INLINE FxVec4f FxVec4f::operator/(float scalar) const
{
    const float32x4_t rvalue = vdupq_n_f32(scalar);
    return FxVec4f(vdivq_f32(mIntrin, rvalue));
}

FX_FORCE_INLINE FxVec4f FxVec4f::operator-() const { return FxVec4f(vnegq_f32(mIntrin)); }


FX_FORCE_INLINE FxVec4f& FxVec4f::operator+=(const FxVec4f& other)
{
    this->mIntrin = vaddq_f32(this->mIntrin, other.mIntrin);
    return *this;
}

FX_FORCE_INLINE FxVec4f& FxVec4f::operator-=(const FxVec4f& other)
{
    this->mIntrin = vsubq_f32(this->mIntrin, other.mIntrin);
    return *this;
}

FX_FORCE_INLINE FxVec4f& FxVec4f::operator*=(const FxVec4f& other)
{
    this->mIntrin = vmulq_f32(this->mIntrin, other.mIntrin);
    return *this;
}

FX_FORCE_INLINE FxVec4f& FxVec4f::operator=(const FxVec4f& other)
{
    this->mIntrin = other.mIntrin;
    return *this;
}


/////////////////////////////////////
// Integer classes
/////////////////////////////////////


FX_FORCE_INLINE FxVec4u::FxVec4u(uint32 x, uint32 y, uint32 z, uint32 w)
{
    const uint32 values[4] = { x, y, z, w };
    mIntrin = vld1q_u32(values);
}


FX_FORCE_INLINE FxVec4u::FxVec4u(const uint32* values) { mIntrin = vld1q_u32(values); }


FX_FORCE_INLINE FxVec4u::FxVec4u(uint32 scalar) { mIntrin = vdupq_n_u32(scalar); }

FX_FORCE_INLINE bool FxVec4u::IsZero() const
{
    const uint32x4_t is_zero_v = vceqzq_u32(mIntrin);
    return vmaxvq_u32(is_zero_v) == 0;
}

FX_FORCE_INLINE void FxVec4u::Set(uint32 x, uint32 y, uint32 z, uint32 w)
{
    const uint32 v[4] = { x, y, z, w };
    mIntrin = vld1q_u32(v);
}

FX_FORCE_INLINE void FxVec4u::Set(uint32 scalar) { mIntrin = vdupq_n_u32(scalar); }


///////////////////////////////
// Vec + Vec Operators
///////////////////////////////

FxVec4u FxVec4u::operator+(const FxVec4u& other) const { return FxVec4u(vaddq_u32(mIntrin, other.mIntrin)); }

FxVec4u FxVec4u::operator-(const FxVec4u& other) const { return FxVec4u(vsubq_u32(mIntrin, other.mIntrin)); }

FxVec4u FxVec4u::operator*(const FxVec4u& other) const { return FxVec4u(vmulq_u32(mIntrin, other.mIntrin)); }


///////////////////////////////
// Vec + Scalar Operators
///////////////////////////////

FX_FORCE_INLINE FxVec4u FxVec4u::operator*(uint32 scalar) const { return FxVec4u(vmulq_n_u32(mIntrin, scalar)); }
FX_FORCE_INLINE FxVec4u& FxVec4u::operator*=(uint32 scalar)
{
    mIntrin = vmulq_n_u32(mIntrin, scalar);
    return *this;
}


FX_FORCE_INLINE FxVec4u& FxVec4u::operator+=(const FxVec4u& other)
{
    mIntrin = vaddq_u32(mIntrin, other.mIntrin);
    return *this;
}

FX_FORCE_INLINE FxVec4u& FxVec4u::operator-=(const FxVec4u& other)
{
    mIntrin = vsubq_u32(mIntrin, other.mIntrin);
    return *this;
}

FX_FORCE_INLINE FxVec4u& FxVec4u::operator*=(const FxVec4u& other)
{
    mIntrin = vmulq_u32(mIntrin, other.mIntrin);
    return *this;
}

FX_FORCE_INLINE FxVec4u& FxVec4u::operator=(const FxVec4u& other)
{
    mIntrin = other.mIntrin;
    return *this;
}


#endif
