/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_VALUE_STORAGE_HPP
#define HYPERION_VALUE_STORAGE_HPP

#include <core/memory/Memory.hpp>

#include <Constants.hpp>
#include <Types.hpp>

#include <type_traits>

namespace hyperion {
namespace utilities {

template <class T>
struct ValueStorageAlignment;

template <class T>
struct ValueStorageAlignment
{
    static constexpr SizeType value = alignof(T);
};

template <>
struct ValueStorageAlignment<void>
{
    static constexpr SizeType value = 1;
};

template <class T, SizeType Alignment = ValueStorageAlignment<T>::value>
struct alignas(Alignment) ValueStorage
{
    static constexpr SizeType alignment = Alignment;

    alignas(Alignment) ubyte data_buffer[sizeof(T)];

    ValueStorage() = default;

    template <class OtherType>
    explicit ValueStorage(const ValueStorage<OtherType>& other) = delete;

    template <class OtherType>
    explicit ValueStorage(const OtherType* ptr) = delete;

    ValueStorage(const ValueStorage& other) = default;
    ValueStorage& operator=(const ValueStorage& other) = default;
    ValueStorage(ValueStorage&& other) noexcept = default;
    ValueStorage& operator=(ValueStorage&& other) noexcept = default;
    ~ValueStorage() = default;

    template <class... Args>
    HYP_FORCE_INLINE T* Construct(Args&&... args)
    {
        T* address = GetPointer();
        new (address) T(std::forward<Args>(args)...);

        return address;
    }

    HYP_FORCE_INLINE void Destruct()
    {
        Get().~T();
    }

    HYP_FORCE_INLINE T& Get() &
    {
        return *reinterpret_cast<T*>(&data_buffer[0]);
    }

    HYP_FORCE_INLINE const T& Get() const&
    {
        return *reinterpret_cast<const T*>(&data_buffer[0]);
    }

    HYP_FORCE_INLINE T* GetPointer() &
    {
        return reinterpret_cast<T*>(&data_buffer[0]);
    }

    HYP_FORCE_INLINE const T* GetPointer() const&
    {
        return reinterpret_cast<const T*>(&data_buffer[0]);
    }
};

template <>
struct ValueStorage<void>
{
};

template <class T, SizeType Count, SizeType Alignment = ValueStorageAlignment<T>::value, typename T2 = void>
struct ValueStorageArray;

template <class T, SizeType Count, SizeType Alignment>
struct ValueStorageArray<T, Count, Alignment, typename std::enable_if_t<Count != 0>>
{
    static constexpr SizeType alignment = Alignment;

    alignas(Alignment) ubyte data_buffer[sizeof(T) * Count];

    template <class... Args>
    HYP_FORCE_INLINE T* ConstructElement(SizeType index, Args&&... args)
    {
        T* address = GetPointer() + index;
        new (address) T(std::forward<Args>(args)...);

        return address;
    }

    HYP_FORCE_INLINE void DestructElement(SizeType index)
    {
        GetPointer()[index].~T();
    }

    HYP_FORCE_INLINE T* GetPointer() &
    {
        return reinterpret_cast<T*>(&data_buffer[0]);
    }

    HYP_FORCE_INLINE const T* GetPointer() const&
    {
        return reinterpret_cast<const T*>(&data_buffer[0]);
    }

    HYP_DEPRECATED HYP_FORCE_INLINE void* GetRawPointer()
    {
        return static_cast<void*>(&data_buffer[0]);
    }

    HYP_DEPRECATED HYP_FORCE_INLINE const void* GetRawPointer() const
    {
        return static_cast<const void*>(&data_buffer[0]);
    }

    HYP_FORCE_INLINE constexpr SizeType Size() const
    {
        return Count;
    }

    HYP_FORCE_INLINE constexpr SizeType TotalSize() const
    {
        return Count * sizeof(T);
    }
};

template <class T, SizeType Alignment>
struct ValueStorageArray<T, 0, Alignment, void>
{
    ValueStorage<char> data[1];

    HYP_FORCE_INLINE void* GetRawPointer()
    {
        return static_cast<void*>(&data[0]);
    }

    HYP_FORCE_INLINE const void* GetRawPointer() const
    {
        return static_cast<const void*>(&data[0]);
    }

    HYP_FORCE_INLINE constexpr SizeType Size() const
    {
        return 0;
    }

    HYP_FORCE_INLINE constexpr SizeType TotalSize() const
    {
        return 0;
    }
};

} // namespace utilities

using utilities::ValueStorage;
using utilities::ValueStorageArray;

} // namespace hyperion

#endif
