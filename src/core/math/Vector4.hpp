/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_VECTOR4_HPP
#define HYPERION_VECTOR4_HPP

#include <core/math/Vector2.hpp>
#include <core/math/Vector3.hpp>

#include <core/Defines.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

#include <cmath>

namespace hyperion {

class Matrix4;

namespace math {
namespace detail {

template <class T>
struct alignas(alignof(T) * 4) HYP_API Vec4
{
    friend std::ostream &operator<<(std::ostream &out, const Vec4 &vec);

    using Type = T;

    static constexpr uint32 size = 4;

    union {
        struct { Type x, y, z, w; };
        Type values[4];
    };

    Vec4()
        : x(0),
          y(0),
          z(0),
          w(0)
    {
    }

    Vec4(Type xyzw)
        : x(xyzw),
          y(xyzw),
          z(xyzw),
          w(xyzw)
    {
    }

    Vec4(Type x, Type y, Type z, Type w)
        : x(x),
          y(y),
          z(z),
          w(w)
    {
    }

    explicit Vec4(const Vec2<Type> &xy, Type z, Type w)
        : x(xy.x),
          y(xy.y),
          z(z),
          w(w)
    {
    }

    explicit Vec4(const Vec2<Type> &xy, const Vec2<Type> &zw)
        : x(xy.x),
          y(xy.y),
          z(zw.x),
          w(zw.y)
    {
    }

    explicit Vec4(const Vec3<Type> &xyz, Type w)
        : x(xyz.x),
          y(xyz.y),
          z(xyz.z),
          w(w)
    {
    }

    Vec4(const Vec4 &other)             = default;
    Vec4 &operator=(const Vec4 &other)  = default;

    Type GetX() const       { return x; }
    Type &GetX()            { return x; }
    Vec4 &SetX(Type x)      { this->x = x; return *this; }
    Type GetY() const       { return y; }
    Type &GetY()            { return y; }
    Vec4 &SetY(Type y)      { this->y = y; return *this; }
    Type GetZ() const       { return z; }
    Type &GetZ()            { return z; }
    Vec4 &SetZ(Type z)      { this->z = z; return *this; }
    Type GetW() const       { return w; }
    Type &GetW()            { return w; }
    Vec4 &SetW(Type w)      { this->w = w; return *this; }

    /*! \brief Get the XY components of this vector as a Vector2. */
    HYP_FORCE_INLINE Vec2<Type> GetXY() const
        { return Vec2<Type>(x, y); }

    /*! \brief Get the XYZ components of this vector as a Vector3. */
    HYP_FORCE_INLINE Vec3<Type> GetXYZ() const
        { return Vec3<Type>(x, y, z); }

    constexpr Type &operator[](SizeType index)
        { return values[index]; }
    
    constexpr Type operator[](SizeType index) const
        { return values[index]; }

    Vec4 operator+(const Vec4 &other) const
    {
        return Vec4(
            x + other.x,
            y + other.y,
            z + other.z,
            w + other.w
        );
    }

    Vec4 &operator+=(const Vec4 &other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
        w += other.w;

        return *this;
    }

    Vec4 operator-(const Vec4 &other) const
    {
        return Vec4(
            x - other.x,
            y - other.y,
            z - other.z,
            w - other.w
        );
    }

    Vec4 &operator-=(const Vec4 &other)
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        w -= other.w;

        return *this;
    }

    Vec4 operator*(const Vec4 &other) const
    {
        return Vec4(
            x * other.x,
            y * other.y,
            z * other.z,
            w * other.w
        );
    }

    Vec4 &operator*=(const Vec4 &other)
    {
        x *= other.x;
        y *= other.y;
        z *= other.z;
        w *= other.w;

        return *this;
    }

    Vec4 operator/(const Vec4 &other) const
    {
        return Vec4(
            x / other.x,
            y / other.y,
            z / other.z,
            w / other.w
        );
    }

    Vec4 &operator/=(const Vec4 &other)
    {
        x /= other.x;
        y /= other.y;
        z /= other.z;
        w /= other.w;

        return *this;
    }

    bool operator==(const Vec4 &other) const
        { return x == other.x && y == other.y && z == other.z && w == other.w; }

    bool operator!=(const Vec4 &other) const
        { return !operator==(other); }

    Vec4 operator-() const
        { return operator*(Type(-1)); }

    bool operator<(const Vec4 &other) const
    {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;
        if (z != other.z) return z < other.z;
        if (w != other.w) return w < other.w;

        return false;
    }

    constexpr Type LengthSquared() const
        { return x * x + y * y + z * z + w * w; }

    Type Length() const { return std::sqrt(LengthSquared()); }

    HYP_FORCE_INLINE constexpr Type Avg() const
        { return (x + y + z + w) / Type(size); }

    HYP_FORCE_INLINE constexpr Type Sum() const
        { return x + y + z + w; }

    HYP_FORCE_INLINE constexpr Type Volume() const
        { return x * y * z * w; }

    HYP_FORCE_INLINE constexpr Type Max() const
        { return x > y ? (x > z ? (x > w ? x : w) : (z > w ? z : w)) : (y > z ? (y > w ? y : w) : (z > w ? z : w)); }

    HYP_FORCE_INLINE constexpr Type Min() const
        { return x < y ? (x < z ? (x < w ? x : w) : (z < w ? z : w)) : (y < z ? (y < w ? y : w) : (z < w ? z : w)); }

    static Vec4 Abs(const Vec4 &);
    static Vec4 Min(const Vec4 &a, const Vec4 &b);
    static Vec4 Max(const Vec4 &a, const Vec4 &b);

    static Vec4 Zero()
        { return Vec4(0, 0, 0, 0); }

    static Vec4 One()
        { return Vec4(1, 1, 1, 1); }

    static Vec4 UnitX()
        { return Vec4(1, 0, 0, 0); }

    static Vec4 UnitY()
        { return Vec4(0, 1, 0, 0); }
    
    static Vec4 UnitZ()
        { return Vec4(0, 0, 1, 0); }

    static Vec4 UnitW()
        { return Vec4(0, 0, 0, 1); }

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(x);
        hc.Add(y);
        hc.Add(z);
        hc.Add(w);

        return hc;
    }
};

template <>
struct alignas(alignof(float) * 4) HYP_API Vec4<float>
{
    friend std::ostream &operator<<(std::ostream &out, const Vec4 &vec);

    using Type = float;

    static constexpr uint32 size = 4;

    union {
        struct { Type x, y, z, w; };
        Type values[4];
    };

    Vec4()
        : x(0),
          y(0),
          z(0),
          w(0)
    {
    }

    Vec4(Type xyzw)
        : x(xyzw),
          y(xyzw),
          z(xyzw),
          w(xyzw)
    {
    }

    Vec4(Type x, Type y, Type z, Type w)
        : x(x),
          y(y),
          z(z),
          w(w)
    {
    }

    explicit Vec4(const Vec2<Type> &xy, Type z, Type w)
        : x(xy.x),
          y(xy.y),
          z(z),
          w(w)
    {
    }

    explicit Vec4(const Vec2<Type> &xy, const Vec2<Type> &zw)
        : x(xy.x),
          y(xy.y),
          z(zw.x),
          w(zw.y)
    {
    }

    explicit Vec4(const Vec3<Type> &xyz, Type w)
        : x(xyz.x),
          y(xyz.y),
          z(xyz.z),
          w(w)
    {
    }

    Vec4(const Vec4 &other)             = default;
    Vec4 &operator=(const Vec4 &other)  = default;

    Type GetX() const           { return x; }
    Type &GetX()                { return x; }
    Vec4 &SetX(Type x)          { this->x = x; return *this; }
    Type GetY() const           { return y; }
    Type &GetY()                { return y; }
    Vec4 &SetY(Type y)          { this->y = y; return *this; }
    Type GetZ() const           { return z; }
    Type &GetZ()                { return z; }
    Vec4 &SetZ(Type z)          { this->z = z; return *this; }
    Type GetW() const           { return w; }
    Type &GetW()                { return w; }
    Vec4 &SetW(Type w)          { this->w = w; return *this; }

    /*! \brief Get the XY components of this vector as a Vector2. */
    Vec2<Type> GetXY() const    { return Vec2<Type>(x, y); }

    /*! \brief Get the XYZ components of this vector as a Vector3. */
    Vec3<Type> GetXYZ() const   { return Vec3<Type>(x, y, z); }

    constexpr Type &operator[](SizeType index)
        { return values[index]; }
    
    constexpr Type operator[](SizeType index) const
        { return values[index]; }

    Vec4 operator+(const Vec4 &other) const
    {
        return Vec4(
            x + other.x,
            y + other.y,
            z + other.z,
            w + other.w
        );
    }

    Vec4 &operator+=(const Vec4 &other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
        w += other.w;

        return *this;
    }

    Vec4 operator-(const Vec4 &other) const
    {
        return Vec4(
            x - other.x,
            y - other.y,
            z - other.z,
            w - other.w
        );
    }

    Vec4 &operator-=(const Vec4 &other)
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        w -= other.w;

        return *this;
    }

    Vec4 operator*(const Vec4 &other) const
    {
        return Vec4(
            x * other.x,
            y * other.y,
            z * other.z,
            w * other.w
        );
    }

    Vec4 &operator*=(const Vec4 &other)
    {
        x *= other.x;
        y *= other.y;
        z *= other.z;
        w *= other.w;

        return *this;
    }

    Vec4 operator/(const Vec4 &other) const
    {
        return Vec4(
            x / other.x,
            y / other.y,
            z / other.z,
            w / other.w
        );
    }

    Vec4 &operator/=(const Vec4 &other)
    {
        x /= other.x;
        y /= other.y;
        z /= other.z;
        w /= other.w;

        return *this;
    }

    bool operator==(const Vec4 &other) const
        { return x == other.x && y == other.y && z == other.z && w == other.w; }

    bool operator!=(const Vec4 &other) const
        { return !operator==(other); }

    Vec4 operator-() const
        { return operator*(Type(-1)); }

    bool operator<(const Vec4 &other) const
    {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;
        if (z != other.z) return z < other.z;
        if (w != other.w) return w < other.w;

        return false;
    }

    constexpr Type LengthSquared() const
        { return x * x + y * y + z * z + w * w; }

    Type Length() const { return std::sqrt(LengthSquared()); }

    HYP_FORCE_INLINE constexpr Type Avg() const
        { return (x + y + z + w) / Type(size); }

    HYP_FORCE_INLINE constexpr Type Sum() const
        { return x + y + z + w; }

    HYP_FORCE_INLINE constexpr Type Volume() const
        { return x * y * z * w; }

    HYP_FORCE_INLINE constexpr Type Max() const
        { return x > y ? (x > z ? (x > w ? x : w) : (z > w ? z : w)) : (y > z ? (y > w ? y : w) : (z > w ? z : w)); }

    HYP_FORCE_INLINE constexpr Type Min() const
        { return x < y ? (x < z ? (x < w ? x : w) : (z < w ? z : w)) : (y < z ? (y < w ? y : w) : (z < w ? z : w)); }

    Vec4 operator*(const Matrix4 &mat) const;
    Vec4 &operator*=(const Matrix4 &mat);

    Type DistanceSquared(const Vec4 &other) const;
    Type Distance(const Vec4 &other) const;
    
    Vec4 Normalized() const;
    Vec4 &Normalize();
    Vec4 &Rotate(const Vec3<float> &axis, float radians);
    Vec4 &Lerp(const Vec4 &to, float amt);
    float Dot(const Vec4 &other) const;

    static Vec4 Abs(const Vec4 &);
    static Vec4 Round(const Vec4 &);
    static Vec4 Clamp(const Vec4 &, float min, float max);
    static Vec4 Min(const Vec4 &a, const Vec4 &b);
    static Vec4 Max(const Vec4 &a, const Vec4 &b);

    static Vec4 Zero()
        { return Vec4(0, 0, 0, 0); }

    static Vec4 One()
        { return Vec4(1, 1, 1, 1); }

    static Vec4 UnitX()
        { return Vec4(1, 0, 0, 0); }

    static Vec4 UnitY()
        { return Vec4(0, 1, 0, 0); }

    static Vec4 UnitZ()
        { return Vec4(0, 0, 1, 0); }
    
    static Vec4 UnitW()
        { return Vec4(0, 0, 0, 1); }

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(x);
        hc.Add(y);
        hc.Add(z);
        hc.Add(w);

        return hc;
    }
};

} // namespace detail

template <class T>
using Vec4 = detail::Vec4<T>;

} // namespace math

template <class T>
using Vec4 = math::Vec4<T>;

using Vec4f = Vec4<float>;
using Vec4i = Vec4<int>;
using Vec4u = Vec4<uint32>;

static_assert(sizeof(Vec4f) == sizeof(float) * 4, "sizeof(Vec4f) must be equal to sizeof(float) * 4");
static_assert(sizeof(Vec4i) == sizeof(int) * 4, "sizeof(Vec4i) must be equal to sizeof(int) * 4");
static_assert(sizeof(Vec4u) == sizeof(uint32) * 4, "sizeof(Vec4u) must be equal to sizeof(uint32) * 4");

template <class T>
inline constexpr bool is_vec4 = false;

template <>
inline constexpr bool is_vec4<Vec4f> = true;

template <>
inline constexpr bool is_vec4<Vec4i> = true;

template <>
inline constexpr bool is_vec4<Vec4u> = true;

// transitional typedef
using Vector4 = Vec4f;

} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::Vector4);

#endif
