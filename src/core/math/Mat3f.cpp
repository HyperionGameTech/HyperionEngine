/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/math/Mat3f.hpp>

#include <core/memory/Memory.hpp>

#ifndef HYP_BUILDTOOL
#include <Mat3f.generated.inl>
#endif

namespace Hyperion {

Mat3f::Mat3f()
    : rows {
          { 1.0f, 0.0f, 0.0f },
          { 0.0f, 1.0f, 0.0f },
          { 0.0f, 0.0f, 1.0f }
      }
{
}

Mat3f::Mat3f(const float* v)
{
    Memory::MemCpy(&values[0], v, HYP_ARRAY_SIZE(values) * sizeof(values[0]));
}

float Mat3f::Determinant() const
{
    float a = rows[0][0] * (rows[1][1] * rows[2][2] - rows[1][2] * rows[2][1]);
    float b = rows[0][1] * (rows[1][0] * rows[2][2] - rows[1][2] * rows[2][0]);
    float c = rows[0][2] * (rows[1][0] * rows[2][1] - rows[1][1] * rows[2][0]);
    return a - b + c;
}

Mat3f Mat3f::Transposed() const
{
    const float v[3][3] = {
        { rows[0][0], rows[1][0], rows[2][0] },
        { rows[0][1], rows[1][1], rows[2][1] },
        { rows[0][2], rows[1][2], rows[2][2] }
    };

    return Mat3f(reinterpret_cast<const float*>(v));
}

Mat3f& Mat3f::Transpose()
{
    return *this = Transposed();
}

Mat3f Mat3f::Inverted() const
{
    const float det = Determinant();
    const float invDet = 1.0f / det;

    Mat3f result;
    result[0][0] = (rows[1][1] * rows[2][2] - rows[2][1] * rows[1][2]) * invDet;
    result[0][1] = (rows[0][2] * rows[2][1] - rows[0][1] * rows[2][2]) * invDet;
    result[0][2] = (rows[0][1] * rows[1][2] - rows[0][2] * rows[1][1]) * invDet;
    result[1][0] = (rows[1][2] * rows[2][0] - rows[1][0] * rows[2][2]) * invDet;
    result[1][1] = (rows[0][0] * rows[2][2] - rows[0][2] * rows[2][0]) * invDet;
    result[1][2] = (rows[1][0] * rows[0][2] - rows[0][0] * rows[1][2]) * invDet;
    result[2][0] = (rows[1][0] * rows[2][1] - rows[2][0] * rows[1][1]) * invDet;
    result[2][1] = (rows[2][0] * rows[0][1] - rows[0][0] * rows[2][1]) * invDet;
    result[2][2] = (rows[0][0] * rows[1][1] - rows[1][0] * rows[0][1]) * invDet;

    return result;
}

Mat3f& Mat3f::Invert()
{
    return *this = Inverted();
}

Mat3f Mat3f::operator+(const Mat3f& other) const
{
    Mat3f result(*this);

    for (int i = 0; i < HYP_ARRAY_SIZE(values); i++)
    {
        result.values[i] += other.values[i];
    }

    return result;
}

Mat3f& Mat3f::operator+=(const Mat3f& other)
{
    for (int i = 0; i < HYP_ARRAY_SIZE(values); i++)
    {
        values[i] += other.values[i];
    }

    return *this;
}

Mat3f Mat3f::operator*(const Mat3f& other) const
{
    const float fv[] = {
        values[0] * other.values[0] + values[1] * other.values[3] + values[2] * other.values[6],
        values[0] * other.values[1] + values[1] * other.values[4] + values[2] * other.values[7],
        values[0] * other.values[2] + values[1] * other.values[5] + values[2] * other.values[8],

        values[3] * other.values[0] + values[4] * other.values[3] + values[5] * other.values[6],
        values[3] * other.values[1] + values[4] * other.values[4] + values[5] * other.values[7],
        values[3] * other.values[2] + values[4] * other.values[5] + values[5] * other.values[8],

        values[6] * other.values[0] + values[7] * other.values[3] + values[8] * other.values[6],
        values[6] * other.values[1] + values[7] * other.values[4] + values[8] * other.values[7],
        values[6] * other.values[2] + values[7] * other.values[5] + values[8] * other.values[8]
    };

    return Mat3f(fv);
}

Mat3f& Mat3f::operator*=(const Mat3f& other)
{
    return (*this) = operator*(other);
}

Mat3f Mat3f::operator*(float scalar) const
{
    Mat3f result(*this);

    for (int i = 0; i < HYP_ARRAY_SIZE(values); i++)
    {
        result.values[i] *= scalar;
    }

    return result;
}

Mat3f& Mat3f::operator*=(float scalar)
{
    for (int i = 0; i < HYP_ARRAY_SIZE(values); i++)
    {
        values[i] *= scalar;
    }

    return *this;
}

float Mat3f::operator()(int i, int j) const
{
    return values[i * 3 + j];
}

float& Mat3f::operator()(int i, int j)
{
    return values[i * 3 + j];
}

float Mat3f::At(int i, int j) const
{
    return operator()(i, j);
}

float& Mat3f::At(int i, int j)
{
    return operator()(i, j);
}

Mat3f Mat3f::Zeros()
{
    float zeroArray[sizeof(values) / sizeof(values[0])] = { 0.0f };

    return Mat3f(zeroArray);
}

Mat3f Mat3f::Ones()
{
    float onesArray[sizeof(values) / sizeof(values[0])] = { 1.0f };

    return Mat3f(onesArray);
}

Mat3f Mat3f::Identity()
{
    return Mat3f(); // constructor fills out identity matrix
}
} // namespace Hyperion
