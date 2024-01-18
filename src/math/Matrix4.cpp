#include <math/Matrix4.hpp>
#include <math/Matrix3.hpp>
#include <math/Vector3.hpp>
#include <math/Quaternion.hpp>
#include <math/Rect.hpp>
#include <math/Halton.hpp>
#include <core/Core.hpp>

namespace hyperion {

const Matrix4 Matrix4::identity = Matrix4::Identity();
const Matrix4 Matrix4::zeros = Matrix4::Zeros();
const Matrix4 Matrix4::ones = Matrix4::Ones();

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

    const Float scale_x = 1.0f / Vector3(transform.GetColumn(0)).Length();
    const Float scale_y = 1.0f / Vector3(transform.GetColumn(1)).Length();
    const Float scale_z = 1.0f / Vector3(transform.GetColumn(2)).Length();

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

    const Float xx = rotation.x * rotation.x,
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

Matrix4 Matrix4::Rotation(const Vector3 &axis, Float radians)
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

Matrix4 Matrix4::Perspective(Float fov, Int w, Int h, Float n, Float f)
{
    Matrix4 mat = zeros;

    Float ar = (Float)w / (Float)h;
    Float tan_half_fov = MathUtil::Tan(MathUtil::DegToRad(fov / 2.0f));
    Float range = n - f;

    mat[0][0] = 1.0f / (tan_half_fov * ar);
    
    mat[1][1] = -(1.0f / (tan_half_fov));
    
    mat[2][2] = (-n - f) / range;
    mat[2][3] = (2.0f * f * n) / range;
    
    mat[3][2] = 1.0f;
    mat[3][3] = 0.0f;
    
    return mat;
}

Matrix4 Matrix4::Orthographic(Float l, Float r, Float b, Float t, Float n, Float f)
{
    Matrix4 mat = zeros;
    
    Float x_orth = 2.0f / (r - l);
    Float y_orth = 2.0f / (t - b);
    Float z_orth = 1.0f / (n - f);
    Float tx = -((r + l) / (r - l));
    Float ty = -((t + b) / (t - b));
    Float tz = ((n) / (n - f));
    
    mat[0] = { x_orth, 0.0f, 0.0f, tx };
    mat[1] = { 0.0f, y_orth, 0.0f, ty };
    mat[2] = { 0.0f, 0.0f, z_orth, tz };
    mat[3] = { 0.0f, 0.0f, 0.0f, 1.0f };

    return mat;
}

Matrix4 Matrix4::Jitter(UInt index, UInt width, UInt height, Vector4 &out_jitter)
{
    static const HaltonSequence halton;

    Matrix4 offset_matrix;

    const UInt frame_counter = index;
    const UInt halton_index = frame_counter % HaltonSequence::size;

    Vector2 jitter = halton.sequence[halton_index];
    Vector2 previous_jitter;

    if (frame_counter != 0) {
        previous_jitter = halton.sequence[(frame_counter - 1) % HaltonSequence::size];
    }

    const Vector2 pixel_size = Vector2::one / Vector2(Float(width), Float(height));

    jitter = (jitter * 2.0f - 1.0f) * pixel_size * 0.5f;
    previous_jitter = (previous_jitter * 2.0f - 1.0f) * pixel_size * 0.5f;

    offset_matrix[0][3] += jitter.x;
    offset_matrix[1][3] += jitter.y;

    out_jitter = Vector4(jitter, previous_jitter);

    return offset_matrix;
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

Matrix4::Matrix4(const Matrix3 &matrix3)
    : rows {
        { matrix3.rows[0][0], matrix3.rows[0][1], matrix3.rows[0][2], 0.0f },
        { matrix3.rows[1][0], matrix3.rows[1][1], matrix3.rows[1][2], 0.0f },
        { matrix3.rows[2][0], matrix3.rows[2][1], matrix3.rows[2][2], 0.0f },
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

Matrix4::Matrix4(const Float *v)
{
    rows[0] = { v[0],  v[1],  v[2],  v[3] };
    rows[1] = { v[4],  v[5],  v[6],  v[7] };
    rows[2] = { v[8],  v[9],  v[10], v[11] };
    rows[3] = { v[12], v[13], v[14], v[15] };
}

Matrix4::Matrix4(const Matrix4 &other)
{
    hyperion::Memory::MemCpy(values, other.values, sizeof(values));
}

Float Matrix4::Determinant() const
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
    const Float v[4][4] = {
        { rows[0][0], rows[1][0], rows[2][0], rows[3][0] },
        { rows[0][1], rows[1][1], rows[2][1], rows[3][1] },
        { rows[0][2], rows[1][2], rows[2][2], rows[3][2] },
        { rows[0][3], rows[1][3], rows[2][3], rows[3][3] }
    };

    return Matrix4(reinterpret_cast<const Float *>(v));
}

Matrix4 &Matrix4::Invert()
{
    return operator=(Inverted());
}

Matrix4 Matrix4::Inverted() const
{
    const Float det = Determinant();
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

    return Matrix4(reinterpret_cast<const Float *>(tmp));
}

Matrix4 &Matrix4::Orthonormalize()
{
    return operator=(Orthonormalized());
}

Matrix4 Matrix4::Orthonormalized() const
{
    Matrix4 mat = *this;
    
    Float length = MathUtil::Sqrt(mat[0][0]*mat[0][0] + mat[0][1]*mat[0][1] + mat[0][2]*mat[0][2]);
    mat[0][0] /= length;
    mat[0][1] /= length;
    mat[0][2] /= length;
    
    Float dot_product = mat[0][0] * mat[1][0] + mat[0][1] * mat[1][1] + mat[0][2] * mat[1][2];

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

Float Matrix4::GetYaw() const
{
    return Quaternion(*this).Yaw();
}

Float Matrix4::GetPitch() const
{
    return Quaternion(*this).Pitch();
}

Float Matrix4::GetRoll() const
{
    return Quaternion(*this).Roll();
}

Matrix4 &Matrix4::operator=(const Matrix4 &other)
{
    hyperion::Memory::MemCpy(values, other.values, sizeof(values));

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

Matrix4 Matrix4::operator*(const Matrix4 &other) const
{
    const Float fv[] = {
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
    for (Float &value : values) {
        value *= scalar;
    }

    return *this;
}

Vector3 Matrix4::operator*(const Vector3 &vec) const
{
    const Vector4 product {
        vec.x * values[0]  + vec.y * values[1]  + vec.z * values[2]  + values[3],
        vec.x * values[4]  + vec.y * values[5]  + vec.z * values[6]  + values[7],
        vec.x * values[8]  + vec.y * values[9]  + vec.z * values[10] + values[11],
        vec.x * values[12] + vec.y * values[13] + vec.z * values[14] + values[15]
    };

    return product.GetXYZ() / product.w;
}

Vector4 Matrix4::operator*(const Vector4 &vec) const
{
    return {
        vec.x * values[0]  + vec.y * values[1]  + vec.z * values[2]  + vec.w * values[3],
        vec.x * values[4]  + vec.y * values[5]  + vec.z * values[6]  + vec.w * values[7],
        vec.x * values[8]  + vec.y * values[9]  + vec.z * values[10] + vec.w * values[11],
        vec.x * values[12] + vec.y * values[13] + vec.z * values[14] + vec.w * values[15]
    };
}

Vector3 Matrix4::ExtractTransformScale() const
{
    Vector3 scale;

    scale.x = GetColumn(0).GetXYZ().Length();
    scale.y = GetColumn(1).GetXYZ().Length();
    scale.z = GetColumn(2).GetXYZ().Length();

    return scale;
}

Quaternion Matrix4::ExtractRotation() const
{
    return Quaternion(*this);
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
    static constexpr Float zero_array[sizeof(values) / sizeof(values[0])] = { 0.0f };

    return Matrix4(zero_array);
}

Matrix4 Matrix4::Ones()
{
    static constexpr Float ones_array[sizeof(values) / sizeof(values[0])] = { 1.0f };

    return Matrix4(ones_array);
}

Matrix4 Matrix4::Identity()
{
    return Matrix4(); // constructor fills out identity matrix
}

std::ostream &operator<<(std::ostream &os, const Matrix4 &mat)
{
    os << "[";
    for (Int i = 0; i < 4; i++) {
        for (Int j = 0; j < 4; j++) {
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
