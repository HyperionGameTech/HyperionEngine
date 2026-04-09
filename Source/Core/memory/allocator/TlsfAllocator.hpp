/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/containers/Array.hpp>
#include <Core/memory/MemoryMetrics.hpp>

namespace Hyperion {
namespace memory {

class HYP_API TlsfAllocator
{
public:
    TlsfAllocator();
    ~TlsfAllocator();

    void AddPool(void* memory, size_t bytes);
    void RemovePool(void* memory);

    void* Allocate(size_t bytes, size_t alignment = 16);
    void* Reallocate(void* ptr, size_t newSize, size_t alignment = 16);
    void Free(void* ptr);

    MemoryMetrics GetMemoryMetrics() const;

private:
    struct PoolInfo
    {
        void* pool;   // pool_t handle from tlsf_add_pool
        void* memory; // base memory pointer
        size_t size;  // total pool size
    };

    void* m_tlsf;
    void* m_mem;
    Array<PoolInfo> m_pools;
};

} // namespace memory

using memory::TlsfAllocator;

} // namespace Hyperion
