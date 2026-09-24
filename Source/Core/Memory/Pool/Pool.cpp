#include <Core/Memory/Pool/Pool.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Utilities/ByteUtil.hpp>

#include <Core/Math/MathUtil.hpp>

#ifdef HYP_WINDOWS
#include <Windows.h>
#endif

namespace Hyperion {
namespace memory {

#pragma region Guard pages

#ifdef HYP_WINDOWS

static size_t GetPageSize()
{
    static const size_t pageSize = []()
    {
        SYSTEM_INFO systemInfo;
        GetSystemInfo(&systemInfo);

        return size_t(systemInfo.dwPageSize);
    }();

    return pageSize;
}

static void* AllocateWithGuardPage(size_t size, size_t alignment)
{
    const size_t pageSize = GetPageSize();

    alignment = MathUtil::Max(alignment, size_t(1));
    Assert(alignment <= pageSize, "Guard page allocations cannot be aligned to more than a page ({} > {})", alignment, pageSize);

    const size_t alignedSize = ByteUtil::AlignAs(MathUtil::Max(size, size_t(1)), uint32(alignment));
    const size_t dataSize = ByteUtil::AlignAs(alignedSize, uint32(pageSize));

    ubyte* base = static_cast<ubyte*>(VirtualAlloc(nullptr, dataSize + pageSize, MEM_RESERVE, PAGE_NOACCESS));
    Assert(base != nullptr, "Failed to reserve {} bytes for a guard page allocation", dataSize + pageSize);

    void* committed = VirtualAlloc(base, dataSize, MEM_COMMIT, PAGE_READWRITE);
    Assert(committed != nullptr, "Failed to commit {} bytes for a guard page allocation", dataSize);

    return base + dataSize - alignedSize;
}

static void FreeWithGuardPage(void* ptr)
{
    MEMORY_BASIC_INFORMATION memoryInfo;

    if (VirtualQuery(ptr, &memoryInfo, sizeof(memoryInfo)) == 0)
    {
        return;
    }

    VirtualFree(memoryInfo.AllocationBase, 0, MEM_DECOMMIT);
}

#else

static void* AllocateWithGuardPage(size_t size, size_t alignment)
{
    return Memory::AllocateAligned(size, alignment);
}

static void FreeWithGuardPage(void* ptr)
{
    Memory::FreeAligned(ptr);
}

#endif

#pragma endregion Guard pages

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
    
    void* p = nullptr;

    if (m_flags & PF_DEBUG_GUARD_PAGES)
    {
        p = AllocateWithGuardPage(size, alignment);

        m_fallbackAllocations.Insert(p, size);
    }
    else if ((m_flags & PF_FALLBACK) && size > m_blockSize)
    {
        p = Memory::AllocateAligned(size, alignment);
        Assert(p != nullptr, "Failed to allocate {} bytes from the system allocator (fallback)", size);

        m_fallbackAllocations.Insert(p, size);
    }
    else
    {
        p = m_tlsf.Allocate(size, alignment);

        if (!p)
        {
            // make a new block and hand it to the TLSF
            m_blocks.EmplaceBack(m_blockSize);

            Block& newBlock = m_blocks.Back();
            m_tlsf.AddPool(newBlock.memory, m_blockSize);

            p = m_tlsf.Allocate(size, alignment);

            if (!p)
            {
                if (m_flags & PF_FALLBACK)
                {
                    p = Memory::AllocateAligned(size, alignment);
                    Assert(p != nullptr, "Failed to allocate {} bytes from the system allocator (fallback)", size);

                    m_fallbackAllocations.Insert(p, size);
                }
                else
                {
                    Assert(p != nullptr, "Failed to allocate from newly created memory block! Out of system memory or pool overflow!");
                }
            }
        }
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

    if (m_flags & PF_DEBUG_GUARD_PAGES)
    {
        const bool wasAllocated = m_fallbackAllocations.Erase(ptr);
        Assert(wasAllocated, "Freeing a pointer which was not allocated by this pool, or was already freed");

        FreeWithGuardPage(ptr);
    }
    else if ((m_flags & PF_FALLBACK) && m_fallbackAllocations.Erase(ptr))
    {
        Memory::FreeAligned(ptr);
    }
    else
    {
        m_tlsf.Free(ptr);
    }

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

    for (const auto& it : m_fallbackAllocations)
    {
        if (m_flags & PF_DEBUG_GUARD_PAGES)
        {
            FreeWithGuardPage(it.first);
        }
        else
        {
            Memory::FreeAligned(it.first);
        }
    }

    m_fallbackAllocations.Clear();

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

    for (const auto& it : m_fallbackAllocations)
    {
        metrics[MemoryMetrics::MM_BYTES_COMMITTED] += it.second;
        metrics[MemoryMetrics::MM_BYTES_USED] += it.second;
        ++metrics[MemoryMetrics::MM_ALLOCATIONS_ACTIVE];
    }

    if (m_flags & PF_THREAD_SAFE)
    {
        m_atomicFlag.Release();
    }

    return metrics;
}

} // namespace memory
} // namespace Hyperion
