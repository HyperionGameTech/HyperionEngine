#pragma once

#include <Core/FxDefines.hpp>
#include <Core/FxTypes.hpp>
#include <format>

#ifdef FX_USE_NEON
#include "FxNeonUtil.hpp"

#include <arm_neon.h>
#elif defined(FX_USE_AVX)
#include "FxSSE.hpp"
#include "FxSSEUtil.hpp"

#endif

class FxVec4f;

namespace JPH {
class Vec3;
using RVec3 = Vec3;
} // namespace JPH

class alignas(16) FxVec3f
{
private:
#if defined(FX_USE_NEON)
    using SimdType = float32x4_t;
#elif defined(FX_USE_AVX)
    using SimdType = __m128;
#endif

public:
    static const FxVec3f sZero;
    static const FxVec3f sOne;

    static const FxVec3f sUp;
    static const FxVec3f sRight;
    static const FxVec3f sForward;

public:
    FxVec3f() = default;
    FX_FORCE_INLINE FxVec3f(float32 x, float32 y, float32 z);
    FX_FORCE_INLINE FxVec3f(const float32* values);
    FxVec3f(const JPH::Vec3& other);

    FX_FORCE_INLINE explicit FxVec3f(float32 scalar);

#ifdef FX_USE_SIMD
    explicit FxVec3f(SimdType intrin) : mIntrin(intrin) {}
    FxVec3f(const FxVec4f& other);
#else
    FxVec3f(const FxVec4f& other) : X(other.X), Y(other.Y), Z(other.Z) {}
#endif


    FX_FORCE_INLINE void Set(float32 x, float32 y, float32 z);

    void ToJoltVec3(JPH::RVec3& jolt_vec) const;
    void FromJoltVec3(const JPH::RVec3& jolt_vec);

    static FX_FORCE_INLINE FxVec3f FromDifference(const float32* a, const float32* b)
    {
        const float32 vx = (a[0] - b[0]);
        const float32 vy = (a[1] - b[1]);
        const float32 vz = (a[2] - b[2]);

        return FxVec3f(vx, vy, vz);
    }

    FX_FORCE_INLINE void CopyTo(float32* output)
    {
        output[0] = X;
        output[1] = Y;
        output[2] = Z;
    }

    FX_FORCE_INLINE float32 DistanceTo(const FxVec3f& other) const { return (other - *this).Length(); }

    FX_FORCE_INLINE bool IntersectsSphere(const FxVec3f& sphere_center, float32 sphere_radius) const
    {
        const FxVec3f diff = (*this) - sphere_center;
        const float dist2 = diff.Dot(diff);

        return dist2 <= sphere_radius * sphere_radius;
    }

    FX_FORCE_INLINE bool operator==(const FxVec3f& other) const;
    bool operator==(const JPH::Vec3& other) const;

    FX_FORCE_INLINE bool IsZero() const;
    FX_FORCE_INLINE bool IsNearZero(const float32 tolerance = 0.00001) const;
    FX_FORCE_INLINE bool IsCloseTo(const FxVec3f& other, const float32 tolerance = 0.00001) const;
#ifdef FX_USE_SIMD
    FX_FORCE_INLINE bool IsCloseTo(const SimdType other, const float32 tolerance = 0.00001) const;
#endif
    bool IsCloseTo(const JPH::Vec3& other, const float32 threshold = 0.001) const;

    FX_FORCE_INLINE FxVec3f Normalize() const;

    /**
     * Normalizes the vector in place (modifies the source vector.)
     * @returns The modified vector.
     */
    FX_FORCE_INLINE FxVec3f& NormalizeIP();

    FX_FORCE_INLINE float32 Length() const;
    FxVec3f Cross(const FxVec3f& other) const;
    FxVec3f CrossSlow(const FxVec3f& other) const;

    FX_FORCE_INLINE float32 Dot(const FxVec3f& other) const;
#ifdef FX_USE_SIMD
    FX_FORCE_INLINE float32 Dot(SimdType other) const;
#endif

    FX_FORCE_INLINE static FxVec3f Min(const FxVec3f& a, const FxVec3f& b);
    FX_FORCE_INLINE static FxVec3f Max(const FxVec3f& a, const FxVec3f& b);
    FX_FORCE_INLINE static FxVec3f Clamp(const FxVec3f& v, const FxVec3f& min, const FxVec3f& max);
    FX_FORCE_INLINE static FxVec3f Lerp(const FxVec3f& a, const FxVec3f& b, const float f);
    FX_FORCE_INLINE FxVec3f& LerpIP(const FxVec3f& dest, const float step);

    FX_FORCE_INLINE FxVec3f& SmoothInterpolate(const FxVec3f& dest, const float speed, const float delta_time)
    {
        LerpIP(dest, 1.0f - expf(-speed * delta_time));
        return *this;
    }

    void Print() const;

#ifdef FX_USE_NEON
    // Neon has optimized fetch's when using the intrinsic
    FX_FORCE_INLINE float32 GetX() const { return vgetq_lane_f32(mIntrin, 0); }
    FX_FORCE_INLINE float32 GetY() const { return vgetq_lane_f32(mIntrin, 1); }
    FX_FORCE_INLINE float32 GetZ() const { return vgetq_lane_f32(mIntrin, 2); }
#else
    FX_FORCE_INLINE float32 GetX() const { return X; }
    FX_FORCE_INLINE float32 GetY() const { return Y; }
    FX_FORCE_INLINE float32 GetZ() const { return Z; }
#endif

#ifdef FX_USE_NEON
    template <int TX, int TY, int TZ, int TW>
    FX_FORCE_INLINE static FxVec3f FlipSigns(const FxVec3f& vec)
    {
        return FxVec3f(FxNeon::FlipSigns<TX, TY, TZ, TW>(vec.mIntrin));
    }

#elif FX_USE_AVX
    template <int TX, int TY, int TZ, int TW>
    FX_FORCE_INLINE static FxVec3f FlipSigns(const FxVec3f& vec)
    {
        return FxVec3f(FxSSE::SetSigns<TX, TY, TZ, TW>(vec.mIntrin));
    }
#else
    template <int TX, int TY, int TZ, int TW>
    FX_FORCE_INLINE static FxVec3f FlipSigns(const FxVec3f& vec)
    {
        constexpr float rx = TX > 0.0 ? vec.X : -vec.X;
        constexpr float ry = TY > 0.0 ? vec.Y : -vec.Y;
        constexpr float rz = TZ > 0.0 ? vec.Z : -vec.Z;
        constexpr float rw = TW > 0.0 ? vec.W : -vec.W;

        return FxVec3f(rx, ry, rz, rw);
    }
#endif

    /////////////////////////////////////
    // Operator overloads
    /////////////////////////////////////

    FX_FORCE_INLINE FxVec3f operator+(const FxVec3f& other) const;
    FX_FORCE_INLINE FxVec3f operator-(const FxVec3f& other) const;
    FX_FORCE_INLINE FxVec3f operator*(const FxVec3f& other) const;
    FX_FORCE_INLINE FxVec3f operator/(const FxVec3f& other) const;

    FX_FORCE_INLINE FxVec3f operator*(float32 scalar) const;
    FX_FORCE_INLINE FxVec3f operator/(float32 scalar) const;

    FX_FORCE_INLINE FxVec3f operator-() const;

    FX_FORCE_INLINE FxVec3f& operator+=(const FxVec3f& other);
    FX_FORCE_INLINE FxVec3f& operator-=(float32 scalar);
    FX_FORCE_INLINE FxVec3f& operator-=(const FxVec3f& other);
    FX_FORCE_INLINE FxVec3f& operator*=(const FxVec3f& other);

#ifdef FX_USE_SIMD
    FxVec3f& operator=(const SimdType& other)
    {
        mIntrin = other;
        return *this;
    }

    FxVec3f& operator=(const SimdType other)
    {
        mIntrin = other;
        return *this;
    }

    operator SimdType() const { return mIntrin; }
#endif

public:
#if defined(FX_USE_SIMD)
    union alignas(16)
    {
        SimdType mIntrin;
        float32 mData[4];

        struct
        {
            float32 X, Y, Z, W;
        };
    };
#else
    struct
    {
        float32 X, Y, Z;
    };
#endif
};

template <>
struct std::formatter<FxVec3f>
{
    auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(const FxVec3f& obj, std::format_context& ctx) const
    {
        return std::format_to(ctx.out(), "({:.04}, {:.04}, {:.04})", obj.X, obj.Y, obj.Z);
    }
};


#include "Impl/Vector/FxVec3_AVX.inl"
#include "Impl/Vector/FxVec3_Fallback.inl"
#include "Impl/Vector/FxVec3_Neon.inl"
