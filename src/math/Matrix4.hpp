/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MATRIX4_HPP
#define HYPERION_MATRIX4_HPP

#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <math/Quaternion.hpp>

#include <core/Defines.hpp>

#include <core/utilities/FormatFwd.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

#include <cstring>

namespace hyperion {

class Matrix3;

HYP_STRUCT(Size=64)
class alignas(16) HYP_API Matrix4
{
    friend std::ostream &operator<<(std::ostream &os, const Matrix4 &mat);
public:
    static const Matrix4 identity;
    static const Matrix4 zeros;
    static const Matrix4 ones;

    static Matrix4 Translation(const Vec3f &translation);
    static Matrix4 Rotation(const Quaternion &rotation);
    static Matrix4 Rotation(const Vec3f &axis, float radians);
    static Matrix4 Scaling(const Vec3f &scaling);
    static Matrix4 Perspective(float fov, int w, int h, float n, float f);
    static Matrix4 Orthographic(float l, float r, float b, float t, float n, float f);
    static Matrix4 Jitter(uint32 index, uint32 width, uint32 height, Vec4f &out_jitter);
    static Matrix4 LookAt(const Vec3f &dir, const Vec3f &up);
    static Matrix4 LookAt(const Vec3f &pos, const Vec3f &target, const Vec3f &up);

    union
    {
        Vec4f   rows[4];
        float   values[16];
    };

    Matrix4();
    explicit Matrix4(const Matrix3 &matrix3);
    explicit Matrix4(const Vec4f *rows);
    explicit Matrix4(const float *v);
    Matrix4(const Matrix4 &other) = default;
    Matrix4 &operator=(const Matrix4 &other) = default;

    float Determinant() const;
    Matrix4 &Transpose();
    Matrix4 Transposed() const;
    Matrix4 &Invert();
    Matrix4 Inverted() const;
    Matrix4 &Orthonormalize();
    Matrix4 Orthonormalized() const;

    float GetYaw() const;
    float GetPitch() const;
    float GetRoll() const;
    
    Matrix4 operator+(const Matrix4 &other) const;
    Matrix4 &operator+=(const Matrix4 &other);
    Matrix4 operator*(const Matrix4 &other) const;
    Matrix4 &operator*=(const Matrix4 &other);
    Matrix4 operator*(float scalar) const;
    Matrix4 &operator*=(float scalar);
    Vec3f operator*(const Vec3f &vec) const;
    Vec4f operator*(const Vec4f &vec) const;

    Vec3f ExtractTranslation() const;
    Vec3f ExtractScale() const;
    Quaternion ExtractRotation() const;

    Vec4f GetColumn(uint32 index) const;

    HYP_FORCE_INLINE bool operator==(const Matrix4 &other) const
        { return &values[0] == &other.values[0] || !std::memcmp(values, other.values, std::size(values) * sizeof(values[0])); }

    HYP_FORCE_INLINE bool operator!=(const Matrix4 &other) const { return !operator==(other); }

    HYP_FORCE_INLINE constexpr Vec4f &operator[](uint32 row)
        { return rows[row]; }

    HYP_FORCE_INLINE constexpr const Vec4f &operator[](uint32 row) const
        { return rows[row]; }

    static Matrix4 Zeros();
    static Matrix4 Ones();
    static Matrix4 Identity();

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        for (float value : values) {
            hc.Add(value);
        }

        return hc;
    }
};

namespace utilities {

template <class StringType>
struct Formatter<StringType, Matrix4>
{
    auto operator()(const Matrix4 &matrix) const
    {
        return StringType("Matrix4(")
            + Formatter<StringType, Vec4f>{}(matrix.rows[0]) + StringType(", ")
            + Formatter<StringType, Vec4f>{}(matrix.rows[1]) + StringType(", ")
            + Formatter<StringType, Vec4f>{}(matrix.rows[2]) + StringType(", ")
            + Formatter<StringType, Vec4f>{}(matrix.rows[3]) + StringType(")");
    }
};
    
} // namespace utilities

} // namespace hyperion

#endif
