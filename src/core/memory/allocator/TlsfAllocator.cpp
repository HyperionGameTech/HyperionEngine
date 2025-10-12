/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <core/memory/allocator/TlsfAllocator.hpp>
#include <core/memory/Memory.hpp>

#include <core/utilities/ByteUtil.hpp>

namespace hyperion {
namespace memory {

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

    uint8* base = reinterpret_cast<uint8*>(memory);

    Pool p {};
    p.base = base;
    p.size = bytes;

    // first block covers the whole area minus the trailing sentinel
    Block* first = reinterpret_cast<Block*>(base);
    SizeType firstSize = ByteUtil::AlignAs(bytes - sizeof(Block), DefaultAlign);
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

    RemoveFree(b);

    Block* rem = reinterpret_cast<Block*>(reinterpret_cast<uint8*>(b) + size);
    rem->sizeAndFlags = 0;
    rem->SetSize(total - size);
    rem->SetUsed(false);
    rem->SetPrevPhysUsed(true); // b will be used after split
    rem->prevPhys = b;

    Block* next = rem->NextPhys();
    next->prevPhys = rem;
    if (!next->IsUsed())
    {
        next->SetPrevPhysUsed(false);
    }
    else
    {
        next->SetPrevPhysUsed(true);
    }

    b->SetSize(size);
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
        return nullptr;
    const SizeType needed = AdjustRequest(bytes, alignment);

    uint32 fli = 0, sli = 0;
    Block* b = FindSuitable(needed, fli, sli);
    if (!b)
        return nullptr;

    RemoveFree(b, fli, sli);

    if (alignment > DefaultAlign)
    {
        uint8* payload = reinterpret_cast<uint8*>(b) + sizeof(Block);
        uint8* aligned = reinterpret_cast<uint8*>(ByteUtil::AlignAs(reinterpret_cast<SizeType>(payload), alignment));
        SizeType frontPad = static_cast<SizeType>(aligned - reinterpret_cast<uint8*>(b));
        if (frontPad >= MinBlockSize)
            b = CarveFront(b, frontPad);
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

} // namespace memory
} // namespace hyperion