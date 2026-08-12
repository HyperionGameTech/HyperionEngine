/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Memory/Memory.hpp>

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

namespace Hyperion {
namespace Strata {

// Shared with the strata JIT's own allocator (registered via
// strataJitSetAllocFreeFunctions), so a string/array a generated thunk hands
// back to Strata can be freed by the Strata runtime with strata_free().
CORE_API void* Alloc(size_t size);
CORE_API void Free(void* ptr);

// Copies `data` (size bytes, not expected to be null-terminated) into a
// Strata-owned, null-terminated buffer - for returning an engine String /
// ANSIString as a Strata `string` value.
CORE_API char* AllocReturnString(const char* data, size_t size);

template <class StringType>
HYP_FORCE_INLINE char* AllocReturnString(const StringType& str)
{
    return AllocReturnString(str.Data(), str.Size());
}

// Mirrors Strata's fat array ABI representation ({ ptr, u64 }). A Strata
// `T[]` crosses the extern boundary as a pointer to this layout.
template <class T>
struct ArrayView
{
    T* data;
    uint64 length;
};

// Copies `count` elements from `data` into a Strata-owned buffer and fills in
// `outArray` - for returning an engine Array<T> as a Strata `T[]` value.
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
