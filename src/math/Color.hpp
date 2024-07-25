/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_COLOR_HPP
#define HYPERION_COLOR_HPP

#include <math/Vector4.hpp>

#include <core/Defines.hpp>

#include <core/utilities/FormatFwd.hpp>

#include <core/memory/Memory.hpp>

#include <util/ByteUtil.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {

class alignas(uint32) HYP_API Color
{
public:
    static constexpr uint size = 4;

public:
    union {
        ubyte   bytes[size];
        struct {
            ubyte r;
            ubyte g;
            ubyte b;
            ubyte a;
        };
        struct {
            ubyte x;
            ubyte y;
            ubyte z;
            ubyte w;
        };
    };

    Color();
    explicit Color(uint32 hex);
    Color(float r, float g, float b, float a = 1.0f);
    Color(const Vec4f &vec);

    Color(const Color &other);
    Color &operator=(const Color &other);

    [[nodiscard]]
    HYP_FORCE_INLINE
    float GetRed() const
        { return float(bytes[0]) / 255.0f; }

    HYP_FORCE_INLINE
    Color &SetRed(float red)
        { bytes[0] = static_cast<ubyte>(red * 255.0f); return *this; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    float GetGreen() const
        { return float(bytes[1]) / 255.0f; }

    HYP_FORCE_INLINE
    Color &SetGreen(float green)
        { bytes[1] = static_cast<ubyte>(green * 255.0f); return *this; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    float GetBlue() const
        { return float(bytes[2]) / 255.0f; }

    HYP_FORCE_INLINE
    Color &SetBlue(float blue)
        { bytes[2] = static_cast<ubyte>(blue * 255.0f); return *this; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    float GetAlpha() const
        { return float(bytes[3]) / 255.0f; }

    HYP_FORCE_INLINE
    Color &SetAlpha(float alpha)
        { bytes[3] = static_cast<ubyte>(alpha * 255.0f); return *this; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr float operator[](uint index) const
        { return float(bytes[index]) / 255.0f; }
    
    [[nodiscard]]
    Color operator+(const Color &other) const;
    
    [[nodiscard]]
    Color &operator+=(const Color &other);
    
    [[nodiscard]]
    Color operator-(const Color &other) const;
    
    [[nodiscard]]
    Color &operator-=(const Color &other);
    
    [[nodiscard]]
    Color operator*(const Color &other) const;
    
    [[nodiscard]]
    Color &operator*=(const Color &other);
    
    [[nodiscard]]
    Color operator/(const Color &other) const;

    [[nodiscard]]
    Color &operator/=(const Color &other);

    [[nodiscard]]
    bool operator==(const Color &other) const;

    [[nodiscard]]
    bool operator!=(const Color &other) const;

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator<(const Color &other) const
        { return Memory::MemCmp(bytes, other.bytes, size) < 0; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    explicit operator uint32() const
    {
        uint32 result;
        Memory::MemCpy(&result, bytes, size);
        return result;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    explicit operator Vec4f() const
        { return Vec4f(GetRed(), GetGreen(), GetBlue(), GetAlpha()); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    uint32 Packed() const
        { return uint32(*this); }

    [[nodiscard]]
    Color &Lerp(const Color &to, float amt);

    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(uint32(*this));
        return hc;
    }
};

static_assert(sizeof(Color) == sizeof(uint32), "sizeof(Color) must be equal to sizeof uint32");
static_assert(alignof(Color) == alignof(uint32), "alignof(Color) must be equal to alignof uint32");

namespace utilities {

template <class StringType>
struct Formatter<StringType, Color>
{
    constexpr auto operator()(const Color &value) const
    {
        return Formatter<StringType, Vec4f>()(Vec4f(value.GetRed(), value.GetGreen(), value.GetBlue(), value.GetAlpha()));
    }
};

} // namespace utilities

} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::Color);

#endif
