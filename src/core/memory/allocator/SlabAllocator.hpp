/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Types.hpp>

#include <core/threading/Spinlock.hpp>

namespace hyperion {
namespace memory {

enum AllocationType : uint32;

struct SlabStats
{
    SizeType bytesCommitted = 0;
    SizeType blocksInUse = 0;
    SizeType slabsTotal = 0;
};

class SlabPage
{
public:
    SlabPage(SizeType blockSize, uint32 blocksPerSlab, SizeType alignment)
        : m_blockSize(uint32(blockSize)),
          m_blocksPerSlab(blocksPerSlab),
          m_alignment(uint32(alignment)),
          m_freeCount(blocksPerSlab)
    {
        const SizeType bytes = SizeType(m_blockSize) * m_blocksPerSlab;
        m_memory = ::operator new(bytes, std::align_val_t(m_alignment));

        const SizeType bitmapBytes = (m_blocksPerSlab + 7u) / 8u;
        m_bitmap.reset(new uint8[bitmapBytes]);
        std::memset(m_bitmap.get(), 0, bitmapBytes);
    }

    ~SlabPage()
    {
        ::operator delete(m_memory, std::align_val_t(m_alignment));
    }

    SlabPage(const SlabPage&) = delete;
    SlabPage& operator=(const SlabPage&) = delete;

    void* Allocate()
    {
        if (m_freeCount == 0)
        {
            return nullptr;
        }

        // Find first zero bit
        for (uint32 byteIndex = 0; byteIndex < (m_blocksPerSlab + 7u) / 8u; ++byteIndex)
        {
            uint8 byte = m_bitmap[byteIndex];

            if (byte != 0xFFu)
            {
                const uint8 inv = uint8(~byte);
                const uint32 bit = uint32(__builtin_ctz(inv));
                const uint32 index = byteIndex * 8u + bit;

                if (index >= m_blocksPerSlab)
                {
                    break;
                }

                m_bitmap[byteIndex] = uint8(byte | (1u << bit));
                --m_freeCount;

                return static_cast<void*>(static_cast<uint8*>(m_memory) + index * m_blockSize);
            }
        }
        return nullptr;
    }

    bool Free(void* ptr)
    {
        const UIntPtr base = reinterpret_cast<UIntPtr>(m_memory);
        const UIntPtr p = reinterpret_cast<UIntPtr>(ptr);

        if (p < base)
        {
            return false;
        }

        const UIntPtr off = p - base;
        if (off % m_blockSize != 0)
        {
            return false;
        }

        const uint32 index = uint32(off / m_blockSize);
        if (index >= m_blocksPerSlab)
        {
            return false;
        }

        const uint32 byteIndex = index / 8u;
        const uint8 bitMask = uint8(1u << (index % 8u));

        if ((m_bitmap[byteIndex] & bitMask) == 0)
        {
            return false; // double free
        }

        m_bitmap[byteIndex] = uint8(m_bitmap[byteIndex] & ~bitMask);
        ++m_freeCount;

        return true;
    }

    bool Contains(const void* ptr) const
    {
        const UIntPtr base = reinterpret_cast<UIntPtr>(m_memory);
        const UIntPtr p = reinterpret_cast<UIntPtr>(ptr);
        return p >= base && p < (base + static_cast<UIntPtr>(m_blockSize) * m_blocksPerSlab);
    }

    HYP_FORCE_INLINE bool IsFull() const
    {
        return m_freeCount == 0;
    }

    HYP_FORCE_INLINE bool IsEmpty() const
    {
        return m_freeCount == m_blocksPerSlab;
    }

    HYP_FORCE_INLINE SizeType BytesCommitted() const
    {
        return SizeType(m_blockSize) * m_blocksPerSlab;
    }

    SlabPage* m_next = nullptr;

private:
    void* m_memory = nullptr;
    std::unique_ptr<uint8[]> m_bitmap;
    uint32 m_blockSize = 0;
    uint32 m_blocksPerSlab = 0;
    uint32 m_alignment = 0;
    uint32 m_freeCount = 0;
};

template <bool ThreadSafe = false>
class SlabAllocator
{
public:
    SlabAllocator(
        SizeType blockSize,
        uint32 blocksPerSlab = 256,
        SizeType alignment = alignof(std::max_align_t))
        : m_blockSize(uint32(blockSize)),
          m_blocksPerSlab(blocksPerSlab),
          m_alignment(uint32(alignment))
    {
        if (m_blockSize == 0 || m_blocksPerSlab == 0)
        {
            HYP_FAIL("Block size and blocks per slab must be greater than zero!");
        }

        if (!MathUtil::IsPowerOfTwo(m_alignment))
        {
            HYP_FAIL("Alignment must be a power of two!");
        }
    }

    ~SlabAllocator()
    {
        Reset();
    }

    SlabAllocator(const SlabAllocator&) = delete;
    SlabAllocator& operator=(const SlabAllocator&) = delete;

    void* Allocate()
    {
        LockGuard guard(m_lock);

        if (!m_partial)
        {
            NewSlabUnlocked();
        }

        // Prefer partially filled slab
        void* p = m_partial ? m_partial->Allocate() : nullptr;

        if (!p && m_empty)
        {
            MoveHead(m_empty, m_partial);
            p = m_partial->Allocate();
        }

        if (!p)
        {
            NewSlabUnlocked();
            p = m_partial->Allocate();
        }

        if (m_partial && m_partial->IsFull())
        {
            MoveHead(m_partial, m_full);
        }

        ++m_stats.blocksInUse;

        return p;
    }

    // To match Allocator interface:
    void* Allocate(SizeType size, SizeType alignment)
    {
#ifdef HYP_DEBUG_MODE
        HYP_CORE_ASSERT(size == m_blockSize);
        HYP_CORE_ASSERT(alignment <= m_alignment);
#endif

        return Allocate();
    }

    void Free(void* ptr)
    {
        if (!ptr)
        {
            return;
        }

        LockGuard guard(m_lock);

        // Try partial list first, then full, then empty.
        if (FreeInList(m_partial, ptr))
        {
            return;
        }

        if (FreeInList(m_full, ptr))
        {
            // If it was full, it may now be partial.
            MoveIf([&](SlabPage* p)
                {
                    return p->IsEmpty() == false;
                },
                m_full, m_partial);

            return;
        }

        if (FreeInList(m_empty, ptr))
        {
            return;
        }

        HYP_FAIL("Pointer does not belong to any slab in this allocator!");
    }

    void Reset()
    {
        LockGuard guard(m_lock);

        FreeList(m_partial);
        FreeList(m_full);
        FreeList(m_empty);

        m_stats = {};
    }

    SlabStats GetStats() const
    {
        LockGuard guard(m_lock);
        return m_stats;
    }

    template <typename T>
    T* New()
    {
        static_assert(std::is_trivially_destructible<T>::value || std::is_trivially_copyable<T>::value || true, "");

        void* mem = Allocate();
        return mem ? new (mem) T() : nullptr;
    }

    template <typename T, typename... Args>
    T* New(Args&&... args)
    {
        void* mem = Allocate();
        return mem ? new (mem) T(std::forward<Args>(args)...) : nullptr;
    }

    template <typename T>
    void Delete(T* obj)
    {
        if (!obj)
        {
            return;
        }

        obj->~T();

        Free(static_cast<void*>(obj));
    }

    HYP_FORCE_INLINE SizeType BlockSize() const
    {
        return m_blockSize;
    }

    HYP_FORCE_INLINE SizeType Alignment() const
    {
        return m_alignment;
    }

    HYP_FORCE_INLINE uint32 BlocksPerSlab() const
    {
        return m_blocksPerSlab;
    }

private:
    struct LockGuard
    {
        explicit LockGuard(Spinlock& l)
            : lock(l)
        {
            if constexpr (ThreadSafe)
            {
                lock.LockWriter();
            }
        }

        ~LockGuard()
        {
            if constexpr (ThreadSafe)
            {
                lock.UnlockWriter();
            }
        }

        Spinlock& lock;
    };

    void NewSlabUnlocked()
    {
        auto* slab = new SlabPage(m_blockSize, m_blocksPerSlab, m_alignment);
        slab->m_next = m_partial;
        m_partial = slab;

        m_stats.slabsTotal++;
        m_stats.bytesCommitted += slab->BytesCommitted();
    }

    static void MoveHead(SlabPage*& from, SlabPage*& to)
    {
        SlabPage* n = from->m_next;
        from->m_next = to;
        to = from;
        from = n;
    }

    template <typename Pred>
    static void MoveIf(Pred pred, SlabPage*& from, SlabPage*& to)
    {
        if (!from)
        {
            return;
        }

        if (pred(from))
        {
            MoveHead(from, to);
        }
    }

    static void FreeList(SlabPage*& list)
    {
        while (list)
        {
            SlabPage* n = list->m_next;
            delete list;
            list = n;
        }
    }

    bool FreeInList(SlabPage*& head, void* ptr)
    {
        SlabPage* prev = nullptr;
        SlabPage* cur = head;

        while (cur)
        {
            if (cur->Contains(ptr))
            {
                const bool ok = cur->Free(ptr);
                if (HYP_UNLIKELY(!ok))
                {
                    HYP_FAIL("Double free detected in slab allocator!");
                }

                --m_stats.blocksInUse;

                if (cur->IsEmpty())
                {
                    // Move to empty list to minimize cache churn in partial list
                    if (prev)
                    {
                        prev->m_next = cur->m_next;
                    }
                    else
                    {
                        head = cur->m_next;
                    }

                    cur->m_next = m_empty;
                    m_empty = cur;
                }
                else
                {
                    // Ensure non-full pages are in partial list
                    if (&head != &m_partial)
                    {
                        if (prev)
                        {
                            prev->m_next = cur->m_next;
                        }
                        else
                        {
                            head = cur->m_next;
                        }

                        cur->m_next = m_partial;
                        m_partial = cur;
                    }
                }
                return true;
            }

            prev = cur;
            cur = cur->m_next;
        }

        return false;
    }

private:
    uint32 m_blockSize = 0;
    uint32 m_blocksPerSlab = 0;
    uint32 m_alignment = 0;

    SlabPage* m_partial = nullptr;
    SlabPage* m_full = nullptr;
    SlabPage* m_empty = nullptr;

    mutable Spinlock m_lock;
    SlabStats m_stats;
};

} // namespace memory

using memory::SlabAllocator;

} // namespace hyperion
