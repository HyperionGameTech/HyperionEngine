/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

namespace Hyperion {

enum class ChunkId : uint32;

struct BlobDesc
{
    ChunkId chunkId;
    SizeType offset;
    SizeType size;
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
