#ifndef MATRIX3_H
#define MATRIX3_H

#include <iostream>
#include <array>

namespace hyperion {
class Matrix3 {
    friend std::ostream &operator<<(std::ostream &os, const Matrix3 &mat);
public:
    std::array<float, 9> values;

    Matrix3();
    explicit Matrix3(float *v);
    Matrix3(const Matrix3 &other);

    float Determinant() const;
    Matrix3 &Transpose();
    Matrix3 &Invert();

    Matrix3 &operator=(const Matrix3 &other);
    Matrix3 operator+(const Matrix3 &other) const;
    Matrix3 &operator+=(const Matrix3 &other);
    Matrix3 operator*(const Matrix3 &other) const;
    Matrix3 &operator*=(const Matrix3 &other);
    Matrix3 operator*(float scalar) const;
    Matrix3 &operator*=(float scalar);
    bool operator==(const Matrix3 &other) const;
    float operator()(int i, int j) const;
    float &operator()(int i, int j);

    float At(int i, int j) const;
    float &At(int i, int j);

    static Matrix3 Zeroes();
    static Matrix3 Ones();
    static Matrix3 Identity();
};

static_assert(sizeof(Matrix3) == sizeof(float) * 9, "sizeof(Matrix3) must be equal to sizeof(float) * 9");

}

#endif
