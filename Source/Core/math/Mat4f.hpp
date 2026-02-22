/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/math/Vector3.hpp>
#include <Core/math/Vector4.hpp>
#include <Core/math/Quaternion.hpp>

#include <Core/Defines.hpp>

#include <Core/utilities/FormatFwd.hpp>

#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

#include <cstring>

namespace Hyperion {

class Mat3f;

HYP_STRUCT()
class HYP_API Mat4f
{
    HYP_STRUCT_BODY(Mat4f);

public:
    static const Mat4f identity;
    static const Mat4f zeros;
    static const Mat4f ones;

    static Mat4f Translation(const Vec3f& translation);
    static Mat4f Rotation(const Quaternion& rotation);
    static Mat4f Rotation(const Vec3f& axis, float radians);
    static Mat4f Scaling(const Vec3f& scaling);
    static Mat4f Perspective(float fov, int w, int h, float n, float f);
    static Mat4f Orthographic(float l, float r, float b, float t, float n, float f);
    static Mat4f Jitter(uint32 index, uint32 width, uint32 height, Vec4f& outJitter);
    static Mat4f LookAt(const Vec3f& dir, const Vec3f& up);
    static Mat4f LookAt(const Vec3f& pos, const Vec3f& target, const Vec3f& up);

    union alignas(16)
    {
        float rows[4][4];
        float values[16];
    };

    Mat4f();
    explicit Mat4f(const Mat3f& matrix3);
    explicit Mat4f(const Vec4f* rows);
    explicit Mat4f(const float* v);
    Mat4f(const Mat4f& other) = default;
    Mat4f& operator=(const Mat4f& other) = default;

    float Determinant() const;

    Mat4f Transpose() const;
    Mat4f Inverse() const;

    Mat4f& Orthonormalize();
    Mat4f Orthonormalized() const;

    float GetYaw() const;
    float GetPitch() const;
    float GetRoll() const;

    Mat4f operator+(const Mat4f& other) const;
    Mat4f& operator+=(const Mat4f& other);
    Mat4f operator*(const Mat4f& other) const;
    Mat4f& operator*=(const Mat4f& other);
    Mat4f operator*(float scalar) const;
    Mat4f& operator*=(float scalar);
    Vec3f operator*(const Vec3f& vec) const;
    Vec4f operator*(const Vec4f& vec) const;

    Vec3f ExtractTranslation() const;
    Vec3f ExtractScale() const;
    Quaternion ExtractRotation() const;

    Vec4f GetColumn(uint32 index) const;

    HYP_FORCE_INLINE bool operator==(const Mat4f& other) const
    {
        return &values[0] == &other.values[0] || !std::memcmp(values, other.values, sizeof(values));
    }

    HYP_FORCE_INLINE bool operator!=(const Mat4f& other) const
    {
        return !operator==(other);
    }

    HYP_FORCE_INLINE constexpr auto operator[](uint32 row) -> float(&)[4]
    {
        return rows[row];
    }

    HYP_FORCE_INLINE constexpr auto operator[](uint32 row) const -> const float (&)[4]
    {
        return rows[row];
    }

    static Mat4f Zeros();
    static Mat4f Ones();
    static Mat4f Identity();

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        for (float value : values)
        {
            hc.Add(value);
        }

        return hc;
    }
};

namespace utilities {

template <class StringType>
struct Formatter<StringType, Mat4f>
{
    auto operator()(const Mat4f& matrix) const
    {
        return StringType("Mat4f(")
            + Formatter<StringType, Vec4f> {}(matrix.rows[0]) + StringType(", ")
            + Formatter<StringType, Vec4f> {}(matrix.rows[1]) + StringType(", ")
            + Formatter<StringType, Vec4f> {}(matrix.rows[2]) + StringType(", ")
            + Formatter<StringType, Vec4f> {}(matrix.rows[3]) + StringType(")");
    }
};

} // namespace utilities

} // namespace Hyperion
