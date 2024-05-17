/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_USER_DATA_HPP
#define HYPERION_USER_DATA_HPP

#include <core/utilities/ValueStorage.hpp>

#include <Types.hpp>

namespace hyperion {
namespace utilities {
template <uint size, uint alignment = alignof(ubyte)>
struct alignas(alignment) UserData
{
    ValueStorageArray<ubyte, size>  data;

    // UserData()
    // {
    //     static_assert(size > 0, "Size must be greater than 0");
    //     static_assert(alignment > 0, "Alignment must be greater than 0");

    //     // zero out the memory
    //     Memory::MemSet(data.GetRawPointer(), 0, size);
    // }

    UserData()                                  = default;

    UserData(const UserData &)                  = default;
    UserData &operator=(const UserData &)       = default;

    template <uint other_size, uint other_alignment>
    UserData(const UserData<other_size, other_alignment> &other)
    {
        static_assert(size >= other_size, "Size must be greater than or equal to other_size");

        Memory::MemCpy(data.GetRawPointer(), other.data.GetRawPointer(), other_size);
    }

    template <uint other_size, uint other_alignment>
    UserData &operator=(const UserData<other_size, other_alignment> &other)
    {
        static_assert(size >= other_size, "Size must be greater than or equal to other_size");

        Memory::MemCpy(data.GetRawPointer(), other.data.GetRawPointer(), other_size);

        return *this;
    }

    UserData(UserData &&) noexcept              = default;
    UserData &operator=(UserData &&) noexcept   = default;

    template <uint other_size, uint other_alignment>
    UserData(UserData<other_size, other_alignment> &&other) noexcept
    {
        static_assert(size >= other_size, "Size must be greater than or equal to other_size");

        Memory::MemCpy(data.GetRawPointer(), other.data.GetRawPointer(), other_size);
    }

    template <uint other_size, uint other_alignment>
    UserData &operator=(UserData<other_size, other_alignment> &&other) noexcept
    {
        static_assert(size >= other_size, "Size must be greater than or equal to other_size");

        Memory::MemCpy(data.GetRawPointer(), other.data.GetRawPointer(), other_size);

        return *this;
    }

    ~UserData()                                 = default;

    [[nodiscard]]
    bool operator==(const UserData &other) const
        { return Memory::MemCmp(data.GetRawPointer(), other.data.GetRawPointer(), size) == 0; }

    [[nodiscard]]
    bool operator!=(const UserData &other) const
        { return Memory::MemCmp(data.GetRawPointer(), other.data.GetRawPointer(), size) != 0; }

    template <class T>
    HYP_FORCE_INLINE
    void Set(const T &value)
    {
        static_assert(std::is_standard_layout_v<T>, "T must be standard layout");
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        static_assert(std::is_trivially_destructible_v<T>, "T must be trivially destructible");

        static_assert(sizeof(T) <= size, "Size of T must be less than or equal to size");

        Memory::MemCpy(data.GetRawPointer(), &value, sizeof(T));
    }

    template <class T>
    [[nodiscard]]
    HYP_FORCE_INLINE
    T &ReinterpretAs()
    {
        static_assert(std::is_standard_layout_v<T>, "T must be standard layout");
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        static_assert(std::is_trivially_destructible_v<T>, "T must be trivially destructible");

        static_assert(sizeof(T) <= size, "Size of T must be less than or equal to size");

        // enforce alignment to prevent undefined behavior
        static_assert(alignment >= alignof(T), "Alignment must be greater than or equal to alignof(T)");

        return *reinterpret_cast<T *>(data.GetRawPointer());
    }

    template <class T>
    [[nodiscard]]
    HYP_FORCE_INLINE
    const T &ReinterpretAs() const
        { return const_cast<UserData *>(this)->ReinterpretAs<T>(); }

    /*! \brief Constructs a new UserData of this type from a pointer to a raw byte buffer.
     *  Proper care must be taken when using this method. You must ensure that the byte buffer matches
     *  in size or else a read access violation will occur.
     *  Also, proper care must be taken to ensure type safety. For instance, don't marshal in an object of
     *  any struct/class type that is non-trival or non-standard layout. */
    [[nodiscard]]
    static UserData<size, alignment> InternFromBytes(const ubyte *bytes)
    {
        UserData<size, alignment> result;
        Memory::MemCpy(result.data.GetRawPointer(), bytes, size);
        return result;
    }
};
} // namespace utilities

template <uint size, uint alignment = alignof(ubyte)>
using UserData = utilities::UserData<size, alignment>;

} // namespace hyperion

#endif
