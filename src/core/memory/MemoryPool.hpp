/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MEMORY_POOL_HPP
#define HYPERION_MEMORY_POOL_HPP

#include <core/Handle.hpp>

#include <core/containers/HeapArray.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>

#include <core/memory/Memory.hpp>

#include <core/IDGenerator.hpp>

#include <Types.hpp>

namespace hyperion {
namespace memory {

template <class ElementType, uint32 NumElementsPerBlock = 16>
class MemoryPool
{
protected:
    static constexpr uint32 num_elements_per_block = NumElementsPerBlock;

    struct Block
    {
        FixedArray<ElementType, num_elements_per_block>         elements;
        AtomicVar<uint32>                                       num_elements { 0 };

#ifdef HYP_ENABLE_MT_CHECK
        FixedArray<DataRaceDetector, num_elements_per_block>    data_race_detectors;
#endif

        Block()
        {
            elements = { };
        }

        HYP_FORCE_INLINE bool IsEmpty() const
            { return num_elements.Get(MemoryOrder::ACQUIRE) == 0; }
    };

public:
    static constexpr uint32 s_invalid_index = ~0u;

    MemoryPool(uint32 initial_count = 16 * num_elements_per_block)
        : m_initial_num_blocks((initial_count + num_elements_per_block - 1) / num_elements_per_block),
          m_num_blocks(m_initial_num_blocks)
    {
        for (uint32 i = 0; i < m_initial_num_blocks; i++) {
            m_blocks.EmplaceBack();
        }
    }

    HYP_FORCE_INLINE uint32 NumAllocatedElements() const
    {
        return m_num_blocks.Get(MemoryOrder::ACQUIRE) * num_elements_per_block;
    }

    uint32 AcquireIndex()
    {
        const uint32 index = m_id_generator.NextID() - 1;

        const uint32 block_index = index / num_elements_per_block;

        if (block_index < m_initial_num_blocks) {
            Block &block = m_blocks[block_index];
            block.num_elements.Increment(1, MemoryOrder::RELEASE);
        } else {
            Mutex::Guard guard(m_blocks_mutex);

            if (index < num_elements_per_block * m_num_blocks.Get(MemoryOrder::ACQUIRE)) {
                Block &block = m_blocks[block_index];
                block.num_elements.Increment(1, MemoryOrder::RELEASE);
            } else {
                // Add blocks until we can insert the element
                while (index >= num_elements_per_block * m_num_blocks.Get(MemoryOrder::ACQUIRE)) {
                    m_blocks.EmplaceBack();
                    m_num_blocks.Increment(1, MemoryOrder::RELEASE);
                }

                Block &block = m_blocks[block_index];
                block.num_elements.Increment(1, MemoryOrder::RELEASE);
            }
        }

        return index;
    }

    void ReleaseIndex(uint32 index)
    {
        m_id_generator.FreeID(index + 1);

        const uint32 block_index = index / num_elements_per_block;

        if (block_index < m_initial_num_blocks) {
            Block &block = m_blocks[block_index];

            block.num_elements.Decrement(1, MemoryOrder::RELEASE);
        } else {
            Mutex::Guard guard(m_blocks_mutex);

            AssertThrow(index < num_elements_per_block * m_num_blocks.Get(MemoryOrder::ACQUIRE));

            Block &block = m_blocks[block_index];
            block.num_elements.Decrement(1, MemoryOrder::RELEASE);
        }
    }

    ElementType &GetElement(uint32 index)
    {
        AssertThrow(index < NumAllocatedElements());

        const uint32 block_index = index / num_elements_per_block;
        const uint32 element_index = index % num_elements_per_block;

        if (block_index < m_initial_num_blocks) {
            Block &block = m_blocks[block_index];
            //HYP_MT_CHECK_READ(block.data_race_detectors[element_index]);

            return block.elements[element_index];
        } else {
            Mutex::Guard guard(m_blocks_mutex);

            Block &block = m_blocks[block_index];
            //HYP_MT_CHECK_READ(block.data_race_detectors[element_index]);
            
            return block.elements[element_index];
        }
    }

    void SetElement(uint32 index, const ElementType &value)
    {
        AssertThrow(index < NumAllocatedElements());

        const uint32 block_index = index / num_elements_per_block;
        const uint32 element_index = index % num_elements_per_block;

        if (block_index < m_initial_num_blocks) {
            Block &block = m_blocks[block_index];
            //HYP_MT_CHECK_RW(block.data_race_detectors[element_index]);
                
            block.elements[element_index] = value;
        } else {
            Mutex::Guard guard(m_blocks_mutex);

            AssertThrow(block_index < m_num_blocks.Get(MemoryOrder::ACQUIRE));

            Block &block = m_blocks[block_index];
            //HYP_MT_CHECK_RW(block.data_race_detectors[element_index]);
                
            block.elements[element_index] = value;
        }
    }

    /*! \brief Remove empty blocks from the back of the list */
    void RemoveEmptyBlocks()
    {
        if (m_num_blocks.Get(MemoryOrder::ACQUIRE) <= m_initial_num_blocks) {
            return;
        }

        Mutex::Guard guard(m_blocks_mutex);

        typename LinkedList<Block>::Iterator begin_it = m_blocks.Begin();
        typename LinkedList<Block>::Iterator end_it = m_blocks.End();

        Array<typename LinkedList<Block>::Iterator> to_remove;

        for (uint32 block_index = 0; block_index < m_num_blocks.Get(MemoryOrder::ACQUIRE) && begin_it != end_it; ++block_index, ++begin_it) {
            if (block_index < m_initial_num_blocks) {
                continue;
            }

            if (begin_it->IsEmpty()) {
                to_remove.PushBack(begin_it);
            } else {
                to_remove.Clear();
            }
        }

        if (to_remove.Any()) {
            m_num_blocks.Decrement(to_remove.Size(), MemoryOrder::RELEASE);

            while (to_remove.Any()) {
                m_blocks.Erase(to_remove.PopBack());
            }
        }
    }

protected:
    uint32                          m_initial_num_blocks;

    LinkedList<Block>               m_blocks;
    AtomicVar<uint32>               m_num_blocks;
    // Needs to be locked when accessing blocks beyond initial_num_blocks or adding/removing blocks.
    Mutex                           m_blocks_mutex;

    IDGenerator                     m_id_generator;
};

} // namespace memory

using memory::MemoryPool;

} // namespace hyperion

#endif