/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <core/memory/allocator/TlsfAllocator.hpp>
#include <core/memory/Memory.hpp>

#include <core/utilities/ByteUtil.hpp>

#if defined(HYP_USE_THIRD_PARTY_TLSF) && HYP_USE_THIRD_PARTY_TLSF
#include <thirdparty/tlsf/tlsf.h>
#endif

namespace hyperion {
namespace memory {

#if HYP_USE_THIRD_PARTY_TLSF

TlsfAllocator::TlsfAllocator()
    : m_tlsf(nullptr),
      m_mem(nullptr)
{
    const SizeType poolSize = tlsf_size();

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

void TlsfAllocator::AddPool(void* memory, SizeType bytes)
{
    AssertDebug(m_tlsf != nullptr);
    AssertDebug(bytes >= tlsf_pool_overhead() + tlsf_block_size_min());

    tlsf_add_pool((tlsf_t)m_tlsf, memory, bytes);
}

void TlsfAllocator::RemovePool(void* memory)
{
    // TLSF does not support removing pools in this implementation
    (void)memory;
}

void* TlsfAllocator::Allocate(SizeType bytes, SizeType alignment)
{
    AssertDebug(m_tlsf != nullptr);
    AssertDebug(bytes > 0 && alignment > 0 && (alignment & (alignment - 1)) == 0); // power of two

    return tlsf_memalign((tlsf_t)m_tlsf, alignment, bytes);
}

void* TlsfAllocator::Reallocate(void* ptr, SizeType newSize, SizeType alignment)
{
    AssertDebug(m_tlsf != nullptr);
    AssertDebug(ptr != nullptr);

    return tlsf_realloc((tlsf_t)m_tlsf, ptr, newSize);
}

void TlsfAllocator::Free(void* ptr)
{
    AssertDebug(m_tlsf != nullptr);
    AssertDebug(ptr != nullptr);

    tlsf_free((tlsf_t)m_tlsf, ptr);
}

MemoryMetrics TlsfAllocator::GetMemoryMetrics() const
{
    MemoryMetrics metrics = {};
    metrics[MemoryMetrics::MM_BYTES_COMMITTED] = 0;    // Not tracked
    metrics[MemoryMetrics::MM_BYTES_USED] = 0;         // Not tracked
    metrics[MemoryMetrics::MM_BYTES_FREE] = 0;         // Not tracked
    metrics[MemoryMetrics::MM_ALLOCATIONS_ACTIVE] = 0; // Not tracked
    metrics[MemoryMetrics::MM_BLOCKS_TOTAL] = 0;       // Not tracked
    metrics[MemoryMetrics::MM_BYTES_PEAK] = 0;         // Not tracked

    return metrics;
}

#else

TlsfAllocator::TlsfAllocator()
    : m_flBitmap(0)
{
    for (uint32 i = 0; i < MaxFli; ++i)
    {
        m_slBitmap[i] = 0;

        for (uint32 j = 0; j < SlCount; ++j)
        {
            m_free[i][j] = nullptr;
        }
    }
}

TlsfAllocator::~TlsfAllocator()
{
}

void TlsfAllocator::AddPool(void* memory, SizeType bytes)
{
    if (bytes < MinBlockSize + sizeof(Block))
    {
        return;
    }

    uintptr_t rawBase = reinterpret_cast<uintptr_t>(memory);
    uintptr_t baseAligned = ByteUtil::AlignAs(rawBase, DefaultAlign);
    SizeType headLoss = static_cast<SizeType>(baseAligned - rawBase);

    if (bytes <= headLoss + sizeof(Block))
    {
        return;
    }

    SizeType usable = bytes - headLoss;
    usable -= (usable % DefaultAlign); // align down

    ubyte* base = reinterpret_cast<ubyte*>(baseAligned);

    Pool p {};
    p.base = base;
    p.size = usable;

    Block* first = reinterpret_cast<Block*>(base);
    SizeType firstSize = usable - sizeof(Block); // sentinel at end

    first->sizeAndFlags = 0;
    first->SetSize(firstSize);
    first->SetUsed(false);
    first->SetPrevPhysUsed(true);
    first->prevPhys = nullptr;
    first->nextFree = nullptr;
    first->prevFree = nullptr;

    Block* sentinel = reinterpret_cast<Block*>(base + firstSize);
    sentinel->sizeAndFlags = 0;
    sentinel->SetSize(sizeof(Block));
    sentinel->SetUsed(true);
    sentinel->SetPrevPhysUsed(false);
    sentinel->prevPhys = first;

    p.first = first;
    p.sentinel = sentinel;

    m_pools.PushBack(p);
    InsertFree(first);
}

void TlsfAllocator::RemovePool(void* memory)
{
    for (SizeType i = 0; i < m_pools.Size(); ++i)
    {
        if (m_pools[i].base == memory)
        {
            // Expect fully free: first should be one big free block
            Pool& pool = m_pools[i];
            Block* b = pool.first;

            if (!b->IsUsed() && b->NextPhys() == pool.sentinel)
            {
                RemoveFree(b);
                m_pools.EraseAt(i);
            }

            return;
        }
    }
}

TlsfAllocator::Block* TlsfAllocator::FindSuitable(SizeType size, uint32& outFli, uint32& outSli)
{
    uint32 fli, sli;
    Mapping(size, fli, sli);

    uint32 flMap = m_flBitmap & (~0u << fli);
    if (!flMap)
    {
        return nullptr;
    }

    uint32 fliIdx = Lsbit(flMap);

    uint32 slMap = m_slBitmap[fliIdx];
    if (fliIdx == fli)
    {
        slMap &= (~0u << sli);
    }

    if (!slMap)
    {
        // advance to next non-empty FLI
        flMap &= ~(1u << fliIdx);

        if (!flMap)
        {
            return nullptr;
        }

        fliIdx = Lsbit(flMap);
        slMap = m_slBitmap[fliIdx];
    }

    uint32 sliIdx = Lsbit(slMap);

    outFli = fliIdx;
    outSli = sliIdx;

    return m_free[fliIdx][sliIdx];
}

void TlsfAllocator::InsertFree(Block* b)
{
    uint32 fli, sli;
    Mapping(b->Size(), fli, sli);

    b->nextFree = m_free[fli][sli];
    b->prevFree = nullptr;

    if (b->nextFree)
    {
        b->nextFree->prevFree = b;
    }

    m_free[fli][sli] = b;

    m_slBitmap[fli] |= (1u << sli);
    m_flBitmap |= (1u << fli);
}

void TlsfAllocator::RemoveFree(Block* b, uint32 fli, uint32 sli)
{
    Block* next = b->nextFree;
    Block* prev = b->prevFree;

    if (next)
    {
        next->prevFree = prev;
    }

    if (prev)
    {
        prev->nextFree = next;
    }
    else
    {
        m_free[fli][sli] = next;
    }

    if (!m_free[fli][sli])
    {
        m_slBitmap[fli] &= ~(1u << sli);

        if (!m_slBitmap[fli])
        {
            m_flBitmap &= ~(1u << fli);
        }
    }

    b->nextFree = b->prevFree = nullptr;
}

void TlsfAllocator::RemoveFree(Block* b)
{
    uint32 fli;
    uint32 sli;

    Mapping(b->Size(), fli, sli);
    RemoveFree(b, fli, sli);
}

TlsfAllocator::Block* TlsfAllocator::Split(Block* b, SizeType size)
{
    const SizeType total = b->Size();
    if (total < size + MinBlockSize)
    {
        return b;
    }

    // b stays as the first piece
    Block* rem = reinterpret_cast<Block*>(reinterpret_cast<ubyte*>(b) + size);

    rem->sizeAndFlags = 0;
    rem->SetSize(total - size);
    rem->SetUsed(false);
    rem->SetPrevPhysUsed(true); // previous (b) will be used
    rem->prevPhys = b;

    b->SetSize(size);

    Block* next = rem->NextPhys();
    next->prevPhys = rem;
    next->SetPrevPhysUsed(false); // previous is free (rem)

    InsertFree(rem);
    return b;
}

TlsfAllocator::Block* TlsfAllocator::MergePrev(Block* b)
{
    if (b->PrevPhysUsed())
    {
        return b;
    }

    Block* prev = b->prevPhys;

    if (prev && !prev->IsUsed())
    {
        RemoveFree(prev);
        prev->SetSize(prev->Size() + b->Size());
        Block* next = prev->NextPhys();
        next->prevPhys = prev;
        return prev;
    }

    return b;
}

TlsfAllocator::Block* TlsfAllocator::MergeNext(Block* b)
{
    Block* next = b->NextPhys();

    if (next && !next->IsUsed())
    {
        RemoveFree(next);
        b->SetSize(b->Size() + next->Size());
        Block* nn = b->NextPhys();

        if (nn)
        {
            nn->prevPhys = b;
        }
    }
    return b;
}

TlsfAllocator::Pool* TlsfAllocator::FindOwningPool(const Block* b)
{
    UIntPtr addr = reinterpret_cast<UIntPtr>(b);

    for (SizeType i = 0; i < m_pools.Size(); ++i)
    {
        const Pool& p = m_pools[i];
        if (addr >= reinterpret_cast<UIntPtr>(p.base) && addr < reinterpret_cast<UIntPtr>(p.base) + p.size)
        {
            return &m_pools[i];
        }
    }

    return nullptr;
}

SizeType TlsfAllocator::AdjustRequest(SizeType bytes, SizeType alignment)
{
    const SizeType aligned = ByteUtil::AlignAs(bytes, alignment);
    const SizeType withHeader = aligned + sizeof(Block);
    return withHeader < MinBlockSize ? MinBlockSize : ByteUtil::AlignAs(withHeader, DefaultAlign);
}

void* TlsfAllocator::Allocate(SizeType bytes, SizeType alignment)
{
    if (bytes == 0)
    {
        return nullptr;
    }

    const SizeType needed = AdjustRequest(bytes, alignment);

    uint32 fli = 0, sli = 0;
    Block* b = FindSuitable(needed, fli, sli);
    if (!b)
    {
        return nullptr;
    }

    RemoveFree(b, fli, sli);

    if (alignment > DefaultAlign)
    {
        uintptr_t payloadAddr = reinterpret_cast<uintptr_t>(b) + sizeof(Block);
        uintptr_t alignedPayload = ByteUtil::AlignAs(payloadAddr, alignment);
        SizeType frontSize = static_cast<SizeType>((alignedPayload - sizeof(Block)) - reinterpret_cast<uintptr_t>(b));

        if (frontSize >= MinBlockSize)
        {
            // front free, remainder candidate
            b = CarveFront(b, frontSize);
        }
    }

    b = Split(b, needed);

    b->SetUsed(true);
    Block* n = b->NextPhys();
    n->SetPrevPhysUsed(true);

    return b->ToPtr();
}

void TlsfAllocator::Free(void* ptr)
{
    if (!ptr)
        return;
    Block* b = Block::FromPtr(ptr);
    if (!b->IsUsed())
        return;

    b->SetUsed(false);

    Block* m = MergePrev(b);
    m = MergeNext(m);

    // fix prev-used flag on physical next
    Block* n = m->NextPhys();
    n->SetPrevPhysUsed(false);

    InsertFree(m);
}

void* TlsfAllocator::Reallocate(void* ptr, SizeType newSize, SizeType alignment)
{
    if (!ptr)
    {
        return Allocate(newSize, alignment);
    }

    if (newSize == 0)
    {
        Free(ptr);
        return nullptr;
    }

    Block* b = Block::FromPtr(ptr);
    const SizeType current = b->Size() - sizeof(Block);

    if (newSize <= current)
    {
        // shrink in place
        const SizeType needed = AdjustRequest(newSize, alignment);

        if (b->Size() >= needed + MinBlockSize)
        {
            SizeType remain = b->Size() - needed;
            b->SetSize(needed);
            Block* rem = reinterpret_cast<Block*>(reinterpret_cast<uint8*>(b) + needed);
            rem->sizeAndFlags = 0;
            rem->SetSize(remain);
            rem->SetUsed(false);
            rem->SetPrevPhysUsed(true);
            rem->prevPhys = b;

            Block* next = rem->NextPhys();
            next->prevPhys = rem;
            next->SetPrevPhysUsed(false);

            InsertFree(rem);
        }

        return ptr;
    }

    Block* next = b->NextPhys();

    if (next && !next->IsUsed())
    {
        SizeType total = b->Size() + next->Size();
        const SizeType needed = AdjustRequest(newSize, alignment);

        if (total >= needed)
        {
            RemoveFree(next);
            b->SetSize(total);
            Split(b, needed);
            b->SetUsed(true);
            b->NextPhys()->SetPrevPhysUsed(true);

            return ptr;
        }
    }

    void* np = Allocate(newSize, alignment);

    if (!np)
    {
        return nullptr;
    }

    // @TODO: Review
    SizeType toCopy = current < newSize ? current : newSize;
    Memory::MemCpy(np, ptr, toCopy);

    Free(ptr);

    return np;
}

SizeType TlsfAllocator::GetUsableSize(void* ptr) const
{
    if (!ptr)
    {
        return 0;
    }

    const Block* b = Block::FromPtr(ptr);
    return b->Size() - sizeof(Block);
}

TlsfAllocator::Block* TlsfAllocator::CarveFront(Block* b, SizeType frontSize)
{
    const SizeType total = b->Size();
    Block* rest = reinterpret_cast<Block*>((uint8*)b + frontSize);

    // front stays free
    b->SetSize(frontSize);
    b->SetUsed(false);

    // rest becomes the candidate block, still free for now
    rest->sizeAndFlags = 0;
    rest->SetSize(total - frontSize);
    rest->SetUsed(false);
    rest->SetPrevPhysUsed(false);
    rest->prevPhys = b;

    Block* next = rest->NextPhys();
    next->prevPhys = rest;
    next->SetPrevPhysUsed(false);

    InsertFree(b);
    return rest;
}

MemoryMetrics TlsfAllocator::GetMemoryMetrics() const
{
    MemoryMetrics metrics;

    for (const Pool& pool : m_pools)
    {
        metrics[MemoryMetrics::MM_BYTES_COMMITTED] += pool.size;

        // Walk the physical block chain to calculate used/free bytes
        const Block* current = pool.first;
        const Block* sentinel = pool.sentinel;

        while (current != sentinel)
        {
            const SizeType blockSize = current->Size();

            if (current->IsUsed())
            {
                // Subtract header size to get actual usable bytes
                const SizeType usableSize = blockSize - sizeof(Block);
                metrics[MemoryMetrics::MM_BYTES_USED] += usableSize;
                ++metrics[MemoryMetrics::MM_ALLOCATIONS_ACTIVE];
            }
            else
            {
                metrics[MemoryMetrics::MM_BYTES_FREE] += blockSize;
            }

            current = current->NextPhys();
        }

        ++metrics[MemoryMetrics::MM_BLOCKS_TOTAL];
    }

    return metrics;
}
#endif

} // namespace memory
} // namespace hyperion