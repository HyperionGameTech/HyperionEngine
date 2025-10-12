#include <core/memory/pool/Pool.hpp>

namespace hyperion {
namespace memory {

#pragma region Block

Pool::Block::Block(SizeType capacity)
{
    buffer.SetSize(capacity);

#if defined(HYP_POOL_USE_TLSF_ALLOCATOR) && HYP_POOL_USE_TLSF_ALLOCATOR
    allocator.AddPool(buffer.Data(), buffer.GetCapacity());
#else
    // whole block initially free
    freeRanges.PushBack({ 0, buffer.GetCapacity() });
#endif
}

void* Pool::Block::Allocate(SizeType size, SizeType alignment)
{
#if defined(HYP_POOL_USE_TLSF_ALLOCATOR) && HYP_POOL_USE_TLSF_ALLOCATOR
    return allocator.Allocate(size, alignment);
#else
    const SizeType headerAlign = alignof(AllocHeader);
    const SizeType headerSize = sizeof(AllocHeader);

    ubyte* base = reinterpret_cast<ubyte*>(buffer.Data());

    for (SizeType i = 0; i < freeRanges.Size(); ++i)
    {
        Range r = freeRanges[i];

        SizeType headerOffset = ByteUtil::AlignAs(r.offset, headerAlign);
        SizeType payloadOffset = ByteUtil::AlignAs(headerOffset + headerSize, alignment);

        if (payloadOffset + size > r.offset + r.size)
        {
            continue; // doesn't fit here
        }

        const SizeType allocStart = headerOffset;
        const SizeType allocEnd = payloadOffset + size;
        const SizeType allocSize = allocEnd - allocStart;

        AllocHeader hdrTmp { this, allocSize };
        Memory::MemCpy(base + headerOffset, &hdrTmp, headerSize);

        // adjust freeRanges
        if (allocStart == r.offset && allocSize == r.size)
        {
            freeRanges.EraseAt(i);
        }
        else if (allocStart == r.offset)
        {
            freeRanges[i].offset += allocSize;
            freeRanges[i].size -= allocSize;
        }
        else if (allocEnd == r.offset + r.size)
        {
            freeRanges[i].size = allocStart - r.offset;
        }
        else
        {
            // split
            const SizeType oldEnd = r.offset + r.size;
            freeRanges[i].size = allocStart - r.offset;
            Range second { allocEnd, oldEnd - allocEnd };
            freeRanges.Insert(freeRanges.Begin() + (i + 1), second);
        }

        // ensure reported size covers used bytes
        if (allocEnd > buffer.Size())
        {
            buffer.SetSize(allocEnd);
        }

        return base + payloadOffset;
    }

    // No free range worked, try allocating at top
    const SizeType currentTop = buffer.Size();
    SizeType headerOffset = ByteUtil::AlignAs(currentTop, headerAlign);
    SizeType payloadOffset = ByteUtil::AlignAs(headerOffset + headerSize, alignment);
    const SizeType allocStart = headerOffset;
    const SizeType allocEnd = payloadOffset + size;
    const SizeType allocSize = allocEnd - allocStart;

    if (allocEnd <= buffer.GetCapacity())
    {
        AllocHeader* hdr = reinterpret_cast<AllocHeader*>(base + headerOffset);
        hdr->owner = this;
        hdr->totalSize = allocSize;

        buffer.SetSize(allocEnd);

        // adjust any free range that overlaps this area (likely the top-most range)
        for (SizeType i = 0; i < freeRanges.Size(); ++i)
        {
            Range& r = freeRanges[i];

            if (allocStart >= r.offset && allocStart < r.offset + r.size)
            {
                if (allocStart == r.offset && allocSize == r.size)
                {
                    freeRanges.EraseAt(i);
                }
                else if (allocStart == r.offset)
                {
                    r.offset += allocSize;
                    r.size -= allocSize;
                }
                else if (allocEnd == r.offset + r.size)
                {
                    r.size = allocStart - r.offset;
                }
                else
                {
                    const SizeType oldEnd = r.offset + r.size;
                    r.size = allocStart - r.offset;
                    Range second { allocEnd, oldEnd - allocEnd };
                    freeRanges.Insert(freeRanges.Begin() + (i + 1), second);
                }
                break;
            }
        }

        return base + payloadOffset;
    }

    return nullptr; // no capacity
#endif
}

void Pool::Block::Free(void* ptr)
{
    if (ptr == nullptr)
    {
        return;
    }

#if defined(HYP_POOL_USE_TLSF_ALLOCATOR) && HYP_POOL_USE_TLSF_ALLOCATOR
    allocator.Free(ptr);
#else
    ubyte* base = reinterpret_cast<ubyte*>(buffer.Data());
    ubyte* payload = reinterpret_cast<ubyte*>(ptr);

    // header sits before payload (allocation enforced alignment)
    const SizeType headerSize = sizeof(AllocHeader);
    AllocHeader hdrTmp;
    Memory::MemCpy(&hdrTmp, payload - headerSize, headerSize);
    if (hdrTmp.owner != this)
    {
        return; // wrong owner or corrupted
    }

    const SizeType headerOffset = static_cast<SizeType>((payload - headerSize) - base);
    const SizeType totalSize = hdrTmp.totalSize;

    Range newRange { headerOffset, totalSize };

    // insert keeping sorted order by offset
    SizeType idx = 0;
    while (idx < freeRanges.Size() && freeRanges[idx].offset < newRange.offset)
    {
        ++idx;
    }
    freeRanges.Insert(freeRanges.Begin() + idx, newRange);

    // merge backward
    if (idx > 0)
    {
        Range& prev = freeRanges[idx - 1];
        Range& cur = freeRanges[idx];
        if (prev.offset + prev.size >= cur.offset)
        {
            prev.size = MathUtil::Max(prev.size, (cur.offset + cur.size) - prev.offset);
            freeRanges.EraseAt(idx);
            idx = idx - 1;
        }
    }

    // merge forward
    if (idx + 1 < freeRanges.Size())
    {
        Range& cur = freeRanges[idx];
        Range& next = freeRanges[idx + 1];
        if (cur.offset + cur.size >= next.offset)
        {
            cur.size = MathUtil::Max(cur.size, (next.offset + next.size) - cur.offset);
            freeRanges.EraseAt(idx + 1);
        }
    }

    // shrink buffer.Size() if top is free
    if (!freeRanges.Empty())
    {
        const Range& last = freeRanges.Back();
        const SizeType lastEnd = last.offset + last.size;
        if (lastEnd >= buffer.Size())
        {
            SizeType newSize = 0;
            if (freeRanges.Size() >= 2)
            {
                const Range& prev = freeRanges[freeRanges.Size() - 2];
                newSize = prev.offset + prev.size;
            }
            else
            {
                newSize = 0;
            }
            buffer.SetSize(newSize);
        }
    }
#endif
}

#pragma endregion Block

Pool::~Pool()
{
    m_blocks.Clear();
}

HYP_NODISCARD void* Pool::Alloc(SizeType size, SizeType alignment)
{
    for (auto& block : m_blocks)
    {
        void* p = block.Allocate(size, alignment);
        if (p != nullptr)
        {
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

    return p;
}

void Pool::Free(void* ptr)
{
#if !defined(HYP_POOL_USE_TLSF_ALLOCATOR) || !HYP_POOL_USE_TLSF_ALLOCATOR
    // Fast path: read header to find owner
    using AllocHeader = Block::AllocHeader;

    AllocHeader* hdr = reinterpret_cast<AllocHeader*>(reinterpret_cast<ubyte*>(ptr) - sizeof(AllocHeader));
    Block* owner = hdr->owner;

    if (owner != nullptr)
    {
        ubyte* base = reinterpret_cast<ubyte*>(owner->buffer.Data());
        ubyte* bptr = reinterpret_cast<ubyte*>(ptr);
        if (bptr > base && bptr <= base + owner->buffer.GetCapacity())
        {
            owner->Free(ptr);
            return;
        }
    }
#endif

    // Fallback: search blocks
    for (auto& block : m_blocks)
    {
        ubyte* base = reinterpret_cast<ubyte*>(block.buffer.Data());
        ubyte* bptr = reinterpret_cast<ubyte*>(ptr);
        if (bptr > base && bptr <= base + block.buffer.GetCapacity())
        {
            block.Free(ptr);
            return;
        }
    }

    // not found
    HYP_FAIL("Pointer {} not found in any pool block!", ptr);
}

void Pool::Reset()
{
    m_blocks.Clear();
}

} // namespace memory
} // namespace hyperion