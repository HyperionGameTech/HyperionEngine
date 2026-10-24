/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>
#include <Core/Math/Quat4f.hpp>

#include <Core/Defines.hpp>

#include <Core/Utilities/FormatFwd.hpp>

#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

#include <cstring>

namespace Hyperion {

class Mat3f;

HYP_STRUCT()
class CORE_API Mat4f
{
    HYP_STRUCT_BODY(Mat4f);

public:
    static const Mat4f identity;
    static const Mat4f zeros;
    static const Mat4f ones;

    static Mat4f Translation(const Vec3f& translation);

    static Mat4f Rotation(const Quat4f& rotation);
    static Mat4f Rotation(const Vec3f& axis, float radians);

    static Mat4f Scaling(const Vec3f& scaling);

    static Mat4f Perspective(float fov, int w, int h, float n, float f);

    static Mat4f Orthographic(float l, float r, float b, float t, float n, float f);

    static Mat4f Jitter(uint32 index, uint32 width, uint32 height, Vec4f& outJitter);

    static Mat4f LookAt(const Vec3f& pos, const Vec3f& target, const Vec3f& up);
    static Mat4f LookAt(const Vec3f& dir, const Vec3f& up);

    union alignas(16)
    {
        float rows[4][4];
        float values[16];
    };

    Mat4f();

    explicit Mat4f(const Mat3f& matrix3);
    explicit Mat4f(const Vec4f* rows);
    explicit Mat4f(const float* v);

    Mat4f(const Mat4f& other) = default;
    Mat4f& operator=(const Mat4f& other) = default;

    HYP_METHOD()
    float Determinant() const;

    HYP_METHOD()
    Mat4f Transpose() const;

    HYP_METHOD()
    Mat4f Inverse() const;

    HYP_METHOD()
    Mat4f& Orthonormalize();

    HYP_METHOD()
    Mat4f Orthonormalized() const;

    HYP_METHOD()
    float GetYaw() const;

    HYP_METHOD()
    float GetPitch() const;

    HYP_METHOD()
    float GetRoll() const;

    HYP_METHOD()
    Mat4f operator*(const Mat4f& other) const;
    Mat4f& operator*=(const Mat4f& other);

    Vec3f TransformVector(const Vec3f& vec) const;
    Vec4f TransformVector(const Vec4f& vec) const;

    HYP_METHOD()
    Vec3f ExtractTranslation() const;

    HYP_METHOD()
    Vec3f ExtractScale() const;

    float ExtractMaxScale() const;

    HYP_METHOD()
    Quat4f ExtractRotation() const;

    HYP_METHOD()
    Vec4f GetColumn(uint32 index) const;

    HYP_METHOD()
    HYP_FORCE_INLINE bool operator==(const Mat4f& other) const
    {
        return &values[0] == &other.values[0] || !std::memcmp(values, other.values, sizeof(values));
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool operator!=(const Mat4f& other) const
    {
        return !operator==(other);
    }

    HYP_FORCE_INLINE constexpr auto operator[](uint32 row) -> float(&)[4]
    {
        return rows[row];
    }

    HYP_FORCE_INLINE constexpr auto operator[](uint32 row) const -> const float (&)[4]
    {
        return rows[row];
    }

    HYP_METHOD()
    static Mat4f Zeros();

    HYP_METHOD()
    static Mat4f Ones();

    HYP_METHOD()
    static Mat4f Identity();

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

namespace utilities {

template <class StringType>
struct Formatter<StringType, Mat4f>
{
    template <class OutType>
    void Append(OutType& out, const Mat4f& matrix) const
    {
        FormatInto_WriteString(out, "Mat4f(");

        for (uint32 rowIndex = 0; rowIndex < 4; rowIndex++)
        {
            if (rowIndex != 0)
            {
                FormatInto_WriteString(out, ", ");
            }

            const float(&row)[4] = matrix.rows[rowIndex];

            Formatter<StringType, Vec4f> {}.Append(out, Vec4f(row[0], row[1], row[2], row[3]));
        }

        FormatInto_WriteString(out, ")");
    }

    auto operator()(const Mat4f& matrix) const
    {
        StringType result;
        Append(result, matrix);

        return result;
    }
};

} // namespace utilities

} // namespace Hyperion
