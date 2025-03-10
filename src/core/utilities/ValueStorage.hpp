/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_VALUE_STORAGE_HPP
#define HYPERION_VALUE_STORAGE_HPP

#include <core/memory/Memory.hpp>

#include <Constants.hpp>
#include <Types.hpp>

#include <type_traits>

namespace hyperion {
namespace utilities {

namespace detail {

template <class T, class T2 = void>
struct ValueStorageAlignment;

template <class T>
struct ValueStorageAlignment<T, typename std::enable_if_t<implementation_exists<T>>>
{
    static constexpr SizeType value = alignof(std::conditional_t<std::is_void_v<T>, ubyte, T>);
};

template <class T>
struct ValueStorageAlignment<T, typename std::enable_if_t<!implementation_exists<T>>>
{
    static constexpr SizeType value = 1;
};

} // namespace detail

template <class T, SizeType Alignment = detail::ValueStorageAlignment<T>::value>
struct alignas(Alignment) ValueStorage
{
    static constexpr SizeType alignment = Alignment;

    alignas(Alignment) ubyte data_buffer[sizeof(T)];

    ValueStorage()                                          = default;

    template <class OtherType>
    explicit ValueStorage(const ValueStorage<OtherType> &other)
    {
        static_assert(std::is_standard_layout_v<OtherType>, "OtherType must be standard layout");
        static_assert(std::is_standard_layout_v<T>, "T must be standard layout");
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

        static_assert(sizeof(T) == sizeof(OtherType), "sizeof must match for both T and OtherType");

        // Should alignof be checked?

        Memory::MemCpy(data_buffer, other.data_buffer, sizeof(T));
    }

    template <class OtherType>
    explicit ValueStorage(const OtherType *ptr)
    {
        static_assert(std::is_standard_layout_v<OtherType>, "OtherType must be standard layout");
        static_assert(std::is_standard_layout_v<T>, "T must be standard layout");
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

        static_assert(sizeof(T) == sizeof(OtherType), "sizeof must match for both T and OtherType");

        Memory::MemCpy(data_buffer, ptr, sizeof(T));
    }

    ValueStorage(const ValueStorage &other)                 = default;
    ValueStorage &operator=(const ValueStorage &other)      = default;
    ValueStorage(ValueStorage &&other) noexcept             = default;
    ValueStorage &operator=(ValueStorage &&other) noexcept  = default;
    ~ValueStorage()                                         = default;

    template <class... Args>
    HYP_FORCE_INLINE T *Construct(Args &&... args)
    {
        Memory::Construct<T>(data_buffer, std::forward<Args>(args)...);

        return &Get();
    }
    
    HYP_FORCE_INLINE void Destruct()
        { Memory::Destruct<T>(static_cast<void *>(data_buffer)); }
    
    HYP_FORCE_INLINE T &Get()
        { return *reinterpret_cast<T *>(&data_buffer); }

    HYP_FORCE_INLINE const T &Get() const
        { return *reinterpret_cast<const T *>(&data_buffer); }
    
    HYP_FORCE_INLINE void *GetPointer()
        { return &data_buffer[0]; }
    
    HYP_FORCE_INLINE const void *GetPointer() const
        { return &data_buffer[0]; }
};

template <>
struct ValueStorage<void>
{
    ValueStorage()                                      = default;
    ValueStorage(const ValueStorage &)                  = default;
    ValueStorage &operator=(const ValueStorage &)       = default;
    ValueStorage(ValueStorage &&) noexcept              = default;
    ValueStorage &operator=(ValueStorage &&) noexcept   = default;
    ~ValueStorage()                                     = default;
};

template <class T, SizeType Count, SizeType Alignment = detail::ValueStorageAlignment<T>::value, typename T2 = void>
struct ValueStorageArray;

template <class T, SizeType Count, SizeType Alignment>
struct ValueStorageArray<T, Count, Alignment, typename std::enable_if_t<implementation_exists<T> && Count != 0>>
{
    static constexpr SizeType alignment = detail::ValueStorageAlignment<T>::value;

    alignas(alignment) ValueStorage<T>  data[Count];
    
    HYP_FORCE_INLINE ValueStorage<T, alignment> &operator[](SizeType index)
        { return data[index]; }
    
    HYP_FORCE_INLINE const ValueStorage<T, alignment> &operator[](SizeType index) const
        { return data[index]; }
    
    HYP_FORCE_INLINE T *GetPointer()
        { return reinterpret_cast<T *>(&data[0]); }
    
    HYP_FORCE_INLINE const T *GetPointer() const
        { return reinterpret_cast<const T *>(&data[0]); }
    
    HYP_FORCE_INLINE void *GetRawPointer()
        { return static_cast<void *>(&data[0]); }
    
    HYP_FORCE_INLINE const void *GetRawPointer() const
        { return static_cast<const void *>(&data[0]); }
    
    HYP_FORCE_INLINE constexpr SizeType Size() const
        { return Count; }

    HYP_FORCE_INLINE constexpr SizeType TotalSize() const
        { return Count * sizeof(T); }
};

template <class T, SizeType Alignment>
struct ValueStorageArray<T, 0, Alignment, void>
{
    ValueStorage<char>  data[1];
    
    HYP_FORCE_INLINE void *GetRawPointer()
        { return static_cast<void *>(&data[0]); }
    
    HYP_FORCE_INLINE const void *GetRawPointer() const
        { return static_cast<const void *>(&data[0]); }
    
    HYP_FORCE_INLINE constexpr SizeType Size() const
        { return 0; }
    
    HYP_FORCE_INLINE constexpr SizeType TotalSize() const
        { return 0; }
};

} // namespace utilities

using utilities::ValueStorage;
using utilities::ValueStorageArray;

} // namespace hyperion

#endif
