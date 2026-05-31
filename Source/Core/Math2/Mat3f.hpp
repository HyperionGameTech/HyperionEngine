/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/math/Vector3.hpp>

#include <Core/Types.hpp>

#include <Core/HashCode.hpp>

#include <cstring>

namespace Hyperion {

class Mat4f;

HYP_STRUCT()
class CORE_API Mat3f
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

    HYP_METHOD()
    float Determinant() const;

    HYP_METHOD()
    Mat3f Transpose() const;

    HYP_METHOD()
    Mat3f Inverse() const;

    HYP_METHOD()
    Mat3f operator*(const Mat3f& other) const;
    Mat3f& operator*=(const Mat3f& other);

    HYP_METHOD()
    HYP_FORCE_INLINE bool operator==(const Mat3f& other) const
    {
        return std::memcmp(rows, other.rows, sizeof(rows)) == 0;
    }

    HYP_METHOD()
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

    HYP_METHOD()
    static Mat3f Zeros();

    HYP_METHOD()
    static Mat3f Ones();

    HYP_METHOD()
    static Mat3f Identity();

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode()
            .Combine(rows[0].GetHashCode())
            .Combine(rows[1].GetHashCode())
            .Combine(rows[2].GetHashCode());
    }
};

} // namespace Hyperion
