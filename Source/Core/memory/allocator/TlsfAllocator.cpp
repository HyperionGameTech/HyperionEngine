/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/memory/allocator/TlsfAllocator.hpp>
#include <Core/memory/Memory.hpp>

#include <Core/utilities/ByteUtil.hpp>

#include <tlsf/tlsf.h>

namespace Hyperion {
namespace memory {

TlsfAllocator::TlsfAllocator()
    : m_tlsf(nullptr),
      m_mem(nullptr)
{
    const size_t poolSize = tlsf_size();

    m_mem = std::malloc(tlsf_size());
    HYP_CORE_ASSERT(m_mem != nullptr, "Failed to allocate memory for TLSF allocator");

    m_tlsf = tlsf_create(m_mem);
    HYP_CORE_ASSERT(m_tlsf != nullptr, "Failed to create TLSF allocator");
}

TlsfAllocator::~TlsfAllocator()
{
    if (m_tlsf != nullptr)
    {
        tlsf_destroy((tlsf_t)m_tlsf);
        m_tlsf = nullptr;
    }

    if (m_mem != nullptr)
    {
        std::free(m_mem);
        m_mem = nullptr;
    }
}

void TlsfAllocator::AddPool(void* memory, size_t bytes)
{
    AssertDebug(m_tlsf != nullptr);
    AssertDebug(bytes >= tlsf_pool_overhead() + tlsf_block_size_min());

    void* pool = tlsf_add_pool((tlsf_t)m_tlsf, memory, bytes);

    if (pool)
    {
        PoolInfo info;
        info.pool = pool;
        info.memory = memory;
        info.size = bytes;
        m_pools.PushBack(info);
    }
}

void TlsfAllocator::RemovePool(void* memory)
{
    // Find and remove the pool from tracking
    for (size_t i = 0; i < m_pools.Size(); ++i)
    {
        if (m_pools[i].memory == memory)
        {
            tlsf_remove_pool((tlsf_t)m_tlsf, m_pools[i].pool);
            m_pools.EraseAt(i);
            return;
        }
    }
}

void* TlsfAllocator::Allocate(size_t bytes, size_t alignment)
{
    if (HYP_UNLIKELY(m_tlsf == nullptr))
    {
        HYP_FAIL("Attempting to allocate memory on destroyed TLSF allocator");
    }

    AssertDebug(m_tlsf != nullptr);
    AssertDebug(bytes > 0 && alignment > 0 && (alignment & (alignment - 1)) == 0); // power of two

    return tlsf_memalign((tlsf_t)m_tlsf, alignment, bytes);
}

void* TlsfAllocator::Reallocate(void* ptr, size_t newSize, size_t alignment)
{
    if (HYP_UNLIKELY(m_tlsf == nullptr))
    {
        HYP_FAIL("Attempting to reallocate memory on destroyed TLSF allocator");
    }

    AssertDebug(m_tlsf != nullptr);
    AssertDebug(ptr != nullptr);

    return tlsf_realloc((tlsf_t)m_tlsf, ptr, newSize);
}

void TlsfAllocator::Free(void* ptr)
{
    if (HYP_UNLIKELY(m_tlsf == nullptr))
    {
        HYP_FAIL("Attempting to free memory from destroyed TLSF allocator");
    }

    AssertDebug(m_tlsf != nullptr);
    AssertDebug(ptr != nullptr);

    tlsf_free((tlsf_t)m_tlsf, ptr);
}

MemoryMetrics TlsfAllocator::GetMemoryMetrics() const
{
    MemoryMetrics metrics = {};

    struct WalkerContext
    {
        MemoryMetrics* metrics;
    };

    auto walker = [](void* ptr, size_t size, int used, void* user)
    {
        WalkerContext* context = static_cast<WalkerContext*>(user);

        if (used)
        {
            // Used blocks - size includes the block header, subtract it for actual usable bytes
            const size_t usableSize = size > tlsf_alloc_overhead() ? size - tlsf_alloc_overhead() : size;
            context->metrics->values[MemoryMetrics::MM_BYTES_USED] += usableSize;
            ++context->metrics->values[MemoryMetrics::MM_ALLOCATIONS_ACTIVE];
        }
        else
        {
            // Free blocks
            context->metrics->values[MemoryMetrics::MM_BYTES_FREE] += size;
        }
    };

    WalkerContext context;
    context.metrics = &metrics;

    for (const PoolInfo& poolInfo : m_pools)
    {
        metrics[MemoryMetrics::MM_BYTES_COMMITTED] += poolInfo.size;
        ++metrics[MemoryMetrics::MM_BLOCKS_TOTAL];

        tlsf_walk_pool(poolInfo.pool, walker, &context);
    }

    // MM_BYTES_PEAK is not tracked in this implementation
    metrics[MemoryMetrics::MM_BYTES_PEAK] = 0;

    return metrics;
}

} // namespace memory
} // namespace Hyperion