/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

namespace Hyperion {

enum class ChunkId : uint32;

struct BlobResourceKey
{
    ChunkId chunkId;
    SizeType offset;
    SizeType size;

    HYP_FORCE_INLINE constexpr bool operator==(const BlobResourceKey& other) const
    {
        return chunkId == other.chunkId
            && offset == other.offset
            && size == other.size;
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(chunkId)
            .Combine(offset)
            .Combine(size);
    }
};

/*! \brief Wrapper around a pointer that is stored in a blob */
template <class T>
struct BlobPointer
{
    ptrdiff_t offset;

    BlobPointer() : offset(0) {}

    explicit BlobPointer(ptrdiff_t offset)
        : offset(offset)
    {
    }
    
    T* Get() const
    {
        UIntPtr basePtr = reinterpret_cast<UIntPtr>(this);
        UIntPtr targetPtr = basePtr + offset;

        return reinterpret_cast<T*>(targetPtr);
    }

    T* operator->() const
    {
        return Get();
    }

    T& operator*() const
    {
        return *Get();
    }

    T& operator[](SizeType index) const
    {
        return Get()[index];
    }
};

} // namespace Hyperion
