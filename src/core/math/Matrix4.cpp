/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/math/Matrix4.hpp>
#include <core/math/Matrix3.hpp>
#include <core/math/Rect.hpp>
#include <core/math/Halton.hpp>

namespace hyperion {

const Matrix4 Matrix4::identity = Matrix4::Identity();
const Matrix4 Matrix4::zeros = Matrix4::Zeros();
const Matrix4 Matrix4::ones = Matrix4::Ones();

Matrix4 Matrix4::Translation(const Vec3f& translation)
{
    Matrix4 mat;

    mat[0][3] = translation.x;
    mat[1][3] = translation.y;
    mat[2][3] = translation.z;

    return mat;
}

Matrix4 Matrix4::Rotation(const Quaternion& rotation)
{
    Matrix4 mat;

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

Matrix4 Matrix4::Rotation(const Vec3f& axis, float radians)
{
    return Rotation(Quaternion(axis, radians));
}

Matrix4 Matrix4::Scaling(const Vec3f& scale)
{
    Matrix4 mat;

    mat[0][0] = scale.x;
    mat[1][1] = scale.y;
    mat[2][2] = scale.z;

    return mat;
}

Matrix4 Matrix4::Perspective(float fov, int w, int h, float n, float f)
{
    Matrix4 mat = zeros;

    float ar = (float)w / (float)h;
    float tan_half_fov = MathUtil::Tan(MathUtil::DegToRad(fov / 2.0f));
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
    Matrix4 mat = zeros;

    float x_orth = 2.0f / (r - l);
    float y_orth = 2.0f / (t - b);
    float z_orth = 1.0f / (n - f);
    float tx = -((r + l) / (r - l));
    float ty = -((t + b) / (t - b));
    float tz = ((n) / (n - f));

    mat[0] = { x_orth, 0.0f, 0.0f, tx };
    mat[1] = { 0.0f, y_orth, 0.0f, ty };
    mat[2] = { 0.0f, 0.0f, z_orth, tz };
    mat[3] = { 0.0f, 0.0f, 0.0f, 1.0f };

    return mat;
}

Matrix4 Matrix4::Jitter(uint32 index, uint32 width, uint32 height, Vec4f& out_jitter)
{
    static const HaltonSequence halton;

    Matrix4 offset_matrix;

    const uint32 frame_counter = index;
    const uint32 halton_index = frame_counter % HaltonSequence::size;

    Vec2f jitter = halton.sequence[halton_index];
    Vec2f previous_jitter;

    if (frame_counter != 0)
    {
        previous_jitter = halton.sequence[(frame_counter - 1) % HaltonSequence::size];
    }

    const Vec2f pixel_size = Vec2f::One() / Vec2f { float(width), float(height) };

    jitter = (jitter * 2.0f - 1.0f) * pixel_size * 0.5f;
    previous_jitter = (previous_jitter * 2.0f - 1.0f) * pixel_size * 0.5f;

    offset_matrix[0][3] += jitter.x;
    offset_matrix[1][3] += jitter.y;

    out_jitter = Vec4f(jitter, previous_jitter);

    return offset_matrix;
}

Matrix4 Matrix4::LookAt(const Vec3f& direction, const Vec3f& up)
{
    auto mat = Identity();

    const Vec3f z = direction.Normalized();
    const Vec3f x = direction.Cross(up).Normalize();
    const Vec3f y = x.Cross(z).Normalize();

    mat[0] = Vec4f(x, 0.0f);
    mat[1] = Vec4f(y, 0.0f);
    mat[2] = Vec4f(z, 0.0f);

    return mat;
}

Matrix4 Matrix4::LookAt(const Vec3f& pos, const Vec3f& target, const Vec3f& up)
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

Matrix4::Matrix4(const Matrix3& matrix3)
    : rows {
          { matrix3.rows[0][0], matrix3.rows[0][1], matrix3.rows[0][2], 0.0f },
          { matrix3.rows[1][0], matrix3.rows[1][1], matrix3.rows[1][2], 0.0f },
          { matrix3.rows[2][0], matrix3.rows[2][1], matrix3.rows[2][2], 0.0f },
          { 0.0f, 0.0f, 0.0f, 1.0f }
      }
{
}

Matrix4::Matrix4(const Vec4f* rows)
    : rows {
          rows[0],
          rows[1],
          rows[2],
          rows[3]
      }
{
}

Matrix4::Matrix4(const float* v)
{
    rows[0] = { v[0], v[1], v[2], v[3] };
    rows[1] = { v[4], v[5], v[6], v[7] };
    rows[2] = { v[8], v[9], v[10], v[11] };
    rows[3] = { v[12], v[13], v[14], v[15] };
}

float Matrix4::Determinant() const
{
    return rows[3][0] * rows[2][1] * rows[1][2] * rows[0][3] - rows[2][0] * rows[3][1] * rows[1][2] * rows[0][3] - rows[3][0] * rows[1][1] * rows[2][2] * rows[0][3] + rows[1][0] * rows[3][1] * rows[2][2] * rows[0][3] + rows[2][0] * rows[1][1] * rows[3][2] * rows[0][3] - rows[1][0] * rows[2][1] * rows[3][2] * rows[0][3] - rows[3][0] * rows[2][1] * rows[0][2] * rows[1][3] + rows[2][0] * rows[3][1] * rows[0][2] * rows[1][3]
        + rows[3][0] * rows[0][1] * rows[2][2] * rows[1][3] - rows[0][0] * rows[3][1] * rows[2][2] * rows[1][3] - rows[2][0] * rows[0][1] * rows[3][2] * rows[1][3] + rows[0][0] * rows[2][1] * rows[3][2] * rows[1][3] + rows[3][0] * rows[1][1] * rows[0][2] * rows[2][3] - rows[1][0] * rows[3][1] * rows[0][2] * rows[2][3] - rows[3][0] * rows[0][1] * rows[1][2] * rows[2][3] + rows[0][0] * rows[3][1] * rows[1][2] * rows[2][3] + rows[1][0] * rows[0][1] * rows[3][2] * rows[2][3] - rows[0][0] * rows[1][1] * rows[3][2] * rows[2][3] - rows[2][0] * rows[1][1] * rows[0][2] * rows[3][3]
        + rows[1][0] * rows[2][1] * rows[0][2] * rows[3][3] + rows[2][0] * rows[0][1] * rows[1][2] * rows[3][3] - rows[0][0] * rows[2][1] * rows[1][2] * rows[3][3] - rows[1][0] * rows[0][1] * rows[2][2] * rows[3][3] + rows[0][0] * rows[1][1] * rows[2][2] * rows[3][3];
}

Matrix4& Matrix4::Transpose()
{
    return operator=(Transposed());
}

Matrix4 Matrix4::Transposed() const
{
    Matrix4 transposed(*this);
    transposed.rows[0][0] = rows[0][0];
    transposed.rows[0][1] = rows[1][0];
    transposed.rows[0][2] = rows[2][0];
    transposed.rows[0][3] = rows[3][0];
    transposed.rows[1][0] = rows[0][1];
    transposed.rows[1][1] = rows[1][1];
    transposed.rows[1][2] = rows[2][1];
    transposed.rows[1][3] = rows[3][1];
    transposed.rows[2][0] = rows[0][2];
    transposed.rows[2][1] = rows[1][2];
    transposed.rows[2][2] = rows[2][2];
    transposed.rows[2][3] = rows[3][2];
    transposed.rows[3][0] = rows[0][3];
    transposed.rows[3][1] = rows[1][3];
    transposed.rows[3][2] = rows[2][3];
    transposed.rows[3][3] = rows[3][3];

    return transposed;
}

Matrix4& Matrix4::Invert()
{
    return operator=(Inverted());
}

Matrix4 Matrix4::Inverted() const
{
    const float det = Determinant();
    float inv_det = 1.0f / det;

    float tmp[4][4];

    tmp[0][0] = (rows[1][2] * rows[2][3] * rows[3][1] - rows[1][3] * rows[2][2] * rows[3][1] + rows[1][3] * rows[2][1] * rows[3][2] - rows[1][1] * rows[2][3] * rows[3][2] - rows[1][2] * rows[2][1] * rows[3][3] + rows[1][1] * rows[2][2] * rows[3][3])
        * inv_det;

    tmp[0][1] = (rows[0][3] * rows[2][2] * rows[3][1] - rows[0][2] * rows[2][3] * rows[3][1] - rows[0][3] * rows[2][1] * rows[3][2] + rows[0][1] * rows[2][3] * rows[3][2] + rows[0][2] * rows[2][1] * rows[3][3] - rows[0][1] * rows[2][2] * rows[3][3])
        * inv_det;

    tmp[0][2] = (rows[0][2] * rows[1][3] * rows[3][1] - rows[0][3] * rows[1][2] * rows[3][1] + rows[0][3] * rows[1][1] * rows[3][2] - rows[0][1] * rows[1][3] * rows[3][2] - rows[0][2] * rows[1][1] * rows[3][3] + rows[0][1] * rows[1][2] * rows[3][3])
        * inv_det;

    tmp[0][3] = (rows[0][3] * rows[1][2] * rows[2][1] - rows[0][2] * rows[1][3] * rows[2][1] - rows[0][3] * rows[1][1] * rows[2][2] + rows[0][1] * rows[1][3] * rows[2][2] + rows[0][2] * rows[1][1] * rows[2][3] - rows[0][1] * rows[1][2] * rows[2][3])
        * inv_det;

    tmp[1][0] = (rows[1][3] * rows[2][2] * rows[3][0] - rows[1][2] * rows[2][3] * rows[3][0] - rows[1][3] * rows[2][0] * rows[3][2] + rows[1][0] * rows[2][3] * rows[3][2] + rows[1][2] * rows[2][0] * rows[3][3] - rows[1][0] * rows[2][2] * rows[3][3])
        * inv_det;

    tmp[1][1] = (rows[0][2] * rows[2][3] * rows[3][0] - rows[0][3] * rows[2][2] * rows[3][0] + rows[0][3] * rows[2][0] * rows[3][2] - rows[0][0] * rows[2][3] * rows[3][2] - rows[0][2] * rows[2][0] * rows[3][3] + rows[0][0] * rows[2][2] * rows[3][3])
        * inv_det;

    tmp[1][2] = (rows[0][3] * rows[1][2] * rows[3][0] - rows[0][2] * rows[1][3] * rows[3][0] - rows[0][3] * rows[1][0] * rows[3][2] + rows[0][0] * rows[1][3] * rows[3][2] + rows[0][2] * rows[1][0] * rows[3][3] - rows[0][0] * rows[1][2] * rows[3][3])
        * inv_det;

    tmp[1][3] = (rows[0][2] * rows[1][3] * rows[2][0] - rows[0][3] * rows[1][2] * rows[2][0] + rows[0][3] * rows[1][0] * rows[2][2] - rows[0][0] * rows[1][3] * rows[2][2] - rows[0][2] * rows[1][0] * rows[2][3] + rows[0][0] * rows[1][2] * rows[2][3])
        * inv_det;

    tmp[2][0] = (rows[1][1] * rows[2][3] * rows[3][0] - rows[1][3] * rows[2][1] * rows[3][0] + rows[1][3] * rows[2][0] * rows[3][1] - rows[1][0] * rows[2][3] * rows[3][1] - rows[1][1] * rows[2][0] * rows[3][3] + rows[1][0] * rows[2][1] * rows[3][3])
        * inv_det;

    tmp[2][1] = (rows[0][3] * rows[2][1] * rows[3][0] - rows[0][1] * rows[2][3] * rows[3][0] - rows[0][3] * rows[2][0] * rows[3][1] + rows[0][0] * rows[2][3] * rows[3][1] + rows[0][1] * rows[2][0] * rows[3][3] - rows[0][0] * rows[2][1] * rows[3][3])
        * inv_det;

    tmp[2][2] = (rows[0][1] * rows[1][3] * rows[3][0] - rows[0][3] * rows[1][1] * rows[3][0] + rows[0][3] * rows[1][0] * rows[3][1] - rows[0][0] * rows[1][3] * rows[3][1] - rows[0][1] * rows[1][0] * rows[3][3] + rows[0][0] * rows[1][1] * rows[3][3])
        * inv_det;

    tmp[2][3] = (rows[0][3] * rows[1][1] * rows[2][0] - rows[0][1] * rows[1][3] * rows[2][0] - rows[0][3] * rows[1][0] * rows[2][1] + rows[0][0] * rows[1][3] * rows[2][1] + rows[0][1] * rows[1][0] * rows[2][3] - rows[0][0] * rows[1][1] * rows[2][3])
        * inv_det;

    tmp[3][0] = (rows[1][2] * rows[2][1] * rows[3][0] - rows[1][1] * rows[2][2] * rows[3][0] - rows[1][2] * rows[2][0] * rows[3][1] + rows[1][0] * rows[2][2] * rows[3][1] + rows[1][1] * rows[2][0] * rows[3][2] - rows[1][0] * rows[2][1] * rows[3][2])
        * inv_det;

    tmp[3][1] = (rows[0][1] * rows[2][2] * rows[3][0] - rows[0][2] * rows[2][1] * rows[3][0] + rows[0][2] * rows[2][0] * rows[3][1] - rows[0][0] * rows[2][2] * rows[3][1] - rows[0][1] * rows[2][0] * rows[3][2] + rows[0][0] * rows[2][1] * rows[3][2])
        * inv_det;

    tmp[3][2] = (rows[0][2] * rows[1][1] * rows[3][0] - rows[0][1] * rows[1][2] * rows[3][0] - rows[0][2] * rows[1][0] * rows[3][1] + rows[0][0] * rows[1][2] * rows[3][1] + rows[0][1] * rows[1][0] * rows[3][2] - rows[0][0] * rows[1][1] * rows[3][2])
        * inv_det;

    tmp[3][3] = (rows[0][1] * rows[1][2] * rows[2][0] - rows[0][2] * rows[1][1] * rows[2][0] + rows[0][2] * rows[1][0] * rows[2][1] - rows[0][0] * rows[1][2] * rows[2][1] - rows[0][1] * rows[1][0] * rows[2][2] + rows[0][0] * rows[1][1] * rows[2][2])
        * inv_det;

    return Matrix4(reinterpret_cast<const float*>(tmp));
}

Matrix4& Matrix4::Orthonormalize()
{
    return operator=(Orthonormalized());
}

Matrix4 Matrix4::Orthonormalized() const
{
    Matrix4 mat = *this;

    float length = MathUtil::Sqrt(mat[0][0] * mat[0][0] + mat[0][1] * mat[0][1] + mat[0][2] * mat[0][2]);
    mat[0][0] /= length;
    mat[0][1] /= length;
    mat[0][2] /= length;

    float dot_product = mat[0][0] * mat[1][0] + mat[0][1] * mat[1][1] + mat[0][2] * mat[1][2];

    mat[1][0] -= dot_product * mat[0][0];
    mat[1][1] -= dot_product * mat[0][1];
    mat[1][2] -= dot_product * mat[0][2];

    length = MathUtil::Sqrt((mat[1][0] * mat[1][0] + mat[1][1] * mat[1][1] + mat[1][2] * mat[1][2]));
    mat[1][0] /= length;
    mat[1][1] /= length;
    mat[1][2] /= length;

    dot_product = mat[0][0] * mat[2][0] + mat[0][1] * mat[2][1] + mat[0][2] * mat[2][2];
    mat[2][0] -= dot_product * mat[0][0];
    mat[2][1] -= dot_product * mat[0][1];
    mat[2][2] -= dot_product * mat[0][2];

    dot_product = mat[1][0] * mat[2][0] + mat[1][1] * mat[2][1] + mat[1][2] * mat[2][2];
    mat[2][0] -= dot_product * mat[1][0];
    mat[2][1] -= dot_product * mat[1][1];
    mat[2][2] -= dot_product * mat[1][2];

    length = MathUtil::Sqrt((mat[2][0] * mat[2][0] + mat[2][1] * mat[2][1] + mat[2][2] * mat[2][2]));
    mat[2][0] /= length;
    mat[2][1] /= length;
    mat[2][2] /= length;

    return mat;
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

Matrix4 Matrix4::operator+(const Matrix4& other) const
{
    Matrix4 result(*this);
    result += other;

    return result;
}

Matrix4& Matrix4::operator+=(const Matrix4& other)
{
    for (int i = 0; i < std::size(values); i++)
    {
        values[i] += other.values[i];
    }

    return *this;
}

Matrix4 Matrix4::operator*(const Matrix4& other) const
{
    const float fv[] = {
        values[0] * other.values[0] + values[1] * other.values[4] + values[2] * other.values[8] + values[3] * other.values[12],
        values[0] * other.values[1] + values[1] * other.values[5] + values[2] * other.values[9] + values[3] * other.values[13],
        values[0] * other.values[2] + values[1] * other.values[6] + values[2] * other.values[10] + values[3] * other.values[14],
        values[0] * other.values[3] + values[1] * other.values[7] + values[2] * other.values[11] + values[3] * other.values[15],

        values[4] * other.values[0] + values[5] * other.values[4] + values[6] * other.values[8] + values[7] * other.values[12],
        values[4] * other.values[1] + values[5] * other.values[5] + values[6] * other.values[9] + values[7] * other.values[13],
        values[4] * other.values[2] + values[5] * other.values[6] + values[6] * other.values[10] + values[7] * other.values[14],
        values[4] * other.values[3] + values[5] * other.values[7] + values[6] * other.values[11] + values[7] * other.values[15],

        values[8] * other.values[0] + values[9] * other.values[4] + values[10] * other.values[8] + values[11] * other.values[12],
        values[8] * other.values[1] + values[9] * other.values[5] + values[10] * other.values[9] + values[11] * other.values[13],
        values[8] * other.values[2] + values[9] * other.values[6] + values[10] * other.values[10] + values[11] * other.values[14],
        values[8] * other.values[3] + values[9] * other.values[7] + values[10] * other.values[11] + values[11] * other.values[15],

        values[12] * other.values[0] + values[13] * other.values[4] + values[14] * other.values[8] + values[15] * other.values[12],
        values[12] * other.values[1] + values[13] * other.values[5] + values[14] * other.values[9] + values[15] * other.values[13],
        values[12] * other.values[2] + values[13] * other.values[6] + values[14] * other.values[10] + values[15] * other.values[14],
        values[12] * other.values[3] + values[13] * other.values[7] + values[14] * other.values[11] + values[15] * other.values[15]
    };

    return Matrix4(fv);
}

Matrix4& Matrix4::operator*=(const Matrix4& other)
{
    return (*this) = operator*(other);
}

Matrix4 Matrix4::operator*(float scalar) const
{
    Matrix4 result(*this);
    result *= scalar;

    return result;
}

Matrix4& Matrix4::operator*=(float scalar)
{
    for (float& value : values)
    {
        value *= scalar;
    }

    return *this;
}

Vec3f Matrix4::operator*(const Vec3f& vec) const
{
    const Vec4f product {
        vec.x * values[0] + vec.y * values[1] + vec.z * values[2] + values[3],
        vec.x * values[4] + vec.y * values[5] + vec.z * values[6] + values[7],
        vec.x * values[8] + vec.y * values[9] + vec.z * values[10] + values[11],
        vec.x * values[12] + vec.y * values[13] + vec.z * values[14] + values[15]
    };

    return product.GetXYZ() / product.w;
}

Vec4f Matrix4::operator*(const Vec4f& vec) const
{
    return {
        vec.x * values[0] + vec.y * values[1] + vec.z * values[2] + vec.w * values[3],
        vec.x * values[4] + vec.y * values[5] + vec.z * values[6] + vec.w * values[7],
        vec.x * values[8] + vec.y * values[9] + vec.z * values[10] + vec.w * values[11],
        vec.x * values[12] + vec.y * values[13] + vec.z * values[14] + vec.w * values[15]
    };
}

Vec3f Matrix4::ExtractTranslation() const
{
    return {
        rows[0][3],
        rows[1][3],
        rows[2][3]
    };
}

Vec3f Matrix4::ExtractScale() const
{
    return {
        rows[0][0],
        rows[1][1],
        rows[2][2]
    };
}

Quaternion Matrix4::ExtractRotation() const
{
    return Quaternion(*this);
}

Vec4f Matrix4::GetColumn(uint32 index) const
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
    static constexpr float zero_array[sizeof(values) / sizeof(values[0])] = { 0.0f };

    return Matrix4(zero_array);
}

Matrix4 Matrix4::Ones()
{
    static constexpr float ones_array[sizeof(values) / sizeof(values[0])] = { 1.0f };

    return Matrix4(ones_array);
}

Matrix4 Matrix4::Identity()
{
    return Matrix4(); // constructor fills out identity matrix
}
} // namespace hyperion
