#include <Core/Memory/Pool/Pool.hpp>

#include <Core/Threading/Threads.hpp>

namespace Hyperion {
namespace memory {

#pragma region Block

Pool::Block::Block(size_t capacity)
{
    memory = Memory::AllocateAligned(capacity, alignof(std::max_align_t));
    Assert(memory != nullptr, "Failed to allocate {} bytes of memory from the system", capacity);
}

Pool::Block::~Block()
{
    Memory::FreeAligned(memory);
}

#pragma endregion Block

Pool::~Pool()
{
    m_blocks.Clear();
}

HYP_NODISCARD void* Pool::Allocate(size_t size, size_t alignment)
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
        m_tlsf.AddPool(newBlock.memory, m_blockSize);

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
        m_tlsf.RemovePool(block.memory);
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
