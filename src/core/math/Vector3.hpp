/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_VECTOR3_HPP
#define HYPERION_VECTOR3_HPP

#include <core/Defines.hpp>

#include <core/math/Vector2.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

#include <cmath>
#include <iostream>

namespace hyperion {

struct Quaternion;
class Matrix3;
class Matrix4;

namespace math {
namespace detail {

template <class T>
struct alignas(alignof(T) * 4) HYP_API Vec3
{
    friend std::ostream &operator<<(std::ostream &out, const Vec3 &vec);

    static constexpr uint32 size = 3;
    
    using Type = T;

    union {
        struct { Type x, y, z; };
        Type values[3];
    };

    constexpr Vec3()                                    : x(0), y(0), z(0) { }
    constexpr Vec3(Type xyz)                            : x(xyz), y(xyz), z(xyz) { }
    constexpr Vec3(Type x, Type y, Type z)              : x(x), y(y), z(z) { }

    explicit constexpr Vec3(const Vec2<Type> &xy, Type z)
        : x(xy.x),
          y(xy.y),
          z(z)
    {
    }

    constexpr Vec3(const Vec3 &other)                   = default;
    constexpr Vec3 &operator=(const Vec3 &other)        = default;
    constexpr Vec3(Vec3 &&other) noexcept               = default;
    constexpr Vec3 &operator=(Vec3 &&other) noexcept    = default;
    ~Vec3()                                             = default;

    Type GetX() const
        { return x; }

    Type &GetX()
        { return x; }

    Vec3 &SetX(Type x)
        { this->x = x; return *this; }

    Type GetY() const
        { return y; }

    Type &GetY()
        { return y; }

    Vec3 &SetY(Type y)
        { this->y = y; return *this; }

    Type GetZ() const
        { return z; }

    Type &GetZ()
        { return z; }

    Vec3 &SetZ(Type z)
        { this->z = z; return *this; }

    /*! \brief Get the XY components of this vector as a Vector2. */
    HYP_FORCE_INLINE Vec2<Type> GetXY() const
        { return Vec2<Type>(x, y); }

    HYP_FORCE_INLINE explicit operator bool() const
        { return Sum() != Type(0); }

    HYP_FORCE_INLINE constexpr Type &operator[](int index)
        { return values[index]; }

    HYP_FORCE_INLINE constexpr Type operator[](int index) const
        { return values[index]; }

    HYP_FORCE_INLINE constexpr Vec3 operator+(const Vec3 &other) const
        { return { x + other.x, y + other.y, z + other.z }; }

    HYP_FORCE_INLINE Vec3 &operator+=(const Vec3 &other)
        { x += other.x; y += other.y; z += other.z; return *this; }

    HYP_FORCE_INLINE constexpr Vec3 operator-(const Vec3 &other) const
        { return { x - other.x, y - other.y, z - other.z }; }

    HYP_FORCE_INLINE Vec3 &operator-=(const Vec3 &other)
        { x -= other.x; y -= other.y; z -= other.z; return *this; }

    HYP_FORCE_INLINE
    constexpr Vec3 operator*(const Vec3 &other) const
        { return { x * other.x, y * other.y, z * other.z }; }

    HYP_FORCE_INLINE Vec3 &operator*=(const Vec3 &other)
        { x *= other.x; y *= other.y; z *= other.z; return *this; }

    HYP_FORCE_INLINE constexpr Vec3 operator/(const Vec3 &other) const
        { return { x / other.x, y / other.y, z / other.z }; }

    HYP_FORCE_INLINE Vec3 &operator/=(const Vec3 &other)
        { x /= other.x; y /= other.y; z /= other.z; return *this; }

    HYP_FORCE_INLINE constexpr Vec3 operator%(const Vec3 &other) const
        { return { x % other.x, y % other.y, z % other.z }; }

    HYP_FORCE_INLINE Vec3 &operator%=(const Vec3 &other)
        { x %= other.x; y %= other.y; z %= other.z; return *this; }

    HYP_FORCE_INLINE constexpr Vec3 operator&(const Vec3 &other) const
        { return { x & other.x, y & other.y, z & other.z }; }

    HYP_FORCE_INLINE Vec3 &operator&=(const Vec3 &other)
        { x &= other.x; y &= other.y; z &= other.z; return *this; }

    HYP_FORCE_INLINE constexpr Vec3 operator|(const Vec3 &other) const
        { return { x | other.x, y | other.y, z | other.z }; }

    HYP_FORCE_INLINE Vec3 &operator|=(const Vec3 &other)
        { x |= other.x; y |= other.y; z |= other.z; return *this; }

    HYP_FORCE_INLINE constexpr Vec3 operator^(const Vec3 &other) const
        { return { x ^ other.x, y ^ other.y, z ^ other.z }; }

    HYP_FORCE_INLINE
    Vec3 &operator^=(const Vec3 &other)
        { x ^= other.x; y ^= other.y; z ^= other.z; return *this; }

    HYP_FORCE_INLINE constexpr bool operator==(const Vec3 &other) const
        { return x == other.x && y == other.y && z == other.z; }

    HYP_FORCE_INLINE constexpr bool operator!=(const Vec3 &other) const
        { return x != other.x || y != other.y || z != other.z; }

    HYP_FORCE_INLINE constexpr Vec3 operator-() const
        { return operator*(Type(-1)); }

    HYP_FORCE_INLINE constexpr Vec3 operator+() const
        { return { +x, +y, +z }; }

    HYP_FORCE_INLINE constexpr bool operator<(const Vec3 &other) const
    {
        if (x != other.x) {
            return x < other.x;
        }

        if (y != other.y) {
            return y < other.y;
        }

        return z < other.z;
    }

    HYP_FORCE_INLINE constexpr Type Avg() const
        { return (x + y + z) / Type(size); }

    HYP_FORCE_INLINE constexpr Type Sum() const
        { return x + y + z; }

    HYP_FORCE_INLINE constexpr Type Volume() const
        { return x * y * z; }
    
    HYP_FORCE_INLINE constexpr Type Max() const
        { return x > y ? (x > z ? x : z) : (y > z ? y : z); }

    HYP_FORCE_INLINE constexpr Type Min() const
        { return x < y ? (x < z ? x : z) : (y < z ? y : z); }

    template <class Ty>
    constexpr explicit operator Vec3<Ty>() const
    {
        return {
            static_cast<Ty>(x),
            static_cast<Ty>(y),
            static_cast<Ty>(z)
        };
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(x);
        hc.Add(y);
        hc.Add(z);

        return hc;
    }

    HYP_FORCE_INLINE static Vec3 Zero()
        { return { Type(0), Type(0), Type(0) }; }

    HYP_FORCE_INLINE static Vec3 One()
        { return { Type(1), Type(1), Type(1) }; }

    HYP_FORCE_INLINE static Vec3 UnitX()
        { return { Type(1), Type(0), Type(0) }; }

    HYP_FORCE_INLINE static Vec3 UnitY()
        { return { Type(0), Type(1), Type(0) }; }

    HYP_FORCE_INLINE static Vec3 UnitZ()
        { return { Type(0), Type(0), Type(1) }; }

};

template <>
struct alignas(alignof(float) * 4) HYP_API Vec3<float>
{
    friend std::ostream &operator<<(std::ostream &out, const Vec3 &vec);

    static constexpr uint32 size = 3;
    
    using Type = float;

    union {
        struct { Type x, y, z; };
        Type values[3];
    };

    constexpr Vec3()                                    : x(0), y(0), z(0) { }
    constexpr Vec3(Type xyz)                            : x(xyz), y(xyz), z(xyz) { }
    constexpr Vec3(Type x, Type y, Type z)              : x(x), y(y), z(z) { }

    explicit constexpr Vec3(const Vec2<Type> &xy, Type z)
        : x(xy.x),
          y(xy.y),
          z(z)
    {
    }

    constexpr Vec3(const Vec3 &other)                   = default;
    constexpr Vec3 &operator=(const Vec3 &other)        = default;
    constexpr Vec3(Vec3 &&other) noexcept               = default;
    constexpr Vec3 &operator=(Vec3 &&other) noexcept    = default;
    ~Vec3()                                             = default;

    Type GetX() const
        { return x; }

    Type &GetX()
        { return x; }

    Vec3 &SetX(Type x)
        { this->x = x; return *this; }

    Type GetY() const
        { return y; }

    Type &GetY()
        { return y; }

    Vec3 &SetY(Type y)
        { this->y = y; return *this; }

    Type GetZ() const
        { return z; }

    Type &GetZ()
        { return z; }

    Vec3 &SetZ(Type z)
        { this->z = z; return *this; }

    /*! \brief Get the XY components of this vector as a Vector2. */
    HYP_FORCE_INLINE Vec2<Type> GetXY() const
        { return Vec2<Type>(x, y); }

    HYP_FORCE_INLINE explicit operator bool() const
        { return Sum() != Type(0); }

    HYP_FORCE_INLINE constexpr Type &operator[](int index)
        { return values[index]; }

    HYP_FORCE_INLINE constexpr Type operator[](int index) const
        { return values[index]; }

    HYP_FORCE_INLINE constexpr Vec3 operator+(const Vec3 &other) const
        { return { x + other.x, y + other.y, z + other.z }; }

    HYP_FORCE_INLINE Vec3 &operator+=(const Vec3 &other)
        { x += other.x; y += other.y; z += other.z; return *this; }

    HYP_FORCE_INLINE constexpr Vec3 operator-(const Vec3 &other) const
        { return { x - other.x, y - other.y, z - other.z }; }

    HYP_FORCE_INLINE Vec3 &operator-=(const Vec3 &other)
        { x -= other.x; y -= other.y; z -= other.z; return *this; }

    HYP_FORCE_INLINE constexpr Vec3 operator*(const Vec3 &other) const
        { return { x * other.x, y * other.y, z * other.z }; }

    HYP_FORCE_INLINE Vec3 &operator*=(const Vec3 &other)
        { x *= other.x; y *= other.y; z *= other.z; return *this; }

    HYP_FORCE_INLINE constexpr Vec3 operator/(const Vec3 &other) const
        { return { x / other.x, y / other.y, z / other.z }; }

    HYP_FORCE_INLINE Vec3 &operator/=(const Vec3 &other)
        { x /= other.x; y /= other.y; z /= other.z; return *this; }

    HYP_FORCE_INLINE
    constexpr bool operator==(const Vec3 &other) const
        { return x == other.x && y == other.y && z == other.z; }

    HYP_FORCE_INLINE
    constexpr bool operator!=(const Vec3 &other) const
        { return x != other.x || y != other.y || z != other.z; }

    HYP_FORCE_INLINE constexpr Vec3 operator-() const
        { return operator*(Type(-1)); }

    HYP_FORCE_INLINE constexpr Vec3 operator+() const
        { return { +x, +y, +z }; }

    HYP_FORCE_INLINE constexpr bool operator<(const Vec3 &other) const
    {
        if (x != other.x) {
            return x < other.x;
        }

        if (y != other.y) {
            return y < other.y;
        }

        return z < other.z;
    }

    Vec3 operator*(const Matrix3 &mat) const;
    Vec3 &operator*=(const Matrix3 &mat);
    Vec3 operator*(const Matrix4 &mat) const;
    Vec3 &operator*=(const Matrix4 &mat);
    Vec3 operator*(const Quaternion &quat) const;
    Vec3 &operator*=(const Quaternion &quat);

    HYP_FORCE_INLINE constexpr Type Avg() const
        { return (x + y + z) / Type(size); }

    HYP_FORCE_INLINE constexpr Type Sum() const
        { return x + y + z; }

    HYP_FORCE_INLINE constexpr Type Volume() const
        { return x * y * z; }
    
    HYP_FORCE_INLINE constexpr Type Max() const
        { return x > y ? (x > z ? x : z) : (y > z ? y : z); }

    HYP_FORCE_INLINE constexpr Type Min() const
        { return x < y ? (x < z ? x : z) : (y < z ? y : z); }

    Type DistanceSquared(const Vec3 &other) const;
    Type Distance(const Vec3 &other) const;
    
    constexpr Type LengthSquared() const { return x * x + y * y + z * z; }
    Type Length() const { return std::sqrt(LengthSquared()); }
    
    Vec3 Normalized() const;
    Vec3 &Normalize();

    Vec3 Cross(const Vec3 &other) const;

    Vec3 Reflect(const Vec3 &normal) const;

    Vec3 &Rotate(const Vec3 &axis, Type radians);
    Vec3 &Rotate(const Quaternion &quaternion);
    Vec3 &Lerp(const Vec3 &to, Type amt);
    Type Dot(const Vec3 &other) const;
    Type AngleBetween(const Vec3 &other) const;
    
    template <class Ty>
    constexpr explicit operator Vec3<Ty>() const
    {
        return {
            static_cast<Ty>(x),
            static_cast<Ty>(y),
            static_cast<Ty>(z)
        };
    }

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(x);
        hc.Add(y);
        hc.Add(z);

        return hc;
    }

    static Vec3<float> Abs(const Vec3<float> &);
    static Vec3<float> Round(const Vec3<float> &);
    static Vec3<float> Clamp(const Vec3<float> &, float min, float max);
    static Vec3<float> Min(const Vec3<float> &a, const Vec3<float> &b);
    static Vec3<float> Max(const Vec3<float> &a, const Vec3<float> &b);

    static Vec3 Zero()
        { return { Type(0), Type(0), Type(0) }; }

    static Vec3 One()
        { return { Type(1), Type(1), Type(1) }; }

    static Vec3 UnitX()
        { return { Type(1), Type(0), Type(0) }; }

    static Vec3 UnitY()
        { return { Type(0), Type(1), Type(0) }; }

    static Vec3 UnitZ()
        { return { Type(0), Type(0), Type(1) }; }
};

} // namespace detail

template <class T>
using Vec3 = detail::Vec3<T>;

} // namespace math

template <class T>
using Vec3 = math::Vec3<T>;

using Vec3i = Vec3<int>;
using Vec3u = Vec3<uint32>;
using Vec3f = Vec3<float>;

template <class T>
inline constexpr bool is_vec3 = false;

template <>
inline constexpr bool is_vec3<Vec3i> = true;

template <>
inline constexpr bool is_vec3<Vec3u> = true;

template <>
inline constexpr bool is_vec3<Vec3f> = true;

static_assert(sizeof(Vec3i) == 16);
static_assert(sizeof(Vec3u) == 16);
static_assert(sizeof(Vec3f) == 16);

// Transitional typedef -- to be removed when code is updated to use Vec3f (to match Vec3i and Vec3u)
using Vector3 = Vec3f;

} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::Vector3);

#endif
