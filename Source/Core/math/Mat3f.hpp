/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/math/Vector3.hpp>

#include <Core/Types.hpp>

#include <Core/HashCode.hpp>

#include <cstring>

namespace Hyperion {

class Mat4f;

HYP_STRUCT()
class HYP_API Mat3f
{
    HYP_STRUCT_BODY(Mat3f);

    enum LazyInitTag
    {
        LazyInit
    };

    explicit Mat3f(LazyInitTag);

public:
    Vec3f rows[3];

    Mat3f();
    explicit Mat3f(const float(&v)[9]);

    /*! \brief Construct a 3x3 matrix from a 4x4 matrix by taking the upper-left 3x3 portion.
     *  \param other The 4x4 matrix to convert from. */
    explicit Mat3f(const Mat4f& other);

    Mat3f(const Mat3f& other) = default;
    Mat3f& operator=(const Mat3f& other) = default;

    float Determinant() const;

    Mat3f Transpose() const;
    Mat3f Inverse() const;

    Mat3f operator*(const Mat3f& other) const;
    Mat3f& operator*=(const Mat3f& other);

    HYP_FORCE_INLINE bool operator==(const Mat3f& other) const
    {
        return std::memcmp(rows, other.rows, sizeof(rows)) == 0;
    }

    HYP_FORCE_INLINE bool operator!=(const Mat3f& other) const
    {
        return !operator==(other);
    }

    HYP_FORCE_INLINE constexpr auto operator[](uint32 row) -> float(&)[3]
    {
        return rows[row].values;
    }

    HYP_FORCE_INLINE constexpr auto operator[](uint32 row) const -> const float (&)[3]
    {
        return rows[row].values;
    }

    static Mat3f Zeros();
    static Mat3f Ones();
    static Mat3f Identity();

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(rows[0].GetHashCode());
        hc.Add(rows[1].GetHashCode());
        hc.Add(rows[2].GetHashCode());

        return hc;
    }
};

} // namespace Hyperion
