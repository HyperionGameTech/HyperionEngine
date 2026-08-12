/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Memory/Memory.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#if defined(HYP_ARM)
#include <arm_neon.h>
#elif defined(__SSE4_1__) || (HYP_MSVC && defined(_M_X64))
#include <immintrin.h>
#endif

namespace Hyperion {
namespace Strata {

#if defined(HYP_ARM)
using SimdVector = float32x4_t;

HYP_FORCE_INLINE SimdVector ToSimdVector(const Vec3f& value)
{
    alignas(16) float data[4] = { value.x, value.y, value.z, 0.0f };

    return vld1q_f32(data);
}

HYP_FORCE_INLINE SimdVector ToSimdVector(const Vec4f& value)
{
    alignas(16) float data[4] = { value.x, value.y, value.z, value.w };

    return vld1q_f32(data);
}

HYP_FORCE_INLINE Vec3f SimdVectorToVec3f(SimdVector value)
{
    alignas(16) float data[4];
    vst1q_f32(data, value);

    return { data[0], data[1], data[2] };
}

HYP_FORCE_INLINE Vec4f SimdVectorToVec4f(SimdVector value)
{
    alignas(16) float data[4];
    vst1q_f32(data, value);

    return { data[0], data[1], data[2], data[3] };
}

#else
using SimdVector = __m128;

HYP_FORCE_INLINE SimdVector ToSimdVector(const Vec3f& value)
{
    return _mm_setr_ps(value.x, value.y, value.z, 0.0f);
}

HYP_FORCE_INLINE SimdVector ToSimdVector(const Vec4f& value)
{
    return _mm_setr_ps(value.x, value.y, value.z, value.w);
}

HYP_FORCE_INLINE Vec3f SimdVectorToVec3f(SimdVector value)
{
    alignas(16) float data[4];
    _mm_store_ps(data, value);

    return { data[0], data[1], data[2] };
}

HYP_FORCE_INLINE Vec4f SimdVectorToVec4f(SimdVector value)
{
    alignas(16) float data[4];
    _mm_store_ps(data, value);

    return { data[0], data[1], data[2], data[3] };
}

#endif

CORE_API void* Alloc(size_t size);
CORE_API void Free(void* ptr);

CORE_API char* AllocReturnString(const char* data, size_t size);

template <class StringType>
HYP_FORCE_INLINE char* AllocReturnString(const StringType& str)
{
    return AllocReturnString(str.Data(), str.Size());
}

template <class T>
struct ArrayView
{
    T* data;
    uint64 length;
};

template <class T>
HYP_FORCE_INLINE void SetReturnArray(ArrayView<T>* outArray, const T* data, size_t count)
{
    T* copy = nullptr;

    if (count > 0)
    {
        copy = reinterpret_cast<T*>(Alloc(count * sizeof(T)));

        Memory::Copy(copy, data, count * sizeof(T));
    }

    outArray->data = copy;
    outArray->length = uint64(count);
}

} // namespace Strata
} // namespace Hyperion
