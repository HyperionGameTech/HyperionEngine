/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

#include <Core/Utilities/FormatFwd.hpp>

#include <Core/Defines.hpp>
#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

#include <cmath>
#include <cstdio>

#if !HYP_ARM && (defined(__SSE4_1__) || (HYP_MSVC && defined(_M_X64)))
#include <immintrin.h>
#define HYP_VEC4F_HAS_SSE 1
#else
#define HYP_VEC4F_HAS_SSE 0
#endif

namespace Hyperion {

class Mat4f;

namespace math {
template <class T>
struct alignas(alignof(T) * 4) Vec4
{
    using Type = T;

    static constexpr uint32 size = 4;

    union
    {
        struct
        {
            Type x, y, z, w;
        };

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

    explicit Vec4(const Vec2<Type>& xy, Type z, Type w)
        : x(xy.x),
          y(xy.y),
          z(z),
          w(w)
    {
    }

    explicit Vec4(const Vec2<Type>& xy, const Vec2<Type>& zw)
        : x(xy.x),
          y(xy.y),
          z(zw.x),
          w(zw.y)
    {
    }

    explicit Vec4(const Vec3<Type>& xyz, Type w)
        : x(xyz.x),
          y(xyz.y),
          z(xyz.z),
          w(w)
    {
    }

    Vec4(const Vec4& other) = default;
    Vec4& operator=(const Vec4& other) = default;

    Type GetX() const
    {
        return x;
    }

    Type& GetX()
    {
        return x;
    }

    Vec4& SetX(Type x)
    {
        this->x = x;
        return *this;
    }

    Type GetY() const
    {
        return y;
    }

    Type& GetY()
    {
        return y;
    }

    Vec4& SetY(Type y)
    {
        this->y = y;
        return *this;
    }

    Type GetZ() const
    {
        return z;
    }

    Type& GetZ()
    {
        return z;
    }

    Vec4& SetZ(Type z)
    {
        this->z = z;
        return *this;
    }

    Type GetW() const
    {
        return w;
    }

    Type& GetW()
    {
        return w;
    }

    Vec4& SetW(Type w)
    {
        this->w = w;
        return *this;
    }

    /*! \brief Get the XY components of this vector as a Vector2. */
    HYP_FORCE_INLINE Vec2<Type> GetXY() const
    {
        return Vec2<Type>(x, y);
    }

    /*! \brief Get the XYZ components of this vector as a Vector3. */
    HYP_FORCE_INLINE Vec3<Type> GetXYZ() const
    {
        return Vec3<Type>(x, y, z);
    }

    constexpr Type& operator[](size_t index)
    {
        return values[index];
    }

    constexpr Type operator[](size_t index) const
    {
        return values[index];
    }

    Vec4 operator+(const Vec4& other) const;

    Vec4& operator+=(const Vec4& other);

    Vec4 operator-(const Vec4& other) const;

    Vec4& operator-=(const Vec4& other);

    Vec4 operator*(const Vec4& other) const;

    Vec4& operator*=(const Vec4& other);

    Vec4 operator/(const Vec4& other) const;

    Vec4& operator/=(const Vec4& other)
    {
        x /= other.x;
        y /= other.y;
        z /= other.z;
        w /= other.w;

        return *this;
    }

    bool operator==(const Vec4& other) const
    {
        return x == other.x && y == other.y && z == other.z && w == other.w;
    }

    bool operator!=(const Vec4& other) const
    {
        return !operator==(other);
    }

    Vec4 operator-() const;

    bool operator<(const Vec4& other) const
    {
        if (x != other.x)
            return x < other.x;
        if (y != other.y)
            return y < other.y;
        if (z != other.z)
            return z < other.z;
        if (w != other.w)
            return w < other.w;

        return false;
    }

    Type LengthSquared() const;

    Type Length() const
    {
        return std::sqrt(LengthSquared());
    }

    HYP_FORCE_INLINE constexpr Type Avg() const
    {
        return (x + y + z + w) / Type(size);
    }

    HYP_FORCE_INLINE constexpr Type Sum() const
    {
        return x + y + z + w;
    }

    HYP_FORCE_INLINE constexpr Type Volume() const
    {
        return x * y * z * w;
    }

    HYP_FORCE_INLINE constexpr Type Max() const
    {
        return x > y ? (x > z ? (x > w ? x : w) : (z > w ? z : w)) : (y > z ? (y > w ? y : w) : (z > w ? z : w));
    }

    HYP_FORCE_INLINE constexpr Type Min() const
    {
        return x < y ? (x < z ? (x < w ? x : w) : (z < w ? z : w)) : (y < z ? (y < w ? y : w) : (z < w ? z : w));
    }

    HYP_FORCE_INLINE constexpr bool IsZero() const
    {
        return (x == 0 && y == 0 && z == 0 && w == 0);
    }

    static Vec4 Abs(const Vec4&);
    static Vec4 Min(const Vec4& a, const Vec4& b);
    static Vec4 Max(const Vec4& a, const Vec4& b);

    static Vec4 Zero()
    {
        return Vec4(0, 0, 0, 0);
    }

    static Vec4 One()
    {
        return Vec4(1, 1, 1, 1);
    }

    static Vec4 UnitX()
    {
        return Vec4(1, 0, 0, 0);
    }

    static Vec4 UnitY()
    {
        return Vec4(0, 1, 0, 0);
    }

    static Vec4 UnitZ()
    {
        return Vec4(0, 0, 1, 0);
    }

    static Vec4 UnitW()
    {
        return Vec4(0, 0, 0, 1);
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode()
            .Combine(x)
            .Combine(y)
            .Combine(z)
            .Combine(w);
    }
};

template <>
struct alignas(alignof(float) * 4) CORE_API Vec4<float>
{
    using Type = float;

    static constexpr uint32 size = 4;

    union
    {
        struct
        {
            Type x, y, z, w;
        };

        Type values[4];

#if HYP_VEC4F_HAS_SSE
        __m128 _value;
#endif
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

    explicit Vec4(const Vec2<Type>& xy, Type z, Type w)
        : x(xy.x),
          y(xy.y),
          z(z),
          w(w)
    {
    }

    explicit Vec4(const Vec2<Type>& xy, const Vec2<Type>& zw)
        : x(xy.x),
          y(xy.y),
          z(zw.x),
          w(zw.y)
    {
    }

    explicit Vec4(const Vec3<Type>& xyz, Type w)
        : x(xyz.x),
          y(xyz.y),
          z(xyz.z),
          w(w)
    {
    }

    Vec4(const Vec4& other) = default;
    Vec4& operator=(const Vec4& other) = default;

    Type GetX() const
    {
        return x;
    }

    Type& GetX()
    {
        return x;
    }

    Vec4& SetX(Type x)
    {
        this->x = x;
        return *this;
    }

    Type GetY() const
    {
        return y;
    }

    Type& GetY()
    {
        return y;
    }

    Vec4& SetY(Type y)
    {
        this->y = y;
        return *this;
    }

    Type GetZ() const
    {
        return z;
    }

    Type& GetZ()
    {
        return z;
    }

    Vec4& SetZ(Type z)
    {
        this->z = z;
        return *this;
    }

    Type GetW() const
    {
        return w;
    }

    Type& GetW()
    {
        return w;
    }

    Vec4& SetW(Type w)
    {
        this->w = w;
        return *this;
    }

    /*! \brief Get the XY components of this vector as a Vector2. */
    Vec2<Type> GetXY() const
    {
        return Vec2<Type>(x, y);
    }

    /*! \brief Get the XYZ components of this vector as a Vector3. */
    Vec3<Type> GetXYZ() const
    {
        return Vec3<Type>(x, y, z);
    }

    constexpr Type& operator[](size_t index)
    {
        return values[index];
    }

    constexpr Type operator[](size_t index) const
    {
        return values[index];
    }

    Vec4 operator+(const Vec4& other) const;

    Vec4& operator+=(const Vec4& other);

    Vec4 operator-(const Vec4& other) const;

    Vec4& operator-=(const Vec4& other);

    Vec4 operator*(const Vec4& other) const;

    Vec4& operator*=(const Vec4& other);

    Vec4 operator/(const Vec4& other) const;

    Vec4& operator/=(const Vec4& other)
    {
        x /= other.x;
        y /= other.y;
        z /= other.z;
        w /= other.w;

        return *this;
    }

    bool operator==(const Vec4& other) const;
    bool operator!=(const Vec4& other) const;

    Vec4 operator-() const;

    bool operator<(const Vec4& other) const
    {
        if (x != other.x)
            return x < other.x;
        if (y != other.y)
            return y < other.y;
        if (z != other.z)
            return z < other.z;
        if (w != other.w)
            return w < other.w;

        return false;
    }

    Type LengthSquared() const;

    Type Length() const
    {
        return std::sqrt(LengthSquared());
    }

    HYP_FORCE_INLINE constexpr Type Avg() const
    {
        return (x + y + z + w) / Type(size);
    }

    HYP_FORCE_INLINE constexpr Type Sum() const
    {
        return x + y + z + w;
    }

    HYP_FORCE_INLINE constexpr Type Volume() const
    {
        return x * y * z * w;
    }

    HYP_FORCE_INLINE constexpr Type Max() const
    {
        return x > y ? (x > z ? (x > w ? x : w) : (z > w ? z : w)) : (y > z ? (y > w ? y : w) : (z > w ? z : w));
    }

    HYP_FORCE_INLINE constexpr Type Min() const
    {
        return x < y ? (x < z ? (x < w ? x : w) : (z < w ? z : w)) : (y < z ? (y < w ? y : w) : (z < w ? z : w));
    }

    HYP_FORCE_INLINE constexpr bool IsZero() const
    {
        return (x == 0.0f && y == 0.0f && z == 0.0f && w == 0.0f);
    }

    Vec4 operator*(const Mat4f& mat) const;
    Vec4& operator*=(const Mat4f& mat);

    /*! \brief 3-component transform by a 3x3 matrix. The w component is preserved. */
    Vec4 operator*(const Mat3f& mat) const;
    Vec4& operator*=(const Mat3f& mat);

    /*! \brief 3-component rotation by quaternion. The w component is preserved. */
    Vec4 operator*(const Quat4f& quat) const;
    Vec4& operator*=(const Quat4f& quat);

    Type DistanceSquared(const Vec4& other) const;
    Type Distance(const Vec4& other) const;

    Vec4 Normalized() const;
    Vec4& Normalize();
    Vec4& Rotate(const Vec3<float>& axis, float radians);
    /*! \brief 3-component rotation by quaternion. The w component is preserved. */
    Vec4& Rotate(const Quat4f& quaternion);
    Vec4& Lerp(const Vec4& to, float amt);
    float Dot(const Vec4& other) const;

    /*! \brief 3-component cross product. The w component is set to 0 in the result. */
    Vec4 Cross(const Vec4& other) const;
    /*! \brief 3-component reflection about a normal. The w component is preserved. */
    Vec4 Reflect(const Vec4& normal) const;
    /*! \brief 3-component angle between vectors (radians). The w component is ignored. */
    float AngleBetween(const Vec4& other) const;

    static Vec4 Abs(const Vec4&);
    static Vec4 Round(const Vec4&);
    static Vec4 Clamp(const Vec4&, float min, float max);
    static Vec4 Min(const Vec4& a, const Vec4& b);
    static Vec4 Max(const Vec4& a, const Vec4& b);

    static Vec4 Zero()
    {
        return Vec4(0, 0, 0, 0);
    }

    static Vec4 One()
    {
        return Vec4(1, 1, 1, 1);
    }

    static Vec4 UnitX()
    {
        return Vec4(1, 0, 0, 0);
    }

    static Vec4 UnitY()
    {
        return Vec4(0, 1, 0, 0);
    }

    static Vec4 UnitZ()
    {
        return Vec4(0, 0, 1, 0);
    }

    static Vec4 UnitW()
    {
        return Vec4(0, 0, 0, 1);
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode()
            .Combine(x)
            .Combine(y)
            .Combine(z)
            .Combine(w);
    }
};

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
inline constexpr bool isVec4 = false;

template <>
inline constexpr bool isVec4<Vec4f> = true;

template <>
inline constexpr bool isVec4<Vec4i> = true;

template <>
inline constexpr bool isVec4<Vec4u> = true;

// transitional typedef
using Vector4 = Vec4f;

// Format specialization
namespace utilities {

template <class StringType, class T>
struct Formatter<StringType, math::Vec4<T>>
{
    HYP_FORCE_INLINE static const char* GetFormatString()
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            static const char* formatString = "[%f %f %f %f]";

            return formatString;
        }
        else if constexpr (std::is_integral_v<T> && std::is_signed_v<T> && sizeof(T) <= 4)
        {
            static const char* formatString = "[%d %d %d %d]";

            return formatString;
        }
        else if constexpr (std::is_integral_v<T> && std::is_signed_v<T> && sizeof(T) <= 8)
        {
            static const char* formatString = "[%lld %lld %lld %lld]";

            return formatString;
        }
        else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T> && sizeof(T) <= 4)
        {
            static const char* formatString = "[%u %u %u %u]";

            return formatString;
        }
        else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T> && sizeof(T) <= 8)
        {
            static const char* formatString = "[%llu %llu %llu %llu]";

            return formatString;
        }
        else
        {
            static_assert(always_fail_v<T>, "Cannot format Vec4 type: unknown inner type");
        }
    }

    auto operator()(const math::Vec4<T>& value) const
    {
        ubyte inlineBuf[1024];
        ubyte* buf = &inlineBuf[0];

        int resultSize = std::snprintf(reinterpret_cast<char*>(buf), 1024, GetFormatString(), value.x, value.y, value.z, value.w) + 1;

        if (resultSize > HYP_ARRAY_SIZE(inlineBuf))
        {
            buf = new ubyte[resultSize];

            resultSize = std::snprintf(reinterpret_cast<char*>(buf), resultSize, GetFormatString(), value.x, value.y, value.z, value.w) + 1;

            StringType res(reinterpret_cast<char*>(buf), reinterpret_cast<char*>(buf + resultSize));

            delete[] buf;

            return res;
        }

        return StringType(reinterpret_cast<char*>(buf), reinterpret_cast<char*>(buf + resultSize));
    }
};

} // namespace utilities

} // namespace Hyperion
