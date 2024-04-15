/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */


#ifndef MATRIX4_H
#define MATRIX4_H

#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <math/Quaternion.hpp>
#include <util/Defines.hpp>
#include <HashCode.hpp>
#include <Types.hpp>

#include <iostream>
#include <cstring>

namespace hyperion {

class Matrix3;

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
    static Matrix4 Jitter(uint index, uint width, uint height, Vec4f &out_jitter);
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

    Vec3f ExtractTransformScale() const;
    Quaternion ExtractRotation() const;

    Vec4f GetColumn(uint index) const;

    HYP_FORCE_INLINE
    bool operator==(const Matrix4 &other) const
        { return &values[0] == &other.values[0] || !std::memcmp(values, other.values, std::size(values) * sizeof(values[0])); }

    HYP_FORCE_INLINE
    bool operator!=(const Matrix4 &other) const { return !operator==(other); }

#pragma region deprecated
    constexpr float operator()(int i, int j) const { return values[i * 4 + j]; }
    constexpr float &operator()(int i, int j) { return values[i * 4 + j]; }

    constexpr float At(int i, int j) const { return rows[i][j]; }
    constexpr float &At(int i, int j) { return rows[i][j]; }
#pragma endregion

    HYP_FORCE_INLINE
    constexpr Vec4f &operator[](uint row) { return rows[row]; }

    HYP_FORCE_INLINE
    constexpr const Vec4f &operator[](uint row) const { return rows[row]; }

    static Matrix4 Zeros();
    static Matrix4 Ones();
    static Matrix4 Identity();

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;

        for (float value : values) {
            hc.Add(value);
        }

        return hc;
    }
};

static_assert(sizeof(Matrix4) == sizeof(float) * 16, "sizeof(Matrix4) must be equal to sizeof(float) * 16");

} // namespace hyperion

#endif
