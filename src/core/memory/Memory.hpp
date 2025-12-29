/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Types.hpp>

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
    HYP_FORCE_INLINE static int MemCmp(const void* lhs, const void* rhs, SizeType size)
    {
        return std::memcmp(lhs, rhs, size);
    }

    HYP_FORCE_INLINE static int StrCmp(const char* lhs, const char* rhs, SizeType length = 0)
    {
        if (length)
        {
            return std::strncmp(lhs, rhs, length);
        }

        return std::strcmp(lhs, rhs);
    }

    template <class T>
    static constexpr bool StrEqual(const T* lhs, const T* rhs, SizeType length, SizeType index = 0)
    {
        if (std::is_constant_evaluated())
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
            for (SizeType i = 0; i < length || length == 0; ++i)
            {
                if (lhs[i] != rhs[i] || lhs[i] == T('\0'))
                {
                    return false;
                }
            }

            return true;
        }
    }

    HYP_FORCE_INLINE static char* StrCpy(char* dest, const char* src, SizeType length = 0)
    {
        if (length)
        {
            // ReSharper disable once CppDeprecatedEntity
            return std::strncpy(dest, src, length);
        }

        // ReSharper disable once CppDeprecatedEntity
        return std::strcpy(dest, src);
    }

    static inline SizeType StrLen(const char* str)
    {
        if (!str)
        {
            return 0;
        }

        return std::strlen(str);
    }

    /*! \brief Alias for memset. Takes in a ubyte (unsigned char) as value,
        To signify that only the lowest byte is copied over. */
    static inline void* MemSet(void* dest, ubyte ch, SizeType size)
    {
        return std::memset(dest, ch, size);
    }

    static inline void* MemCpy(void* dest, const void* src, SizeType size)
    {
        return std::memcpy(dest, src, size);
    }

    static inline void* MemMove(void* dest, const void* src, SizeType size)
    {
        return std::memmove(dest, src, size);
    }

    static inline void* Clear(void* dest, SizeType size)
    {
        return std::memset(dest, 0, size);
    }

    static void Garble(void* dest, SizeType length)
    {
        if (!dest || length == 0)
        {
            return;
        }

        std::memset(dest, 0xDEAD, length);
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

    template <class T, class Context, class... Args>
    static void ConstructWithContext(void* where, Args&&... args)
    {
        auto contextResult = Context(where);

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

    /*! \brief Allocates memory for an object of type T, constructs it in-place using the given arguments, and constructs a Context object, passing in the address of the allocated memory before the construction of T.
     *  \tparam T The type of the object to allocate and construct.
     *  \tparam Context The context type that will be constructed before T is constructed. Useful for setting data on the allocated memory before the object is constructed. (e.g. ObjectInitializerGuard). This is an internal feature and should not be used by users of the API.
     *  \tparam Args The types of the arguments to pass to the constructor of T.
     *  \returns A pointer to the newly allocated and constructed object of type T. */
    template <class T, class Context, class... Args>
    HYP_NODISCARD static T* AllocateAndConstructWithContext(Args&&... args)
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

        auto contextResult = Context(ptr);

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
#ifdef HYP_DEBUG_MODE
        Memory::Garble(&object, sizeof(T));
#endif
    }

    template <class T>
    static std::enable_if_t<std::is_trivially_destructible_v<T>> Destruct(void* ptr)
    { /* Do nothing */
#ifdef HYP_DEBUG_MODE
        if (HYP_UNLIKELY(!ptr))
        {
            HYP_BREAKPOINT;
        }
#endif
    }

    template <class T>
    static std::enable_if_t<!std::is_trivially_destructible_v<T>> Destruct(void* ptr)
    {
#ifdef HYP_DEBUG_MODE
        if (HYP_UNLIKELY(!ptr))
        {
            HYP_BREAKPOINT;
        }
#endif

        static_cast<T*>(ptr)->~T();

#ifdef HYP_DEBUG_MODE
        Memory::Garble(ptr, sizeof(T));
#endif
    }

    template <class T>
    static typename std::enable_if_t<!std::is_same_v<void*, std::add_pointer_t<T>>, void> DestructAndFree(void* ptr)
    {
#ifdef HYP_DEBUG_MODE
        if (HYP_UNLIKELY(!ptr))
        {
            HYP_BREAKPOINT;
        }
#endif

        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            static_cast<T*>(ptr)->~T();
        }

#ifdef HYP_DEBUG_MODE
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
#ifdef HYP_DEBUG_MODE
        if (HYP_UNLIKELY(!ptr))
        {
            HYP_BREAKPOINT;
        }
#endif

        std::free(ptr);
    }

    HYP_FORCE_INLINE static void* AllocateZeros(SizeType count)
    {
        return std::calloc(count, 1);
    }

    HYP_FORCE_INLINE static void* Allocate(SizeType count)
    {
        return std::malloc(count);
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
    HYP_FORCE_INLINE static T* Allocate(SizeType count)
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
};

} // namespace memory

using memory::Memory;

} // namespace Hyperion

#define StackAlloc(size) alloca(size)
