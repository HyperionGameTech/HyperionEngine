/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/containers/LinkedList.hpp>

#include <Core/threading/util/ThreadId.hpp>
#include <Core/threading/AtomicFlag.hpp>

#include <Core/utilities/EnumFlags.hpp>

#include <Core/memory/ByteBuffer.hpp>
#include <Core/memory/MemoryMetrics.hpp>

#include <Core/memory/allocator/Allocator.hpp>
#include <Core/memory/allocator/AllocatorFlags.hpp>
#include <Core/memory/allocator/TlsfAllocator.hpp>

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

enum PoolFlags : uint32
{
    PF_NONE = AF_NONE,
    PF_THREAD_SAFE = AF_THREAD_SAFE,

    PF_DEFAULT = PF_THREAD_SAFE
};

HYP_MAKE_ENUM_FLAGS(PoolFlags);

namespace memory {

class HYP_API Pool
{
    Pool() = delete;

public:
    static constexpr uint32 maxAlign = 16;

    template <class T>
    struct Allocation : DynamicAllocationBase<T>
    {
    };

    struct Block
    {
        ByteBuffer buffer;

        explicit Block(size_t capacity);
        ~Block() = default;
    };

    explicit Pool(size_t blockSize, EnumFlags<PoolFlags> flags = PF_DEFAULT, const ThreadId& ownerThreadId = ThreadId::Invalid())
        : m_blockSize(blockSize),
          m_flags(flags),
          m_ownerThreadId(ownerThreadId)
    {
        if (ownerThreadId.IsValid())
        {
            // disable PF_THREAD_SAFE if using owner thread id - will assert instead of locking
            m_flags &= ~PF_THREAD_SAFE;
        }

        HYP_CORE_ASSERT(m_blockSize > 0);
    }

    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    Pool(Pool&&) = delete;
    Pool& operator=(Pool&&) = delete;

    ~Pool();

    HYP_FORCE_INLINE size_t GetBlockSize() const
    {
        return m_blockSize;
    }

    HYP_FORCE_INLINE EnumFlags<PoolFlags> GetFlags() const
    {
        return m_flags;
    }

    HYP_FORCE_INLINE AtomicFlag& GetAtomicFlag()
    {
        return m_atomicFlag;
    }

    /*! \brief Allocates memory from the pool with the given size and alignment. */
    HYP_NODISCARD void* Allocate(size_t size, size_t alignment = 16);

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
    TlsfAllocator m_tlsf;

    LinkedList<Block> m_blocks;
    size_t m_blockSize;
    EnumFlags<PoolFlags> m_flags;
    AtomicFlag m_atomicFlag;

    const ThreadId& m_ownerThreadId;
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

#define HYP_POOL_NEW(pool, T, ...) new ((*pool).Allocate(sizeof(T), alignof(T))) T(__VA_ARGS__)

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

} // namespace Hyperion

#pragma region Global operator new and delete

inline void* operator new(size_t size, Hyperion::Pool& pool)
{
    return pool.Allocate(size);
}

inline void* operator new(size_t size, std::align_val_t alignment, Hyperion::Pool& pool)
{
    return pool.Allocate(size, size_t(alignment));
}

inline void* operator new[](size_t size, Hyperion::Pool& pool)
{
    return pool.Allocate(size);
}

inline void* operator new[](size_t size, std::align_val_t alignment, Hyperion::Pool& pool)
{
    return pool.Allocate(size, size_t(alignment));
}

inline void operator delete(void* ptr, Hyperion::Pool& pool)
{
    pool.Free(ptr);
}

inline void operator delete(void* ptr, std::align_val_t alignment, Hyperion::Pool& pool)
{
    pool.Free(ptr);
}

inline void operator delete[](void* ptr, Hyperion::Pool& pool)
{
    pool.Free(ptr);
}

inline void operator delete[](void* ptr, std::align_val_t alignment, Hyperion::Pool& pool)
{
    pool.Free(ptr);
}

#pragma endregion Global operator new and delete

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
        return poolName->Allocate(size, static_cast<size_t>(alignment)); \
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
