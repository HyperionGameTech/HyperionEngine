#include <core/memory/pool/Pool.hpp>

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
    if (m_flags & PF_THREAD_SAFE)
    {
        m_atomicFlag.Acquire();
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
                m_atomicFlag.Release();
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
        m_atomicFlag.Release();
    }

    return p;
}

void Pool::Free(void* ptr)
{
    if (!ptr)
    {
        return;
    }

    if (m_flags & PF_THREAD_SAFE)
    {
        m_atomicFlag.Acquire();
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
                m_atomicFlag.Release();
            }

            return;
        }
    }

    // not found
    HYP_FAIL("Pointer not found in any pool block!");

    if (m_flags & PF_THREAD_SAFE)
    {
        m_atomicFlag.Release();
    }
}

void Pool::Reset()
{
    if (m_flags & PF_THREAD_SAFE)
    {
        m_atomicFlag.Acquire();
    }
    else if (m_ownerThreadId.IsValid())
    {
        AssertOnThread(m_ownerThreadId, "Pool reset from wrong thread!");
    }

    m_blocks.Clear();

    if (m_flags & PF_THREAD_SAFE)
    {
        m_atomicFlag.Release();
    }
}

MemoryMetrics Pool::GetMemoryMetrics() const
{
    if (m_flags & PF_THREAD_SAFE)
    {
        m_atomicFlag.Acquire();
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
        m_atomicFlag.Release();
    }

    return metrics;
}

} // namespace memory
} // namespace Hyperion