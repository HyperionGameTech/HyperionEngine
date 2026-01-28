/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/math/Vector3.hpp>

#include <core/Types.hpp>

#include <core/HashCode.hpp>

#include <cstring>

namespace Hyperion {

class Mat4f;

HYP_STRUCT()
class HYP_API Mat3f
{
    HYP_STRUCT_BODY(Mat3f);

public:
    union
    {
        float rows[3][3];

        struct
        {
            float values[9];
        };
    };

    Mat3f();
    explicit Mat3f(const float* v);

    /*! \brief Construct a 3x3 matrix from a 4x4 matrix by taking the upper-left 3x3 portion.
     *  \param other The 4x4 matrix to convert from. */
    explicit Mat3f(const Mat4f& other);

    Mat3f(const Mat3f& other) = default;
    Mat3f& operator=(const Mat3f& other) = default;

    float Determinant() const;

    Mat3f Transpose() const;
    Mat3f Inverse() const;

    Mat3f operator+(const Mat3f& other) const;
    Mat3f& operator+=(const Mat3f& other);
    Mat3f operator*(const Mat3f& other) const;
    Mat3f& operator*=(const Mat3f& other);
    Mat3f operator*(float scalar) const;
    Mat3f& operator*=(float scalar);

    HYP_FORCE_INLINE bool operator==(const Mat3f& other) const
    {
        return &values[0] == &other.values[0] || !std::memcmp(values, other.values, sizeof(values));
    }

    HYP_FORCE_INLINE bool operator!=(const Mat3f& other) const
    {
        return !operator==(other);
    }

#pragma region deprecated
    float operator()(int i, int j) const;
    float& operator()(int i, int j);

    float At(int i, int j) const;
    float& At(int i, int j);
#pragma endregion deprecated

    HYP_FORCE_INLINE constexpr auto operator[](uint32 row) -> float(&)[3]
    {
        return rows[row];
    }

    HYP_FORCE_INLINE constexpr auto operator[](uint32 row) const -> const float (&)[3]
    {
        return rows[row];
    }

    static Mat3f Zeros();
    static Mat3f Ones();
    static Mat3f Identity();

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

} // namespace Hyperion
