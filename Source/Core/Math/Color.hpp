/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Math/Vector4.hpp>
#include <Core/Math/MathUtil.hpp>

#include <Core/Defines.hpp>

#include <Core/Utilities/FormatFwd.hpp>

#include <Core/Reflection/ObjectMacros.hpp>

#include <Core/Memory/Memory.hpp>

#include <Core/Utilities/ByteUtil.hpp>

#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

HYP_STRUCT()
class alignas(uint32) CORE_API Color
{
public:
    HYP_STRUCT_BODY(Color);

    static constexpr uint32 NumComponents = 4;

    union
    {
        ubyte bytes[NumComponents];

        struct
        {
            ubyte r;
            ubyte g;
            ubyte b;
            ubyte a;
        };

        struct
        {
            ubyte x;
            ubyte y;
            ubyte z;
            ubyte w;
        };

        uint32 color32;
    };

    constexpr Color()
        : bytes {} // all zero
    {
    }

    constexpr Color(uint32 hex)
        : Color(ByteUtil::UnpackVec4f(hex))
    {
    }

    constexpr Color(float r, float g, float b, float a = 1.0f)
        : Color(Vec4f(r, g, b, a))
    {
    }

    constexpr Color(const Vec4f& vec)
    {
        bytes[0] = ubyte(MathUtil::Clamp(vec.x * 255.0f, 0.0f, 255.0f));
        bytes[1] = ubyte(MathUtil::Clamp(vec.y * 255.0f, 0.0f, 255.0f));
        bytes[2] = ubyte(MathUtil::Clamp(vec.z * 255.0f, 0.0f, 255.0f));
        bytes[3] = ubyte(MathUtil::Clamp(vec.w * 255.0f, 0.0f, 255.0f));
    }

    constexpr Color(const Color& other) = default;
    Color& operator=(const Color& other) = default;

    HYP_METHOD(Property = "Red", Serialize = true)
    HYP_FORCE_INLINE float GetRed() const
    {
        return float(bytes[0]) / 255.0f;
    }

    HYP_METHOD(Property = "Red", Serialize = true)
    HYP_FORCE_INLINE Color& SetRed(float red)
    {
        bytes[0] = static_cast<ubyte>(MathUtil::Clamp(red * 255.0f, 0.0f, 255.0f));
        return *this;
    }

    HYP_METHOD(Property = "Green", Serialize = true)
    HYP_FORCE_INLINE float GetGreen() const
    {
        return float(bytes[1]) / 255.0f;
    }

    HYP_METHOD(Property = "Green", Serialize = true)
    HYP_FORCE_INLINE Color& SetGreen(float green)
    {
        bytes[1] = static_cast<ubyte>(MathUtil::Clamp(green * 255.0f, 0.0f, 255.0f));
        return *this;
    }

    HYP_METHOD(Property = "Blue", Serialize = true)
    HYP_FORCE_INLINE float GetBlue() const
    {
        return float(bytes[2]) / 255.0f;
    }

    HYP_METHOD(Property = "Blue", Serialize = true)
    HYP_FORCE_INLINE Color& SetBlue(float blue)
    {
        bytes[2] = static_cast<ubyte>(MathUtil::Clamp(blue * 255.0f, 0.0f, 255.0f));
        return *this;
    }

    HYP_METHOD(Property = "Alpha", Serialize = true)
    HYP_FORCE_INLINE float GetAlpha() const
    {
        return float(bytes[3]) / 255.0f;
    }

    HYP_METHOD(Property = "Alpha", Serialize = true)
    HYP_FORCE_INLINE Color& SetAlpha(float alpha)
    {
        bytes[3] = static_cast<ubyte>(MathUtil::Clamp(alpha * 255.0f, 0.0f, 255.0f));
        return *this;
    }

    HYP_FORCE_INLINE constexpr float operator[](uint32 index) const
    {
        return float(bytes[index]) / 255.0f;
    }

    Color operator+(const Color& other) const;
    Color& operator+=(const Color& other);
    Color operator-(const Color& other) const;
    Color& operator-=(const Color& other);
    Color operator*(const Color& other) const;
    Color& operator*=(const Color& other);
    Color operator/(const Color& other) const;
    Color& operator/=(const Color& other);

    bool operator==(const Color& other) const;
    bool operator!=(const Color& other) const;

    HYP_FORCE_INLINE constexpr bool operator<(const Color& other) const
    {
        return color32 < other.color32;
    }

    HYP_FORCE_INLINE constexpr explicit operator uint32() const
    {
        return color32;
    }

    HYP_FORCE_INLINE explicit operator Vec4f() const
    {
        return Vec4f(GetRed(), GetGreen(), GetBlue(), GetAlpha());
    }

    HYP_FORCE_INLINE constexpr uint32 Packed() const
    {
        return uint32(*this);
    }

    Color& Lerp(const Color& to, float amt);

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(color32);
    }

    static constexpr Color Black()
    {
        return { 0, 0, 0, 1 };
    }

    static constexpr Color White()
    {
        return { 1, 1, 1, 1 };
    }

    static constexpr Color Red()
    {
        return { 1, 0, 0, 1 };
    }

    static constexpr  Color Green()
    {
        return { 0, 1, 0, 1 };
    }

    static constexpr Color Blue()
    {
        return { 0, 0, 1, 1 };
    }

    static constexpr Color Yellow()
    {
        return { 1, 1, 0, 1 };
    }

    static constexpr Color Cyan()
    {
        return { 0, 1, 1, 1 };
    }

    static constexpr Color Magenta()
    {
        return { 1, 0, 1, 1 };
    }

    static constexpr Color Gray()
    {
        return { 0.5f, 0.5f, 0.5f, 1 };
    }

    static constexpr Color DarkGray()
    {
        return { 0.25f, 0.25f, 0.25f, 1 };
    }

    static constexpr  Color LightGray()
    {
        return { 0.75f, 0.75f, 0.75f, 1 };
    }

    static constexpr Color Transparent()
    {
        return { 0, 0, 0, 0 };
    }
};

static_assert(sizeof(Color) == sizeof(uint32), "sizeof(Color) must be equal to sizeof uint32");
static_assert(alignof(Color) == alignof(uint32), "alignof(Color) must be equal to alignof uint32");

namespace utilities {

template <class StringType>
struct Formatter<StringType, Color>
{
    template <class OutType>
    void Append(OutType& out, const Color& value) const
    {
        Formatter<StringType, Vec4f> {}.Append(out, Vec4f(value.GetRed(), value.GetGreen(), value.GetBlue(), value.GetAlpha()));
    }

    auto operator()(const Color& value) const
    {
        StringType result;
        Append(result, value);

        return result;
    }
};

} // namespace utilities

} // namespace Hyperion
