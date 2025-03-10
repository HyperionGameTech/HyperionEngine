/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_USER_DATA_HPP
#define HYPERION_USER_DATA_HPP

#include <core/utilities/ValueStorage.hpp>

#include <Types.hpp>

namespace hyperion {
namespace utilities {

template <SizeType Size, SizeType Alignment = alignof(ubyte)>
struct alignas(Alignment) UserData
{
    ValueStorageArray<ubyte, Size, Alignment>   data;

    UserData()
    {
        // zero out the memory
        Memory::MemSet(data.GetRawPointer(), 0, Size);
    }

    UserData(const UserData &)                  = default;
    UserData &operator=(const UserData &)       = default;

    template <SizeType OtherSize, SizeType OtherAlignment>
    UserData(const UserData<OtherSize, OtherAlignment> &other)
    {
        static_assert(Size >= OtherSize, "Size must be greater than or equal to OtherSize");

        Memory::MemCpy(data.GetRawPointer(), other.data.GetRawPointer(), OtherSize);
    }

    template <SizeType OtherSize, SizeType OtherAlignment>
    UserData &operator=(const UserData<OtherSize, OtherAlignment> &other)
    {
        static_assert(Size >= OtherSize, "Size must be greater than or equal to OtherSize");

        Memory::MemCpy(data.GetRawPointer(), other.data.GetRawPointer(), OtherSize);

        return *this;
    }

    UserData(UserData &&) noexcept              = default;
    UserData &operator=(UserData &&) noexcept   = default;

    template <SizeType OtherSize, SizeType OtherAlignment>
    UserData(UserData<OtherSize, OtherAlignment> &&other) noexcept
    {
        static_assert(Size >= OtherSize, "Size must be greater than or equal to OtherSize");

        Memory::MemCpy(data.GetRawPointer(), other.data.GetRawPointer(), OtherSize);
    }

    template <SizeType OtherSize, SizeType OtherAlignment>
    UserData &operator=(UserData<OtherSize, OtherAlignment> &&other) noexcept
    {
        static_assert(Size >= OtherSize, "Size must be greater than or equal to OtherSize");

        Memory::MemCpy(data.GetRawPointer(), other.data.GetRawPointer(), OtherSize);

        return *this;
    }

    ~UserData()                                 = default;

    HYP_FORCE_INLINE bool operator==(const UserData &other) const
        { return Memory::MemCmp(data.GetRawPointer(), other.data.GetRawPointer(), Size) == 0; }

    HYP_FORCE_INLINE bool operator!=(const UserData &other) const
        { return Memory::MemCmp(data.GetRawPointer(), other.data.GetRawPointer(), Size) != 0; }

    template <class T>
    HYP_FORCE_INLINE void Set(const T &value)
    {
        static_assert(std::is_standard_layout_v<T>, "T must be standard layout");
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        static_assert(std::is_trivially_destructible_v<T>, "T must be trivially destructible");

        static_assert(sizeof(T) <= Size, "Size of T must be less than or equal to Size");

        Memory::MemCpy(data.GetRawPointer(), &value, sizeof(T));
    }

    template <class T>
    HYP_FORCE_INLINE T &ReinterpretAs()
    {
        static_assert(std::is_standard_layout_v<T>, "T must be standard layout");
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        static_assert(std::is_trivially_destructible_v<T>, "T must be trivially destructible");

        static_assert(sizeof(T) <= Size, "Size of T must be less than or equal to Size");

        // enforce Alignment to prevent undefined behavior
        static_assert(Alignment >= alignof(T), "Alignment must be greater than or equal to alignof(T)");

        return *reinterpret_cast<T *>(data.GetRawPointer());
    }

    template <class T>
    HYP_FORCE_INLINE const T &ReinterpretAs() const
        { return const_cast<UserData *>(this)->ReinterpretAs<T>(); }

    /*! \brief Constructs a new UserData of this type from a pointer to a raw byte buffer.
     *  Proper care must be taken when using this method. You must ensure that the byte buffer matches
     *  in Size or else a read access violation will occur.
     *  Also, proper care must be taken to ensure type safety. For instance, don't marshal in an object of
     *  any struct/class type that is non-trival or non-standard layout. */
    HYP_FORCE_INLINE static UserData<Size, Alignment> InternFromBytes(const ubyte *bytes)
    {
        UserData<Size, Alignment> result;
        Memory::MemCpy(result.data.GetRawPointer(), bytes, Size);
        return result;
    }
};
} // namespace utilities

template <SizeType Size, SizeType Alignment = alignof(ubyte)>
using UserData = utilities::UserData<Size, Alignment>;

} // namespace hyperion

#endif
