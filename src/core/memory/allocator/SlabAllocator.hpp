/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Types.hpp>

#include <core/containers/Array.hpp>

#include <core/memory/MemoryMetrics.hpp>

#include <core/memory/allocator/Allocator.hpp>

#include <new>
#include <limits>
#include <cstring>

namespace hyperion {
namespace memory {

enum AllocationType : uint32;

class SlabAllocator
{
public:
    template <class T>
    struct Allocation : DynamicAllocationBase<T>
    {
    };

    SlabAllocator(
        SizeType blockSize,
        uint32 blocksPerSlab = 256,
        SizeType alignment = 16)
        : m_blockSize(0),
          m_alignment(0),
          m_blocksPerSlab(blocksPerSlab),
          m_slabs(),
          m_activeAllocations(0)
    {
        HYP_CORE_ASSERT(m_blocksPerSlab != 0);
        HYP_CORE_ASSERT(IsPowerOfTwo(alignment));

        const SizeType minBlock = sizeof(uint32) > sizeof(void*) ? sizeof(uint32) : sizeof(void*);
        const SizeType reqBlock = blockSize < minBlock ? minBlock : blockSize;

        m_alignment = alignment < alignof(void*) ? alignof(void*) : alignment;
        HYP_CORE_ASSERT(IsPowerOfTwo(m_alignment));

        m_blockSize = AlignUp(reqBlock, m_alignment);
    }

    ~SlabAllocator()
    {
        Reset();
    }

    SlabAllocator(const SlabAllocator&) = delete;
    SlabAllocator& operator=(const SlabAllocator&) = delete;

    void* Allocate()
    {
        for (uint32 i = 0; i < m_slabs.Size(); ++i)
        {
            Slab& slab = m_slabs[i];
            if (slab.freeCount != 0)
            {
                if (void* p = PopFromSlab(slab))
                {
                    ++m_activeAllocations;
                    return p;
                }
            }
        }

        if (!CreateSlab())
        {
            return nullptr;
        }

        Slab& slab = m_slabs.Back();
        void* p = PopFromSlab(slab);
        if (!p)
            return nullptr;

        ++m_activeAllocations;
        return p;
    }

    // To match Allocator interface:
    void* Allocate(SizeType size, SizeType alignment)
    {
        if (size == 0)
            size = m_blockSize;
        if (alignment == 0)
            alignment = m_alignment;

        if (HYP_UNLIKELY(size > m_blockSize))
            return nullptr;
        if (HYP_UNLIKELY(!IsPowerOfTwo(alignment)))
            return nullptr;
        if (HYP_UNLIKELY(alignment > m_alignment))
            return nullptr;

        return Allocate();
    }

    void Free(void* ptr)
    {
        if (!ptr)
            return;

        Slab* slab = FindOwningSlab(ptr);
        HYP_CORE_ASSERT(slab != nullptr);
        if (HYP_UNLIKELY(!slab))
            return;

        const ubyte* base = static_cast<const ubyte*>(slab->base);
        const ubyte* p = static_cast<const ubyte*>(ptr);
        const SizeType slabBytes = TotalSlabBytes();

        HYP_CORE_ASSERT(p >= base && p < base + slabBytes);
        if (HYP_UNLIKELY(!(p >= base && p < base + slabBytes)))
            return;

        const SizeType offset = static_cast<SizeType>(p - base);
        HYP_CORE_ASSERT((offset % m_blockSize) == 0);
        if (HYP_UNLIKELY((offset % m_blockSize) != 0))
            return;

        const uint32 blockIndex = static_cast<uint32>(offset / m_blockSize);
        HYP_CORE_ASSERT(blockIndex < m_blocksPerSlab);
        if (HYP_UNLIKELY(blockIndex >= m_blocksPerSlab))
            return;

#if defined(HYP_DEBUG_MODE)
        {
            // Linear scan is acceptable for small slabs; avoids extra bitmaps.
            uint32 it = slab->freeHead;
            while (it != InvalidIndex())
            {
                HYP_CORE_ASSERT(it != blockIndex);
                if (HYP_UNLIKELY(it == blockIndex))
                    return; // double free detected in debug
                it = ReadNextIndex(BlockPtr(*slab, it));
            }
        }
#endif

        WriteNextIndex(ptr, slab->freeHead);
        slab->freeHead = blockIndex;
        ++slab->freeCount;

        if (m_activeAllocations > 0)
            --m_activeAllocations;
    }

    void Reset()
    {
        for (uint32 i = 0; i < m_slabs.Size(); ++i)
        {
            if (m_slabs[i].base)
            {
                HYP_FREE_ALIGNED(m_slabs[i].base);
                m_slabs[i].base = nullptr;
            }
            m_slabs[i].freeHead = InvalidIndex();
            m_slabs[i].freeCount = 0;
        }
        m_slabs.Clear();
        m_activeAllocations = 0;
    }

    MemoryMetrics GetMemoryMetrics() const
    {
        MemoryMetrics metrics;

        const SizeType slabsCount = m_slabs.Size();
        const SizeType blocksTotal = slabsCount * static_cast<SizeType>(m_blocksPerSlab);
        const SizeType bytesCommitted = slabsCount * TotalSlabBytes();
        const SizeType bytesUsed = static_cast<SizeType>(m_activeAllocations) * m_blockSize;
        const SizeType bytesFree = bytesCommitted - bytesUsed;

        metrics[MemoryMetrics::MM_BYTES_COMMITTED] = bytesCommitted;
        metrics[MemoryMetrics::MM_BYTES_USED] = bytesUsed;
        metrics[MemoryMetrics::MM_BYTES_FREE] = bytesFree;
        metrics[MemoryMetrics::MM_ALLOCATIONS_ACTIVE] = static_cast<SizeType>(m_activeAllocations);
        metrics[MemoryMetrics::MM_BLOCKS_TOTAL] = blocksTotal;

        return metrics;
    }

private:
    struct Slab
    {
        void* base = nullptr;
        uint32 freeHead = InvalidIndex();
        uint32 freeCount = 0;
    };

    static HYP_FORCE_INLINE uint32 InvalidIndex()
    {
        return UINT32_MAX;
    }

    static HYP_FORCE_INLINE bool IsPowerOfTwo(SizeType v)
    {
        return v && ((v & (v - 1)) == 0);
    }

    static HYP_FORCE_INLINE SizeType AlignUp(SizeType value, SizeType alignment)
    {
        const SizeType mask = alignment - 1;
        return (value + mask) & ~mask;
    }

    HYP_FORCE_INLINE SizeType TotalSlabBytes() const
    {
        return SizeType(m_blocksPerSlab) * m_blockSize;
    }

    HYP_FORCE_INLINE void* BlockPtr(const Slab& slab, uint32 index) const
    {
        return static_cast<ubyte*>(slab.base) + SizeType(index) * m_blockSize;
    }

    static HYP_FORCE_INLINE uint32 ReadNextIndex(const void* block)
    {
        return *reinterpret_cast<const uint32*>(block);
    }

    static HYP_FORCE_INLINE void WriteNextIndex(void* block, uint32 next)
    {
        *reinterpret_cast<uint32*>(block) = next;
    }

    bool CreateSlab()
    {
        void* mem = HYP_ALLOC_ALIGNED(TotalSlabBytes(), m_alignment);
        if (!mem)
            return false;

        Slab slab;
        slab.base = mem;
        slab.freeHead = 0;
        slab.freeCount = m_blocksPerSlab;

        for (uint32 i = 0; i < m_blocksPerSlab; ++i)
        {
            const uint32 next = (i + 1u < m_blocksPerSlab) ? (i + 1u) : InvalidIndex();
            WriteNextIndex(BlockPtr(slab, i), next);
        }

        m_slabs.PushBack(slab);
        return true;
    }

    HYP_FORCE_INLINE void* PopFromSlab(Slab& slab)
    {
        if (slab.freeCount == 0 || slab.freeHead == InvalidIndex())
        {
            return nullptr;
        }

        const uint32 index = slab.freeHead;
        void* block = BlockPtr(slab, index);

        const uint32 next = ReadNextIndex(block);
        slab.freeHead = next;
        --slab.freeCount;

        return block;
    }

    Slab* FindOwningSlab(void* ptr)
    {
        const ubyte* p = static_cast<const ubyte*>(ptr);
        const SizeType slabBytes = TotalSlabBytes();

        for (uint32 i = 0; i < m_slabs.Size(); ++i)
        {
            const ubyte* base = static_cast<const ubyte*>(m_slabs[i].base);
            const ubyte* end = base + slabBytes; // uniform slab size
            if (p >= base && p < end)
            {
                return &m_slabs[i];
            }
        }
        return nullptr;
    }

private:
    SizeType m_blockSize;
    SizeType m_alignment;
    uint32 m_blocksPerSlab;
    Array<Slab> m_slabs;
    uint64 m_activeAllocations;
};

} // namespace memory

using memory::SlabAllocator;

} // namespace hyperion
