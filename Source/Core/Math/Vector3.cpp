/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Math/Vector3.hpp>
#include <Core/Math/Quat4f.hpp>
#include <Core/Math/Mat3f.hpp>
#include <Core/Math/Mat4f.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <cmath>

#if !HYP_ARM && (defined(__SSE4_1__) || (HYP_MSVC && defined(_M_X64)))
#include <immintrin.h>
#define HYP_VECTOR3_USE_SSE 1
#else
#define HYP_VECTOR3_USE_SSE 0
#endif

namespace {

#if HYP_VECTOR3_USE_SSE
HYP_FORCE_INLINE __m128 LoadVec3f(const Hyperion::math::Vec3<float>& value)
{
    return _mm_setr_ps(value.x, value.y, value.z, 0.0f);
}

HYP_FORCE_INLINE Hyperion::math::Vec3<float> StoreVec3f(__m128 value)
{
    alignas(16) float data[4];
    _mm_store_ps(data, value);

    return { data[0], data[1], data[2] };
}
#endif

} // namespace

namespace Hyperion {

CORE_API const Class* g_clsVec3f = nullptr;
CORE_API const Class* g_clsVec3i = nullptr;
CORE_API const Class* g_clsVec3u = nullptr;

// clang-format off
HYP_BEGIN_STRUCT(Vec3f, -1, 0, {})
    Field(NAME(HYP_STR(x)), &Type::x, HYP_OFFSET_OF(Type, x)),
    Field(NAME(HYP_STR(y)), &Type::y, HYP_OFFSET_OF(Type, y)),
    Field(NAME(HYP_STR(z)), &Type::z, HYP_OFFSET_OF(Type, z))
HYP_END_STRUCT

HYP_REGISTER_STATIC_CLASS(Vec3f);

HYP_BEGIN_STRUCT(Vec3i, -1, 0, {})
    Field(NAME(HYP_STR(x)), &Type::x, HYP_OFFSET_OF(Type, x)),
    Field(NAME(HYP_STR(y)), &Type::y, HYP_OFFSET_OF(Type, y)),
    Field(NAME(HYP_STR(z)), &Type::z, HYP_OFFSET_OF(Type, z))
HYP_END_STRUCT

HYP_REGISTER_STATIC_CLASS(Vec3i);

HYP_BEGIN_STRUCT(Vec3u, -1, 0, {})
    Field(NAME(HYP_STR(x)), &Type::x, HYP_OFFSET_OF(Type, x)),
    Field(NAME(HYP_STR(y)), &Type::y, HYP_OFFSET_OF(Type, y)),
    Field(NAME(HYP_STR(z)), &Type::z, HYP_OFFSET_OF(Type, z))
HYP_END_STRUCT

HYP_REGISTER_STATIC_CLASS(Vec3u);

// clang-format on

Vec3<float> math::Vec3<float>::operator+(const Vec3<float>& other) const
{
#if HYP_VECTOR3_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec3_AVX.inl
    return StoreVec3f(_mm_add_ps(LoadVec3f(*this), LoadVec3f(other)));
#else
    return { x + other.x, y + other.y, z + other.z };
#endif
}

Vec3<float>& math::Vec3<float>::operator+=(const Vec3<float>& other)
{
#if HYP_VECTOR3_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec3_AVX.inl
    *this = StoreVec3f(_mm_add_ps(LoadVec3f(*this), LoadVec3f(other)));

    return *this;
#else
    x += other.x;
    y += other.y;
    z += other.z;
    return *this;
#endif
}

Vec3<float> math::Vec3<float>::operator-(const Vec3<float>& other) const
{
#if HYP_VECTOR3_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec3_AVX.inl
    return StoreVec3f(_mm_sub_ps(LoadVec3f(*this), LoadVec3f(other)));
#else
    return { x - other.x, y - other.y, z - other.z };
#endif
}

Vec3<float>& math::Vec3<float>::operator-=(const Vec3<float>& other)
{
#if HYP_VECTOR3_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec3_AVX.inl
    *this = StoreVec3f(_mm_sub_ps(LoadVec3f(*this), LoadVec3f(other)));

    return *this;
#else
    x -= other.x;
    y -= other.y;
    z -= other.z;
    return *this;
#endif
}

Vec3<float> math::Vec3<float>::operator*(const Vec3<float>& other) const
{
#if HYP_VECTOR3_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec3_AVX.inl
    return StoreVec3f(_mm_mul_ps(LoadVec3f(*this), LoadVec3f(other)));
#else
    return { x * other.x, y * other.y, z * other.z };
#endif
}

Vec3<float>& math::Vec3<float>::operator*=(const Vec3<float>& other)
{
#if HYP_VECTOR3_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec3_AVX.inl
    *this = StoreVec3f(_mm_mul_ps(LoadVec3f(*this), LoadVec3f(other)));

    return *this;
#else
    x *= other.x;
    y *= other.y;
    z *= other.z;
    return *this;
#endif
}

Vec3<float> math::Vec3<float>::operator/(const Vec3<float>& other) const
{
#if HYP_VECTOR3_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec3_AVX.inl
    return StoreVec3f(_mm_div_ps(LoadVec3f(*this), LoadVec3f(other)));
#else
    return { x / other.x, y / other.y, z / other.z };
#endif
}

bool math::Vec3<float>::operator==(const Vec3<float>& other) const
{
#if HYP_VECTOR3_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec3_AVX.inl
    __m128i cmp_v = _mm_castps_si128(_mm_cmpeq_ps(LoadVec3f(*this), LoadVec3f(other)));
    // This is frequently done as a comparison against _mm_movemask, but _mm_test_all_ones potentially saves one extra
    // instruction.
    return static_cast<bool>(_mm_test_all_ones(cmp_v));
#else
    return x == other.x && y == other.y && z == other.z;
#endif
}

Vec3<float> math::Vec3<float>::operator-() const
{
#if HYP_VECTOR3_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec3_AVX.inl
    return StoreVec3f(_mm_sub_ps(_mm_setzero_ps(), LoadVec3f(*this)));
#else
    return { -x, -y, -z };
#endif
}

float math::Vec3<float>::Length() const
{
#if HYP_VECTOR3_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec3_AVX.inl
    return MathUtil::Sqrt(Dot(*this));
#else
    return std::sqrt(LengthSquared());
#endif
}

Vec3<float> math::Vec3<float>::operator*(const Mat3f& mat) const
{
    return {
        x * mat.rows[0][0] + y * mat.rows[1][0] + z * mat.rows[2][0],
        x * mat.rows[0][1] + y * mat.rows[1][1] + z * mat.rows[2][1],
        x * mat.rows[0][2] + y * mat.rows[1][2] + z * mat.rows[2][2]
    };
}

Vec3<float>& math::Vec3<float>::operator*=(const Mat3f& mat)
{
    return operator=(operator*(mat));
}

Vec3<float> math::Vec3<float>::operator*(const Mat4f& mat) const
{
    Vector4 product {
        x * mat.values[0] + y * mat.values[4] + z * mat.values[8] + mat.values[12],
        x * mat.values[1] + y * mat.values[5] + z * mat.values[9] + mat.values[13],
        x * mat.values[2] + y * mat.values[6] + z * mat.values[10] + mat.values[14],
        x * mat.values[3] + y * mat.values[7] + z * mat.values[11] + mat.values[15]
    };

    product /= product.w;

    return {
        product.x,
        product.y,
        product.z
    };
}

Vec3<float>& math::Vec3<float>::operator*=(const Mat4f& mat)
{
    return operator=(operator*(mat));
}

Vec3<float> math::Vec3<float>::operator*(const Quat4f& quat) const
{
    Vec3<float> result;
    result.x = quat.w * quat.w * x + 2 * quat.y * quat.w * z - 2 * quat.z * quat.w * y + quat.x * quat.x * x + 2 * quat.y * quat.x * y + 2 * quat.z * quat.x * z - quat.z * quat.z * x - quat.y * quat.y * x;

    result.y = 2 * quat.x * quat.y * x + quat.y * quat.y * y + 2 * quat.z * quat.y * z + 2 * quat.w * quat.z * x - quat.z * quat.z * y + quat.w * quat.w * y - 2 * quat.x * quat.w * z - quat.x * quat.x * y;

    result.z = 2 * quat.x * quat.z * x + 2 * quat.y * quat.z * y + quat.z * quat.z * z - 2 * quat.w * quat.y * x - quat.y * quat.y * z + 2 * quat.w * quat.x * y - quat.x * quat.x * z + quat.w * quat.w * z;
    return result;
}

Vec3<float>& math::Vec3<float>::operator*=(const Quat4f& quat)
{
    return operator=(operator*(quat));
}

float math::Vec3<float>::DistanceSquared(const Vec3f& other) const
{
#if HYP_VECTOR3_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec3_AVX.inl
    const __m128 diff = _mm_sub_ps(LoadVec3f(*this), LoadVec3f(other));
    return _mm_cvtss_f32(_mm_dp_ps(diff, diff, 0x7F));
#else
    float dx = x - other.x;
    float dy = y - other.y;
    float dz = z - other.z;
    return dx * dx + dy * dy + dz * dz;
#endif
}

/* Euclidean distance */
float math::Vec3<float>::Distance(const Vec3f& other) const
{
    return MathUtil::Sqrt(DistanceSquared(other));
}

Vec3<float> math::Vec3<float>::Normalized() const
{
#if HYP_VECTOR3_USE_SSE
    const float len = MathUtil::Max(Length(), MathUtil::epsilonF);
    const __m128 invLen = _mm_set1_ps(1.0f / len);

    return StoreVec3f(_mm_mul_ps(LoadVec3f(*this), invLen));
#else
    return *this / MathUtil::Max(Length(), MathUtil::epsilonF);
#endif
}

Vec3<float>& math::Vec3<float>::Normalize()
{
#if HYP_VECTOR3_USE_SSE
    const float len = MathUtil::Max(Length(), MathUtil::epsilonF);
    const __m128 invLen = _mm_set1_ps(1.0f / len);
    *this = StoreVec3f(_mm_mul_ps(LoadVec3f(*this), invLen));

    return *this;
#else
    return *this /= MathUtil::Max(Length(), MathUtil::epsilonF);
#endif
}

Vec3<float> math::Vec3<float>::Cross(const Vec3<float>& other) const
{
#if HYP_VECTOR3_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec3_AVX.cpp
    //
    // Permute both operands to (Y, Z, X, W), compute a*b_yzxw - a_yzxw*b,
    // which yields the cross product in (Z, X, Y, 0) order, then permute
    // once more with the same mask to arrive at (X, Y, Z, 0).
    const __m128 a = LoadVec3f(*this);
    const __m128 b = LoadVec3f(other);

    const __m128 a_yzxw = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1));
    const __m128 b_yzxw = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1));

    const __m128 result_yzxw = _mm_sub_ps(_mm_mul_ps(a, b_yzxw), _mm_mul_ps(a_yzxw, b));

    return StoreVec3f(_mm_shuffle_ps(result_yzxw, result_yzxw, _MM_SHUFFLE(3, 0, 2, 1)));
#else
    return {
        y * other.z - z * other.y,
        z * other.x - x * other.z,
        x * other.y - y * other.x
    };
#endif
}

Vec3<float> math::Vec3<float>::Reflect(const Vec3<float>& normal) const
{
    const Vec3& incident = *this;
    return incident - Vec3(2.0f) * Dot(normal) * normal;
}

Vec3<float>& math::Vec3<float>::Rotate(const Vec3<float>& axis, float radians)
{
    return (*this) = Mat4f::Rotation(axis, radians).TransformVector(*this);
}

Vec3<float>& math::Vec3<float>::Rotate(const Quat4f& quaternion)
{
    return (*this) = Mat4f::Rotation(quaternion).TransformVector(*this);
}

Vec3<float>& math::Vec3<float>::Lerp(const Vec3<float>& to, const float amt)
{
#if HYP_VECTOR3_USE_SSE
    // a + f * (b - a);
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec3_AVX.inl
    const __m128 a = LoadVec3f(*this);
    const __m128 b = LoadVec3f(to);
    const __m128 f = _mm_set1_ps(amt);
    *this = StoreVec3f(_mm_add_ps(a, _mm_mul_ps(_mm_sub_ps(b, a), f)));
#else
    x = MathUtil::Lerp(x, to.x, amt);
    y = MathUtil::Lerp(y, to.y, amt);
    z = MathUtil::Lerp(z, to.z, amt);
#endif

    return *this;
}

float math::Vec3<float>::Dot(const Vec3<float>& other) const
{
#if HYP_VECTOR3_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec3_AVX.inl
    // Mask 0x7F: Src=0111 (x,y,z only), Dest=1111 (broadcast to all lanes).
    return _mm_cvtss_f32(_mm_dp_ps(LoadVec3f(*this), LoadVec3f(other), 0x7F));
#else
    return x * other.x + y * other.y + z * other.z;
#endif
}

float math::Vec3<float>::AngleBetween(const Vector3& other) const
{
    const float dotProduct = x * other.x + y * other.y + z * other.z;
    const float arcCos = MathUtil::Arccos(dotProduct);

    return arcCos / (Length() * other.Length());
}

Vec3<float> math::Vec3<float>::Abs(const Vec3<float>& vec)
{
#if HYP_VECTOR3_USE_SSE
    const __m128 signMask = _mm_set1_ps(-0.0f);
    return StoreVec3f(_mm_andnot_ps(signMask, LoadVec3f(vec)));
#else
    return {
        MathUtil::Abs(vec.x),
        MathUtil::Abs(vec.y),
        MathUtil::Abs(vec.z)
    };
#endif
}

Vec3<float> math::Vec3<float>::Round(const Vec3<float>& vec)
{
    return {
        MathUtil::Round(vec.x),
        MathUtil::Round(vec.y),
        MathUtil::Round(vec.z)
    };
}

Vec3<float> math::Vec3<float>::Clamp(const Vec3<float>& vec, float minValue, float maxValue)
{
    return Max(minValue, Min(vec, maxValue));
}

Vec3<float> math::Vec3<float>::Min(const Vec3<float>& a, const Vec3<float>& b)
{
#if HYP_VECTOR3_USE_SSE
    return StoreVec3f(_mm_min_ps(LoadVec3f(a), LoadVec3f(b)));
#else
    return {
        MathUtil::Min(a.x, b.x),
        MathUtil::Min(a.y, b.y),
        MathUtil::Min(a.z, b.z)
    };
#endif
}

Vec3<float> math::Vec3<float>::Max(const Vec3<float>& a, const Vec3<float>& b)
{
#if HYP_VECTOR3_USE_SSE
    return StoreVec3f(_mm_max_ps(LoadVec3f(a), LoadVec3f(b)));
#else
    return {
        MathUtil::Max(a.x, b.x),
        MathUtil::Max(a.y, b.y),
        MathUtil::Max(a.z, b.z)
    };
#endif
}

} // namespace Hyperion
