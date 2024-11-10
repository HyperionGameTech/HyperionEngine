/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MATRIX3_HPP
#define HYPERION_MATRIX3_HPP

#include <math/Vector3.hpp>

#include <Types.hpp>

#include <HashCode.hpp>

#include <string.h>

namespace hyperion {

HYP_STRUCT(Size=48)
class HYP_API Matrix3
{
    friend std::ostream &operator<<(std::ostream &os, const Matrix3 &mat);
public:
    union
    {
        Vec3f   rows[3];

        struct
        {
            float   values[9];
            float   _pad[3];
        };
    };

    Matrix3();
    explicit Matrix3(const float *v);
    Matrix3(const Matrix3 &other) = default;
    Matrix3 &operator=(const Matrix3 &other) = default;

    float Determinant() const;
    Matrix3 &Transpose();
    Matrix3 Transposed() const;
    Matrix3 &Invert();
    Matrix3 Inverted() const;

    Matrix3 operator+(const Matrix3 &other) const;
    Matrix3 &operator+=(const Matrix3 &other);
    Matrix3 operator*(const Matrix3 &other) const;
    Matrix3 &operator*=(const Matrix3 &other);
    Matrix3 operator*(float scalar) const;
    Matrix3 &operator*=(float scalar);

    HYP_FORCE_INLINE bool operator==(const Matrix3 &other) const
        { return &values[0] == &other.values[0] || !memcmp(values, other.values, std::size(values) * sizeof(values[0])); }
    
    HYP_FORCE_INLINE bool operator!=(const Matrix3 &other) const
        { return !operator==(other); }

#pragma region deprecated
    float operator()(int i, int j) const;
    float &operator()(int i, int j);

    float At(int i, int j) const;
    float &At(int i, int j);
#pragma endregion deprecated

    HYP_FORCE_INLINE constexpr Vec3f &operator[](uint row)
        { return rows[row]; }

    HYP_FORCE_INLINE constexpr const Vec3f &operator[](uint row) const
        { return rows[row]; }

    static Matrix3 Zeros();
    static Matrix3 Ones();
    static Matrix3 Identity();

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        for (float value : values) {
            hc.Add(value);
        }

        return hc;
    }
};

} // namespace hyperion

#endif
