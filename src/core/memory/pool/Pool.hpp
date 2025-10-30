/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/Bitset.hpp>
#include <core/containers/SparsePagedArray.hpp>
#include <core/containers/LinkedList.hpp>
#include <core/containers/HashSet.hpp>
#include <core/containers/SortedArray.hpp>

#include <core/threading/ThreadId.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/MemoryMetrics.hpp>

#include <core/memory/allocator/Allocator.hpp>
#include <core/memory/allocator/AllocatorFlags.hpp>
#include <core/memory/allocator/TlsfAllocator.hpp>

#include <core/Defines.hpp>
#include <core/Types.hpp>

namespace hyperion {

// Pool-specific flags (inherits from AllocatorFlags)
enum PoolFlags : uint32
{
    PF_NONE = AF_NONE,
    PF_THREAD_SAFE = AF_THREAD_SAFE, //!< pool is thread-safe
};

HYP_MAKE_ENUM_FLAGS(PoolFlags);

namespace memory {

class HYP_API Pool
{
    template <class T, class T2>
    friend struct DefaultAllocatorInstanceHelper;

    /*! \brief This constructor should generally not be used, but exists so we don't have compiler errors for GetDefaultAllocatorInstance */
    Pool()
        : Pool(1024 * 1024) // default to 1 MB blocks
    {
    }

public:
    template <class T>
    struct Allocation : DynamicAllocationBase<T>
    {
    };

    struct Block
    {
        struct Range
        {
            SizeType offset;
            SizeType size;

            HYP_FORCE_INLINE bool operator<(const Range& other) const
            {
                return offset < other.offset;
            }

            HYP_FORCE_INLINE bool operator==(const Range& other) const
            {
                return offset == other.offset && size == other.size;
            }
        };

        struct AllocHeader
        {
            Block* owner;
            SizeType totalSize; // header + payload
        };

        ByteBuffer buffer;
        TlsfAllocator allocator;

        explicit Block(SizeType capacity);
        ~Block() = default;

        void* Allocate(SizeType size, SizeType alignment);
        void Free(void* ptr);
    };

    explicit Pool(SizeType blockSize, EnumFlags<PoolFlags> flags = PF_NONE, const ThreadId& ownerThreadId = ThreadId::Invalid())
        : m_blockSize(blockSize),
          m_flags(flags),
          m_ownerThreadId(ownerThreadId),
          m_lockState(0)
    {
        HYP_CORE_ASSERT(m_blockSize > 0);
    }

    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    Pool(Pool&&) = delete;
    Pool& operator=(Pool&&) = delete;

    ~Pool();

    HYP_FORCE_INLINE SizeType GetBlockSize() const
    {
        return m_blockSize;
    }

    HYP_FORCE_INLINE EnumFlags<PoolFlags> GetFlags() const
    {
        return m_flags;
    }

    /*! \brief Allocates memory from the pool with the given size and alignment. */
    HYP_NODISCARD void* Allocate(SizeType size, SizeType alignment = 16);

    /*! \brief Allocates an object of type T from the pool. */
    template <class T>
    HYP_FORCE_INLINE HYP_NODISCARD T* Allocate()
    {
        return reinterpret_cast<T*>(Allocate(sizeof(T), alignof(T)));
    }

    /*! \brief Frees a pointer previously allocated by this pool. */
    void Free(void* ptr);

    /*! \brief Frees all memory allocated by the pool and resets it to its initial state. */
    void Reset();

    MemoryMetrics GetMemoryMetrics() const;

protected:
    LinkedList<Block> m_blocks;
    SizeType m_blockSize;
    EnumFlags<PoolFlags> m_flags;
    ThreadId m_ownerThreadId;

    mutable volatile int64 m_lockState;
};

template <class T>
static inline HYP_NODISCARD T* PoolAlloc(Pool& pool)
{
    return pool.Allocate<T>();
}

static inline void PoolFree(Pool& pool, void* ptr)
{
    pool.Free(ptr);
}

template <class T, class... Args>
static inline HYP_NODISCARD T* PoolNew(Pool& pool, Args&&... args)
{
    T* ptr = pool.Allocate<T>();

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

#define HYP_DEF_POOL_NEW_DELETE(poolName)                                  \
    void* operator new(size_t size)                                        \
    {                                                                      \
        return poolName->Allocate(size);                                   \
    }                                                                      \
                                                                           \
    void operator delete(void* ptr)                                        \
    {                                                                      \
        poolName->Free(ptr);                                               \
    }                                                                      \
                                                                           \
    void* operator new(size_t size, std::align_val_t alignment)            \
    {                                                                      \
        return poolName->Allocate(size, static_cast<SizeType>(alignment)); \
    }                                                                      \
                                                                           \
    void operator delete(void* ptr, std::align_val_t)                      \
    {                                                                      \
        poolName->Free(ptr);                                               \
    }                                                                      \
                                                                           \
    void* operator new[](size_t size) = delete;                            \
    void operator delete[](void* ptr) = delete;                            \
                                                                           \
    static void* operator new(size_t, void* p) noexcept                    \
    {                                                                      \
        return p;                                                          \
    }                                                                      \
                                                                           \
    static void operator delete(void*, void*) noexcept                     \
    {                                                                      \
    }