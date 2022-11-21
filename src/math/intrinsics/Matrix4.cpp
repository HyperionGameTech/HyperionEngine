#include "../Matrix4.hpp"
#include "../Vector3.hpp"
#include "../Quaternion.hpp"
#include "../Rect.hpp"

#include <core/Core.hpp>

#include "Intrinsics.hpp"

#if defined(HYP_FEATURES_INTRINSICS) && HYP_FEATURES_INTRINSICS

namespace hyperion {

using namespace intrinsics;

Matrix4 Matrix4::Translation(const Vector3 &translation)
{
    Matrix4 mat;

    mat[0][3] = translation.x;
    mat[1][3] = translation.y;
    mat[2][3] = translation.z;

    return mat;
}

#if 0
Matrix4 Matrix4::Rotation(const Matrix4 &t)
{
    Matrix4 mat;
    Matrix4 transform = t;

    const float scale_x = 1.0f / Vector3(transform.GetColumn(0)).Length();
    const float scale_y = 1.0f / Vector3(transform.GetColumn(1)).Length();
    const float scale_z = 1.0f / Vector3(transform.GetColumn(2)).Length();

    mat[0] = {
        transform[0][0] * scale_x,
        transform[0][1] * scale_x,
        transform[0][2] * scale_x,
        0.0f
    };

    mat[1] = {
        transform[1][0] * scale_y,
        transform[1][1] * scale_y,
        transform[1][2] * scale_y,
        0.0f
    };

    mat[2] = {
        transform[2][0] * scale_z,
        transform[2][1] * scale_z,
        transform[2][2] * scale_z,
        0.0f
    };

    return mat;
}

#endif

Matrix4 Matrix4::Rotation(const Quaternion &rotation)
{
    Matrix4 mat;

    Float128 result = Float128Zero();

    const float xx = rotation.x * rotation.x,
        xy = rotation.x * rotation.y,
        xz = rotation.x * rotation.z,
        xw = rotation.x * rotation.w,
        yy = rotation.y * rotation.y,
        yz = rotation.y * rotation.z,
        yw = rotation.y * rotation.w,
        zz = rotation.z * rotation.z,
        zw = rotation.z * rotation.w;

    mat[0] = {
        1.0f - 2.0f * (yy + zz),
        2.0f * (xy + zw),
        2.0f * (xz - yw),
        0.0f
    };

    mat[1] = {
        2.0f * (xy - zw),
        1.0f - 2.0f * (xx + zz),
        2.0f * (yz + xw),
        0.0f
    };

    mat[2] = {
        2.0f * (xz + yw),
        2.0f * (yz - xw),
        1.0f - 2.0f * (xx + yy),
        0.0f
    };

    return mat;
}

Matrix4 Matrix4::Rotation(const Vector3 &axis, float radians)
{
    return Rotation(Quaternion(axis, radians));
}

Matrix4 Matrix4::Scaling(const Vector3 &scale)
{
    Matrix4 mat;

    mat[0][0] = scale.x;
    mat[1][1] = scale.y;
    mat[2][2] = scale.z;

    return mat;
}

Matrix4 Matrix4::Perspective(float fov, int w, int h, float n, float f)
{
    Matrix4 mat = Zeros();

    float ar = (float)w / (float)h;
    float tan_half_fov = tan(MathUtil::DegToRad(fov / 2.0f));
    float range = n - f;

    mat[0][0] = 1.0f / (tan_half_fov * ar);

    mat[1][1] = -(1.0f / (tan_half_fov));

    mat[2][2] = (-n - f) / range;
    mat[2][3] = (2.0f * f * n) / range;

    mat[3][2] = 1.0f;
    mat[3][3] = 0.0f;

    return mat;
}

Matrix4 Matrix4::Orthographic(float l, float r, float b, float t, float n, float f)
{
    Matrix4 mat;

    float x_orth = 2.0f / (r - l);
    float y_orth = 2.0f / (t - b);
    float z_orth = 1.0f / (f - n);
    float tx = ((r + l) / (l - r));
    float ty = ((b + t) / (b - t));
    float tz = ((-n) / (f - n));

    mat[0] = { x_orth, 0, 0, 0 };
    mat[1] = { 0, y_orth, 0, 0 };
    mat[2] = { 0, 0, z_orth, 0 };
    mat[3] = { tx, ty, tz, 1 };

    mat.Transpose();

    return mat;
}

Matrix4 Matrix4::LookAt(const Vector3 &direction, const Vector3 &up)
{
    auto mat = Identity();

    const Vector3 z = direction.Normalized();
    const Vector3 x = direction.Cross(up).Normalize();
    const Vector3 y = x.Cross(z).Normalize();

    mat[0] = Vector4(x, 0.0f);
    mat[1] = Vector4(y, 0.0f);
    mat[2] = Vector4(z, 0.0f);

    // const Vector3 z = direction.Normalized();
    // const Vector3 x = up.Cross(direction).Normalized();
    // const Vector3 y = direction.Cross(x).Normalized();

    // mat[0] = Vector4(x, 0.0f);
    // mat[1] = Vector4(y, 0.0f);
    // mat[2] = Vector4(z, 0.0f);

    return mat;
}

Matrix4 Matrix4::LookAt(const Vector3 &pos, const Vector3 &target, const Vector3 &up)
{
    return LookAt(target - pos, up) * Translation(pos * -1);
}

Matrix4::Matrix4()
    : rows {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
}
{
}

Matrix4::Matrix4(const Vector4 *rows)
    : rows {
    rows[0],
    rows[1],
    rows[2],
    rows[3]
}
{
}

Matrix4::Matrix4(const float *v)
{
    rows[0] = { v[0],  v[1],  v[2],  v[3] };
    rows[1] = { v[4],  v[5],  v[6],  v[7] };
    rows[2] = { v[8],  v[9],  v[10], v[11] };
    rows[3] = { v[12], v[13], v[14], v[15] };
}

Matrix4::Matrix4(const Matrix4 &other)
{
    rows[0] = other.rows[0];
    rows[1] = other.rows[1];
    rows[2] = other.rows[2];
    rows[3] = other.rows[3];
    //hyperion::Memory::Copy(values, other.values, sizeof(values));
}

float Matrix4::Determinant() const
{
    return rows[3][0] * rows[2][1] * rows[1][2] * rows[0][3] - rows[2][0] * rows[3][1] * rows[1][2] * rows[0][3] - rows[3][0] * rows[1][1]
                                                                                                                   * rows[2][2] * rows[0][3] + rows[1][0] * rows[3][1] * rows[2][2] * rows[0][3] + rows[2][0] * rows[1][1] * rows[3][2] * rows[0][3] - rows[1][0]
                                                                                                                                                                                                                                                       * rows[2][1] * rows[3][2] * rows[0][3] - rows[3][0] * rows[2][1] * rows[0][2] * rows[1][3] + rows[2][0] * rows[3][1] * rows[0][2] * rows[1][3]
           + rows[3][0] * rows[0][1] * rows[2][2] * rows[1][3] - rows[0][0] * rows[3][1] * rows[2][2] * rows[1][3] - rows[2][0] * rows[0][1] * rows[3][2]
                                                                                                                     * rows[1][3] + rows[0][0] * rows[2][1] * rows[3][2] * rows[1][3] + rows[3][0] * rows[1][1] * rows[0][2] * rows[2][3] - rows[1][0] * rows[3][1]
                                                                                                                                                                                                                                            * rows[0][2] * rows[2][3] - rows[3][0] * rows[0][1] * rows[1][2] * rows[2][3] + rows[0][0] * rows[3][1] * rows[1][2] * rows[2][3] + rows[1][0]
                                                                                                                                                                                                                                                                                                                                                                                * rows[0][1] * rows[3][2] * rows[2][3] - rows[0][0] * rows[1][1] * rows[3][2] * rows[2][3] - rows[2][0] * rows[1][1] * rows[0][2] * rows[3][3]
           + rows[1][0] * rows[2][1] * rows[0][2] * rows[3][3] + rows[2][0] * rows[0][1] * rows[1][2] * rows[3][3] - rows[0][0] * rows[2][1] * rows[1][2]
                                                                                                                     * rows[3][3] - rows[1][0] * rows[0][1] * rows[2][2] * rows[3][3] + rows[0][0] * rows[1][1] * rows[2][2] * rows[3][3];
}

Matrix4 &Matrix4::Transpose()
{
    return operator=(Transposed());
}

Matrix4 Matrix4::Transposed() const
{
    const float v[4][4] = {
        { rows[0][0], rows[1][0], rows[2][0], rows[3][0] },
        { rows[0][1], rows[1][1], rows[2][1], rows[3][1] },
        { rows[0][2], rows[1][2], rows[2][2], rows[3][2] },
        { rows[0][3], rows[1][3], rows[2][3], rows[3][3] }
    };

    return Matrix4(reinterpret_cast<const float *>(v));
}

Matrix4 &Matrix4::Invert()
{
    return operator=(Inverted());
}

Matrix4 Matrix4::Inverted() const
{
    Float det = Determinant();
    Float inv_det = 1.0f / det;

    Float tmp[4][4];

    tmp[0][0] = (rows[1][2] * rows[2][3] * rows[3][1] - rows[1][3] * rows[2][2] * rows[3][1] + rows[1][3] * rows[2][1] * rows[3][2] - rows[1][1]
                                                                                                                                      * rows[2][3] * rows[3][2] - rows[1][2] * rows[2][1] * rows[3][3] + rows[1][1] * rows[2][2] * rows[3][3])
                * inv_det;

    tmp[0][1] = (rows[0][3] * rows[2][2] * rows[3][1] - rows[0][2] * rows[2][3] * rows[3][1] - rows[0][3] * rows[2][1] * rows[3][2] + rows[0][1]
                                                                                                                                      * rows[2][3] * rows[3][2] + rows[0][2] * rows[2][1] * rows[3][3] - rows[0][1] * rows[2][2] * rows[3][3])
                * inv_det;

    tmp[0][2] = (rows[0][2] * rows[1][3] * rows[3][1] - rows[0][3] * rows[1][2] * rows[3][1] + rows[0][3] * rows[1][1] * rows[3][2] - rows[0][1]
                                                                                                                                      * rows[1][3] * rows[3][2] - rows[0][2] * rows[1][1] * rows[3][3] + rows[0][1] * rows[1][2] * rows[3][3])
                * inv_det;

    tmp[0][3] = (rows[0][3] * rows[1][2] * rows[2][1] - rows[0][2] * rows[1][3] * rows[2][1] - rows[0][3] * rows[1][1] * rows[2][2] + rows[0][1]
                                                                                                                                      * rows[1][3] * rows[2][2] + rows[0][2] * rows[1][1] * rows[2][3] - rows[0][1] * rows[1][2] * rows[2][3])
                * inv_det;

    tmp[1][0] = (rows[1][3] * rows[2][2] * rows[3][0] - rows[1][2] * rows[2][3] * rows[3][0] - rows[1][3] * rows[2][0] * rows[3][2] + rows[1][0]
                                                                                                                                      * rows[2][3] * rows[3][2] + rows[1][2] * rows[2][0] * rows[3][3] - rows[1][0] * rows[2][2] * rows[3][3])
                * inv_det;

    tmp[1][1] = (rows[0][2] * rows[2][3] * rows[3][0] - rows[0][3] * rows[2][2] * rows[3][0] + rows[0][3] * rows[2][0] * rows[3][2] - rows[0][0]
                                                                                                                                      * rows[2][3] * rows[3][2] - rows[0][2] * rows[2][0] * rows[3][3] + rows[0][0] * rows[2][2] * rows[3][3])
                * inv_det;

    tmp[1][2] = (rows[0][3] * rows[1][2] * rows[3][0] - rows[0][2] * rows[1][3] * rows[3][0] - rows[0][3] * rows[1][0] * rows[3][2] + rows[0][0]
                                                                                                                                      * rows[1][3] * rows[3][2] + rows[0][2] * rows[1][0] * rows[3][3] - rows[0][0] * rows[1][2] * rows[3][3])
                * inv_det;

    tmp[1][3] = (rows[0][2] * rows[1][3] * rows[2][0] - rows[0][3] * rows[1][2] * rows[2][0] + rows[0][3] * rows[1][0] * rows[2][2] - rows[0][0]
                                                                                                                                      * rows[1][3] * rows[2][2] - rows[0][2] * rows[1][0] * rows[2][3] + rows[0][0] * rows[1][2] * rows[2][3])
                * inv_det;

    tmp[2][0] = (rows[1][1] * rows[2][3] * rows[3][0] - rows[1][3] * rows[2][1] * rows[3][0] + rows[1][3] * rows[2][0] * rows[3][1] - rows[1][0]
                                                                                                                                      * rows[2][3] * rows[3][1] - rows[1][1] * rows[2][0] * rows[3][3] + rows[1][0] * rows[2][1] * rows[3][3])
                * inv_det;

    tmp[2][1] = (rows[0][3] * rows[2][1] * rows[3][0] - rows[0][1] * rows[2][3] * rows[3][0] - rows[0][3] * rows[2][0] * rows[3][1] + rows[0][0]
                                                                                                                                      * rows[2][3] * rows[3][1] + rows[0][1] * rows[2][0] * rows[3][3] - rows[0][0] * rows[2][1] * rows[3][3])
                * inv_det;

    tmp[2][2] = (rows[0][1] * rows[1][3] * rows[3][0] - rows[0][3] * rows[1][1] * rows[3][0] + rows[0][3] * rows[1][0] * rows[3][1] - rows[0][0]
                                                                                                                                      * rows[1][3] * rows[3][1] - rows[0][1] * rows[1][0] * rows[3][3] + rows[0][0] * rows[1][1] * rows[3][3])
                * inv_det;

    tmp[2][3] = (rows[0][3] * rows[1][1] * rows[2][0] - rows[0][1] * rows[1][3] * rows[2][0] - rows[0][3] * rows[1][0] * rows[2][1] + rows[0][0]
                                                                                                                                      * rows[1][3] * rows[2][1] + rows[0][1] * rows[1][0] * rows[2][3] - rows[0][0] * rows[1][1] * rows[2][3])
                * inv_det;

    tmp[3][0] = (rows[1][2] * rows[2][1] * rows[3][0] - rows[1][1] * rows[2][2] * rows[3][0] - rows[1][2] * rows[2][0] * rows[3][1] + rows[1][0]
                                                                                                                                      * rows[2][2] * rows[3][1] + rows[1][1] * rows[2][0] * rows[3][2] - rows[1][0] * rows[2][1] * rows[3][2])
                * inv_det;

    tmp[3][1] = (rows[0][1] * rows[2][2] * rows[3][0] - rows[0][2] * rows[2][1] * rows[3][0] + rows[0][2] * rows[2][0] * rows[3][1] - rows[0][0]
                                                                                                                                      * rows[2][2] * rows[3][1] - rows[0][1] * rows[2][0] * rows[3][2] + rows[0][0] * rows[2][1] * rows[3][2])
                * inv_det;

    tmp[3][2] = (rows[0][2] * rows[1][1] * rows[3][0] - rows[0][1] * rows[1][2] * rows[3][0] - rows[0][2] * rows[1][0] * rows[3][1] + rows[0][0]
                                                                                                                                      * rows[1][2] * rows[3][1] + rows[0][1] * rows[1][0] * rows[3][2] - rows[0][0] * rows[1][1] * rows[3][2])
                * inv_det;

    tmp[3][3] = (rows[0][1] * rows[1][2] * rows[2][0] - rows[0][2] * rows[1][1] * rows[2][0] + rows[0][2] * rows[1][0] * rows[2][1] - rows[0][0]
                                                                                                                                      * rows[1][2] * rows[2][1] - rows[0][1] * rows[1][0] * rows[2][2] + rows[0][0] * rows[1][1] * rows[2][2])
                * inv_det;

    return Matrix4(reinterpret_cast<const float *>(tmp));
}

float Matrix4::GetYaw() const
{
    return Quaternion(*this).Yaw();
}

float Matrix4::GetPitch() const
{
    return Quaternion(*this).Pitch();
}

float Matrix4::GetRoll() const
{
    return Quaternion(*this).Roll();
}

Matrix4 &Matrix4::operator=(const Matrix4 &other)
{
    hyperion::Memory::Copy(values, other.values, sizeof(values));

    return *this;
}

Matrix4 Matrix4::operator+(const Matrix4 &other) const
{
    Matrix4 result(*this);
    result += other;

    return result;
}

Matrix4 &Matrix4::operator+=(const Matrix4 &other)
{
    for (Int i = 0; i < std::size(values); i++) {
        values[i] += other.values[i];
    }

    return *this;
}

/* This code is based off of source found in DirectXMathOptimizations.
 * License: MIT
 * Link: https://github.com/nfrechette/DirectXMathOptimizations/blob/master/Inc/MatrixMultiply/MatrixMultiply_V2.h */

Matrix4 Matrix4::operator*(const Matrix4 &other) const
{
    Float128 vx, vy, vz, vw;
    // Load all 4 rows of our "other" matrix into registers
    const Float128 m2r0 = Float128Load(&other.values[0]);
    const Float128 m2r1 = Float128Load(&other.values[4]);
    const Float128 m2r2 = Float128Load(&other.values[8]);
    const Float128 m2r3 = Float128Load(&other.values[12]);

    Matrix4 result;

    for (Int i = 0; i < 4; i++) {
        // Load our first row
        vw = Float128Load(&values[i * 4]);
        // Splat each component
        vx = Float128Permute(vw, Float128ShuffleMask(0, 0, 0, 0));
        vy = Float128Permute(vw, Float128ShuffleMask(1, 1, 1, 1));
        vz = Float128Permute(vw, Float128ShuffleMask(2, 2, 2, 2));
        // Finally, isolate our W back
        vw = Float128Permute(vw, Float128ShuffleMask(3, 3, 3, 3));

        // Multiply components by rows
        vx = Float128Mul(vx, m2r0);
        vy = Float128Mul(vy, m2r1);
        vz = Float128Mul(vz, m2r2);
        vw = Float128Mul(vw, m2r3);

        // Add back to X
        vx = Float128Add(vx, vz);
        vy = Float128Add(vy, vw);
        vx = Float128Add(vx, vy);

        // Store X vector
        Float128Store(&(result.values[i * 4]), vx);
    }

    return result;
}

Matrix4 &Matrix4::operator*=(const Matrix4 &other)
{
    return (*this) = operator*(other);
}

Matrix4 Matrix4::operator*(Float scalar) const
{
    Matrix4 result(*this);
    result *= scalar;

    return result;
}

Matrix4 &Matrix4::operator*=(Float scalar)
{
    // Broadcast our scalar value to all components of a vector
    Float128 scalarv = Float128Set(scalar);
    Float128 original = Float128Zero();

    for (Int i = 0; i < 4; i++) {
        // Load a vec4 from our matrix
        original = Float128Load(&values[i * 4]);
        // Multiply by scalar value
        original = Float128Mul(original, scalarv);
        // Store back into memory
        Float128Store(&values[i * 4], original);
    }

    return *this;
}

Vector3 Matrix4::operator*(const Vector3 &vec) const
{
    Float vec_values[] = { vec.x, vec.y, vec.z, 1.0f };
    Float rvals[4];
    // Zero our SSE registers
    Float128 original_vec = Float128Zero();
    // Load a vec4 into our registers
    Float128 vec_sc = Float128Load(vec_values);

    for (Int i = 0; i < 4; i++) {
        // Load our matrix values into an SSE vec4
        original_vec = Float128Load(&values[i * 4]);
        // Multiply by constant vector
        original_vec = Float128Mul(vec_sc, original_vec);
        // Output the sum of all components as a value of our result
        rvals[i] = Float128Sum(original_vec);
    }

    Vector4 product(rvals[0], rvals[1], rvals[2], rvals[3]);

    return Vector3(product / product.w);
}

Vector4 Matrix4::operator*(const Vector4 &vec) const
{
    Float rvals[4];
    // Zero our SSE registers
    Float128 original_vec = Float128Zero();
    // Load a vec4 into our registers
    Float128 vec_sc = Float128Load(vec.values);

    for (int i = 0; i < 4; i++) {
        // Load our matrix values into an SSE vec4
        original_vec = Float128Load(&values[i * 4]);
        // Multiply by constant vector
        original_vec = Float128Mul(vec_sc, original_vec);
        // Output the sum of all components as a value of our result
        rvals[i] = Float128Sum(original_vec);
    }

    Vector4 product(rvals[0], rvals[1], rvals[2], rvals[3]);
    return product;
}

Vector4 Matrix4::GetColumn(UInt index) const
{
    return {
        rows[0][index],
        rows[1][index],
        rows[2][index],
        rows[3][index]
    };
}

Matrix4 Matrix4::Zeros()
{
    Float zero_array[sizeof(values) / sizeof(values[0])] = { 0.0f };

    return Matrix4(zero_array);
}

Matrix4 Matrix4::Ones()
{
    Float ones_array[sizeof(values) / sizeof(values[0])] = { 1.0f };

    return Matrix4(ones_array);
}

Matrix4 Matrix4::Identity()
{
    return Matrix4(); // constructor fills out identity matrix
}

std::ostream &operator<<(std::ostream &os, const Matrix4 &mat)
{
    os << "[";
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            os << mat.values[i * 4 + j];

            if (i != 3 && j == 3) {
                os << "\n";
            } else if (!(i == 3 && j == 3)) {
                os << ", ";
            }
        }
    }
    os << "]";
    return os;
}
} // namespace hyperion

#endif
