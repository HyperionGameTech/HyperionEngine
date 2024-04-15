/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_LIB_VALUE_STORAGE_HPP
#define HYPERION_V2_LIB_VALUE_STORAGE_HPP

#include <core/lib/Memory.hpp>
#include <Types.hpp>

#include <type_traits>

namespace hyperion {

template <class T, uint alignment = alignof(T)>
struct alignas(alignment) ValueStorage
{
    alignas(alignment) ubyte data_buffer[sizeof(T)];

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

        // Should alignof be checked?

        Memory::MemCpy(data_buffer, ptr, sizeof(T));
    }

    ValueStorage(const ValueStorage &other)                 = default;
    ValueStorage &operator=(const ValueStorage &other)      = default;
    ValueStorage(ValueStorage &&other) noexcept             = default;
    ValueStorage &operator=(ValueStorage &&other) noexcept  = default;
    ~ValueStorage()                                         = default;

    template <class ...Args>
    HYP_FORCE_INLINE
    T *Construct(Args &&... args)
    {
        Memory::Construct<T>(data_buffer, std::forward<Args>(args)...);

        return &Get();
    }
    
    HYP_FORCE_INLINE
    void Destruct()
        { Memory::Destruct<T>(static_cast<void *>(data_buffer)); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    T &Get()
        { return *reinterpret_cast<T *>(&data_buffer); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const T &Get() const
        { return *reinterpret_cast<const T *>(&data_buffer); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    void *GetPointer()
        { return &data_buffer[0]; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    const void *GetPointer() const
        { return &data_buffer[0]; }
};

struct alignas(16) Tmp { int x; float y; void *stuff[16]; };
static_assert(sizeof(ValueStorage<Tmp>) == sizeof(Tmp));
static_assert(alignof(ValueStorage<Tmp>) == alignof(Tmp));

template <class T, SizeType Sz>
struct ValueStorageArray
{
    ValueStorage<T> data[Sz];
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    ValueStorage<T> &operator[](SizeType index)
        { return data[index]; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    const ValueStorage<T> &operator[](SizeType index) const
        { return data[index]; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    T *GetPointer()
        { return static_cast<T *>(&data[0]); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    const T *GetPointer() const
        { return static_cast<T *>(&data[0]); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    void *GetRawPointer()
        { return static_cast<void *>(&data[0]); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    const void *GetRawPointer() const
        { return static_cast<const void *>(&data[0]); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr SizeType Size() const
        { return Sz; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr SizeType TotalSize() const
        { return Sz * sizeof(T); }
};

template <class T>
struct ValueStorageArray<T, 0>
{
    ValueStorage<char> data[1];
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    void *GetRawPointer()
        { return static_cast<void *>(&data[0]); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    const void *GetRawPointer() const
        { return static_cast<const void *>(&data[0]); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr SizeType Size() const
        { return 0; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr SizeType TotalSize() const
        { return 0; }
};

static_assert(sizeof(ValueStorageArray<int, 200>) == sizeof(int) * 200);
static_assert(sizeof(ValueStorageArray<int, 0>) == 1);

} // namespace hyperion

#endif
