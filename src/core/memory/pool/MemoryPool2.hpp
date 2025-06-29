/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MEMORY_POOL2_HPP
#define HYPERION_MEMORY_POOL2_HPP

#include <core/containers/Array.hpp>
#include <core/containers/Bitset.hpp>
#include <core/containers/SparsePagedArray.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {
namespace memory {

/*! \brief Pool for small objects using aligned pages
 *  \internal Not battle tested yet! Use at your own peril */
template <uint32 PageSize>
class Pool : SparsePagedArray<ubyte, PageSize>
{
    static_assert(MathUtil::IsPowerOfTwo(PageSize), "PageSize must be power-of-two!");

public:
    Pool()
    {
    }

    Pool(const Pool& other) = delete;
    Pool& operator=(const Pool& other) = delete;

    Pool(Pool&& other) noexcept
    {
    }

    Pool& operator=(Pool&& other) noexcept = delete;

    // Pool& operator=(Pool&& other) noexcept
    // {
    //     if (this == &other)
    //     {
    //         return *this;
    //     }

    //     Clear();

    //     m_pages = std::move(other.m_pages);
    //     m_size = other.m_size;

    //     other.m_size = 0;

    //     return *this;
    // }

    ~Pool()
    {
    }

    void* Alloc(SizeType size, SizeType alignment)
    {
    }

protected:
};

} // namespace memory

template <uint32 PageSize = 8192>
using Pool = memory::Pool<PageSize>;

} // namespace hyperion

#endif
