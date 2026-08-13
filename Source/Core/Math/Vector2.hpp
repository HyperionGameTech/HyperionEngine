/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Utilities/FormatFwd.hpp>

#include <Core/HashCode.hpp>
#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <cmath>
#include <cstdio>

namespace Hyperion {

namespace math {
template <class T>
struct alignas(alignof(T) * 2) Vec2
{
    static constexpr uint32 size = 2;

    using Type = T;

    union
    {
        struct
        {
            T x, y;
        };

        T values[2];
    };

    constexpr Vec2()
        : x(0),
          y(0)
    {
    }

    constexpr Vec2(T xy)
        : x(xy),
          y(xy)
    {
    }

    constexpr Vec2(T x, T y)
        : x(x),
          y(y)
    {
    }

    constexpr Vec2(const Vec2& other) = default;
    constexpr Vec2& operator=(const Vec2& other) = default;
    constexpr Vec2(Vec2&& other) noexcept = default;
    constexpr Vec2& operator=(Vec2&& other) noexcept = default;
    ~Vec2() = default;

    HYP_FORCE_INLINE constexpr T GetX() const
    {
        return x;
    }

    HYP_FORCE_INLINE constexpr T& GetX()
    {
        return x;
    }

    HYP_FORCE_INLINE constexpr T GetY() const
    {
        return y;
    }

    HYP_FORCE_INLINE constexpr T& GetY()
    {
        return y;
    }

    HYP_FORCE_INLINE Vec2& SetX(T x)
    {
        this->x = x;
        return *this;
    }

    HYP_FORCE_INLINE Vec2& SetY(T y)
    {
        this->y = y;
        return *this;
    }

    constexpr T& operator[](size_t index)
    {
        return values[index];
    }

    constexpr T operator[](size_t index) const
    {
        return values[index];
    }

    constexpr Vec2 operator+(const Vec2& other) const
    {
        return { x + other.x, y + other.y };
    }

    Vec2& operator+=(const Vec2& other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }

    constexpr Vec2 operator-(const Vec2& other) const
    {
        return { x - other.x, y - other.y };
    }

    Vec2& operator-=(const Vec2& other)
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    constexpr Vec2 operator*(const Vec2& other) const
    {
        return { x * other.x, y * other.y };
    }

    Vec2& operator*=(const Vec2& other)
    {
        x *= other.x;
        y *= other.y;
        return *this;
    }

    constexpr Vec2 operator*(int i) const
    {
        return Vec2 { Type(x * i), Type(y * i) };
    }

    Vec2& operator*=(int i)
    {
        x *= i;
        y *= i;
        return *this;
    }

    constexpr Vec2 operator*(uint32 u) const
    {
        return Vec2 { Type(x * u), Type(y * u) };
    }

    Vec2& operator*=(uint32 u)
    {
        x *= u;
        y *= u;
        return *this;
    }

    constexpr Vec2 operator*(float f) const
    {
        return Vec2 { Type(x * f), Type(y * f) };
    }

    Vec2& operator*=(float f)
    {
        x *= f;
        y *= f;
        return *this;
    }

    constexpr Vec2 operator/(int i) const
    {
        return Vec2 { Type(x / i), Type(y / i) };
    }

    Vec2& operator/=(int i)
    {
        x /= i;
        y /= i;
        return *this;
    }

    constexpr Vec2 operator/(uint32 u) const
    {
        return Vec2 { Type(x / u), Type(y / u) };
    }

    Vec2& operator/=(uint32 u)
    {
        x /= u;
        y /= u;
        return *this;
    }

    constexpr Vec2 operator/(float f) const
    {
        return Vec2 { Type(x / f), Type(y / f) };
    }

    Vec2& operator/=(float f)
    {
        x /= f;
        y /= f;
        return *this;
    }

    constexpr Vec2 operator/(const Vec2& other) const
    {
        return { x / other.x, y / other.y };
    }

    Vec2& operator/=(const Vec2& other)
    {
        x /= other.x;
        y /= other.y;
        return *this;
    }

    constexpr Vec2 operator%(const Vec2& other) const
    {
        return { x % other.x, y % other.y };
    }

    Vec2& operator%=(const Vec2& other)
    {
        x %= other.x;
        y %= other.y;
        return *this;
    }

    constexpr Vec2 operator&(const Vec2& other) const
    {
        return { x & other.x, y & other.y };
    }

    Vec2& operator&=(const Vec2& other)
    {
        x &= other.x;
        y &= other.y;
        return *this;
    }

    constexpr Vec2 operator|(const Vec2& other) const
    {
        return { x | other.x, y | other.y };
    }

    Vec2& operator|=(const Vec2& other)
    {
        x |= other.x;
        y |= other.y;
        return *this;
    }

    constexpr Vec2 operator^(const Vec2& other) const
    {
        return { x ^ other.x, y ^ other.y };
    }

    Vec2& operator^=(const Vec2& other)
    {
        x ^= other.x;
        y ^= other.y;
        return *this;
    }

    constexpr bool operator==(const Vec2& other) const
    {
        return x == other.x && y == other.y;
    }

    constexpr bool operator!=(const Vec2& other) const
    {
        return x != other.x || y != other.y;
    }

    constexpr Vec2 operator-() const
    {
        return operator*(Type(-1));
    }

    constexpr bool operator<(const Vec2& other) const
    {
        if (x != other.x)
            return x < other.x;
        if (y != other.y)
            return y < other.y;

        return false;
    }

    HYP_FORCE_INLINE constexpr Type Avg() const
    {
        return (x + y) / Type(size);
    }

    HYP_FORCE_INLINE constexpr Type Sum() const
    {
        return x + y;
    }

    HYP_FORCE_INLINE constexpr Type Volume() const
    {
        return x * y;
    }

    HYP_FORCE_INLINE constexpr Type Max() const
    {
        return x > y ? x : y;
    }

    HYP_FORCE_INLINE constexpr Type Min() const
    {
        return x < y ? x : y;
    }

    static Vec2 Zero()
    {
        return Vec2(0, 0);
    }

    static Vec2 One()
    {
        return Vec2(1, 1);
    }

    static Vec2 UnitX()
    {
        return Vec2(1, 0);
    }

    static Vec2 UnitY()
    {
        return Vec2(0, 1);
    }

    template <class Ty>
    constexpr explicit operator Vec2<Ty>() const
    {
        return {
            static_cast<Ty>(x),
            static_cast<Ty>(y)
        };
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode()
            .Combine(x)
            .Combine(y);
    }
};

template <>
struct alignas(alignof(float) * 2) CORE_API Vec2<float>
{
    using Type = float;

    static constexpr uint32 size = 2;

    union
    {
        struct
        {
            float x, y;
        };

        float values[2];
    };

    constexpr Vec2()
        : x(0.0f),
          y(0.0f)
    {
    }

    constexpr Vec2(float xy)
        : x(xy),
          y(xy)
    {
    }

    constexpr Vec2(float x, float y)
        : x(x),
          y(y)
    {
    }

    constexpr Vec2(const Vec2& other) = default;
    constexpr Vec2& operator=(const Vec2& other) = default;

    HYP_FORCE_INLINE constexpr float GetX() const
    {
        return x;
    }

    HYP_FORCE_INLINE constexpr float& GetX()
    {
        return x;
    }

    HYP_FORCE_INLINE constexpr float GetY() const
    {
        return y;
    }

    HYP_FORCE_INLINE constexpr float& GetY()
    {
        return y;
    }

    HYP_FORCE_INLINE Vec2& SetX(float x)
    {
        this->x = x;
        return *this;
    }

    HYP_FORCE_INLINE Vec2& SetY(float y)
    {
        this->y = y;
        return *this;
    }

    constexpr float& operator[](size_t index)
    {
        return values[index];
    }

    constexpr float operator[](size_t index) const
    {
        return values[index];
    }

    explicit operator bool() const
    {
        return Sum() != 0.0f;
    }

    template <class U>
    explicit operator Vec2<U>() const
    {
        return Vec2<U>(static_cast<U>(x), static_cast<U>(y));
    }

    constexpr Vec2 operator+(const Vec2& other) const
    {
        return { x + other.x, y + other.y };
    }

    Vec2& operator+=(const Vec2& other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }

    constexpr Vec2 operator-(const Vec2& other) const
    {
        return { x - other.x, y - other.y };
    }

    Vec2& operator-=(const Vec2& other)
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    constexpr Vec2 operator*(const Vec2& other) const
    {
        return { x * other.x, y * other.y };
    }

    Vec2& operator*=(const Vec2& other)
    {
        x *= other.x;
        y *= other.y;
        return *this;
    }

    constexpr Vec2 operator*(int i) const
    {
        return Vec2 { Type(x * i), Type(y * i) };
    }

    Vec2& operator*=(int i)
    {
        x *= i;
        y *= i;
        return *this;
    }

    constexpr Vec2 operator*(uint32 u) const
    {
        return Vec2 { Type(x * u), Type(y * u) };
    }

    Vec2& operator*=(uint32 u)
    {
        x *= u;
        y *= u;
        return *this;
    }

    constexpr Vec2 operator*(float f) const
    {
        return Vec2 { Type(x * f), Type(y * f) };
    }

    Vec2& operator*=(float f)
    {
        x *= f;
        y *= f;
        return *this;
    }

    constexpr Vec2 operator/(const Vec2& other) const
    {
        return Vec2 { x / other.x, y / other.y };
    }

    Vec2& operator/=(const Vec2& other)
    {
        x /= other.x;
        y /= other.y;
        return *this;
    }

    constexpr Vec2 operator/(int i) const
    {
        return Vec2 { Type(x / i), Type(y / i) };
    }

    Vec2& operator/=(int i)
    {
        x /= i;
        y /= i;
        return *this;
    }

    constexpr Vec2 operator/(uint32 u) const
    {
        return Vec2 { Type(x / u), Type(y / u) };
    }

    Vec2& operator/=(uint32 u)
    {
        x /= u;
        y /= u;
        return *this;
    }

    constexpr Vec2 operator/(float f) const
    {
        return Vec2 { Type(x / f), Type(y / f) };
    }

    Vec2& operator/=(float f)
    {
        x /= f;
        y /= f;
        return *this;
    }

    constexpr bool operator==(const Vec2& other) const
    {
        return x == other.x && y == other.y;
    }

    constexpr bool operator!=(const Vec2& other) const
    {
        return x != other.x || y != other.y;
    }

    constexpr Vec2 operator-() const
    {
        return operator*(-1.0f);
    }

    constexpr bool operator<(const Vec2& other) const
    {
        if (x != other.x)
            return x < other.x;
        if (y != other.y)
            return y < other.y;

        return false;
    }

    HYP_FORCE_INLINE constexpr float LengthSquared() const
    {
        return x * x + y * y;
    }

    HYP_FORCE_INLINE float Length() const
    {
        return std::sqrt(LengthSquared());
    }

    HYP_FORCE_INLINE constexpr float Avg() const
    {
        return (x + y) / 2.0f;
    }

    HYP_FORCE_INLINE constexpr float Sum() const
    {
        return x + y;
    }

    HYP_FORCE_INLINE constexpr float Volume() const
    {
        return x * y;
    }

    HYP_FORCE_INLINE constexpr float Max() const
    {
        return x > y ? x : y;
    }

    HYP_FORCE_INLINE constexpr float Min() const
    {
        return x < y ? x : y;
    }

    HYP_FORCE_INLINE constexpr bool IsZero() const
    {
        return (x == 0 && y == 0);
    }

    float Distance(const Vec2& other) const;
    float DistanceSquared(const Vec2& other) const;

    Vec2& Normalize();
    Vec2& Lerp(const Vec2& to, const float amt);
    float Dot(const Vec2& other) const;

    static Vec2 Abs(const Vec2&);
    static Vec2 Round(const Vec2&);
    static Vec2 Clamp(const Vec2&, float min, float max);
    static Vec2 Min(const Vec2& a, const Vec2& b);
    static Vec2 Max(const Vec2& a, const Vec2& b);

    static Vec2 Zero()
    {
        return Vec2(0.0f, 0.0f);
    }

    static Vec2 One()
    {
        return Vec2(1.0f, 1.0f);
    }

    static Vec2 UnitX()
    {
        return Vec2(1.0f, 0.0f);
    }

    static Vec2 UnitY()
    {
        return Vec2(0.0f, 1.0f);
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode()
            .Combine(x)
            .Combine(y);
    }
};

} // namespace math

template <class T>
using Vec2 = math::Vec2<T>;

using Vec2f = Vec2<float>;
using Vec2i = Vec2<int>;
using Vec2u = Vec2<uint32>;

static_assert(sizeof(Vec2f) == 8);
static_assert(sizeof(Vec2i) == 8);
static_assert(sizeof(Vec2u) == 8);

template <class T>
inline constexpr bool isVec2 = false;

template <>
inline constexpr bool isVec2<Vec2f> = true;

template <>
inline constexpr bool isVec2<Vec2i> = true;

template <>
inline constexpr bool isVec2<Vec2u> = true;

// transitional typedef
using Vector2 = Vec2f;

// Format specialization

namespace utilities {

template <class StringType, class T>
struct Formatter<StringType, math::Vec2<T>>
{
    HYP_FORCE_INLINE static const char* GetFormatString()
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            static const char* formatString = "[%f %f]";

            return formatString;
        }
        else if constexpr (std::is_integral_v<T> && std::is_signed_v<T> && sizeof(T) <= 4)
        {
            static const char* formatString = "[%d %d]";

            return formatString;
        }
        else if constexpr (std::is_integral_v<T> && std::is_signed_v<T> && sizeof(T) <= 8)
        {
            static const char* formatString = "[%lld %lld]";

            return formatString;
        }
        else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T> && sizeof(T) <= 4)
        {
            static const char* formatString = "[%u %u]";

            return formatString;
        }
        else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T> && sizeof(T) <= 8)
        {
            static const char* formatString = "[%llu %llu]";

            return formatString;
        }
        else
        {
            static_assert(always_fail_v<T>, "Cannot format Vec2 type: unknown inner type");
        }
    }

    auto operator()(const math::Vec2<T>& value) const
    {
        ubyte inlineBuf[1024];
        ubyte* buf = &inlineBuf[0];

        int resultSize = std::snprintf(reinterpret_cast<char*>(buf), 1024, GetFormatString(), value.x, value.y) + 1;

        if (resultSize > GetArrayCount(inlineBuf))
        {
            buf = new ubyte[resultSize];

            resultSize = std::snprintf(reinterpret_cast<char*>(buf), resultSize, GetFormatString(), value.x, value.y) + 1;

            StringType res(reinterpret_cast<char*>(buf), reinterpret_cast<char*>(buf + resultSize));

            delete[] buf;

            return res;
        }

        return StringType(reinterpret_cast<char*>(buf), reinterpret_cast<char*>(buf + resultSize));
    }
};

} // namespace utilities

} // namespace Hyperion
