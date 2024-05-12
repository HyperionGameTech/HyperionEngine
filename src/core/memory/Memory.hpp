/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MEMORY_HPP
#define HYPERION_MEMORY_HPP

#include <core/Defines.hpp>
#include <Types.hpp>

#include <type_traits>
#include <cstring>
#include <cstdlib>

#ifdef HYP_WINDOWS
#define _CRT_SECURE_NO_WARNINGS 1
#endif

namespace hyperion {
namespace memory {

class Memory
{
public:
    static int MemCmp(const void *lhs, const void *rhs, SizeType size)
    {
        return std::memcmp(lhs, rhs, size);
    }

    static int StrCmp(const char *lhs, const char *rhs, SizeType length = 0)
    {
        if (length)
        {
            return std::strncmp(lhs, rhs, length);
        }

        return std::strcmp(lhs, rhs);
    }

    static constexpr bool AreStaticStringsEqual(const char *lhs, const char *rhs)
    {
        if (std::is_constant_evaluated())
        {
            return *lhs == *rhs
                && (*lhs == '\0' || AreStaticStringsEqual(lhs + 1, rhs + 1));
        }

        return StrCmp(lhs, rhs) == 0;
    }

    static char *StrCpy(char *dest, const char *src, SizeType length = 0)
    {
        if (length)
        {
            // ReSharper disable once CppDeprecatedEntity
            return std::strncpy(dest, src, length);
        }

        // ReSharper disable once CppDeprecatedEntity
        return std::strcpy(dest, src);
    }

    static inline SizeType StrLen(const char *str)
    {
        if (!str)
        {
            return 0;
        }

        return std::strlen(str);
    }

    /*! \brief Alias for memset. Takes in a ubyte (unsigned char) as value,
        To signify that only the lowest byte is copied over. */
    static inline void *MemSet(void *dest, ubyte ch, SizeType size)
    {
        return std::memset(dest, ch, size);
    }

    static inline void *MemCpy(void *dest, const void *src, SizeType size) 
    {
        return std::memcpy(dest, src, size);
    }

    static inline void *MemMove(void *dest, const void *src, SizeType size)
    {
        return std::memmove(dest, src, size);
    }

    static inline void *Clear(void *dest, SizeType size)
    {
        return std::memset(dest, 0, size);
    }

    static void Garble(void *dest, SizeType length)
    {
        std::memset(dest, 0xDEAD, length);
    }

    template <class T, class ...Args>
    [[nodiscard]] static void *New(Args &&... args)
    {
        return new T(std::forward<Args>(args)...);
    }

    template <class T>
    static void Delete(void *ptr)
    {
        delete static_cast<T *>(ptr);
    }

    template <class T, class ...Args>
    static void Construct(void *where, Args &&... args)
    {
        new (where) T(std::forward<Args>(args)...);
    }

    template <class T, class ...Args>
    [[nodiscard]] static void *AllocateAndConstruct(Args &&... args)
    {
        void *ptr = std::malloc(sizeof(T));
        new (ptr) T(std::forward<Args>(args)...);
        return ptr;
    }

    template <class T>
    static std::enable_if_t<std::is_trivially_destructible_v<T>> Destruct(T &) { /* Do nothing */ }

    template <class T>
    static std::enable_if_t<!std::is_trivially_destructible_v<T>> Destruct(T &object)
    {
        object.~T();
#ifdef HYP_DEBUG_MODE
        Memory::Garble(&object, sizeof(T));
#endif
    }

    template <class T>
    static std::enable_if_t<std::is_trivially_destructible_v<T>> Destruct(void *) { /* Do nothing */ }

    template <class T>
    static std::enable_if_t<!std::is_trivially_destructible_v<T>> Destruct(void *ptr)
    {
        static_cast<T *>(ptr)->~T();

#ifdef HYP_DEBUG_MODE
        Memory::Garble(ptr, sizeof(T));
#endif
    }

    template <class T>
    static typename std::enable_if_t<!std::is_same_v<void *, std::add_pointer_t<T>>, void>
    DestructAndFree(void *ptr)
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
        { 
            static_cast<T *>(ptr)->~T();
        }

#ifdef HYP_DEBUG_MODE
        Memory::Garble(ptr, sizeof(T));
#endif

        std::free(ptr);
    }

    template <class T>
    static typename std::enable_if_t<std::is_same_v<void *, std::add_pointer_t<T>>, void>
    DestructAndFree(void *ptr)
    {
        std::free(ptr);
    }

    /*! \brief No operation function for deleting a trivially destructible object. */
    static void NoOp(void *) { /* Do nothing */ }

    static void Free(void *ptr)
    {
        std::free(ptr);
    }

    static void *AllocateZeros(SizeType count)
    {
        return std::calloc(count, 1);
    }

    static void *Allocate(SizeType count)
    {
        return std::malloc(count);
    }
};

} // namespace memory

using memory::Memory;

} // namespace hyperion

#define StackAlloc(size) alloca(size)

#endif