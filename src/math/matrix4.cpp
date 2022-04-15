#include "matrix4.h"
#include "vector3.h"
#include "quaternion.h"

#include <string.h>
#include <cstring>

namespace hyperion {

Matrix4 Matrix4::Translation(const Vector3 &translation)
{
    Matrix4 mat;

    mat[0][3] = translation.x;
    mat[1][3] = translation.y;
    mat[2][3] = translation.z;

    return mat;
}

Matrix4 Matrix4::Rotation(const Quaternion &rotation)
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
    Matrix4 mat;

    float ar = (float)w / (float)h;
    float tan_half_fov = tan(MathUtil::DegToRad(fov / 2.0f));
    float range = n - f;

    mat[0][0] = 1.0f / (tan_half_fov * ar);
    
    mat[1][1] = 1.0f / (tan_half_fov);
    
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
    float z_orth = 2.0f / (f - n);
    float tx = -1.0f * ((r + l) / (r - l));
    float ty = -1.0f * ((t + b) / (t - b));
    float tz = -1.0f * ((f + n) / (f - n));

    mat[0][0] = x_orth;
    mat[0][3] = tx;
    
    mat[1][1] = y_orth;
    mat[1][3] = ty;
    
    mat[2][2] = z_orth;
    mat[2][3] = tz;

    return mat;
}

Matrix4 Matrix4::LookAt(const Vector3 &dir, const Vector3 &up)
{
    auto mat = Identity();

    Vector3 z(dir);
    z.Normalize();

    Vector3 x(dir);
    x.Normalize();
    x.Cross(up);
    x.Normalize();

    Vector3 y(x);
    y.Cross(z);
    y.Normalize();

    mat[0] = Vector4(x, 0.0f);
    mat[1] = Vector4(y, 0.0f);
    mat[2] = Vector4(z, 0.0f);

    return mat;
}

Matrix4 Matrix4::LookAt(const Vector3 &pos, const Vector3 &target, const Vector3 &up)
{
    return Translation(pos * -1) * LookAt(target - pos, up);
}

Matrix4::Matrix4()
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            values[i * 4 + j] = !(j - i);
        }
    }
}

Matrix4::Matrix4(float *v)
{
    std::memcpy(values, v, sizeof(values));
}

Matrix4::Matrix4(const Matrix4 &other)
{
    std::memcpy(values, other.values, sizeof(values));
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
    Matrix4 transposed;

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            transposed[j][i] = rows[i][j];
        }
    }

    return operator=(transposed);
}

Matrix4 &Matrix4::Invert()
{
    float det = Determinant();
    float inv_det = 1.0f / det;

    float tmp[4][4];

    tmp[0][0] = rows[1][2] * rows[2][3] * rows[3][1] - rows[1][3] * rows[2][2] * rows[3][1] + rows[1][3] * rows[2][1] * rows[3][2] - rows[1][1]
              * rows[2][3] * rows[3][2] - rows[1][2] * rows[2][1] * rows[3][3] + rows[1][1] * rows[2][2] * rows[3][3];

    tmp[0][1] = rows[0][3] * rows[2][2] * rows[3][1] - rows[0][2] * rows[2][3] * rows[3][1] - rows[0][3] * rows[2][1] * rows[3][2] + rows[0][1]
              * rows[2][3] * rows[3][2] + rows[0][2] * rows[2][1] * rows[3][3] - rows[0][1] * rows[2][2] * rows[3][3];

    tmp[0][2] = rows[0][2] * rows[1][3] * rows[3][1] - rows[0][3] * rows[1][2] * rows[3][1] + rows[0][3] * rows[1][1] * rows[3][2] - rows[0][1]
              * rows[1][3] * rows[3][2] - rows[0][2] * rows[1][1] * rows[3][3] + rows[0][1] * rows[1][2] * rows[3][3];

    tmp[0][3] = rows[0][3] * rows[1][2] * rows[2][1] - rows[0][2] * rows[1][3] * rows[2][1] - rows[0][3] * rows[1][1] * rows[2][2] + rows[0][1]
              * rows[1][3] * rows[2][2] + rows[0][2] * rows[1][1] * rows[2][3] - rows[0][1] * rows[1][2] * rows[2][3];

    tmp[1][0] = rows[1][3] * rows[2][2] * rows[3][0] - rows[1][2] * rows[2][3] * rows[3][0] - rows[1][3] * rows[2][0] * rows[3][2] + rows[1][0]
              * rows[2][3] * rows[3][2] + rows[1][2] * rows[2][0] * rows[3][3] - rows[1][0] * rows[2][2] * rows[3][3];

    tmp[1][1] = rows[0][2] * rows[2][3] * rows[3][0] - rows[0][3] * rows[2][2] * rows[3][0] + rows[0][3] * rows[2][0] * rows[3][2] - rows[0][0]
              * rows[2][3] * rows[3][2] - rows[0][2] * rows[2][0] * rows[3][3] + rows[0][0] * rows[2][2] * rows[3][3];

    tmp[1][2] = rows[0][3] * rows[1][2] * rows[3][0] - rows[0][2] * rows[1][3] * rows[3][0] - rows[0][3] * rows[1][0] * rows[3][2] + rows[0][0]
              * rows[1][3] * rows[3][2] + rows[0][2] * rows[1][0] * rows[3][3] - rows[0][0] * rows[1][2] * rows[3][3];

    tmp[1][3] = rows[0][2] * rows[1][3] * rows[2][0] - rows[0][3] * rows[1][2] * rows[2][0] + rows[0][3] * rows[1][0] * rows[2][2] - rows[0][0]
              * rows[1][3] * rows[2][2] - rows[0][2] * rows[1][0] * rows[2][3] + rows[0][0] * rows[1][2] * rows[2][3];

    tmp[2][0] = rows[1][1] * rows[2][3] * rows[3][0] - rows[1][3] * rows[2][1] * rows[3][0] + rows[1][3] * rows[2][0] * rows[3][1] - rows[1][0]
              * rows[2][3] * rows[3][1] - rows[1][1] * rows[2][0] * rows[3][3] + rows[1][0] * rows[2][1] * rows[3][3];

    tmp[2][1] = rows[0][3] * rows[2][1] * rows[3][0] - rows[0][1] * rows[2][3] * rows[3][0] - rows[0][3] * rows[2][0] * rows[3][1] + rows[0][0]
              * rows[2][3] * rows[3][1] + rows[0][1] * rows[2][0] * rows[3][3] - rows[0][0] * rows[2][1] * rows[3][3];

    tmp[2][2] = rows[0][1] * rows[1][3] * rows[3][0] - rows[0][3] * rows[1][1] * rows[3][0] + rows[0][3] * rows[1][0] * rows[3][1] - rows[0][0]
              * rows[1][3] * rows[3][1] - rows[0][1] * rows[1][0] * rows[3][3] + rows[0][0] * rows[1][1] * rows[3][3];

    tmp[2][3] = rows[0][3] * rows[1][1] * rows[2][0] - rows[0][1] * rows[1][3] * rows[2][0] - rows[0][3] * rows[1][0] * rows[2][1] + rows[0][0]
              * rows[1][3] * rows[2][1] + rows[0][1] * rows[1][0] * rows[2][3] - rows[0][0] * rows[1][1] * rows[2][3];

    tmp[3][0] = rows[1][2] * rows[2][1] * rows[3][0] - rows[1][1] * rows[2][2] * rows[3][0] - rows[1][2] * rows[2][0] * rows[3][1] + rows[1][0]
              * rows[2][2] * rows[3][1] + rows[1][1] * rows[2][0] * rows[3][2] - rows[1][0] * rows[2][1] * rows[3][2];

    tmp[3][1] = rows[0][1] * rows[2][2] * rows[3][0] - rows[0][2] * rows[2][1] * rows[3][0] + rows[0][2] * rows[2][0] * rows[3][1] - rows[0][0]
              * rows[2][2] * rows[3][1] - rows[0][1] * rows[2][0] * rows[3][2] + rows[0][0] * rows[2][1] * rows[3][2];

    tmp[3][2] = rows[0][2] * rows[1][1] * rows[3][0] - rows[0][1] * rows[1][2] * rows[3][0] - rows[0][2] * rows[1][0] * rows[3][1] + rows[0][0]
              * rows[1][2] * rows[3][1] + rows[0][1] * rows[1][0] * rows[3][2] - rows[0][0] * rows[1][1] * rows[3][2];

    tmp[3][3] = rows[0][1] * rows[1][2] * rows[2][0] - rows[0][2] * rows[1][1] * rows[2][0] + rows[0][2] * rows[1][0] * rows[2][1] - rows[0][0]
              * rows[1][2] * rows[2][1] - rows[0][1] * rows[1][0] * rows[2][2] + rows[0][0] * rows[1][1] * rows[2][2];

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            rows[i][j] = tmp[i][j] * inv_det;
        }
    }

    return *this;
}

Matrix4 &Matrix4::operator=(const Matrix4 &other)
{
    std::memcpy(values, other.values, sizeof(values));

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
    for (int i = 0; i < std::size(values); i++) {
        values[i] += other.values[i];
    }

    return *this;
}

Matrix4 Matrix4::operator*(const Matrix4 &other) const
{
    Matrix4 result(Zeroes());

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 4; k++) {
                result[i][j] += rows[k][j] * other.rows[i][k];
            }
        }
    }

    return result;
}

Matrix4 &Matrix4::operator*=(const Matrix4 &other)
{
    (*this) = operator*(other);

    return *this;
}

Matrix4 Matrix4::operator*(float scalar) const
{
    Matrix4 result(*this);
    result *= scalar;

    return result;
}

Matrix4 &Matrix4::operator*=(float scalar)
{
    for (float &value : values) {
        value *= scalar;
    }

    return *this;
}

bool Matrix4::operator==(const Matrix4 &other) const
{
    return values == other.values;
}

Matrix4 Matrix4::Zeroes()
{
    float zero_array[sizeof(values)] = {0.0f};

    return Matrix4(zero_array);
}

Matrix4 Matrix4::Ones()
{
    float ones_array[sizeof(values)] = {1.0f};

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
