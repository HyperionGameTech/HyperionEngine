/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/memory/allocator/Allocator.hpp>

#include <Core/memory/allocator/SlabAllocator.hpp>

#include <Core/containers/Array.hpp>

namespace Hyperion {
namespace containers {

template <class TAllocator>
class TStridedBuffer
{
public:
    TStridedBuffer(size_t elementSize, size_t alignment, size_t blocksPerSlab = 256)
        : m_allocator(elementSize, alignment, blocksPerSlab)
    {
    }

    TStridedBuffer(TAllocator* pAllocator, size_t elementSize, size_t alignment, size_t blocksPerSlab = 256)
        : m_allocator(pAllocator, elementSize, alignment, blocksPerSlab)
    {
    }

    TStridedBuffer(const TStridedBuffer& other) = delete;
    TStridedBuffer& operator=(const TStridedBuffer& other) = delete;

    TStridedBuffer(TStridedBuffer&& other) noexcept = delete;
    TStridedBuffer& operator=(TStridedBuffer&& other) noexcept = delete;

    ~TStridedBuffer() = default;

    template <class T>
    HYP_FORCE_INLINE const T* GetElement(size_t index) const
    {
        AssertDebug(sizeof(T) <= m_allocator.GetBlockSize(), "Element type is too large for the buffer's block size!");

        if (index >= m_blocks.Size())
        {
            return nullptr;
        }

        return reinterpret_cast<const T*>(m_blocks[index]);
    }

    HYP_FORCE_INLINE const ubyte* GetElementRaw(size_t index) const
    {
        if (index >= m_blocks.Size())
        {
            return nullptr;
        }

        return m_blocks[index];
    }

    template <class T>
    void SetElement(size_t index, const T& value)
    {
        if (index >= m_blocks.Size())
        {
            m_blocks.Resize(index + 1);
        }

        if (!m_blocks[index])
        {
            m_blocks[index] = (ubyte*)m_allocator.Allocate();
            AssertDebug(m_blocks[index] != nullptr);
            
            new (m_blocks[index]) T(value);

            return;
        }

        *reinterpret_cast<T*>(m_blocks[index]) = value;
    }

    template <class T>
    void DeleteElement(size_t index)
    {
        AssertDebug(index < m_blocks.Size());
        if (!m_blocks[index])
        {
            return;
        }

        if (m_blocks[index] != nullptr)
        {
            reinterpret_cast<T*>(m_blocks[index])->~T();
            m_allocator.Free(m_blocks[index]);
        }

        m_blocks[index] = nullptr;
    }

    void DeleteElementRaw(size_t index, void (*dtor)(void*))
    {
        AssertDebug(index < m_blocks.Size());
        if (!m_blocks[index])
        {
            return;
        }
        
        if (m_blocks[index] != nullptr)
        {
            if (dtor != nullptr)
            {
                dtor(m_blocks[index]);
            }

            m_allocator.Free(m_blocks[index]);
        }

        m_blocks[index] = nullptr;
    }

    HYP_FORCE_INLINE bool HasIndex(size_t index) const
    {
        if (index >= m_blocks.Size())
        {
            return false;
        }

        return m_blocks[index] != nullptr;
    }

    /*! \brief Clear all elements, optionally calling a destructor on each element. */
    void Clear(void(*dtor)(void*) = nullptr)
    {
        if (dtor)
        {
            for (size_t i = 0; i < m_blocks.Size(); i++)
            {
                if (m_blocks[i] != nullptr)
                {
                    dtor(m_blocks[i]);
                }
            }
        }

        for (size_t i = 0; i < m_blocks.Size(); i++)
        {
            if (m_blocks[i] != nullptr)
            {
                m_allocator.Free(m_blocks[i]);
            }
        }

        m_blocks.Clear();
    }

    HYP_FORCE_INLINE size_t NumActiveAllocations() const
    {
        return m_allocator.GetMemoryMetrics()[MemoryMetrics::MM_ALLOCATIONS_ACTIVE];
    }

private:
    using SlabAllocatorType = memory::TSlabAllocator<TAllocator>;

    SlabAllocatorType m_allocator;
    Array<ubyte*, TAllocator> m_blocks;
};

} // namespace containers

template <class TAllocator = DynamicAllocator>
using StridedBuffer = containers::TStridedBuffer<TAllocator>;

} // namespace Hyperion
