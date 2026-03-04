/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/containers/Array.hpp>
#include <Core/memory/MemoryMetrics.hpp>

#define HYP_USE_THIRD_PARTY_TLSF 1

namespace Hyperion {
namespace memory {

#if HYP_USE_THIRD_PARTY_TLSF

class HYP_API TlsfAllocator
{
public:
    TlsfAllocator();
    ~TlsfAllocator();

    void AddPool(void* memory, size_t bytes);
    void RemovePool(void* memory);

    void* Allocate(size_t bytes, size_t alignment = 16);
    void* Reallocate(void* ptr, size_t newSize, size_t alignment = 16);
    void Free(void* ptr);

    MemoryMetrics GetMemoryMetrics() const;

private:
    struct PoolInfo
    {
        void* pool;    // pool_t handle from tlsf_add_pool
        void* memory;  // base memory pointer
        size_t size; // total pool size
    };

    void* m_tlsf;
    void* m_mem;
    Array<PoolInfo> m_pools;
};

#else

class HYP_API TlsfAllocator
{
public:
    // SLI = 5 gives 32 second-level lists per FLI, as recommended in TLSF v2.0
    static constexpr uint32 SliBits = 5;
    static constexpr uint32 SlCount = 1 << SliBits; // 32

    // Smallest block size managed by TLSF, including header; must be power of two
    static constexpr size_t MinBlockSize = 32;

    static constexpr size_t DefaultAlign = 16;

    // Maximum first-level classes supported (kept <= 30 so we can pack bitmap in 32 bits)
    static constexpr uint32 MaxFli = 30; // supports blocks up to ~1<<30 bytes with SliBits splitting

    TlsfAllocator();
    ~TlsfAllocator();

    // Adds a memory pool to manage. The memory must remain valid for the lifetime of the allocator.
    void AddPool(void* memory, size_t bytes);

    // Removes a previously added pool. The pool must be completely free.
    void RemovePool(void* memory);

    void* Allocate(size_t bytes, size_t alignment = DefaultAlign);
    void* Reallocate(void* ptr, size_t newSize, size_t alignment = DefaultAlign);
    void Free(void* ptr);

    size_t GetUsableSize(void* ptr) const;

    /*! \brief Returns memory usage metrics for this allocator.
     *  This provides standardized statistics about memory consumption, utilization, and fragmentation. */
    MemoryMetrics GetMemoryMetrics() const;

    struct Block
    {
        // sizeAndFlags: upper bits store block size, lower 2 bits are flags
        // bit 0: used flag, bit 1: prevPhysUsed flag
        size_t sizeAndFlags;
        Block* prevPhys; // physical neighbor before this block
        Block* nextFree; // freelist links when free
        Block* prevFree;

        static constexpr size_t UsedMask = 0x1;
        static constexpr size_t PrevUsedMask = 0x2;
        static constexpr size_t FlagMask = UsedMask | PrevUsedMask;

        HYP_FORCE_INLINE size_t Size() const
        {
            return sizeAndFlags & ~FlagMask;
        }

        HYP_FORCE_INLINE void SetSize(size_t sz)
        {
            sizeAndFlags = (sz & ~FlagMask) | (sizeAndFlags & FlagMask);
        }

        HYP_FORCE_INLINE bool IsUsed() const
        {
            return (sizeAndFlags & UsedMask) != 0;
        }
        HYP_FORCE_INLINE void SetUsed(bool v)
        {
            sizeAndFlags = v ? (sizeAndFlags | UsedMask) : (sizeAndFlags & ~UsedMask);
        }

        HYP_FORCE_INLINE bool PrevPhysUsed() const
        {
            return (sizeAndFlags & PrevUsedMask) != 0;
        }
        HYP_FORCE_INLINE void SetPrevPhysUsed(bool v)
        {
            sizeAndFlags = v ? (sizeAndFlags | PrevUsedMask) : (sizeAndFlags & ~PrevUsedMask);
        }

        HYP_FORCE_INLINE Block* NextPhys()
        {
            return reinterpret_cast<Block*>(reinterpret_cast<uint8*>(this) + Size());
        }

        HYP_FORCE_INLINE const Block* NextPhys() const
        {
            return reinterpret_cast<const Block*>(reinterpret_cast<const uint8*>(this) + Size());
        }

        // User payload pointer
        HYP_FORCE_INLINE void* ToPtr()
        {
            return reinterpret_cast<void*>(this + 1);
        }

        HYP_FORCE_INLINE static Block* FromPtr(void* p)
        {
            return reinterpret_cast<Block*>(p) - 1;
        }
    };

    struct Pool
    {
        uint8* base;
        size_t size;
        Block* first;
        Block* sentinel; // zero-sized used block at end
    };

private:
    // Bitmaps
    uint32 m_flBitmap;         // FLI bitmap, 1 bit per non-empty FLI
    uint32 m_slBitmap[MaxFli]; // SLI bitmap per FLI

    // Free lists grid [FLI][SLI]
    Block* m_free[MaxFli][SlCount];

    // Pools we manage
    Array<Pool> m_pools;

private:
    Block* CarveFront(Block* b, size_t size);

    // Mapping of size -> (fli, sli)
    static HYP_FORCE_INLINE void Mapping(size_t size, uint32& fli, uint32& sli)
    {
        // Round to at least MinBlockSize
        if (size < MinBlockSize)
        {
            fli = 0;
            sli = uint32(size) / (MinBlockSize / SlCount);

            if (sli >= SlCount)
            {
                sli = SlCount - 1;
            }

            return;
        }

        const uint32 l = Msbit(size);
        fli = l - Msbit(MinBlockSize);

        const size_t base = size_t(1) << l;
        const size_t step = base >> SliBits;
        sli = uint32((size - base) / step);
        if (sli >= SlCount)
        {
            sli = SlCount - 1;
        }
    }

    static HYP_FORCE_INLINE uint32 Lsbit(uint32 x)
    {
#if defined(_MSC_VER)
        unsigned long idx;
        _BitScanForward(&idx, x);
        return uint32(idx);
#else
        return uint32(__builtin_ctz(x));
#endif
    }

    static HYP_FORCE_INLINE uint32 Msbit(size_t x)
    {
#if defined(_MSC_VER) && INTPTR_MAX == INT64_MAX
        unsigned long idx;
        _BitScanReverse64(&idx, x);
        return uint32(idx);
#elif defined(_MSC_VER)
        unsigned long idx;
        _BitScanReverse(&idx, uint32(x));
        return uint32(idx);
#else
        return uint32((sizeof(size_t) * 8) - 1 - __builtin_clzl(x));
#endif
    }

    void InsertFree(Block* b);
    void RemoveFree(Block* b, uint32 fli, uint32 sli);
    void RemoveFree(Block* b);

    Block* FindSuitable(size_t size, uint32& outFli, uint32& outSli);
    Block* Split(Block* b, size_t size);
    Block* MergePrev(Block* b);
    Block* MergeNext(Block* b);

    static size_t AdjustRequest(size_t bytes, size_t alignment);

    Pool* FindOwningPool(const Block* b);
};
#endif

} // namespace memory

using memory::TlsfAllocator;

} // namespace Hyperion
