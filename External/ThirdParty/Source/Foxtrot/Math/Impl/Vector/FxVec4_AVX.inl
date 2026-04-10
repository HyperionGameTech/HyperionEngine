#pragma once

#include <Core/FxDefines.hpp>

#ifdef FX_USE_AVX
#include <Math/FxSSE.hpp>
#include <Math/FxSSEUtil.hpp>
#include <Math/FxVec4.hpp>

FX_FORCE_INLINE FxVec4f::FxVec4f(float32 x, float32 y, float32 z, float32 w)
{
    const float32 values[4] = { x, y, z, w };
    mIntrin = _mm_loadu_ps(values);
}

FX_FORCE_INLINE FxVec4f::FxVec4f(const float32* values) { mIntrin = _mm_loadu_ps(values); }
FX_FORCE_INLINE FxVec4f::FxVec4f(float32 scalar) { mIntrin = _mm_set1_ps(scalar); }


FX_FORCE_INLINE float32 FxVec4f::LengthSquared() const { return FxSSE::LengthSquared(mIntrin); }

FX_FORCE_INLINE void FxVec4f::Set(float32 x, float32 y, float32 z, float32 w) { mIntrin = _mm_setr_ps(x, y, z, w); }
FX_FORCE_INLINE void FxVec4f::Set(float32 scalar) { mIntrin = _mm_set1_ps(scalar); }

///////////////////////////////
// Vec + Vec Operators
///////////////////////////////

FxVec4f FxVec4f::operator+(const FxVec4f& other) const { return FxVec4f(_mm_add_ps(mIntrin, other.mIntrin)); }

FxVec4f FxVec4f::operator-(const FxVec4f& other) const { return FxVec4f(_mm_sub_ps(mIntrin, other.mIntrin)); }

FxVec4f FxVec4f::operator*(const FxVec4f& other) const { return FxVec4f(_mm_mul_ps(mIntrin, other.mIntrin)); }

FxVec4f FxVec4f::operator/(const FxVec4f& other) const { return FxVec4f(_mm_div_ps(mIntrin, other.mIntrin)); }

///////////////////////////////
// Vec + Scalar Operators
///////////////////////////////

FX_FORCE_INLINE FxVec4f FxVec4f::operator*(float32 scalar) const
{
    const __m128 scalar_v = _mm_set1_ps(scalar);
    return FxVec4f(_mm_mul_ps(mIntrin, scalar_v));
}

FX_FORCE_INLINE FxVec4f FxVec4f::operator/(float32 scalar) const
{
    const __m128 scalar_v = _mm_set1_ps(scalar);
    return FxVec4f(_mm_div_ps(mIntrin, scalar_v));
}

FX_FORCE_INLINE FxVec4f FxVec4f::operator-() const { return FxVec4f(_mm_xor_ps(mIntrin, _mm_set1_ps(-0.0f))); }


FX_FORCE_INLINE FxVec4f& FxVec4f::operator+=(const FxVec4f& other)
{
    mIntrin = _mm_add_ps(mIntrin, other.mIntrin);
    return *this;
}

FX_FORCE_INLINE FxVec4f& FxVec4f::operator-=(const FxVec4f& other)
{
    mIntrin = _mm_sub_ps(mIntrin, other.mIntrin);
    return *this;
}

FX_FORCE_INLINE FxVec4f& FxVec4f::operator*=(const FxVec4f& other)
{
    mIntrin = _mm_mul_ps(mIntrin, other.mIntrin);
    return *this;
}

FX_FORCE_INLINE FxVec4f& FxVec4f::operator*=(float32 scalar)
{
    const __m128 scalar_v = _mm_set1_ps(scalar);
    mIntrin = _mm_mul_ps(mIntrin, scalar_v);
    return *this;
}

FX_FORCE_INLINE FxVec4f& FxVec4f::operator=(const FxVec4f& other)
{
    mIntrin = other.mIntrin;
    return *this;
}

#endif // #ifdef FX_USE_AVX
