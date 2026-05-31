/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <Core/Memory/Pool/Pool.hpp>

namespace Hyperion {
namespace memory {

static constexpr size_t ThreadAllocatorPoolSize = 1024 * 1024 * 10; // 10 MB per thread for thread allocator pool

void InitThreadAllocatorPool(void* allocator)
{
    Pool* pool = static_cast<Pool*>(allocator);
    Assert(pool != nullptr);

    new (pool) Pool(ThreadAllocatorPoolSize, PF_NONE);
}

} // namespace memory
} // namespace Hyperion
