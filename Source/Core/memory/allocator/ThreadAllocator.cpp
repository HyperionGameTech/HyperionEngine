/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/memory/allocator/ThreadAllocator.hpp>

#include <Core/memory/pool/Pool.hpp>

namespace Hyperion {
namespace memory {

static constexpr size_t ThreadAllocatorPoolSize = 1024 * 1024 * 10; // 10 MB per thread for thread allocator pool

void InitThreadAllocatorPool(void* allocator)
{
    Pool* pool = static_cast<Pool*>(allocator);

    new (pool) Pool(ThreadAllocatorPoolSize, PF_NONE);
}

} // namespace memory
} // namespace Hyperion