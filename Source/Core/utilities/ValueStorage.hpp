/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/Memory.hpp>

#include <core/Constants.hpp>
#include <core/Types.hpp>

#include <type_traits>

namespace Hyperion {
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

/*! \brief  Provides storage values and arrays of type T, providing methods for manual construction and destruction.
 *  \details This class provides a way to store an array of values of type T in a buffer with a specified alignment.
 *  It allows for explicit construction, destruction, and retrieval of the values stored in the buffer.
 *  The alignment can be specified as a template parameter, defaulting to the alignment of T. */
template <class T, SizeType Count = 1, SizeType Alignment = ValueStorageAlignment<T>::value, typename T2 = void>
struct ValueStorage;

/*! \brief A storage class for values of type T with a specified alignment.
 *  \details This class provides a way to store values of type T in a buffer with a specified alignment.
 *  It allows for explicit construction, destruction, and retrieval of the value stored in the buffer.
 *  The alignment can be specified as a template parameter, defaulting to the alignment of T. */
template <class T, SizeType Alignment>
struct alignas(Alignment) ValueStorage<T, 1, Alignment, std::enable_if_t<!std::is_void_v<T>>>
{
    struct ConstructTag
    {
    };

    static constexpr SizeType alignment = Alignment;

    alignas(Alignment) ubyte dataBuffer[sizeof(T)];

    constexpr ValueStorage() = default;

    template <class... Args>
    constexpr ValueStorage(ConstructTag, Args&&... args)
    {
        new (dataBuffer) T(std::forward<Args>(args)...);
    }

    template <class OtherType, SizeType OtherCount, SizeType OtherAlignment>
    explicit ValueStorage(const ValueStorage<OtherType, OtherCount, OtherAlignment>& other) = delete;

    template <class OtherType>
    explicit ValueStorage(const OtherType* ptr) = delete;

    constexpr ValueStorage(const ValueStorage& other) = default;
    ValueStorage& operator=(const ValueStorage& other) = default;
    constexpr ValueStorage(ValueStorage&& other) noexcept = default;
    ValueStorage& operator=(ValueStorage&& other) noexcept = default;
    ~ValueStorage() = default;

    template <class... Args>
    HYP_FORCE_INLINE constexpr T* Construct(Args&&... args)
    {
        return new (dataBuffer) T(std::forward<Args>(args)...);
    }

    HYP_FORCE_INLINE void Destruct()
    {
        Get().~T();
    }

    HYP_FORCE_INLINE T& Get() &
    {
        return *reinterpret_cast<T*>(&dataBuffer[0]);
    }

    HYP_FORCE_INLINE const T& Get() const&
    {
        return *reinterpret_cast<const T*>(&dataBuffer[0]);
    }

    HYP_FORCE_INLINE T Get() &&
    {
        return std::move(*reinterpret_cast<T*>(&dataBuffer[0]));
    }

    HYP_FORCE_INLINE T* GetPointer() &
    {
        return reinterpret_cast<T*>(&dataBuffer[0]);
    }

    HYP_FORCE_INLINE const T* GetPointer() const&
    {
        return reinterpret_cast<const T*>(&dataBuffer[0]);
    }

    HYP_FORCE_INLINE constexpr SizeType Size() const
    {
        return 1;
    }

    HYP_FORCE_INLINE constexpr SizeType TotalSize() const
    {
        return sizeof(T);
    }
};

// Void type specialization
template <SizeType Count, SizeType Alignment>
struct ValueStorage<void, Count, Alignment>
{
    void* GetPointer() &
    {
        return nullptr;
    }

    const void* GetPointer() const&
    {
        return nullptr;
    }
};

// 0 count specialization
template <class T, SizeType Alignment>
struct ValueStorage<T, 0, Alignment>
{
};

// Array specialization
template <class T, SizeType Count, SizeType Alignment>
struct alignas(Alignment) ValueStorage<T, Count, Alignment, std::enable_if_t<!std::is_void_v<T> && (Count > 1)>>
{
    static constexpr SizeType alignment = Alignment;

    alignas(Alignment) ubyte dataBuffer[sizeof(T) * Count];

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
        return reinterpret_cast<T*>(&dataBuffer[0]);
    }

    HYP_FORCE_INLINE const T* GetPointer() const&
    {
        return reinterpret_cast<const T*>(&dataBuffer[0]);
    }

    HYP_DEPRECATED HYP_FORCE_INLINE void* GetRawPointer()
    {
        return static_cast<void*>(&dataBuffer[0]);
    }

    HYP_DEPRECATED HYP_FORCE_INLINE const void* GetRawPointer() const
    {
        return static_cast<const void*>(&dataBuffer[0]);
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

} // namespace utilities

using utilities::ValueStorage;

} // namespace Hyperion
