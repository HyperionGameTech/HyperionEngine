/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Memory/Memory.hpp>

#include <Core/Memory/Allocator/Allocator.hpp>

#include <Core/Utilities/ByteUtil.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Types.hpp>
#include <Core/Defines.hpp>

namespace Hyperion {
namespace memory {

struct DynamicAllocator;

/*! \brief A dynamically growing linear bump-allocator for transient allocations
    \details Allocates memory from a fixed buffer using a simple offset pointer (bump allocation) */
template <class AllocatorType>
class TArena
{
    /*! Using 64 bytes to align to common cpu cache line size
      to prevent arenas from overlapping in memory causing false sharing
      \note if the overarching allocator's maxAlign is less than 64, we have to go by that instead */
    static constexpr uint32 chunkAllocationAlignment = AllocatorType::maxAlign < 64 ? AllocatorType::maxAlign : 64;

    struct Block
    {
        Block* next;

        typename AllocatorType::template Allocation<ubyte> allocation;
        size_t offset;
    };

public:
    static constexpr uint32 maxAlign = ~0u;

    template <class T>
    struct Allocation : DynamicAllocationBase<T>
    {
    };

    /*! \param blockSize Size of a block. */
    explicit TArena(size_t blockSize);

    TArena(AllocatorType* pAllocator, size_t blockSize);

    TArena(const TArena& other) = delete;
    TArena& operator=(const TArena& other) = delete;

    TArena(TArena&& other) noexcept = delete;
    TArena& operator=(TArena&& other) noexcept = delete;

    ~TArena()
    {
        Block* block = &m_firstBlock;
        while (block != nullptr)
        {
            block->allocation.Free(m_pAllocator);

            Block* prev = block;
            block = block->next;

            if (prev != &m_firstBlock)
            {
                prev->allocation.Free(m_pAllocator);
                m_pAllocator->Free(prev);
            }
        }
    }

    /*! \brief Resets the arena by settings all blocks' bump pointer to 0 */
    HYP_FORCE_INLINE void Reset()
    {
        Block* block = &m_firstBlock;
        while (block != nullptr)
        {
            block->offset = 0;
            block = block->next;
        }
    }

    /*! \brief Allocates memory from the arena.
        \param size Number of bytes to allocate
        \param alignment Alignment requirement (must be <= 16)
        \return Pointer to allocated memory, or nullptr if out of space */
    void* Allocate(size_t size, size_t alignment);

    /*! \brief Does nothing as individual allocations from Arena cannot be freed. This method is only here to confirm to Allocator interface. */
    void Free(void* ptr)
    {
    }

private:
    AllocatorType* m_pAllocator;
    Block m_firstBlock;
    size_t m_blockSize;
};

template <class AllocatorType>
TArena<AllocatorType>::TArena(size_t blockSize)
    : m_pAllocator(GetDefaultAllocatorInstance<AllocatorType>()),
      m_blockSize(blockSize)
{
    HYP_CORE_ASSERT(m_pAllocator != nullptr);
    HYP_CORE_ASSERT(m_blockSize > 0, "Arena blockSize must be greater than 0");

    m_firstBlock = Block {};
    m_firstBlock.allocation.SetToInitialState();
    m_firstBlock.allocation.Allocate(m_pAllocator, m_blockSize, chunkAllocationAlignment);
}

template <class AllocatorType>
TArena<AllocatorType>::TArena(AllocatorType* pAllocator, size_t blockSize)
    : m_pAllocator(pAllocator),
      m_blockSize(blockSize)
{
    HYP_CORE_ASSERT(m_pAllocator != nullptr);
    HYP_CORE_ASSERT(m_blockSize > 0, "Arena blockSize must be greater than 0");

    m_firstBlock = Block {};
    m_firstBlock.allocation.SetToInitialState();
    m_firstBlock.allocation.Allocate(m_pAllocator, m_blockSize, chunkAllocationAlignment);
}

template <class AllocatorType>
void* TArena<AllocatorType>::Allocate(size_t size, size_t alignment)
{
    HYP_CORE_ASSERT(alignment != 0 && alignment <= maxAlign && ((alignment & (alignment - 1)) == 0),
                    "Arena requires power-of-two, non-zero alignment");

    Block* block = &m_firstBlock;
    bool isNewBlock = false;

    while (block != nullptr)
    {
        ubyte* base = block->allocation.GetBuffer();

        UIntPtr currentAddress = reinterpret_cast<UIntPtr>(base + block->offset);
        UIntPtr alignedAddress = ByteUtil::AlignAs(currentAddress, uint32(alignment));
        size_t padding = alignedAddress - currentAddress;
        size_t totalSize = size + padding;

        if (block->offset + totalSize <= m_blockSize)
        {
            block->offset += totalSize;

            return reinterpret_cast<void*>(alignedAddress);
        }

        if (!block->next)
        {
            if (isNewBlock)
            {
                break;
            }

            // allocation failed; allocate new block
            Block* newBlock = (Block*)m_pAllocator->Allocate(sizeof(Block), alignof(Block));

            new (newBlock) Block {};
            newBlock->offset = 0;
            newBlock->next = nullptr;
            newBlock->allocation.SetToInitialState();
            newBlock->allocation.Allocate(m_pAllocator, m_blockSize, chunkAllocationAlignment);

            block->next = newBlock;

            isNewBlock = true;
        }

        block = block->next;
    }

#if HYP_DEBUG_MODE
    HYP_FAIL("Failed to allocate from linear memory allocator!");
#endif

    return nullptr;
}

using Arena = TArena<DynamicAllocator>;

} // namespace memory

using memory::Arena;
using memory::TArena;

} // namespace Hyperion
