#include <core/memory/pool/Pool.hpp>

#include <core/threading/Threads.hpp>

namespace Hyperion {
namespace memory {

#pragma region Block

Pool::Block::Block(SizeType capacity)
{
    buffer.SetSize(capacity);
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
    
    void* p = m_tlsf.Allocate(size, alignment);

    if (!p)
    {
        // make a new block and hand it to the TLSF
        m_blocks.EmplaceBack(m_blockSize);

        Block& newBlock = m_blocks.Back();
        m_tlsf.AddPool(newBlock.buffer.Data(), newBlock.buffer.GetCapacity());

        p = m_tlsf.Allocate(size, alignment);
        Assert(p != nullptr, "Failed to allocate from newly created memory block! Out of system memory or pool overflow!");
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

    m_tlsf.Free(ptr);

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

    for (Block& block : m_blocks)
    {
        m_tlsf.RemovePool(block.buffer.Data());
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

    MemoryMetrics metrics = m_tlsf.GetMemoryMetrics();

    if (m_flags & PF_THREAD_SAFE)
    {
        m_atomicFlag.Release();
    }

    return metrics;
}

} // namespace memory
} // namespace Hyperion