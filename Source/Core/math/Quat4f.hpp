/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/math/MathUtil.hpp>
#include <Core/math/Vector3.hpp>

#include <Core/utilities/FormatFwd.hpp>

#include <Core/reflection/ObjectMacros.hpp>

#include <Core/HashCode.hpp>

namespace Hyperion {

class Mat4f;

HYP_STRUCT(Size = 16)
struct alignas(16) HYP_API Quat4f
{
    HYP_STRUCT_BODY(Quat4f);

    HYP_FIELD()
    float x;

    HYP_FIELD()
    float y;

    HYP_FIELD()
    float z;

    HYP_FIELD()
    float w;

    Quat4f();
    Quat4f(float x, float y, float z, float w);
    explicit Quat4f(const Mat4f& mat);
    explicit Quat4f(const Vec3f& euler);
    Quat4f(const Vec3f& axis, float radians);

    Quat4f(const Quat4f& other) = default;
    Quat4f& operator=(const Quat4f& other) = default;

    HYP_FORCE_INLINE bool operator==(const Quat4f& other) const
    {
        return x == other.x
            && y == other.y
            && z == other.z
            && w == other.w;
    }

    HYP_FORCE_INLINE bool operator!=(const Quat4f& other) const
    {
        return x != other.x
            || y != other.y
            || z != other.z
            || w != other.w;
    }

    HYP_FORCE_INLINE float GetX() const
    {
        return x;
    }

    HYP_FORCE_INLINE void SetX(float x)
    {
        this->x = x;
    }

    HYP_FORCE_INLINE float GetY() const
    {
        return y;
    }

    HYP_FORCE_INLINE void SetY(float y)
    {
        this->y = y;
    }

    HYP_FORCE_INLINE float GetZ() const
    {
        return z;
    }

    HYP_FORCE_INLINE void SetZ(float z)
    {
        this->z = z;
    }

    HYP_FORCE_INLINE float GetW() const
    {
        return w;
    }

    HYP_FORCE_INLINE void SetW(float w)
    {
        this->w = w;
    }

    Quat4f operator*(const Quat4f& other) const;
    Quat4f& operator*=(const Quat4f& other);
    Quat4f& operator+=(const Vec3f& vec);
    Vec3f operator*(const Vec3f& vec) const;

    float Length() const;
    float LengthSquared() const;
    Quat4f& Normalize();

    Quat4f Inverse() const;

    Quat4f& Slerp(const Quat4f& to, float amt);

    int GimbalPole() const;
    float Roll() const;
    float Pitch() const;
    float Yaw() const;

    static Quat4f Identity();
    static Quat4f LookAt(const Vec3f& direction, const Vec3f& up);
    static Quat4f AxisAngles(const Vec3f& axis, float radians);

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

// Format specialization

namespace utilities {

template <class StringType>
struct Formatter<StringType, Quat4f>
{
    auto operator()(const Quat4f& value) const
    {
        ubyte inlineBuf[1024];
        ubyte* buf = &inlineBuf[0];

        int resultSize = std::snprintf(reinterpret_cast<char*>(buf), 1024, "[%f %f %f %f]", value.x, value.y, value.z, value.w) + 1;

        if (resultSize > HYP_ARRAY_SIZE(inlineBuf))
        {
            buf = new ubyte[resultSize];

            resultSize = std::snprintf(reinterpret_cast<char*>(buf), resultSize, "[%f %f %f %f]", value.x, value.y, value.z, value.w) + 1;

            StringType res(reinterpret_cast<char*>(buf), reinterpret_cast<char*>(buf + resultSize));

            delete[] buf;

            return res;
        }

        return StringType(reinterpret_cast<char*>(buf), reinterpret_cast<char*>(buf + resultSize));
    }
};

} // namespace utilities

} // namespace Hyperion
