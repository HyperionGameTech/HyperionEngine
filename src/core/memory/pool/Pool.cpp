#include <core/memory/pool/Pool.hpp>

#include <core/threading/Spinlock.hpp>
#include <core/threading/Threads.hpp>

namespace Hyperion {
namespace memory {

#pragma region Block

Pool::Block::Block(SizeType capacity)
{
    buffer.SetSize(capacity);
    allocator.AddPool(buffer.Data(), buffer.GetCapacity());
}

void* Pool::Block::Allocate(SizeType size, SizeType alignment)
{
    return allocator.Allocate(size, alignment);
}

void Pool::Block::Free(void* ptr)
{
    if (ptr == nullptr)
    {
        return;
    }

    allocator.Free(ptr);
}

#pragma endregion Block

Pool::~Pool()
{
    m_blocks.Clear();
}

HYP_NODISCARD void* Pool::Allocate(SizeType size, SizeType alignment)
{
    Spinlock<MPMC> lock(&m_lockState);
    if (m_flags & PF_THREAD_SAFE)
    {
        lock.Lock();
    }
    else if (m_ownerThreadId.IsValid())
    {
        AssertOnThread(m_ownerThreadId, "Pool allocation from wrong thread!");
    }

    for (auto& block : m_blocks)
    {
        void* p = block.Allocate(size, alignment);
        if (p != nullptr)
        {
            if (m_flags & PF_THREAD_SAFE)
            {
                lock.Unlock();
            }

            return p;
        }
    }

    m_blocks.EmplaceBack(m_blockSize);

    Block& newBlock = m_blocks.Back();

    void* p = newBlock.Allocate(size, alignment);

    if (HYP_UNLIKELY(p == nullptr))
    {
        HYP_FAIL("Failed to allocate from new block!");
    }

    if (m_flags & PF_THREAD_SAFE)
    {
        lock.Unlock();
    }

    return p;
}

void Pool::Free(void* ptr)
{
    if (!ptr)
    {
        return;
    }

    Spinlock<MPMC> lock(&m_lockState);
    if (m_flags & PF_THREAD_SAFE)
    {
        lock.Lock();
    }
    else if (m_ownerThreadId.IsValid())
    {
        AssertOnThread(m_ownerThreadId, "Freeing from wrong thread!");
    }

    for (auto& block : m_blocks)
    {
        ubyte* base = reinterpret_cast<ubyte*>(block.buffer.Data());
        ubyte* bptr = reinterpret_cast<ubyte*>(ptr);
        if (bptr > base && bptr <= base + block.buffer.GetCapacity())
        {
            block.Free(ptr);

            if (m_flags & PF_THREAD_SAFE)
            {
                lock.Unlock();
            }

            return;
        }
    }

    // not found
    HYP_FAIL("Pointer not found in any pool block!");

    if (m_flags & PF_THREAD_SAFE)
    {
        lock.Unlock();
    }
}

void Pool::Reset()
{
    Spinlock<MPMC> lock(&m_lockState);
    if (m_flags & PF_THREAD_SAFE)
    {
        lock.Lock();
    }
    else if (m_ownerThreadId.IsValid())
    {
        AssertOnThread(m_ownerThreadId, "Pool reset from wrong thread!");
    }

    m_blocks.Clear();

    if (m_flags & PF_THREAD_SAFE)
    {
        lock.Unlock();
    }
}

MemoryMetrics Pool::GetMemoryMetrics() const
{
    Spinlock<MPMC> lock(&m_lockState);
    if (m_flags & PF_THREAD_SAFE)
    {
        lock.Lock();
    }

    MemoryMetrics metrics;

    for (const auto& block : m_blocks)
    {
        const SizeType blockCapacity = block.buffer.GetCapacity();

        // With TLSF allocator, we can get accurate statistics from the allocator itself
        MemoryMetrics blockMetrics = block.allocator.GetMemoryMetrics();
        metrics += blockMetrics;
    }

    if (m_flags & PF_THREAD_SAFE)
    {
        lock.Unlock();
    }

    return metrics;
}

} // namespace memory
} // namespace Hyperion