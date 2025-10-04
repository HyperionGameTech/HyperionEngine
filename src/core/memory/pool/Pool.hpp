/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/Bitset.hpp>
#include <core/containers/SparsePagedArray.hpp>
#include <core/containers/LinkedList.hpp>
#include <core/containers/HashSet.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <core/Defines.hpp>

#include <core/Types.hpp>

namespace hyperion {
namespace memory {

class HYP_API Pool
{
public:
    struct Block
    {
        struct Range
        {
            SizeType offset;
            SizeType size;
        };

        struct AllocHeader
        {
            Block* owner;
            SizeType totalSize; // header + payload
        };

        ByteBuffer buffer;
        Array<Range> freeRanges;

        explicit Block(SizeType capacity)
        {
            buffer.SetCapacity(capacity);
            // whole block initially free
            freeRanges.PushBack({ 0, buffer.GetCapacity() });
        }

        void* Allocate(SizeType size, SizeType alignment)
        {
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
        }

        void Free(void* ptr)
        {
            if (ptr == nullptr)
            {
                return;
            }

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
        }
    };

    Pool()
        : m_blockSize(1024 * 1024) // 1 MiB
    {
    }

    explicit Pool(SizeType blockSize)
        : m_blockSize(blockSize)
    {
        HYP_CORE_ASSERT(m_blockSize > 0);
    }

    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    Pool(Pool&&) = delete;
    Pool& operator=(Pool&&) = delete;

    ~Pool();

    /*! \brief Allocates memory from the pool with the given size and alignment. */
    HYP_NODISCARD void* Alloc(SizeType size, SizeType alignment = alignof(std::max_align_t));

    /*! \brief Allocates an object of type T from the pool. */
    template <class T>
    HYP_FORCE_INLINE HYP_NODISCARD T* Alloc()
    {
        return reinterpret_cast<T*>(Alloc(sizeof(T), alignof(T)));
    }

    /*! \brief Frees a pointer previously allocated by this pool. */
    void Free(void* ptr);

    /*! \brief Frees all memory allocated by the pool and resets it to its initial state. */
    void Reset();

protected:
    LinkedList<Block> m_blocks;
    SizeType m_blockSize;
};

template <class T>
static inline HYP_NODISCARD T* PoolAlloc(Pool& pool)
{
    return pool.Alloc<T>();
}

static inline void PoolFree(Pool& pool, void* ptr)
{
    pool.Free(ptr);
}

template <class T, class... Args>
static inline HYP_NODISCARD T* PoolNew(Pool& pool, Args&&... args)
{
    T* ptr = pool.Alloc<T>();

    if (HYP_UNLIKELY(!ptr))
    {
        HYP_CORE_ASSERT(0, "Pool allocation failed!");
    }

    Memory::Construct<T>(ptr, std::forward<Args>(args)...);

    return ptr;
}

template <class T>
static inline void PoolDelete(Pool& pool, T* ptr)
{
    if (HYP_LIKELY(ptr))
    {
        ptr->~T();
        pool.Free(ptr);
    }
}

} // namespace memory

using memory::Pool;
using memory::PoolAlloc;
using memory::PoolDelete;
using memory::PoolFree;
using memory::PoolNew;

} // namespace hyperion
