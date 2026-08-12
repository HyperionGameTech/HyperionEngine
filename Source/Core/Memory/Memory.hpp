/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Utilities/Traits.hpp>
#include <Core/Utilities/ByteUtil.hpp>

#include <type_traits>
#include <utility>
#include <cstring>
#include <cstdlib>
#include <cstddef>

#ifdef HYP_WINDOWS
#define _CRT_SECURE_NO_WARNINGS 1
#endif

namespace Hyperion {
namespace memory {

class Memory
{
public:
    template <class T, class U, typename = std::enable_if_t<BitwiseComparable<T> && BitwiseComparable<U>>>
    HYP_FORCE_INLINE static int Compare(const T* a, const U* b, size_t count)
    {
        return std::memcmp(a, b, count);
    }

    template <class T, class U, typename = std::enable_if_t<BitwiseCopyable<T> && BitwiseCopyable<U>>>
    HYP_FORCE_INLINE static void* Copy(T* dest, const U* src, size_t count)
    {
        return std::memcpy(dest, src, count);
    }

    template <class T, class U, typename = std::enable_if_t<BitwiseCopyable<T> && BitwiseCopyable<U>>>
    HYP_FORCE_INLINE static void* Move(T* dest, const U* src, size_t size)
    {
        return std::memmove(dest, src, size);
    }

    template <class T>
    HYP_FORCE_INLINE static void* Fill(T* dest, ubyte ch, size_t size)
    {
        return std::memset(dest, ch, size);
    }

    template <class T>
    HYP_FORCE_INLINE static void* Zero(T* dest, size_t size)
    {
        return std::memset(dest, 0, size);
    }

#if HYP_DEBUG_MODE
    HYP_FORCE_INLINE static void Garble(void* dest, size_t length)
    {
        if (length == 0)
        {
            return;
        }

        std::memset(dest, 0xDEAD, length);
    }
#else
    HYP_FORCE_INLINE static void Garble(void* dest, size_t length)
    {
    }
#endif

    HYP_FORCE_INLINE static int StrCmp(const char* lhs, const char* rhs, size_t length = 0)
    {
        if (length)
        {
            return std::strncmp(lhs, rhs, length);
        }

        return std::strcmp(lhs, rhs);
    }

    template <class T>
    static constexpr bool StrEqual(const T* lhs, const T* rhs, size_t length, size_t index = 0)
    {
        if (HYP_CONSTEVAL_CONTEXT)
        {
            return *lhs == *rhs
                && ((*lhs == '\0' || (!length || index >= length)) || StrEqual(lhs + 1, rhs + 1, length, index + 1));
        }

        if constexpr (sizeof(T) == sizeof(char))
        {
            return StrCmp((const char*)lhs, (const char*)rhs, length) == 0;
        }
        else
        {
            // Fallback for non 8-bit char types
            for (size_t i = 0; i < length || length == 0; ++i)
            {
                if (lhs[i] != rhs[i] || lhs[i] == T('\0'))
                {
                    return false;
                }
            }

            return true;
        }
    }

    template <size_t DstBufferSize>
    static constexpr HYP_FORCE_INLINE size_t CopyString(char (&dst)[DstBufferSize], size_t& offset, const char* __restrict src, size_t len)
    {
        if (offset >= DstBufferSize)
        {
            return 0;
        }

        size_t maxWrite = DstBufferSize - offset - 1;

        size_t numToCopy = (len < maxWrite) ? len : maxWrite;

        if (numToCopy == 0)
        {
            return 0;
        }

        memcpy(dst + offset, src, numToCopy);

        offset += numToCopy;

        dst[offset] = '\0';

        return numToCopy;
    }

    template <size_t DstBufferSize>
    static constexpr HYP_FORCE_INLINE size_t CopyString(char (&dst)[DstBufferSize], const char* __restrict src, size_t len)
    {
        [[maybe_unused]] size_t offset = 0;
        return CopyString<DstBufferSize>(dst, offset, src, len);
    }

    /// Assumes len of dst and src are the same or dst is larger.
    static constexpr HYP_FORCE_INLINE size_t CopyString(char* __restrict dst, size_t& offset, const char* __restrict src, size_t len)
    {
        if (offset >= len)
        {
            return 0;
        }

        size_t maxWrite = len - offset - 1;

        size_t numToCopy = (len < maxWrite) ? len : maxWrite;

        if (numToCopy == 0)
        {
            return 0;
        }

        memcpy(dst + offset, src, numToCopy);

        offset += numToCopy;

        dst[offset] = '\0';

        return numToCopy;
    }

    /// Assumes len of dst and src are the same or dst is larger.
    static constexpr HYP_FORCE_INLINE size_t CopyString(char* __restrict dst, const char* __restrict src, size_t len)
    {
        [[maybe_unused]] size_t offset = 0;
        return CopyString(dst, offset, src, len);
    }

    static inline size_t StrLen(const char* str)
    {
        if (!str)
        {
            return 0;
        }

        return std::strlen(str);
    }

    template <class T, class... Args>
    HYP_NODISCARD static T* New(Args&&... args)
    {
        return new T(std::forward<Args>(args)...);
    }

    template <class T>
    static void Delete(void* ptr)
    {
        delete static_cast<T*>(ptr);
    }

    template <class T, class... Args>
    static void Construct(void* where, Args&&... args)
    {
        new (where) T(std::forward<Args>(args)...);
    }

    /*! \brief Allocates memory for an object of type T, and constructs it in-place using the given arguments. The pointer will be aligned to the type's alignment requirements.
     *  \tparam T The type of the object to allocate and construct.
     *  \tparam Args The types of the arguments to pass to the constructor of T.
        \returns A pointer to the newly allocated and constructed object of type T. */
    template <class T, class... Args>
    HYP_NODISCARD static T* AllocateAndConstruct(Args&&... args)
    {
        void* ptr = AllocateAligned(sizeof(T), alignof(T));

        if (HYP_UNLIKELY(!ptr))
        {
            return nullptr;
        }

        return new (ptr) T(std::forward<Args>(args)...);
    }

    template <class T>
    static std::enable_if_t<std::is_trivially_destructible_v<T>> Destruct(T&)
    { /* Do nothing */
    }

    template <class T>
    static std::enable_if_t<!std::is_trivially_destructible_v<T>> Destruct(T& object)
    {
        object.~T();
#if HYP_DEBUG_MODE
        Memory::Garble(&object, sizeof(T));
#endif
    }

    template <class T>
    static std::enable_if_t<std::is_trivially_destructible_v<T>> Destruct(void* ptr)
    { /* Do nothing */
#if HYP_DEBUG_MODE
        if (HYP_UNLIKELY(!ptr))
        {
            HYP_BREAKPOINT;
        }
#endif
    }

    template <class T>
    static std::enable_if_t<!std::is_trivially_destructible_v<T>> Destruct(void* ptr)
    {
#if HYP_DEBUG_MODE
        if (HYP_UNLIKELY(!ptr))
        {
            HYP_BREAKPOINT;
        }
#endif

        static_cast<T*>(ptr)->~T();

#if HYP_DEBUG_MODE
        Memory::Garble(ptr, sizeof(T));
#endif
    }

    /*! \brief No operation function for deleting a trivially destructible object. */
    HYP_FORCE_INLINE static void NoOp(void*)
    { /* Do nothing */
    }

    HYP_FORCE_INLINE static void Free(void* ptr)
    {
#if HYP_DEBUG_MODE
        if (HYP_UNLIKELY(!ptr))
        {
            HYP_BREAKPOINT;
        }
#endif

        free(ptr);
    }

    HYP_FORCE_INLINE static void* AllocateZeros(size_t count)
    {
        return calloc(count, 1);
    }

    HYP_FORCE_INLINE static void* Allocate(size_t count)
    {
        return malloc(count);
    }

    HYP_NODISCARD HYP_FORCE_INLINE static void* AllocateAligned(size_t count, size_t alignment)
    {
#ifdef HYP_UNIX
        // POSIX requires alignment be a multiple of sizeof(void*)
        alignment = ByteUtil::AlignAs(alignment, sizeof(void*));
#endif

        count = ByteUtil::AlignAs(count, alignment);

        return HYP_ALLOC_ALIGNED(count, alignment);
    }

    HYP_FORCE_INLINE static void FreeAligned(void* ptr)
    {
        if (!ptr)
        {
            return;
        }

        HYP_FREE_ALIGNED(ptr);
    }
};

} // namespace memory

using memory::Memory;

} // namespace Hyperion

#define StackAlloc(size) alloca(size)
