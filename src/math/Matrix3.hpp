#ifndef MATRIX3_H
#define MATRIX3_H

#include "Vector3.hpp"

#include <Types.hpp>

#include <iostream>
#include <array>
#include <string.h>

namespace hyperion {

class HYP_API Matrix3
{
    friend std::ostream &operator<<(std::ostream &os, const Matrix3 &mat);
public:
    union {
        Vector3 rows[3];

        struct {
            float values[9];
            float _pad[3];
        };
    };

    Matrix3();
    explicit Matrix3(const float *v);
    Matrix3(const Matrix3 &other);

    float Determinant() const;
    Matrix3 &Transpose();
    Matrix3 Transposed() const;
    Matrix3 &Invert();
    Matrix3 Inverted() const;

    Matrix3 &operator=(const Matrix3 &other);
    Matrix3 operator+(const Matrix3 &other) const;
    Matrix3 &operator+=(const Matrix3 &other);
    Matrix3 operator*(const Matrix3 &other) const;
    Matrix3 &operator*=(const Matrix3 &other);
    Matrix3 operator*(float scalar) const;
    Matrix3 &operator*=(float scalar);

    bool operator==(const Matrix3 &other) const
    {  return &values[0] == &other.values[0] || !memcmp(values, other.values, std::size(values) * sizeof(values[0])); }

    bool operator!=(const Matrix3 &other) const { return !operator==(other); }

#pragma region deprecated
    float operator()(int i, int j) const;
    float &operator()(int i, int j);

    float At(int i, int j) const;
    float &At(int i, int j);
#pragma endregion

    constexpr Vector3 &operator[](uint row) { return rows[row]; }
    constexpr const Vector3 &operator[](uint row) const { return rows[row]; }

    static Matrix3 Zeros();
    static Matrix3 Ones();
    static Matrix3 Identity();
};

static_assert(sizeof(Matrix3) == sizeof(float) * 12, "sizeof(Matrix3) must be equal to sizeof(float) * 12");

} // namespace hyperion

#endif
