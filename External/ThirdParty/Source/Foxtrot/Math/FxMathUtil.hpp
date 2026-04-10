#pragma once

#include <Core/FxDefines.hpp>
#include <Core/FxTypes.hpp>

#define FxDegToRad(deg) ((deg) * (M_PI / 180.0f))
#define FxRadToDeg(rad) ((rad) * (180.0f / M_PI))

#include <cmath>

namespace FxMath {

constexpr float32 DegreesToRadians(float32 degrees) { return ((degrees) * static_cast<float32>(M_PI / 180.0)); }
constexpr float32 RadiansToDegrees(float32 radians) { return ((radians) * static_cast<float32>(180.0 / M_PI)); }

void SinCos(float32 rad, float32* out_sine, float32* out_cosine);

FX_FORCE_INLINE float32 RSqrt(float32 x);

FX_FORCE_INLINE float32 Clamp(float32 value, float32 lower, float32 upper) { return fmin(fmax(value, lower), upper); }

FX_FORCE_INLINE float32 SmoothInterpolate(float32 a, float32 b, float32 speed, float32 delta_time)
{
    return std::lerp(a, b, 1.0f - expf(-speed * delta_time));
}

FX_FORCE_INLINE uint64 AlignValue(uint64 value, const uint16 alignto)
{
    // Generic case
    const uint16 remainder = (value % alignto);

    // If the value is not aligned(there is a remainder in the division), offset our value by the missing bytes
    if (remainder != 0) {
        value += (alignto - remainder);
    }

    return value;
}

template <uint32 TAlignTo>
FX_FORCE_INLINE uint64 AlignValue(uint64 value)
{
    // Generic case

    const uint16 remainder = (value % TAlignTo);

    // If the value is not aligned(there is a remainder in the division), offset our value by the missing bytes
    if (remainder != 0) {
        value += (TAlignTo - remainder);
    }

    return value;
}

template <>
FX_FORCE_INLINE uint64 AlignValue<2>(uint64 value)
{
    // Get the bottom four bits (value % 16)
    const uint8 remainder = (value & 0x01);

    if (remainder != 0) {
        ++value;
    }

    return value;
}


template <>
FX_FORCE_INLINE uint64 AlignValue<4>(uint64 value)
{
    // Get the bottom four bits (value % 16)
    const uint8 remainder = (value & 0x03);

    if (remainder != 0) {
        value += (4 - remainder);
    }

    return value;
}


template <>
FX_FORCE_INLINE uint64 AlignValue<8>(uint64 value)
{
    // Get the bottom four bits (value % 16)
    const uint8 remainder = (value & 0x07);

    if (remainder != 0) {
        value += (8 - remainder);
    }

    return value;
}

template <>
FX_FORCE_INLINE uint64 AlignValue<16>(uint64 value)
{
    // Get the bottom four bits (value % 16)
    const uint64 remainder = (value & 0x0F);

    if (remainder != 0) {
        value += (16 - remainder);
    }

    return value;
}

template <>
FX_FORCE_INLINE uint64 AlignValue<32>(uint64 value)
{
    // Get the bottom four bits (value % 32)
    const uint8 remainder = (value & 0x1F);

    if (remainder != 0) {
        value += (32 - remainder);
    }

    return value;
}

template <typename TPtrType, uint32 TAlignTo>
FX_FORCE_INLINE TPtrType AlignPtr(TPtrType ptr)
{
    return reinterpret_cast<TPtrType>(AlignValue<TAlignTo>(reinterpret_cast<uintptr_t>(ptr)));
}

template <typename TPtrType>
FX_FORCE_INLINE TPtrType AlignPtr(TPtrType ptr, uint32 alignto)
{
    return reinterpret_cast<TPtrType>(AlignValue(reinterpret_cast<uintptr_t>(ptr), alignto));
}


} // namespace FxMath

#include "FxMathUtil.inl"
