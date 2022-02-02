#ifndef MATRIX4_H
#define MATRIX4_H

#include "../hash_code.h"

#include <iostream>
#include <array>

namespace hyperion {
class Matrix4 {
    friend std::ostream &operator<<(std::ostream &os, const Matrix4 &mat);
public:
    std::array<float, 16> values;

    Matrix4();
    Matrix4(float *v);
    Matrix4(const Matrix4 &other);

    float Determinant() const;
    Matrix4 &Transpose();
    Matrix4 &Invert();

    Matrix4 &operator=(const Matrix4 &other);
    Matrix4 operator+(const Matrix4 &other) const;
    Matrix4 &operator+=(const Matrix4 &other);
    Matrix4 operator*(const Matrix4 &other) const;
    Matrix4 &operator*=(const Matrix4 &other);
    Matrix4 operator*(float scalar) const;
    Matrix4 &operator*=(float scalar);
    bool operator==(const Matrix4 &other) const;

    float operator()(int i, int j) const { return values[i * 4 + j]; }
    inline float &operator()(int i, int j) { return values[i * 4 + j]; }
    inline float At(int i, int j) const { return operator()(i, j); }
    inline float &At(int i, int j) { return operator()(i, j); }
    inline constexpr float operator[](int index) const { return values[index]; }
    inline constexpr float &operator[](int index) { return values[index]; }

    static Matrix4 Zeroes();
    static Matrix4 Ones();
    static Matrix4 Identity();

    inline HashCode GetHashCode() const
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
