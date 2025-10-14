/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>
#include <core/math/Quaternion.hpp>

#include <core/Defines.hpp>

#include <core/utilities/FormatFwd.hpp>

#include <core/HashCode.hpp>
#include <core/Types.hpp>

#include <cstring>

namespace hyperion {

class Mat3f;

HYP_STRUCT(Size = 64)
class alignas(16) HYP_API Mat4f
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

    union
    {
        Vec4f rows[4];
        float values[16];
    };

    Mat4f();
    explicit Mat4f(const Mat3f& matrix3);
    explicit Mat4f(const Vec4f* rows);
    explicit Mat4f(const float* v);
    Mat4f(const Mat4f& other) = default;
    Mat4f& operator=(const Mat4f& other) = default;

    float Determinant() const;
    Mat4f& Transpose();
    Mat4f Transposed() const;
    Mat4f& Invert();
    Mat4f Inverted() const;
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
        return &values[0] == &other.values[0] || !std::memcmp(values, other.values, std::size(values) * sizeof(values[0]));
    }

    HYP_FORCE_INLINE bool operator!=(const Mat4f& other) const
    {
        return !operator==(other);
    }

    HYP_FORCE_INLINE constexpr Vec4f& operator[](uint32 row)
    {
        return rows[row];
    }

    HYP_FORCE_INLINE constexpr const Vec4f& operator[](uint32 row) const
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

} // namespace hyperion
