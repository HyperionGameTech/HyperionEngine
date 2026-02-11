/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/resource/Resource.hpp>

#include <core/io/MemoryMappedFile.hpp>

#include <asset/BlobStorageStructs.hpp>

namespace Hyperion {

class MemoryMappedFileView;

struct BlobAllocation
{
    BlobResourceKey key;
    void* address;
};

} // namespace Hyperion
