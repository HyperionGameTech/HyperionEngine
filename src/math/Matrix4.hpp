
#ifndef MATRIX4_H
#define MATRIX4_H

#include "Vector4.hpp"
#include "../HashCode.hpp"
#include "../Util.hpp"
#include <Types.hpp>
#include <math/intrinsics/Intrinsics.hpp>

#include <iostream>
#include <array>
#include <cstring>

namespace hyperion {

class Vector3;
class Quaternion;

class Matrix4 {
    friend std::ostream &operator<<(std::ostream &os, const Matrix4 &mat);
public:
    static Matrix4 Translation(const Vector3 &translation);
#if 0
    static Matrix4 Rotation(const Matrix4 &transform);
#endif
    static Matrix4 Rotation(const Quaternion &rotation);
    static Matrix4 Rotation(const Vector3 &axis, Float radians);
    static Matrix4 Scaling(const Vector3 &scaling);
    static Matrix4 Perspective(Float fov, Int w, Int h, Float n, Float f);
    static Matrix4 Orthographic(Float l, Float r, Float b, Float t, Float n, Float f);
    static Matrix4 LookAt(const Vector3 &dir, const Vector3 &up);
    static Matrix4 LookAt(const Vector3 &pos, const Vector3 &target, const Vector3 &up);

    union {
        Vector4 rows[4];
        Float values[16];
    };

    Matrix4();
    explicit Matrix4(const Vector4 *rows);
    explicit Matrix4(const Float *v);
    Matrix4(const Matrix4 &other);

    Float Determinant() const;
    Matrix4 &Transpose();
    Matrix4 Transposed() const;
    Matrix4 &Invert();
    Matrix4 Inverted() const;

    Float GetYaw() const;
    Float GetPitch() const;
    Float GetRoll() const;

    Matrix4 &operator=(const Matrix4 &other);
    Matrix4 operator+(const Matrix4 &other) const;
    Matrix4 &operator+=(const Matrix4 &other);
    Matrix4 operator*(const Matrix4 &other) const;
    Matrix4 &operator*=(const Matrix4 &other);
    Matrix4 operator*(Float scalar) const;
    Matrix4 &operator*=(Float scalar);
    Vector3 operator*(const Vector3 &vec) const;
    Vector4 operator*(const Vector4 &vec) const;

    Vector4 GetColumn(UInt index) const;

    bool operator==(const Matrix4 &other) const
    {  return &values[0] == &other.values[0] || !std::memcmp(values, other.values, std::size(values) * sizeof(values[0])); }

    bool operator!=(const Matrix4 &other) const { return !operator==(other); }

#pragma region deprecated
    constexpr float operator()(int i, int j) const { return values[i * 4 + j]; }
    constexpr float &operator()(int i, int j) { return values[i * 4 + j]; }

    constexpr float At(int i, int j) const { return rows[i][j]; }
    constexpr float &At(int i, int j) { return rows[i][j]; }
#pragma endregion

    constexpr Vector4 &operator[](UInt row) { return rows[row]; }
    constexpr const Vector4 &operator[](UInt row) const { return rows[row]; }

    static Matrix4 Zeros();
    static Matrix4 Ones();
    static Matrix4 Identity();

    HashCode GetHashCode() const
    {
        HashCode hc;

        for (Float value : values) {
            hc.Add(value);
        }

        return hc;
    }
};

static_assert(sizeof(Matrix4) == sizeof(float) * 16, "sizeof(Matrix4) must be equal to sizeof(float) * 16");

} // namespace hyperion

#endif
