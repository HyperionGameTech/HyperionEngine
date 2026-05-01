/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/math/Mat4f.hpp>
#include <Core/math/Mat3f.hpp>
#include <Core/math/Rect.hpp>
#include <Core/math/Halton.hpp>

#include <Core/memory/Memory.hpp>

#ifndef HYP_TOOL
#include <Mat4f.generated.inl>
#endif

#if !HYP_ARM && (defined(__SSE4_1__) || (HYP_MSVC && defined(_M_X64)))
#include <immintrin.h>
#define HYP_MAT4F_USE_SSE 1
#else
#define HYP_MAT4F_USE_SSE 0
#endif

#if !HYP_ARM && defined(__AVX__)
#define HYP_MAT4F_USE_AVX 1
#else
#define HYP_MAT4F_USE_AVX 0
#endif

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE && defined(HYP_VULKAN)
static constexpr float PerspectiveMat11Div = -1.0f;
#else
static constexpr float PerspectiveMat11Div = 1.0f;
#endif

namespace {

#if HYP_MAT4F_USE_SSE
static HYP_FORCE_INLINE Hyperion::math::Vec4<float> StoreVec4f(__m128 value)
{
    Hyperion::math::Vec4<float> result;
    result._value = value;
    return result;
}
#endif

} // namespace

namespace Hyperion {

const Mat4f Mat4f::identity = Mat4f::Identity();
const Mat4f Mat4f::zeros = Mat4f::Zeros();
const Mat4f Mat4f::ones = Mat4f::Ones();

Mat4f Mat4f::Translation(const Vec3f& translation)
{
    Mat4f mat;

    mat[0][3] = translation.x;
    mat[1][3] = translation.y;
    mat[2][3] = translation.z;

    return mat;
}

Mat4f Mat4f::Rotation(const Quat4f& rotation)
{
    Mat4f mat;

    const float xx = rotation.x * rotation.x,
                xy = rotation.x * rotation.y,
                xz = rotation.x * rotation.z,
                xw = rotation.x * rotation.w,
                yy = rotation.y * rotation.y,
                yz = rotation.y * rotation.z,
                yw = rotation.y * rotation.w,
                zz = rotation.z * rotation.z,
                zw = rotation.z * rotation.w;

    mat[0][0] = 1.0f - 2.0f * (yy + zz);
    mat[0][1] = 2.0f * (xy + zw);
    mat[0][2] = 2.0f * (xz - yw);
    mat[0][3] = 0.0f;

    mat[1][0] = 2.0f * (xy - zw);
    mat[1][1] = 1.0f - 2.0f * (xx + zz);
    mat[1][2] = 2.0f * (yz + xw);
    mat[1][3] = 0.0f;

    mat[2][0] = 2.0f * (xz + yw);
    mat[2][1] = 2.0f * (yz - xw);
    mat[2][2] = 1.0f - 2.0f * (xx + yy);
    mat[2][3] = 0.0f;

    return mat;
}

Mat4f Mat4f::Rotation(const Vec3f& axis, float radians)
{
    return Rotation(Quat4f(axis, radians));
}

Mat4f Mat4f::Scaling(const Vec3f& scale)
{
    Mat4f mat;

    mat[0][0] = scale.x;
    mat[1][1] = scale.y;
    mat[2][2] = scale.z;

    return mat;
}

Mat4f Mat4f::Perspective(float fov, int w, int h, float n, float f)
{
    // Mat4f mat = zeros;

    // float ar = (float)w / (float)h;
    // float tanHalfFov = MathUtil::Tan(MathUtil::DegToRad(fov / 2.0f));

    // mat[0][0] = 1.0f / (tanHalfFov * ar);

    // mat[1][1] = (PerspectiveMat11Div / tanHalfFov);

    // mat[2][2] = f / (f - n);
    // mat[2][3] = -(f * n) / (f - n);

    // mat[3][2] = 1.0f;
    // mat[3][3] = 0.0f;

    Mat4f mat = zeros;

    float ar = (float)w / (float)h;
    float tanHalfFov = MathUtil::Tan(MathUtil::DegToRad(fov / 2.0f));
    float range = n - f;

    mat[0][0] = 1.0f / (tanHalfFov * ar);

    mat[1][1] = -(1.0f / (tanHalfFov));

    mat[2][2] = (-n - f) / range;
    mat[2][3] = (2.0f * f * n) / range;

    mat[3][2] = 1.0f;
    mat[3][3] = 0.0f;

    return mat;
}

Mat4f Mat4f::Orthographic(float l, float r, float b, float t, float n, float f)
{
    Mat4f mat = zeros;

    float xOrth = 2.0f / (r - l);
    float yOrth = 2.0f / (t - b);
    float zOrth = 1.0f / (n - f);
    float tx = -((r + l) / (r - l));
    float ty = -((t + b) / (t - b));
    float tz = ((n) / (n - f));

    mat[0][0] = xOrth;
    mat[0][1] = 0.0f;
    mat[0][2] = 0.0f;
    mat[0][3] = tx;

    mat[1][0] = 0.0f;
    mat[1][1] = yOrth;
    mat[1][2] = 0.0f;
    mat[1][3] = ty;

    mat[2][0] = 0.0f;
    mat[2][1] = 0.0f;
    mat[2][2] = zOrth;
    mat[2][3] = tz;

    mat[3][0] = 0.0f;
    mat[3][1] = 0.0f;
    mat[3][2] = 0.0f;
    mat[3][3] = 1.0f;

    return mat;
}

Mat4f Mat4f::Jitter(uint32 index, uint32 width, uint32 height, Vec4f& outJitter)
{
    static const HaltonSequence halton;

    Mat4f offsetMatrix;

    const uint32 frameCounter = index;
    const uint32 haltonIndex = frameCounter % HaltonSequence::size;

    Vec2f jitter = halton.sequence[haltonIndex];
    Vec2f previousJitter;

    if (frameCounter != 0)
    {
        previousJitter = halton.sequence[(frameCounter - 1) % HaltonSequence::size];
    }

    const Vec2f pixelSize = Vec2f::One() / Vec2f { float(width), float(height) };

    jitter = (jitter * 2.0f - 1.0f) * pixelSize * 0.5f;
    previousJitter = (previousJitter * 2.0f - 1.0f) * pixelSize * 0.5f;

    offsetMatrix[0][3] += jitter.x;
    offsetMatrix[1][3] += jitter.y;

    outJitter = Vec4f(jitter, previousJitter);

    return offsetMatrix;
}

Mat4f Mat4f::LookAt(const Vec3f& direction, const Vec3f& up)
{
    auto mat = Identity();

    const Vec3f z = direction.Normalized();
    const Vec3f x = direction.Cross(up).Normalize();
    const Vec3f y = x.Cross(z).Normalize();

    mat[0][0] = x.x;
    mat[0][1] = x.y;
    mat[0][2] = x.z;
    mat[0][3] = 0.0f;

    mat[1][0] = y.x;
    mat[1][1] = y.y;
    mat[1][2] = y.z;
    mat[1][3] = 0.0f;

    mat[2][0] = z.x;
    mat[2][1] = z.y;
    mat[2][2] = z.z;
    mat[2][3] = 0.0f;

    return mat;
}

Mat4f Mat4f::LookAt(const Vec3f& pos, const Vec3f& target, const Vec3f& up)
{
    return LookAt(target - pos, up) * Translation(pos * -1);
}

Mat4f::Mat4f()
    : rows {
          { 1.0f, 0.0f, 0.0f, 0.0f },
          { 0.0f, 1.0f, 0.0f, 0.0f },
          { 0.0f, 0.0f, 1.0f, 0.0f },
          { 0.0f, 0.0f, 0.0f, 1.0f }
      }
{
}

Mat4f::Mat4f(const Mat3f& other)
    : rows {
          { other.rows[0][0], other.rows[0][1], other.rows[0][2], 0.0f },
          { other.rows[1][0], other.rows[1][1], other.rows[1][2], 0.0f },
          { other.rows[2][0], other.rows[2][1], other.rows[2][2], 0.0f },
          { 0.0f, 0.0f, 0.0f, 1.0f }
      }
{
}

Mat4f::Mat4f(const Vec4f* rows)
    : Mat4f(reinterpret_cast<const float*>(rows))
{
}

Mat4f::Mat4f(const float* v)
{
    Memory::Copy(&rows[0][0], v + 0, sizeof(float) * 4);
    Memory::Copy(&rows[1][0], v + 4, sizeof(float) * 4);
    Memory::Copy(&rows[2][0], v + 8, sizeof(float) * 4);
    Memory::Copy(&rows[3][0], v + 12, sizeof(float) * 4);
}

float Mat4f::Determinant() const
{
    return rows[3][0] * rows[2][1] * rows[1][2] * rows[0][3] - rows[2][0] * rows[3][1] * rows[1][2] * rows[0][3] - rows[3][0] * rows[1][1] * rows[2][2] * rows[0][3] + rows[1][0] * rows[3][1] * rows[2][2] * rows[0][3] + rows[2][0] * rows[1][1] * rows[3][2] * rows[0][3] - rows[1][0] * rows[2][1] * rows[3][2] * rows[0][3] - rows[3][0] * rows[2][1] * rows[0][2] * rows[1][3] + rows[2][0] * rows[3][1] * rows[0][2] * rows[1][3]
        + rows[3][0] * rows[0][1] * rows[2][2] * rows[1][3] - rows[0][0] * rows[3][1] * rows[2][2] * rows[1][3] - rows[2][0] * rows[0][1] * rows[3][2] * rows[1][3] + rows[0][0] * rows[2][1] * rows[3][2] * rows[1][3] + rows[3][0] * rows[1][1] * rows[0][2] * rows[2][3] - rows[1][0] * rows[3][1] * rows[0][2] * rows[2][3] - rows[3][0] * rows[0][1] * rows[1][2] * rows[2][3] + rows[0][0] * rows[3][1] * rows[1][2] * rows[2][3] + rows[1][0] * rows[0][1] * rows[3][2] * rows[2][3] - rows[0][0] * rows[1][1] * rows[3][2] * rows[2][3] - rows[2][0] * rows[1][1] * rows[0][2] * rows[3][3]
        + rows[1][0] * rows[2][1] * rows[0][2] * rows[3][3] + rows[2][0] * rows[0][1] * rows[1][2] * rows[3][3] - rows[0][0] * rows[2][1] * rows[1][2] * rows[3][3] - rows[1][0] * rows[0][1] * rows[2][2] * rows[3][3] + rows[0][0] * rows[1][1] * rows[2][2] * rows[3][3];
}

Mat4f Mat4f::Transpose() const
{
    Mat4f transposed(*this);

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

Mat4f Mat4f::Inverse() const
{
    const float det = Determinant();
    float invDet = 1.0f / det;

    float tmp[4][4];

    tmp[0][0] = (rows[1][2] * rows[2][3] * rows[3][1] - rows[1][3] * rows[2][2] * rows[3][1] + rows[1][3] * rows[2][1] * rows[3][2] - rows[1][1] * rows[2][3] * rows[3][2] - rows[1][2] * rows[2][1] * rows[3][3] + rows[1][1] * rows[2][2] * rows[3][3])
        * invDet;

    tmp[0][1] = (rows[0][3] * rows[2][2] * rows[3][1] - rows[0][2] * rows[2][3] * rows[3][1] - rows[0][3] * rows[2][1] * rows[3][2] + rows[0][1] * rows[2][3] * rows[3][2] + rows[0][2] * rows[2][1] * rows[3][3] - rows[0][1] * rows[2][2] * rows[3][3])
        * invDet;

    tmp[0][2] = (rows[0][2] * rows[1][3] * rows[3][1] - rows[0][3] * rows[1][2] * rows[3][1] + rows[0][3] * rows[1][1] * rows[3][2] - rows[0][1] * rows[1][3] * rows[3][2] - rows[0][2] * rows[1][1] * rows[3][3] + rows[0][1] * rows[1][2] * rows[3][3])
        * invDet;

    tmp[0][3] = (rows[0][3] * rows[1][2] * rows[2][1] - rows[0][2] * rows[1][3] * rows[2][1] - rows[0][3] * rows[1][1] * rows[2][2] + rows[0][1] * rows[1][3] * rows[2][2] + rows[0][2] * rows[1][1] * rows[2][3] - rows[0][1] * rows[1][2] * rows[2][3])
        * invDet;

    tmp[1][0] = (rows[1][3] * rows[2][2] * rows[3][0] - rows[1][2] * rows[2][3] * rows[3][0] - rows[1][3] * rows[2][0] * rows[3][2] + rows[1][0] * rows[2][3] * rows[3][2] + rows[1][2] * rows[2][0] * rows[3][3] - rows[1][0] * rows[2][2] * rows[3][3])
        * invDet;

    tmp[1][1] = (rows[0][2] * rows[2][3] * rows[3][0] - rows[0][3] * rows[2][2] * rows[3][0] + rows[0][3] * rows[2][0] * rows[3][2] - rows[0][0] * rows[2][3] * rows[3][2] - rows[0][2] * rows[2][0] * rows[3][3] + rows[0][0] * rows[2][2] * rows[3][3])
        * invDet;

    tmp[1][2] = (rows[0][3] * rows[1][2] * rows[3][0] - rows[0][2] * rows[1][3] * rows[3][0] - rows[0][3] * rows[1][0] * rows[3][2] + rows[0][0] * rows[1][3] * rows[3][2] + rows[0][2] * rows[1][0] * rows[3][3] - rows[0][0] * rows[1][2] * rows[3][3])
        * invDet;

    tmp[1][3] = (rows[0][2] * rows[1][3] * rows[2][0] - rows[0][3] * rows[1][2] * rows[2][0] + rows[0][3] * rows[1][0] * rows[2][2] - rows[0][0] * rows[1][3] * rows[2][2] - rows[0][2] * rows[1][0] * rows[2][3] + rows[0][0] * rows[1][2] * rows[2][3])
        * invDet;

    tmp[2][0] = (rows[1][1] * rows[2][3] * rows[3][0] - rows[1][3] * rows[2][1] * rows[3][0] + rows[1][3] * rows[2][0] * rows[3][1] - rows[1][0] * rows[2][3] * rows[3][1] - rows[1][1] * rows[2][0] * rows[3][3] + rows[1][0] * rows[2][1] * rows[3][3])
        * invDet;

    tmp[2][1] = (rows[0][3] * rows[2][1] * rows[3][0] - rows[0][1] * rows[2][3] * rows[3][0] - rows[0][3] * rows[2][0] * rows[3][1] + rows[0][0] * rows[2][3] * rows[3][1] + rows[0][1] * rows[2][0] * rows[3][3] - rows[0][0] * rows[2][1] * rows[3][3])
        * invDet;

    tmp[2][2] = (rows[0][1] * rows[1][3] * rows[3][0] - rows[0][3] * rows[1][1] * rows[3][0] + rows[0][3] * rows[1][0] * rows[3][1] - rows[0][0] * rows[1][3] * rows[3][1] - rows[0][1] * rows[1][0] * rows[3][3] + rows[0][0] * rows[1][1] * rows[3][3])
        * invDet;

    tmp[2][3] = (rows[0][3] * rows[1][1] * rows[2][0] - rows[0][1] * rows[1][3] * rows[2][0] - rows[0][3] * rows[1][0] * rows[2][1] + rows[0][0] * rows[1][3] * rows[2][1] + rows[0][1] * rows[1][0] * rows[2][3] - rows[0][0] * rows[1][1] * rows[2][3])
        * invDet;

    tmp[3][0] = (rows[1][2] * rows[2][1] * rows[3][0] - rows[1][1] * rows[2][2] * rows[3][0] - rows[1][2] * rows[2][0] * rows[3][1] + rows[1][0] * rows[2][2] * rows[3][1] + rows[1][1] * rows[2][0] * rows[3][2] - rows[1][0] * rows[2][1] * rows[3][2])
        * invDet;

    tmp[3][1] = (rows[0][1] * rows[2][2] * rows[3][0] - rows[0][2] * rows[2][1] * rows[3][0] + rows[0][2] * rows[2][0] * rows[3][1] - rows[0][0] * rows[2][2] * rows[3][1] - rows[0][1] * rows[2][0] * rows[3][2] + rows[0][0] * rows[2][1] * rows[3][2])
        * invDet;

    tmp[3][2] = (rows[0][2] * rows[1][1] * rows[3][0] - rows[0][1] * rows[1][2] * rows[3][0] - rows[0][2] * rows[1][0] * rows[3][1] + rows[0][0] * rows[1][2] * rows[3][1] + rows[0][1] * rows[1][0] * rows[3][2] - rows[0][0] * rows[1][1] * rows[3][2])
        * invDet;

    tmp[3][3] = (rows[0][1] * rows[1][2] * rows[2][0] - rows[0][2] * rows[1][1] * rows[2][0] + rows[0][2] * rows[1][0] * rows[2][1] - rows[0][0] * rows[1][2] * rows[2][1] - rows[0][1] * rows[1][0] * rows[2][2] + rows[0][0] * rows[1][1] * rows[2][2])
        * invDet;

    return Mat4f(reinterpret_cast<const float*>(tmp));
}

Mat4f& Mat4f::Orthonormalize()
{
    return operator=(Orthonormalized());
}

Mat4f Mat4f::Orthonormalized() const
{
    Mat4f mat = *this;

    float length = MathUtil::Sqrt(mat[0][0] * mat[0][0] + mat[0][1] * mat[0][1] + mat[0][2] * mat[0][2]);
    mat[0][0] /= length;
    mat[0][1] /= length;
    mat[0][2] /= length;

    float dotProduct = mat[0][0] * mat[1][0] + mat[0][1] * mat[1][1] + mat[0][2] * mat[1][2];

    mat[1][0] -= dotProduct * mat[0][0];
    mat[1][1] -= dotProduct * mat[0][1];
    mat[1][2] -= dotProduct * mat[0][2];

    length = MathUtil::Sqrt((mat[1][0] * mat[1][0] + mat[1][1] * mat[1][1] + mat[1][2] * mat[1][2]));
    mat[1][0] /= length;
    mat[1][1] /= length;
    mat[1][2] /= length;

    dotProduct = mat[0][0] * mat[2][0] + mat[0][1] * mat[2][1] + mat[0][2] * mat[2][2];
    mat[2][0] -= dotProduct * mat[0][0];
    mat[2][1] -= dotProduct * mat[0][1];
    mat[2][2] -= dotProduct * mat[0][2];

    dotProduct = mat[1][0] * mat[2][0] + mat[1][1] * mat[2][1] + mat[1][2] * mat[2][2];
    mat[2][0] -= dotProduct * mat[1][0];
    mat[2][1] -= dotProduct * mat[1][1];
    mat[2][2] -= dotProduct * mat[1][2];

    length = MathUtil::Sqrt((mat[2][0] * mat[2][0] + mat[2][1] * mat[2][1] + mat[2][2] * mat[2][2]));
    mat[2][0] /= length;
    mat[2][1] /= length;
    mat[2][2] /= length;

    return mat;
}

float Mat4f::GetYaw() const
{
    return Quat4f(*this).Yaw();
}

float Mat4f::GetPitch() const
{
    return Quat4f(*this).Pitch();
}

float Mat4f::GetRoll() const
{
    return Quat4f(*this).Roll();
}

Mat4f Mat4f::operator+(const Mat4f& other) const
{
    Mat4f result(*this);
    result += other;

    return result;
}

Mat4f& Mat4f::operator+=(const Mat4f& other)
{
#if HYP_MAT4F_USE_SSE
    _mm_store_ps(values + 0, _mm_add_ps(_mm_load_ps(values + 0), _mm_load_ps(other.values + 0)));
    _mm_store_ps(values + 4, _mm_add_ps(_mm_load_ps(values + 4), _mm_load_ps(other.values + 4)));
    _mm_store_ps(values + 8, _mm_add_ps(_mm_load_ps(values + 8), _mm_load_ps(other.values + 8)));
    _mm_store_ps(values + 12, _mm_add_ps(_mm_load_ps(values + 12), _mm_load_ps(other.values + 12)));

    return *this;
#else
    for (int i = 0; i < HYP_ARRAY_SIZE(values); i++)
    {
        values[i] += other.values[i];
    }

    return *this;
#endif
}

Mat4f Mat4f::operator*(const Mat4f& other) const
{
#if HYP_MAT4F_USE_AVX
    // Adapted from Foxtrot SIMD matrix paths:
    // Math/Impl/Matrix/FxMat4_AVX.cpp
    //
    // Foxtrot is column-major; Hyperion is row-major. The algorithm structure is
    // identical - pack two rows of A into a 256-bit register, splat each scalar
    // element of those rows, and multiply against the matching row of B.
    // result.Row_i = A.Row_i.x*B.Row_0 + A.Row_i.y*B.Row_1 + A.Row_i.z*B.Row_2 + A.Row_i.w*B.Row_3
    //
    // Note: Foxtrot uses _mm256_fmadd_ps (FMA); replaced here with
    // _mm256_add_ps + _mm256_mul_ps to require only AVX, not FMA.

    // Pack rows 0,1 of A (lower=Row0, upper=Row1)
    __m256 ahalf0 = _mm256_castps128_ps256(_mm_load_ps(values + 0));
    ahalf0 = _mm256_insertf128_ps(ahalf0, _mm_load_ps(values + 4), 1);
    // Pack rows 2,3 of A
    __m256 ahalf1 = _mm256_castps128_ps256(_mm_load_ps(values + 8));
    ahalf1 = _mm256_insertf128_ps(ahalf1, _mm_load_ps(values + 12), 1);

    // Pack rows 0,1 of B
    __m256 bhalf0 = _mm256_castps128_ps256(_mm_load_ps(other.values + 0));
    bhalf0 = _mm256_insertf128_ps(bhalf0, _mm_load_ps(other.values + 4), 1);
    // Pack rows 2,3 of B
    __m256 bhalf1 = _mm256_castps128_ps256(_mm_load_ps(other.values + 8));
    bhalf1 = _mm256_insertf128_ps(bhalf1, _mm_load_ps(other.values + 12), 1);

    __m256 temp0_l, temp0_r;
    __m256 temp1;

    // Multiply by element 0 (x) of each A row against B.Row_0
    temp0_l = _mm256_shuffle_ps(ahalf0, ahalf0, _MM_SHUFFLE(0, 0, 0, 0));
    temp0_r = _mm256_shuffle_ps(ahalf1, ahalf1, _MM_SHUFFLE(0, 0, 0, 0));
    temp1 = _mm256_permute2f128_ps(bhalf0, bhalf0, 0x00);
    __m256 r0_l = _mm256_mul_ps(temp0_l, temp1);
    __m256 r0_r = _mm256_mul_ps(temp0_r, temp1);

    // Multiply by element 1 (y) against B.Row_1 and accumulate
    temp0_l = _mm256_shuffle_ps(ahalf0, ahalf0, _MM_SHUFFLE(1, 1, 1, 1));
    temp0_r = _mm256_shuffle_ps(ahalf1, ahalf1, _MM_SHUFFLE(1, 1, 1, 1));
    temp1 = _mm256_permute2f128_ps(bhalf0, bhalf0, 0x11);
    __m256 r1_l = _mm256_add_ps(_mm256_mul_ps(temp0_l, temp1), r0_l);
    __m256 r1_r = _mm256_add_ps(_mm256_mul_ps(temp0_r, temp1), r0_r);

    // Multiply by element 2 (z) against B.Row_2
    temp0_l = _mm256_shuffle_ps(ahalf0, ahalf0, _MM_SHUFFLE(2, 2, 2, 2));
    temp0_r = _mm256_shuffle_ps(ahalf1, ahalf1, _MM_SHUFFLE(2, 2, 2, 2));
    __m256 b1 = _mm256_permute2f128_ps(bhalf1, bhalf1, 0x00);
    __m256 r2_l = _mm256_mul_ps(temp0_l, b1);
    __m256 r2_r = _mm256_mul_ps(temp0_r, b1);

    // Multiply by element 3 (w) against B.Row_3 and accumulate
    temp0_l = _mm256_shuffle_ps(ahalf0, ahalf0, _MM_SHUFFLE(3, 3, 3, 3));
    temp0_r = _mm256_shuffle_ps(ahalf1, ahalf1, _MM_SHUFFLE(3, 3, 3, 3));
    b1 = _mm256_permute2f128_ps(bhalf1, bhalf1, 0x11);
    __m256 c6 = _mm256_add_ps(_mm256_mul_ps(temp0_l, b1), r2_l);
    __m256 c7 = _mm256_add_ps(_mm256_mul_ps(temp0_r, b1), r2_r);

    // Final accumulate: result halves = [C.Row0|C.Row1] and [C.Row2|C.Row3]
    temp0_l = _mm256_add_ps(r1_l, c6);
    temp0_r = _mm256_add_ps(r1_r, c7);

    alignas(32) float fv[16];
    _mm256_store_ps(fv + 0, temp0_l);
    _mm256_store_ps(fv + 8, temp0_r);

    return Mat4f(fv);
#else
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

    return Mat4f(fv);
#endif
}

Mat4f& Mat4f::operator*=(const Mat4f& other)
{
    return (*this) = operator*(other);
}

Mat4f Mat4f::operator*(float scalar) const
{
    Mat4f result(*this);
    result *= scalar;

    return result;
}

Mat4f& Mat4f::operator*=(float scalar)
{
#if HYP_MAT4F_USE_SSE
    const __m128 s = _mm_set1_ps(scalar);
    _mm_store_ps(values + 0, _mm_mul_ps(_mm_load_ps(values + 0), s));
    _mm_store_ps(values + 4, _mm_mul_ps(_mm_load_ps(values + 4), s));
    _mm_store_ps(values + 8, _mm_mul_ps(_mm_load_ps(values + 8), s));
    _mm_store_ps(values + 12, _mm_mul_ps(_mm_load_ps(values + 12), s));

    return *this;
#else
    for (float& value : values)
    {
        value *= scalar;
    }

    return *this;
#endif
}

Vec3f Mat4f::operator*(const Vec3f& vec) const
{
#if HYP_MAT4F_USE_SSE
    // Adapted from Foxtrot SIMD matrix paths:
    // Math/Impl/Matrix/FxMat4_AVX.cpp MultiplyVec4f_SSE
    // Treat vec as homogeneous (w=1), compute dot(Row_i, v) via _mm_dp_ps,
    // then assemble the result Vec4f and apply the perspective divide.
    const __m128 v = _mm_setr_ps(vec.x, vec.y, vec.z, 1.0f);
    const __m128 x = _mm_dp_ps(_mm_load_ps(values + 0), v, 0xF1);
    const __m128 y = _mm_dp_ps(_mm_load_ps(values + 4), v, 0xF2);
    const __m128 z = _mm_dp_ps(_mm_load_ps(values + 8), v, 0xF4);
    const __m128 w = _mm_dp_ps(_mm_load_ps(values + 12), v, 0xF8);
    const Vec4f product = StoreVec4f(_mm_or_ps(_mm_or_ps(x, y), _mm_or_ps(z, w)));

    return product.GetXYZ() / product.w;
#else
    const Vec4f product {
        vec.x * values[0] + vec.y * values[1] + vec.z * values[2] + values[3],
        vec.x * values[4] + vec.y * values[5] + vec.z * values[6] + values[7],
        vec.x * values[8] + vec.y * values[9] + vec.z * values[10] + values[11],
        vec.x * values[12] + vec.y * values[13] + vec.z * values[14] + values[15]
    };

    return product.GetXYZ() / product.w;
#endif
}

Vec4f Mat4f::operator*(const Vec4f& vec) const
{
#if HYP_MAT4F_USE_SSE
    // Adapted from Foxtrot SIMD matrix paths:
    // Math/Impl/Matrix/FxMat4_AVX.cpp MultiplyVec4f_SSE
    // Row-major: result[i] = dot(Row_i, v). Each _mm_dp_ps deposits its scalar
    // into a distinct lane via the dest mask, then the lanes are OR'd together.
    const __m128 v = vec._value;
    const __m128 x = _mm_dp_ps(_mm_load_ps(values + 0), v, 0xF1);
    const __m128 y = _mm_dp_ps(_mm_load_ps(values + 4), v, 0xF2);
    const __m128 z = _mm_dp_ps(_mm_load_ps(values + 8), v, 0xF4);
    const __m128 w = _mm_dp_ps(_mm_load_ps(values + 12), v, 0xF8);

    return StoreVec4f(_mm_or_ps(_mm_or_ps(x, y), _mm_or_ps(z, w)));
#else
    return {
        vec.x * values[0] + vec.y * values[1] + vec.z * values[2] + vec.w * values[3],
        vec.x * values[4] + vec.y * values[5] + vec.z * values[6] + vec.w * values[7],
        vec.x * values[8] + vec.y * values[9] + vec.z * values[10] + vec.w * values[11],
        vec.x * values[12] + vec.y * values[13] + vec.z * values[14] + vec.w * values[15]
    };
#endif
}

Vec3f Mat4f::ExtractTranslation() const
{
    return {
        rows[0][3],
        rows[1][3],
        rows[2][3]
    };
}

Vec3f Mat4f::ExtractScale() const
{
    return {
        rows[0][0],
        rows[1][1],
        rows[2][2]
    };
}

Quat4f Mat4f::ExtractRotation() const
{
    return Quat4f(*this);
}

Vec4f Mat4f::GetColumn(uint32 index) const
{
    return {
        rows[0][index],
        rows[1][index],
        rows[2][index],
        rows[3][index]
    };
}

Mat4f Mat4f::Zeros()
{
    static constexpr float zeroArray[sizeof(values) / sizeof(values[0])] = { 0.0f };

    return Mat4f(zeroArray);
}

Mat4f Mat4f::Ones()
{
    static constexpr float onesArray[sizeof(values) / sizeof(values[0])] = { 1.0f };

    return Mat4f(onesArray);
}

Mat4f Mat4f::Identity()
{
    return Mat4f(); // constructor fills out identity matrix
}
} // namespace Hyperion
