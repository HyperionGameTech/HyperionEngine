/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/Memory.hpp>

#include <core/memory/allocator/Allocator.hpp>

#include <core/utilities/ByteUtil.hpp>

#include <core/debug/Debug.hpp>

#include <core/Types.hpp>
#include <core/Defines.hpp>

namespace hyperion {
namespace memory {

struct DynamicAllocator;

/*! \brief A fixed-size, bump-allocator arena for temporary allocations.
    \details Allocates memory from a fixed buffer using a simple offset pointer (bump allocation) */
template <class AllocatorType>
class TArena
{
public:
    template <class T>
    struct Allocation : DynamicAllocationBase<T>
    {
    };

    /*! \brief Creates a TArena with the specified fixed size.
        \param size Total size of the arena in bytes. Must be > 0. */
    explicit TArena(SizeType size);

    TArena(AllocatorType* pAllocator, SizeType size);

    TArena(const TArena& other) = delete;
    TArena& operator=(const TArena& other) = delete;

    TArena(TArena&& other) noexcept;
    TArena& operator=(TArena&& other) noexcept;

    ~TArena() = default;

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

    /*! \brief Resets the arena, allowing all memory to be re-allocated. */
    HYP_FORCE_INLINE void Reset()
    {
        m_offset = 0;
    }

    /*! \brief Allocates memory from the arena.
        \param size Number of bytes to allocate
        \param alignment Alignment requirement (must be <= 16)
        \return Pointer to allocated memory, or nullptr if out of space */
    void* Allocate(SizeType size, SizeType alignment);

    /*! \brief Does nothing as individual allocations from Arena cannot be freed. This method is only here to confirm to Allocator interface. */
    void Free(void* ptr)
    {
        // Do nothing; can't free from Arena
    }

private:
    TByteBuffer<AllocatorType> m_buffer;
    SizeType m_offset;
};

template <class AllocatorType>
TArena<AllocatorType>::TArena(SizeType size)
    : m_offset(0)
{
    HYP_CORE_ASSERT(size > 0, "Arena size must be greater than 0");

    m_buffer.SetSize(size);
}

template <class AllocatorType>
TArena<AllocatorType>::TArena(AllocatorType* pAllocator, SizeType size)
    : m_buffer(pAllocator),
      m_offset(0)
{
    HYP_CORE_ASSERT(pAllocator != nullptr);
    HYP_CORE_ASSERT(size > 0, "Arena size must be greater than 0");

    m_buffer.SetSize(size);
}

template <class AllocatorType>
TArena<AllocatorType>::TArena(TArena&& other) noexcept
    : m_buffer(std::move(other.m_buffer)),
      m_offset(other.m_offset)
{
    other.m_offset = 0;
}

template <class AllocatorType>
TArena<AllocatorType>& TArena<AllocatorType>::operator=(TArena&& other) noexcept
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
void* TArena<AllocatorType>::Allocate(SizeType size, SizeType alignment)
{
    HYP_CORE_ASSERT(alignment != 0 && ((alignment & (alignment - 1)) == 0),
        "Arena requires power-of-two, non-zero alignment");

    ubyte* base = static_cast<ubyte*>(static_cast<void*>(m_buffer.Data()));
    const uintptr_t baseAddr = reinterpret_cast<uintptr_t>(base);
    const uintptr_t current = baseAddr + static_cast<uintptr_t>(m_offset);
    const uintptr_t aligned = ByteUtil::AlignAs(current, static_cast<uintptr_t>(alignment));
    const SizeType alignedOffset = static_cast<SizeType>(aligned - baseAddr);

    // Use subtraction form to avoid overflow.
    if (size > m_buffer.Size() - alignedOffset)
    {
        HYP_CORE_ASSERT(false,
            "Arena out of memory: requested=%llu, align=%llu, remaining=%llu",
            size, alignment, m_buffer.Size() - alignedOffset);
        return nullptr;
    }

    void* ptr = base + alignedOffset;
    m_offset = alignedOffset + size;
    return ptr;
}

using Arena = TArena<DynamicAllocator>;

} // namespace memory

using memory::Arena;
using memory::TArena;

} // namespace hyperion
