/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/Memory.hpp>

#include <core/utilities/ByteUtil.hpp>

#include <core/debug/Debug.hpp>

#include <core/Types.hpp>
#include <core/Defines.hpp>

namespace hyperion {
namespace memory {

struct DynamicAllocator;

/*! \brief A fixed-size, bump-allocator arena for temporary allocations.
    \details Allocates memory from a fixed buffer using a simple offset pointer (bump allocation) */
template <class AllocatorType = DynamicAllocator>
class TLinearArena
{
public:
    /*! \brief Creates a TLinearArena with the specified fixed size.
        \param size Total size of the arena in bytes. Must be > 0. */
    explicit TLinearArena(SizeType size);

    TLinearArena(const TLinearArena& other) = delete;
    TLinearArena& operator=(const TLinearArena& other) = delete;

    TLinearArena(TLinearArena&& other) noexcept;
    TLinearArena& operator=(TLinearArena&& other) noexcept;

    ~TLinearArena() = default;

    /*! \brief Returns the total capacity of the arena in bytes. */
    HYP_FORCE_INLINE SizeType GetCapacity() const
    {
        return m_buffer.Size();
    }

    /*! \brief Returns the current offset (number of bytes allocated). */
    HYP_FORCE_INLINE SizeType GetOffset() const
    {
        return m_offset;
    }

    /*! \brief Returns the number of bytes remaining in the arena. */
    HYP_FORCE_INLINE SizeType GetRemaining() const
    {
        return m_buffer.Size() - m_offset;
    }

    /*! \brief Resets the arena offset to 0 and frees up memory if freeMemory is passed as true */
    HYP_FORCE_INLINE void Reset(bool freeMemory = false)
    {
        m_offset = 0;

        if (freeMemory)
        {
            m_buffer.SetCapacity(0);
        }
    }

    /*! \brief Allocates memory from the arena.
        \param size Number of bytes to allocate
        \param alignment Alignment requirement (must be <= 16)
        \return Pointer to allocated memory, or nullptr if out of space */
    void* Alloc(SizeType size, SizeType alignment);

private:
    TByteBuffer<AllocatorType> m_buffer;
    SizeType m_offset = 0;
};

template <class AllocatorType>
TLinearArena<AllocatorType>::TLinearArena(SizeType size)
    : m_offset(0)
{
    HYP_CORE_ASSERT(size > 0, "LinearArena size must be greater than 0");

    m_buffer.SetSize(size);
}

template <class AllocatorType>
TLinearArena<AllocatorType>::TLinearArena(TLinearArena&& other) noexcept
    : m_buffer(std::move(other.m_buffer)),
      m_offset(other.m_offset)
{
    other.m_offset = 0;
}
template <class AllocatorType>
TLinearArena<AllocatorType>& TLinearArena<AllocatorType>::operator=(TLinearArena&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    m_buffer = std::move(other.m_buffer);
    m_offset = other.m_offset;

    other.m_offset = 0;

    return *this;
}

template <class AllocatorType>
void* TLinearArena<AllocatorType>::Alloc(SizeType size, SizeType alignment)
{
    HYP_CORE_ASSERT(alignment <= 16, "LinearArena only supports alignment up to 16 bytes");

    const SizeType alignedOffset = ByteUtil::AlignAs(m_offset, alignment);

    if (alignedOffset + size > m_buffer.Size())
    {
        HYP_CORE_ASSERT(false, "LinearArena out of memory: tried to allocate %llu bytes (aligned to %llu), but only %llu bytes remaining",
            size, alignment, m_buffer.Size() - alignedOffset);

        return nullptr;
    }

    void* ptr = m_buffer.Data() + alignedOffset;
    m_offset = alignedOffset + size;

    return ptr;
}

using LinearArena = TLinearArena<DynamicAllocator>;

} // namespace memory

using memory::LinearArena;
using memory::TLinearArena;

} // namespace hyperion
