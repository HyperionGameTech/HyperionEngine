/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_COLOR_HPP
#define HYPERION_COLOR_HPP

#include <math/Vector4.hpp>

#include <HashCode.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

namespace hyperion {

class alignas(uint32) HYP_API Color
{
public:
    static constexpr uint size = 4;

private:
    friend std::ostream &operator<<(std::ostream &out, const Color &color);

    union {
        uint32 value;
        ubyte bytes[size];
    };

public:
    Color();
    Color(uint32 hex);
    Color(float r, float g, float b, float a = 1.0f);
    explicit Color(float rgba);
    Color(const Color &other);
    Color(const Vec4f &vec);

    HYP_FORCE_INLINE float GetRed() const
        { return float(bytes[0]) / 255.0f; }

    HYP_FORCE_INLINE Color &SetRed(float red)
        { bytes[0] = static_cast<ubyte>(red * 255.0f); return *this; }

    HYP_FORCE_INLINE float GetGreen() const
        { return float(bytes[1]) / 255.0f; }

    HYP_FORCE_INLINE Color &SetGreen(float green)
        { bytes[1] = static_cast<ubyte>(green * 255.0f); return *this; }

    HYP_FORCE_INLINE float GetBlue() const
        { return float(bytes[2]) / 255.0f; }

    HYP_FORCE_INLINE Color &SetBlue(float blue)
        { bytes[2] = static_cast<ubyte>(blue * 255.0f); return *this; }

    HYP_FORCE_INLINE float GetAlpha() const
        { return float(bytes[3]) / 255.0f; }

    HYP_FORCE_INLINE Color &SetAlpha(float alpha)
        { bytes[3] = static_cast<ubyte>(alpha * 255.0f); return *this; }
    
    constexpr float operator[](uint index) const
        { return float(bytes[index]) / 255.0f; }

    Color &operator=(const Color &other);
    Color operator+(const Color &other) const;
    Color &operator+=(const Color &other);
    Color operator-(const Color &other) const;
    Color &operator-=(const Color &other);
    Color operator*(const Color &other) const;
    Color &operator*=(const Color &other);
    Color operator/(const Color &other) const;
    Color &operator/=(const Color &other);
    bool operator==(const Color &other) const;
    bool operator!=(const Color &other) const;

    HYP_FORCE_INLINE bool operator<(const Color &other) const
        { return value < other.value; }

    HYP_FORCE_INLINE explicit operator uint32() const
        { return value; }

    HYP_FORCE_INLINE explicit operator Vec4f() const
        { return Vec4f(GetRed(), GetGreen(), GetBlue(), GetAlpha()); }

    HYP_FORCE_INLINE uint32 Packed() const
        { return value; }

    Color &Lerp(const Color &to, float amt);

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(value);

        return hc;
    }
};

static_assert(sizeof(Color) == sizeof(uint32), "sizeof(Color) must be equal to sizeof uint32");
static_assert(alignof(Color) == alignof(uint32), "alignof(Color) must be equal to alignof uint32");

} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::Color);

#endif
