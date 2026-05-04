/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/utilities/Traits.hpp>

#include <type_traits>
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
    HYP_FORCE_INLINE static int Compare(const HYP_NOTNULL T* a, const HYP_NOTNULL U* b, size_t count)
    {
        return std::memcmp(a, b, count);
    }
    
    template <class T, class U, typename = std::enable_if_t<BitwiseCopyable<T> && BitwiseCopyable<U>>>
    HYP_FORCE_INLINE static void* Copy(HYP_NOTNULL T* dest, const HYP_NOTNULL U* src, size_t count)
    {
        return std::memcpy(dest, src, count);
    }
    
template <class T, class U, typename = std::enable_if_t<BitwiseCopyable<T> && BitwiseCopyable<U>>>
    HYP_FORCE_INLINE static void* Move(HYP_NOTNULL T* dest, const U* src, size_t size)
    {
        return std::memmove(dest, src, size);
    }

    template <class T>
    HYP_FORCE_INLINE static void* Fill(HYP_NOTNULL T* dest, ubyte ch, size_t size)
    {
        return std::memset(dest, ch, size);
    }

    template <class T>
    HYP_FORCE_INLINE static void* Zero(HYP_NOTNULL T* dest, size_t size)
    {
        return std::memset(dest, 0, size);
    }

    HYP_FORCE_INLINE static void Garble(HYP_NOTNULL void* dest, size_t length)
    {
        if (!dest || length == 0)
        {
            return;
        }

        std::memset(dest, 0xDEAD, length);
    }

    HYP_FORCE_INLINE static int StrCmp(const HYP_NOTNULL char* lhs, HYP_NOTNULL const char* rhs, size_t length = 0)
    {
        if (length)
        {
            return std::strncmp(lhs, rhs, length);
        }

        return std::strcmp(lhs, rhs);
    }

    template <class T>
    static constexpr bool StrEqual(const HYP_NOTNULL T* lhs, const HYP_NOTNULL T* rhs, size_t length, size_t index = 0)
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

    HYP_FORCE_INLINE static char* StrCpy(HYP_NOTNULL char* dest, const HYP_NOTNULL char* src, size_t length = 0)
    {
        if (length)
        {
            // ReSharper disable once CppDeprecatedEntity
            return std::strncpy(dest, src, length);
        }

        // ReSharper disable once CppDeprecatedEntity
        return std::strcpy(dest, src);
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
    static void Delete(HYP_NOTNULL void* ptr)
    {
        delete static_cast<T*>(ptr);
    }

    template <class T, class... Args>
    static void Construct(HYP_NOTNULL void* where, Args&&... args)
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
        void* ptr;

        if constexpr (alignof(T) <= alignof(std::max_align_t))
        {
            // Use standard allocation if alignment is not greater than max alignment
            ptr = Memory::Allocate(sizeof(T));
        }
        else
        {
            // Use aligned allocation if alignment is greater than max alignment
            ptr = HYP_ALLOC_ALIGNED(sizeof(T), alignof(T));
        }

        new (ptr) T(std::forward<Args>(args)...);

        return static_cast<T*>(ptr);
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

    template <class T>
    static typename std::enable_if_t<!std::is_same_v<void*, std::add_pointer_t<T>>, void> DestructAndFree(void* ptr)
    {
#if HYP_DEBUG_MODE
        if (HYP_UNLIKELY(!ptr))
        {
            HYP_BREAKPOINT;
        }
#endif

        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            static_cast<T*>(ptr)->~T();
        }

#if HYP_DEBUG_MODE
        Memory::Garble(ptr, sizeof(T));
#endif

        if constexpr (alignof(T) <= alignof(std::max_align_t))
        {
            // Use standard deallocation if alignment is not greater than max alignment
            std::free(ptr);
        }
        else
        {
            // Use aligned deallocation if alignment is greater than max alignment
            HYP_FREE_ALIGNED(ptr);
        }
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

    template <class T>
    HYP_FORCE_INLINE static T* Allocate()
    {
        if constexpr (alignof(T) <= alignof(std::max_align_t))
        {
            // Use standard allocation if alignment is not greater than max alignment
            return static_cast<T*>(Memory::Allocate(sizeof(T)));
        }
        else
        {
            // Use aligned allocation if alignment is greater than max alignment
            return static_cast<T*>(HYP_ALLOC_ALIGNED(sizeof(T), alignof(T)));
        }
    }

    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE static T* Allocate(size_t count)
    {
        if (count == 0)
        {
            return nullptr;
        }

        if constexpr (alignof(T) <= alignof(std::max_align_t))
        {
            // Use standard allocation if alignment is not greater than max alignment
            return static_cast<T*>(Memory::Allocate(sizeof(T) * count));
        }
        else
        {
            // Use aligned allocation if alignment is greater than max alignment
            return static_cast<T*>(HYP_ALLOC_ALIGNED(sizeof(T) * count, alignof(T)));
        }
    }

    HYP_NODISCARD HYP_FORCE_INLINE static void* AllocateAligned(size_t count, size_t alignment)
    {
        return HYP_ALLOC_ALIGNED(count, alignment);
    }

    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE static T* AllocateAligned(size_t count, size_t alignment)
    {
        if (count == 0)
        {
            return nullptr;
        }

        return static_cast<T*>(HYP_ALLOC_ALIGNED(sizeof(T) * count, alignment));
    }

    HYP_FORCE_INLINE static void FreeAligned(HYP_NOTNULL void* ptr)
    {
        HYP_FREE_ALIGNED(ptr);
    }
};

} // namespace memory

using memory::Memory;

} // namespace Hyperion

#define StackAlloc(size) alloca(size)
