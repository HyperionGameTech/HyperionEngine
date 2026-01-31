/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/math/Mat3f.hpp>
#include <core/math/Mat4f.hpp>

#include <core/memory/Memory.hpp>

#ifndef HYP_BUILDTOOL
#include <Mat3f.generated.inl>
#endif

namespace Hyperion {

Mat3f::Mat3f(LazyInitTag)
{
    // do nothing
}

Mat3f::Mat3f()
{
    Memory::Zero(rows, sizeof(rows));

    rows[0][0] = 1.0f;
    rows[1][1] = 1.0f;
    rows[2][2] = 1.0f;
}

Mat3f::Mat3f(const float(&v)[9])
{   
    Memory::Copy(rows[0].values, v + 0, sizeof(float) * 3);
    Memory::Copy(rows[1].values, v + 3, sizeof(float) * 3);
    Memory::Copy(rows[2].values, v + 6, sizeof(float) * 3);

    rows[0].values[3] = 0.0f;
    rows[1].values[3] = 0.0f;
    rows[2].values[3] = 0.0f;
}

Mat3f::Mat3f(const Mat4f& other)
{
    Memory::Copy(rows[0].values, other.rows[0], sizeof(float) * 3);
    Memory::Copy(rows[1].values, other.rows[1], sizeof(float) * 3);
    Memory::Copy(rows[2].values, other.rows[2], sizeof(float) * 3);

    rows[0].values[3] = 0.0f;
    rows[1].values[3] = 0.0f;
    rows[2].values[3] = 0.0f;
}

float Mat3f::Determinant() const
{
    float a = rows[0][0] * (rows[1][1] * rows[2][2] - rows[1][2] * rows[2][1]);
    float b = rows[0][1] * (rows[1][0] * rows[2][2] - rows[1][2] * rows[2][0]);
    float c = rows[0][2] * (rows[1][0] * rows[2][1] - rows[1][1] * rows[2][0]);

    return a - b + c;
}

Mat3f Mat3f::Transpose() const
{
    return Mat3f({
        rows[0][0], rows[1][0], rows[2][0],
        rows[0][1], rows[1][1], rows[2][1],
        rows[0][2], rows[1][2], rows[2][2]
    });
}

Mat3f Mat3f::Inverse() const
{
    const float det = Determinant();
    const float invDet = 1.0f / det;

    Mat3f result(LazyInit);

    result[0][0] = (rows[1][1] * rows[2][2] - rows[2][1] * rows[1][2]) * invDet;
    result[0][1] = (rows[0][2] * rows[2][1] - rows[0][1] * rows[2][2]) * invDet;
    result[0][2] = (rows[0][1] * rows[1][2] - rows[0][2] * rows[1][1]) * invDet;
    result[0][3] = 0.0f;

    result[1][0] = (rows[1][2] * rows[2][0] - rows[1][0] * rows[2][2]) * invDet;
    result[1][1] = (rows[0][0] * rows[2][2] - rows[0][2] * rows[2][0]) * invDet;
    result[1][2] = (rows[1][0] * rows[0][2] - rows[0][0] * rows[1][2]) * invDet;
    result[1][3] = 0.0f;

    result[2][0] = (rows[1][0] * rows[2][1] - rows[2][0] * rows[1][1]) * invDet;
    result[2][1] = (rows[2][0] * rows[0][1] - rows[0][0] * rows[2][1]) * invDet;
    result[2][2] = (rows[0][0] * rows[1][1] - rows[1][0] * rows[0][1]) * invDet;
    result[2][3] = 0.0f;

    return result;
}

Mat3f Mat3f::operator*(const Mat3f& other) const
{
    return Mat3f({
        rows[0][0] * other.rows[0][0] + rows[0][1] * other.rows[1][0] + rows[0][2] * other.rows[2][0],
        rows[0][0] * other.rows[0][1] + rows[0][1] * other.rows[1][1] + rows[0][2] * other.rows[2][1],
        rows[0][0] * other.rows[0][2] + rows[0][1] * other.rows[1][2] + rows[0][2] * other.rows[2][2],

        rows[1][0] * other.rows[0][0] + rows[1][1] * other.rows[1][0] + rows[1][2] * other.rows[2][0],
        rows[1][0] * other.rows[0][1] + rows[1][1] * other.rows[1][1] + rows[1][2] * other.rows[2][1],
        rows[1][0] * other.rows[0][2] + rows[1][1] * other.rows[1][2] + rows[1][2] * other.rows[2][2],

        rows[2][0] * other.rows[0][0] + rows[2][1] * other.rows[1][0] + rows[2][2] * other.rows[2][0],
        rows[2][0] * other.rows[0][1] + rows[2][1] * other.rows[1][1] + rows[2][2] * other.rows[2][1],
        rows[2][0] * other.rows[0][2] + rows[2][1] * other.rows[1][2] + rows[2][2] * other.rows[2][2]
    });
}

Mat3f& Mat3f::operator*=(const Mat3f& other)
{
    return (*this) = operator*(other);
}

Mat3f Mat3f::Zeros()
{
    float zeroArray[9] = { 0.0f };

    return Mat3f(zeroArray);
}

Mat3f Mat3f::Ones()
{
    static constexpr float Ones[9] = {
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f
    };

    return Mat3f(Ones);
}

Mat3f Mat3f::Identity()
{
    return Mat3f(); // constructor fills out identity matrix
}

} // namespace Hyperion
