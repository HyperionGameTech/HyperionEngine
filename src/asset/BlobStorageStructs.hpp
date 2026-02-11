/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

namespace Hyperion {

struct BlobResourceKey
{
    SizeType offset = 0;
    SizeType size = 0;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return size != 0;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return size == 0;
    }

    HYP_FORCE_INLINE constexpr bool operator==(const BlobResourceKey& other) const
    {
        return offset == other.offset
            && size == other.size;
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(offset)
            .Combine(size);
    }
};

struct BlobHeader
{
    uint8 magic[4];
    uint32 version : 8;
    uint32 payloadOffset : 24;
    uint64 payloadSize;
};

HYP_STRUCT()
struct BlobMappingRange
{
    HYP_STRUCT_BODY(BlobMappingRange)

    HYP_FIELD()
    uint64 start = 0;
    
    HYP_FIELD()
    uint64 end = 0;
};

/*! \brief Wrapper around a pointer that is stored in a blob */
template <class T>
struct BlobPointer
{
    int32 offset;

    BlobPointer() : offset(0) {}

    explicit BlobPointer(int32 offset)
        : offset(offset)
    {
    }
    
    T* Get() const
    {
        UIntPtr basePtr = reinterpret_cast<UIntPtr>(this);
        UIntPtr targetPtr = basePtr + offset;

        return reinterpret_cast<T*>(targetPtr);
    }

    bool operator!() const
    {
        return offset == 0;
    }

    T* operator->() const
    {
        AssertDebug(offset != 0);
        return Get();
    }

    T& operator*() const
    {
        AssertDebug(offset != 0);
        return *Get();
    }

    T& operator[](SizeType index) const
    {
        AssertDebug(offset != 0);
        return Get()[index];
    }

    operator T*()
    {
        if (!(*this))
            return nullptr;

        return Get();
    }

    operator const T* () const
    {
        if (!(*this))
            return nullptr;

        return Get();
    }
};

template <class T>
concept BlobSerializable = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>
    && requires {
        { T::Version } -> std::convertible_to<uint8>;
        { T::Header } -> std::convertible_to<const char*>;
    };

} // namespace Hyperion
