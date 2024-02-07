#ifndef VECTOR2_H
#define VECTOR2_H

#include <HashCode.hpp>

#include <util/Defines.hpp>
#include <Types.hpp>

#include <ostream>
#include <cmath>

#include "Vector3.hpp"

namespace hyperion {

namespace math {
namespace detail {

template <class T>
struct alignas(alignof(T) * 2) Vec2
{
    static constexpr UInt size = 2;

    using Type = T;

    union {
        struct { T x, y; };
        T values[2];
    };

    constexpr Vec2() : x(0), y(0) { }
    constexpr Vec2(T x, T y) : x(x), y(y) { }
    constexpr Vec2(const Vec2 &other) = default;
    constexpr Vec2 &operator=(const Vec2 &other) = default;
    constexpr Vec2(Vec2 &&other) noexcept = default;
    constexpr Vec2 &operator=(Vec2 &&other) noexcept = default;
    ~Vec2() = default;

    constexpr T &operator[](SizeType index)
        { return values[index]; }

    constexpr T operator[](SizeType index) const
        { return values[index]; }

    constexpr Vec2 operator+(const Vec2 &other) const
        { return { x + other.x, y + other.y }; }

    Vec2 &operator+=(const Vec2 &other)
        { x += other.x; y += other.y; return *this; }

    constexpr Vec2 operator-(const Vec2 &other) const
        { return { x - other.x, y - other.y }; }

    Vec2 &operator-=(const Vec2 &other)
        { x -= other.x; y -= other.y; return *this; }

    constexpr Vec2 operator*(const Vec2 &other) const
        { return { x * other.x, y * other.y }; }

    Vec2 &operator*=(const Vec2 &other)
        { x *= other.x; y *= other.y; return *this; }

    constexpr Vec2 operator/(const Vec2 &other) const
        { return { x / other.x, y / other.y }; }

    Vec2 &operator/=(const Vec2 &other)
        { x /= other.x; y /= other.y; return *this; }

    constexpr Vec2 operator%(const Vec2 &other) const
        { return { x % other.x, y % other.y }; }

    Vec2 &operator%=(const Vec2 &other)
        { x %= other.x; y %= other.y; return *this; }

    constexpr Vec2 operator&(const Vec2 &other) const
        { return { x & other.x, y & other.y }; }

    Vec2 &operator&=(const Vec2 &other)
        { x &= other.x; y &= other.y; return *this; }

    constexpr Vec2 operator|(const Vec2 &other) const
        { return { x | other.x, y | other.y }; }

    Vec2 &operator|=(const Vec2 &other)
        { x |= other.x; y |= other.y; return *this; }

    constexpr Vec2 operator^(const Vec2 &other) const
        { return { x ^ other.x, y ^ other.y }; }

    Vec2 &operator^=(const Vec2 &other)
        { x ^= other.x; y ^= other.y; return *this; }

    constexpr Bool operator==(const Vec2 &other) const
        { return x == other.x && y == other.y; }

    constexpr Bool operator!=(const Vec2 &other) const
        { return x != other.x || y != other.y; }

    constexpr Vec2 operator-() const
        { return operator*(T(-1)); }

    constexpr Bool operator<(const Vec2 &other) const
    {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;

        return false;
    }

    Type Max() const;
    Type Min() const;

    static Vec2 Zero();
    static Vec2 One();
    static Vec2 UnitX();
    static Vec2 UnitY();

    template <class Ty>
    constexpr explicit operator Vec2<Ty>() const
    {
        return {
            static_cast<Ty>(x),
            static_cast<Ty>(y)
        };
    }

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(x);
        hc.Add(y);

        return hc;
    }
};

template <>
struct alignas(alignof(Float) * 2) Vec2<Float>
{
    friend std::ostream &operator<<(std::ostream &out, const Vec2<Float> &vec);
public:
    static constexpr UInt size = 2;

    static const Vec2 zero;
    static const Vec2 one;

    union {
        struct { Float x, y; };
        Float values[2];
    };

    constexpr Vec2()
        : x(0.0f), y(0.0f)
    {
    }

    constexpr Vec2(Float x, Float y)
        : x(x), y(y)
    {
    }

    constexpr Vec2(Float xy)
        : x(xy), y(xy)
    {
    }

    constexpr Vec2(const Vec2 &other)
        : x(other.x), y(other.y)
    {
    }

    Float GetX() const      { return x; }
    Float &GetX()           { return x; }
    Vec2 &SetX(Float x)     { this->x = x; return *this; }
    Float GetY() const      { return y; }
    Float &GetY()           { return y; }
    Vec2 &SetY(Float y)     { this->y = y; return *this; }
    
    constexpr Float operator[](SizeType index) const
        { return values[index]; }

    constexpr Float &operator[](SizeType index)
        { return values[index]; }

    explicit operator Bool() const
        { return Sum() != 0.0f; }

    template <class U>
    explicit operator Vec2<U>() const
        { return Vec2i(static_cast<U>(x), static_cast<U>(y)); }

    Vec2 &operator=(const Vec2 &other)
        { x = other.x; y = other.y; return *this; }

    constexpr Vec2 operator+(const Vec2 &other) const
        { return { x + other.x, y + other.y }; }

    Vec2 &operator+=(const Vec2 &other)
        { x += other.x; y += other.y; return *this; }

    constexpr Vec2 operator-(const Vec2 &other) const
        { return { x - other.x, y - other.y }; }

    Vec2 &operator-=(const Vec2 &other)
        { x -= other.x; y -= other.y; return *this; }

    constexpr Vec2 operator*(const Vec2 &other) const
        { return { x * other.x, y * other.y }; }

    Vec2 &operator*=(const Vec2 &other)
        { x *= other.x; y *= other.y; return *this; }

    constexpr Vec2 operator/(const Vec2 &other) const
        { return { x / other.x, y / other.y }; }

    Vec2 &operator/=(const Vec2 &other)
        { x /= other.x; y /= other.y; return *this; }

    constexpr Bool operator==(const Vec2 &other) const
        { return x == other.x && y == other.y; }

    constexpr Bool operator!=(const Vec2 &other) const
        { return x != other.x || y != other.y; }

    constexpr Vec2 operator-() const
        { return operator*(-1.0f); }

    constexpr Bool operator<(const Vec2 &other) const
    {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;

        return false;
    }

    constexpr Float LengthSquared() const { return x * x + y * y; }
    Float Length() const { return std::sqrt(LengthSquared()); }

    constexpr Float Avg() const { return (x + y) / 2.0f; }
    constexpr Float Sum() const { return x + y; }
    Float Max() const;
    Float Min() const;

    Float Distance(const Vec2 &other) const;
    Float DistanceSquared(const Vec2 &other) const;

    Vec2 &Normalize();
    Vec2 &Lerp(const Vec2 &to, const Float amt);

    static Vec2 Abs(const Vec2 &);
    static Vec2 Round(const Vec2 &);
    static Vec2 Clamp(const Vec2 &, Float min, Float max);
    static Vec2 Min(const Vec2 &a, const Vec2 &b);
    static Vec2 Max(const Vec2 &a, const Vec2 &b);

    static Vec2 Zero();
    static Vec2 One();
    static Vec2 UnitX();
    static Vec2 UnitY();

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(x);
        hc.Add(y);

        return hc;
    }
};

} // namespace detail

template <class T>
using Vec2 = detail::Vec2<T>;

} // namespace math


template <class T>
using Vec2 = math::Vec2<T>;

using Vec2f = Vec2<Float>;
using Vec2i = Vec2<Int>;
using Vec2u = Vec2<UInt>;

static_assert(sizeof(Vec2f) == 8);
static_assert(sizeof(Vec2i) == 8);
static_assert(sizeof(Vec2u) == 8);

template <class T>
inline constexpr Bool is_vec2 = false;

template <>
inline constexpr Bool is_vec2<Vec2f> = true;

template <>
inline constexpr Bool is_vec2<Vec2i> = true;

template <>
inline constexpr Bool is_vec2<Vec2u> = true;

// transitional typedef
using Vector2 = Vec2f;

} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::Vector2);

#endif
