/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

namespace Hyperion {

HYP_STRUCT()
struct BlobResourceKey
{
    HYP_STRUCT_BODY(BlobResourceKey)

    HYP_FIELD()
    uint64 offset = 0;

    HYP_FIELD()
    uint64 size = 0;

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

HYP_STRUCT()
struct BlobDataReference
{
    HYP_STRUCT_BODY(BlobDataReference)

    HYP_FIELD()
    uint64 bufferOffset = 0;
    
    HYP_FIELD()
    uint64 size = 0;
    
    HYP_FIELD(Transient)
    void* raw = nullptr;
    
    HYP_FIELD(Transient)
    bool readOnly = false;
};

} // namespace Hyperion
