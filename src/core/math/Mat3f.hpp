/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/math/Vector3.hpp>

#include <core/Types.hpp>

#include <core/HashCode.hpp>

#include <string.h>

namespace hyperion {

HYP_STRUCT(Size = 48)
class HYP_API Mat3f
{
    HYP_STRUCT_BODY(Mat3f);

public:
    union
    {
        Vec3f rows[3];

        struct
        {
            float values[9];
            float _pad[3];
        };
    };

    Mat3f();
    explicit Mat3f(const float* v);
    Mat3f(const Mat3f& other) = default;
    Mat3f& operator=(const Mat3f& other) = default;

    float Determinant() const;
    Mat3f& Transpose();
    Mat3f Transposed() const;
    Mat3f& Invert();
    Mat3f Inverted() const;

    Mat3f operator+(const Mat3f& other) const;
    Mat3f& operator+=(const Mat3f& other);
    Mat3f operator*(const Mat3f& other) const;
    Mat3f& operator*=(const Mat3f& other);
    Mat3f operator*(float scalar) const;
    Mat3f& operator*=(float scalar);

    HYP_FORCE_INLINE bool operator==(const Mat3f& other) const
    {
        return &values[0] == &other.values[0] || !memcmp(values, other.values, std::size(values) * sizeof(values[0]));
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

    HYP_FORCE_INLINE constexpr Vec3f& operator[](uint32 row)
    {
        return rows[row];
    }

    HYP_FORCE_INLINE constexpr const Vec3f& operator[](uint32 row) const
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

} // namespace hyperion
