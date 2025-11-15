/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/LinkedList.hpp>
#include <core/containers/FixedArray.hpp>

#include <core/Name.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>
#include <core/threading/DataRaceDetector.hpp>
#include <core/threading/Threads.hpp>

#include <core/memory/Memory.hpp>

#include <core/memory/allocator/Allocator.hpp>

#include <core/math/MathUtil.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/utilities/IdGenerator.hpp>

#include <core/Types.hpp>

namespace hyperion {
namespace memory {

class MemoryPoolBase;

struct MemoryPoolBlock final
{
    void* mem;
    AtomicVar<uint32> numElements { 0 };

    explicit MemoryPoolBlock(void* mem)
        : mem(mem)
    {
    }

    MemoryPoolBlock(const MemoryPoolBlock& other) = delete;
    MemoryPoolBlock& operator=(const MemoryPoolBlock& other) = delete;

    MemoryPoolBlock(MemoryPoolBlock&& other) noexcept = delete;
    MemoryPoolBlock& operator=(MemoryPoolBlock&& other) noexcept = delete;

    ~MemoryPoolBlock() = default;

    HYP_FORCE_INLINE bool IsEmpty() const
    {
        return numElements.Get(MemoryOrder::ACQUIRE) == 0;
    }
};

class HYP_API MemoryPoolBase
{
public:
    MemoryPoolBase(const MemoryPoolBase& other) = delete;
    MemoryPoolBase& operator=(const MemoryPoolBase& other) = delete;
    MemoryPoolBase(MemoryPoolBase&& other) noexcept = delete;
    MemoryPoolBase& operator=(MemoryPoolBase&& other) noexcept = delete;

protected:
    MemoryPoolBase();
    ~MemoryPoolBase();

    IdGenerator m_idGenerator;
};

template <class AllocatorType>
class MemoryPool : MemoryPoolBase
{
protected:
    using Block = MemoryPoolBlock;

protected:
    void CreateInitialBlocks()
    {
        m_numBlocks.Set(m_initialNumBlocks, MemoryOrder::RELEASE);

        for (uint32 i = 0; i < m_initialNumBlocks; i++)
        {
            void* mem = m_pAllocator->Allocate(m_elemSize * m_elemsPerBlock, m_elemAlignment);
            HYP_CORE_ASSERT(mem != nullptr, "Allocation failed! Size: %u, Alignment: %u", m_elemSize * m_elemsPerBlock, m_elemAlignment);

            m_blocks.EmplaceBack(mem);
        }
    }

public:
    static constexpr uint32 InvalidIndex = ~0u;

    MemoryPool(uint32 elemSize, uint32 elemAlignment, uint32 elemsPerBlock = 64, uint32 initialCount = 0)
        : MemoryPool(GetAllocatorInstance<AllocatorType>(), elemSize, elemAlignment, elemsPerBlock, initialCount)
    {
    }

    MemoryPool(AllocatorType* pAllocator, uint32 elemSize, uint32 elemAlignment, uint32 elemsPerBlock = 64, uint32 initialCount = 0)
        : m_pAllocator(pAllocator),
          m_elemSize(elemSize),
          m_elemAlignment(elemAlignment),
          m_elemsPerBlock(elemsPerBlock),
          m_initialNumBlocks((initialCount + elemsPerBlock - 1) / elemsPerBlock),
          m_numBlocks(0)
    {
        CreateInitialBlocks();
    }

    ~MemoryPool()
    {
        Mutex::Guard guard(m_blocksMutex);

        for (auto it = m_blocks.Begin(); it != m_blocks.End(); ++it)
        {
            m_pAllocator->Free(it->mem);
        }
    }

    HYP_FORCE_INLINE SizeType NumAllocatedElements() const
    {
        return m_numBlocks.Get(MemoryOrder::ACQUIRE) * m_elemsPerBlock;
    }

    HYP_FORCE_INLINE uint32 GetNumElementsPerBlock() const
    {
        return m_elemsPerBlock;
    }
    
    template <class ElementType>
    uint32 AcquireIndex(ElementType** outElementPtr = nullptr)
    {
        HYP_SCOPE;

        HYP_CORE_ASSERT(sizeof(ElementType) <= m_elemSize);
        HYP_CORE_ASSERT(alignof(ElementType) <= m_elemAlignment);

        const uint32 index = m_idGenerator.Next() - 1;

        const uint32 blockIndex = index / m_elemsPerBlock;
        const uint32 elementIndex = index % m_elemsPerBlock;

        if (blockIndex < m_initialNumBlocks)
        {
            Block& block = m_blocks[blockIndex];
            block.numElements.Increment(1, MemoryOrder::RELEASE);

            if (outElementPtr != nullptr)
            {
                *outElementPtr = reinterpret_cast<ElementType*>(UIntPtr(block.mem) + m_elemSize * elementIndex);
            }
        }
        else
        {
            Mutex::Guard guard(m_blocksMutex);

            if (index < m_elemsPerBlock * m_numBlocks.Get(MemoryOrder::ACQUIRE))
            {
                Block& block = m_blocks[blockIndex];
                block.numElements.Increment(1, MemoryOrder::RELEASE);

                if (outElementPtr != nullptr)
                {
                    *outElementPtr = reinterpret_cast<ElementType*>(UIntPtr(block.mem) + m_elemSize * elementIndex);
                }
            }
            else
            {
                // Add blocks until we can insert the element
                uint32 currentBlockIndex = uint32(m_blocks.Size());

                while (index >= m_elemsPerBlock * m_numBlocks.Get(MemoryOrder::ACQUIRE))
                {
                    void* mem = m_pAllocator->Allocate(m_elemSize * m_elemsPerBlock, m_elemAlignment);
                    HYP_CORE_ASSERT(mem != nullptr, "Allocation failed! Size: %u, Alignment: %u", m_elemSize * m_elemsPerBlock, m_elemAlignment);

                    m_blocks.EmplaceBack(mem);

                    m_numBlocks.Increment(1, MemoryOrder::RELEASE);

                    ++currentBlockIndex;
                }

                Block& block = m_blocks[blockIndex];
                block.numElements.Increment(1, MemoryOrder::RELEASE);

                if (outElementPtr != nullptr)
                {
                    *outElementPtr = reinterpret_cast<ElementType*>(UIntPtr(block.mem) + m_elemSize * elementIndex);
                }
            }
        }

        return index;
    }

    void ReleaseIndex(uint32 index)
    {
        HYP_SCOPE;

        m_idGenerator.ReleaseId(index + 1);

        const uint32 blockIndex = index / m_elemsPerBlock;

        if (blockIndex < m_initialNumBlocks)
        {
            Block& block = m_blocks[blockIndex];

            block.numElements.Decrement(1, MemoryOrder::RELEASE);
        }
        else
        {
            Mutex::Guard guard(m_blocksMutex);

            HYP_CORE_ASSERT(index < m_elemsPerBlock * m_numBlocks.Get(MemoryOrder::ACQUIRE));

            Block& block = m_blocks[blockIndex];
            block.numElements.Decrement(1, MemoryOrder::RELEASE);
        }
    }

    /*! \brief Ensure that the pool has enough capacity for the given index
     * After calling, you'll need to ensure that the blocks have numElements properly set,
     * or else the next call to RemoveEmptyBlocks() will just remove the newly added blocks. */
    void EnsureCapacity(uint32 index)
    {
        HYP_SCOPE;

        HYP_CORE_ASSERT(index != InvalidIndex);

        const uint32 requiredBlocks = (index + m_elemsPerBlock) / m_elemsPerBlock;

        if (requiredBlocks <= m_numBlocks.Get(MemoryOrder::ACQUIRE))
        {
            return; // already has enough blocks
        }

        Mutex::Guard guard(m_blocksMutex);

        uint32 currentBlockIndex = uint32(m_blocks.Size());

        while (requiredBlocks > m_numBlocks.Get(MemoryOrder::ACQUIRE))
        {
            void* mem = m_pAllocator->Allocate(m_elemSize * m_elemsPerBlock, m_elemAlignment);
            HYP_CORE_ASSERT(mem != nullptr, "Allocation failed! Size: %u, Alignment: %u", m_elemSize * m_elemsPerBlock, m_elemAlignment);

            m_blocks.EmplaceBack(mem);

            m_numBlocks.Increment(1, MemoryOrder::RELEASE);

            ++currentBlockIndex;
        }
    }

    template <class ElementType>
    ElementType& GetElement(uint32 index)
    {
        HYP_SCOPE;

#ifdef HYP_DEBUG_MODE
        HYP_CORE_ASSERT(index < NumAllocatedElements());

        HYP_CORE_ASSERT(sizeof(ElementType) <= m_elemSize);
        HYP_CORE_ASSERT(alignof(ElementType) <= m_elemAlignment);
#endif
        
        const uint32 blockIndex = index / m_elemsPerBlock;
        const uint32 elementIndex = index % m_elemsPerBlock;

        if (blockIndex < m_initialNumBlocks)
        {
            Block& block = m_blocks[blockIndex];

            return *reinterpret_cast<ElementType*>(UIntPtr(block.mem) + m_elemSize * elementIndex);
        }
        else
        {
            Mutex::Guard guard(m_blocksMutex);

            Block& block = m_blocks[blockIndex];

            return *reinterpret_cast<ElementType*>(UIntPtr(block.mem) + m_elemSize * elementIndex);
        }
    }

    template <class ElementType>
    void SetElement(uint32 index, const ElementType& value)
    {
        HYP_SCOPE;

#ifdef HYP_DEBUG_MODE
        HYP_CORE_ASSERT(index < NumAllocatedElements());

        HYP_CORE_ASSERT(sizeof(ElementType) <= m_elemSize);
        HYP_CORE_ASSERT(alignof(ElementType) <= m_elemAlignment);
#endif

        const uint32 blockIndex = index / m_elemsPerBlock;
        const uint32 elementIndex = index % m_elemsPerBlock;

        if (blockIndex < m_initialNumBlocks)
        {
            Block& block = m_blocks[blockIndex];

            *reinterpret_cast<ElementType*>(UIntPtr(block.mem) + m_elemSize * elementIndex) = value;
        }
        else
        {
            Mutex::Guard guard(m_blocksMutex);

            HYP_CORE_ASSERT(blockIndex < m_numBlocks.Get(MemoryOrder::ACQUIRE));

            Block& block = m_blocks[blockIndex];

            *reinterpret_cast<ElementType*>(UIntPtr(block.mem) + m_elemSize * elementIndex) = value;
        }
    }

    /*! \brief Remove empty blocks from the back of the list */
    void RemoveEmptyBlocks()
    {
        HYP_SCOPE;
        // // Must be on the owner thread to remove empty blocks.
        // AssertOnThread(m_ownerThreadId);

        if (m_numBlocks.Get(MemoryOrder::ACQUIRE) <= m_initialNumBlocks)
        {
            return;
        }

        Mutex::Guard guard(m_blocksMutex);

        typename LinkedList<Block>::Iterator beginIt = m_blocks.Begin();
        typename LinkedList<Block>::Iterator endIt = m_blocks.End();

        Array<typename LinkedList<Block>::Iterator> toRemove;

        for (uint32 blockIndex = 0; blockIndex < m_numBlocks.Get(MemoryOrder::ACQUIRE) && beginIt != endIt; ++blockIndex, ++beginIt)
        {
            if (blockIndex < m_initialNumBlocks)
            {
                continue;
            }

            if (beginIt->IsEmpty())
            {
                toRemove.PushBack(beginIt);
            }
            else
            {
                toRemove.Clear();
            }
        }

        if (toRemove.Any())
        {
            m_numBlocks.Decrement(toRemove.Size(), MemoryOrder::RELEASE);

            while (toRemove.Any())
            {
                HYP_CORE_ASSERT(&m_blocks.Back() == &*toRemove.Back());

                Block& block = *toRemove.Back();
                m_pAllocator->Free(block.mem);

                m_blocks.Erase(toRemove.PopBack());
            }
        }
    }

    void ClearUsedIndices()
    {
        HYP_SCOPE;

        // // Must be on the owner thread to reset indices.
        // AssertOnThread(m_ownerThreadId);

        m_idGenerator.Reset();
    }

protected:
    AllocatorType* m_pAllocator;

    uint32 m_elemSize;
    uint32 m_elemAlignment;

    uint32 m_elemsPerBlock;

    uint32 m_initialNumBlocks;

    LinkedList<Block> m_blocks;
    AtomicVar<uint32> m_numBlocks;
    // Needs to be locked when accessing blocks beyond initialNumBlocks or adding/removing blocks.
    Mutex m_blocksMutex;
};

// struct MemoryPoolAllocator : Allocator<MemoryPoolAllocator>
// {

// };

} // namespace memory

using memory::MemoryPool;
using memory::MemoryPoolBase;

} // namespace hyperion
