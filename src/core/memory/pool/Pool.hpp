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

#define HYP_POOL_USE_TLSF_ALLOCATOR 1

#if defined(HYP_POOL_USE_TLSF_ALLOCATOR) && HYP_POOL_USE_TLSF_ALLOCATOR
#include <core/memory/allocator/TlsfAllocator.hpp>
#endif

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

#if defined(HYP_POOL_USE_TLSF_ALLOCATOR) && HYP_POOL_USE_TLSF_ALLOCATOR
        TlsfAllocator allocator;
#else
        Array<Range> freeRanges;
#endif

        explicit Block(SizeType capacity);
        ~Block();

        void* Allocate(SizeType size, SizeType alignment);
        void Free(void* ptr);
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
