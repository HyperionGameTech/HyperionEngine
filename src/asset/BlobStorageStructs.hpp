/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

namespace Hyperion {

struct BlobHeader
{
    uint8 magic[4];
    uint32 version : 8;
    uint32 payloadOffset : 24;
    uint64 payloadSize;
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
