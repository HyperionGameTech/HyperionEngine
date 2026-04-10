#pragma once

#include <Core/FxTypes.hpp>
#include <format>


#ifdef FX_USE_NEON
#include "FxNeonUtil.hpp"

#include <arm_neon.h>
#elif defined(FX_USE_AVX)
#include "FxSSE.hpp"
#include "FxSSEUtil.hpp"
#endif

class alignas(16) FxVec4f
{
private:
#if defined(FX_USE_NEON)
    using SimdType = float32x4_t;
#elif defined(FX_USE_AVX)
    using SimdType = __m128;
#endif

public:
    static const FxVec4f sZero;
    static const FxVec4f sOne;

public:
    friend class FxMat4f;

    FxVec4f() = default;
    FxVec4f(float32 x, float32 y, float32 z, float32 w);
    FxVec4f(const float32* values);
    explicit FxVec4f(float32 scalar);

    FX_FORCE_INLINE void Set(float32 x, float32 y, float32 z, float32 w);
    FX_FORCE_INLINE void Set(float32 scalar);


    FX_FORCE_INLINE bool IsZero() const;
    FX_FORCE_INLINE bool IsNearZero(const float32 tolerance = 0.00001) const;
    FX_FORCE_INLINE bool IsCloseTo(const FxVec4f& other, const float32 tolerance = 0.00001) const;
#ifdef FX_USE_SIMD
    FX_FORCE_INLINE bool IsCloseTo(const SimdType other, const float32 tolerance = 0.00001) const;
#endif

    FxVec4f& operator=(const FxVec4f& other);

    FX_FORCE_INLINE FxVec4f Normalize() const;
    FX_FORCE_INLINE FxVec4f& NormalizeIP();

    FX_FORCE_INLINE float32 LengthSquared() const;
    FX_FORCE_INLINE bool IsNormalized(float32 tolerance) const { return abs(LengthSquared() - 1.0f) <= tolerance; }


#ifdef FX_USE_NEON
#else
    FX_FORCE_INLINE float32 GetX() const { return X; }
    FX_FORCE_INLINE float32 GetY() const { return Y; }
    FX_FORCE_INLINE float32 GetZ() const { return Z; }
    FX_FORCE_INLINE float32 GetW() const { return W; }
#endif

#ifdef FX_USE_NEON
    template <int TX, int TY, int TZ, int TW>
    FX_FORCE_INLINE static FxVec4f FlipSigns(const FxVec4f& vec)
    {
        return FxVec4f(FxNeon::FlipSigns<TX, TY, TZ, TW>(vec.mIntrin));
    }

#elif FX_USE_AVX
    template <int TX, int TY, int TZ, int TW>
    FX_FORCE_INLINE static FxVec4f FlipSigns(const FxVec4f& vec)
    {
        return FxVec4f(FxSSE::SetSigns<TX, TY, TZ, TW>(vec.mIntrin));
    }
#else
    template <int TX, int TY, int TZ, int TW>
    FX_FORCE_INLINE static Fxvec4f FlipSigns(const FxVec4f& vec)
    {
        constexpr float rx = TX > 0.0 ? vec.X : -vec.X;
        constexpr float ry = TY > 0.0 ? vec.Y : -vec.Y;
        constexpr float rz = TZ > 0.0 ? vec.Z : -vec.Z;
        constexpr float rw = TW > 0.0 ? vec.W : -vec.W;

        return FxVec4f(rx, ry, rz, rw);
    }
#endif

    /////////////////////////////////////
    // Operator overloads
    /////////////////////////////////////

    FX_FORCE_INLINE FxVec4f operator+(const FxVec4f& other) const;
    FX_FORCE_INLINE FxVec4f operator-(const FxVec4f& other) const;
    FX_FORCE_INLINE FxVec4f operator*(const FxVec4f& other) const;
    FX_FORCE_INLINE FxVec4f operator/(const FxVec4f& other) const;

    FX_FORCE_INLINE FxVec4f operator*(float32 scalar) const;
    FX_FORCE_INLINE FxVec4f operator/(float32 scalar) const;

    FxVec4f operator-() const;

    FxVec4f& operator+=(const FxVec4f& other);
    FxVec4f& operator-=(const FxVec4f& other);
    FxVec4f& operator*=(const FxVec4f& other);
    FX_FORCE_INLINE FxVec4f& operator*=(float32 scalar);

#ifdef FX_USE_SIMD
    explicit FxVec4f(SimdType intrin) : mIntrin(intrin) {}

    FxVec4f& operator=(SimdType other)
    {
        mIntrin = other;
        return *this;
    }
#endif

public:
#if FX_USE_SIMD
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
    float32 X, Y, Z, W;
#endif
};


template <>
struct std::formatter<FxVec4f>
{
    auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(const FxVec4f& obj, std::format_context& ctx) const
    {
        return std::format_to(ctx.out(), "({:.04}, {:.04}, {:.04}, {:.04})", obj.X, obj.Y, obj.Z, obj.W);
    }
};

/////////////////////////////////////
// Integer vector classes
/////////////////////////////////////

class alignas(16) FxVec4u
{
private:
#if defined(FX_USE_NEON)
    using SimdType = uint32x4_t;
#elif defined(FX_USE_AVX)
    using SimdType = __m128i;
#endif

public:
    static const FxVec4u sZero;
    static const FxVec4u sOne;

public:
    FxVec4u() = default;
    FxVec4u(uint32 x, uint32 y, uint32 z, uint32 w);
    FxVec4u(const uint32* values);
    explicit FxVec4u(uint32 scalar);

    FX_FORCE_INLINE void Set(uint32 x, uint32 y, uint32 z, uint32 w);
    FX_FORCE_INLINE void Set(uint32 scalar);

    FX_FORCE_INLINE bool IsZero() const;

    FxVec4u& operator=(const FxVec4u& other);

#ifdef FX_USE_NEON
    FX_FORCE_INLINE uint32 GetX() const { return vgetq_lane_u32(mIntrin, 0); }
    FX_FORCE_INLINE uint32 GetY() const { return vgetq_lane_u32(mIntrin, 1); }
    FX_FORCE_INLINE uint32 GetZ() const { return vgetq_lane_u32(mIntrin, 2); }
    FX_FORCE_INLINE uint32 GetW() const { return vgetq_lane_u32(mIntrin, 3); }
#else
    FX_FORCE_INLINE uint32 GetX() const { return X; }
    FX_FORCE_INLINE uint32 GetY() const { return Y; }
    FX_FORCE_INLINE uint32 GetZ() const { return Z; }
    FX_FORCE_INLINE uint32 GetW() const { return W; }
#endif

#ifdef FX_USE_NEON
    template <int TX, int TY, int TZ, int TW>
    FX_FORCE_INLINE static FxVec4u FlipSigns(const FxVec4u& vec)
    {
        return FxVec4u(FxNeon::FlipSigns<TX, TY, TZ, TW>(vec.mIntrin));
    }

#elif FX_USE_AVX
    template <int TX, int TY, int TZ, int TW>
    FX_FORCE_INLINE static FxVec4u FlipSigns(const FxVec4u& vec)
    {
        return FxVec4u(FxSSE::SetSigns<TX, TY, TZ, TW>(vec.mIntrin));
    }
#else
    template <int TX, int TY, int TZ, int TW>
    FX_FORCE_INLINE static FxVec4u FlipSigns(const FxVec4u& vec)
    {
        constexpr float rx = TX > 0.0 ? vec.X : -vec.X;
        constexpr float ry = TY > 0.0 ? vec.Y : -vec.Y;
        constexpr float rz = TZ > 0.0 ? vec.Z : -vec.Z;
        constexpr float rw = TW > 0.0 ? vec.W : -vec.W;

        return FxVec4u(rx, ry, rz, rw);
    }
#endif

    /////////////////////////////////////
    // Operator overloads
    /////////////////////////////////////

    FX_FORCE_INLINE FxVec4u operator+(const FxVec4u& other) const;
    FX_FORCE_INLINE FxVec4u operator-(const FxVec4u& other) const;
    FX_FORCE_INLINE FxVec4u operator*(const FxVec4u& other) const;

    FX_FORCE_INLINE FxVec4u operator*(uint32 scalar) const;

    FxVec4u& operator+=(const FxVec4u& other);
    FxVec4u& operator-=(const FxVec4u& other);
    FxVec4u& operator*=(const FxVec4u& other);
    FX_FORCE_INLINE FxVec4u& operator*=(uint32 scalar);

#ifdef FX_USE_SIMD
    FxVec4u& operator=(const SimdType other)
    {
        mIntrin = other;
        return *this;
    }

    explicit FxVec4u(SimdType intrin) : mIntrin(intrin) {}
#endif

public:
#if FX_USE_SIMD
    union alignas(16)
    {
        SimdType mIntrin;
        uint32 mData[4];
        struct
        {
            uint32 X, Y, Z, W;
        };
    };
#else
    uint32 X, Y, Z, W;
#endif
};


template <>
struct std::formatter<FxVec4u>
{
    auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(const FxVec4u& obj, std::format_context& ctx) const
    {
        return std::format_to(ctx.out(), "({}, {}, {}, {})", obj.X, obj.Y, obj.Z, obj.W);
    }
};


/* Definitions for inline functions */
#include "Impl/Vector/FxVec4_AVX.inl"
#include "Impl/Vector/FxVec4_Fallback.inl"
#include "Impl/Vector/FxVec4_Neon.inl"
