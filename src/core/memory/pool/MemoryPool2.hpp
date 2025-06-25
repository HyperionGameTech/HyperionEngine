/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MEMORY_POOL2_HPP
#define HYPERION_MEMORY_POOL2_HPP

#include <core/containers/Array.hpp>
#include <core/containers/Bitset.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {
namespace memory {

/*! \brief Pool for small objects using aligned pages
 *  \internal Not battle tested yet! Use at your own peril */
template <uint32 PageSize>
class Pool
{
    static_assert(MathUtil::IsPowerOfTwo(PageSize), "PageSize must be power-of-two!");
    static_assert(PageSize % sizeof(void*) == 0, "PageSize must be a multiple of sizeof(void*)!");
    static_assert(PageSize <= 8192, "PageSize should be <= 8192 (will fail to allocate on Windows)");

    template <class Scalar>
    static inline constexpr Scalar AlignAs(Scalar value, uint32 alignment)
    {
        return ((value + alignment - 1) / alignment) * alignment;
    }

    struct PageBody;
    struct PageFooter;

    static_assert(PageSize > sizeof(PageFooter), "PageSize must be > sizeof(PageFooter)");

    struct PageFooter
    {
        Bitset used_indices;

        PageFooter()
        {
        }

        HYP_FORCE_INLINE PageBody* GetBody() const
        {
            return reinterpret_cast<PageBody*>(reinterpret_cast<uintptr_t>(this) - sizeof(PageBody));
        }
    };

    struct PageBody
    {
        ValueStorageArray<ubyte, PageSize - sizeof(PageFooter), 1> storage;

        HYP_FORCE_INLINE PageFooter* GetFooter() const
        {
            return reinterpret_cast<PageFooter*>(reinterpret_cast<uintptr_t>(this) + sizeof(PageBody));
        }
    };

public:
    static constexpr bool is_contiguous = false;

    Pool(uint32 elem_size, uint32 elem_alignment = 16)
        : m_size(0),
          m_elem_size(elem_size),
          m_elem_alignment(elem_alignment)
    {
        AssertThrowMsg(AlignAs(elem_size, elem_alignment) <= PageSize, "page size not large enough to fit a single element");
    }

    Pool(const Pool& other) = delete;
    Pool& operator=(const Pool& other) = delete;

    Pool(Pool&& other) noexcept
        : m_pages(std::move(other.m_pages)),
          m_size(other.m_size)
    {
        other.m_size = 0;
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
        FreeMemory();
    }

    void* Alloc(uint32 size, uint32 alignment)
    {
        AssertThrowMsg(size <= m_elem_size, "Requested size %u is larger than maximum element size %u", size, m_elem_size);
        AssertThrowMsg(alignment <= m_elem_alignment, "Requested alignment %u is larger than maximum element alignment %u", alignment, m_elem_alignment);

        PageBody* body = nullptr;
        PageFooter* footer = nullptr;

        if (m_pages.Empty())
        {
            // may end up needing to allocate more bytes than sizeof(PageBody) + sizeof(PageFooter) to get the size to be a multiple of PageSize.
            void* vp = (HYP_ALLOC_ALIGNED(AlignAs(sizeof(PageBody) + sizeof(PageFooter), PageSize), PageSize));

            if (!vp)
            {
                HYP_FAIL("Failed to allocate page!!");
            }

            AssertDebug(reinterpret_cast<uintptr_t>(vp) % PageSize == 0);

            body = new (vp) PageBody;
            footer = new (static_cast<char*>(vp) + sizeof(PageBody)) PageFooter;

            m_pages.PushBack(body);
        }
        else
        {
            for (SizeType i = m_pages.Size(); i; i--)
            {
                PageBody* pg = m_pages[i - 1];
                PageFooter* foot = pg->GetFooter();

                Bitset::BitIndex bit = foot->used_indices.FirstZeroBitIndex();

                if (bit < MaxElementsPerPage())
                {
                    body = pg;
                    footer = foot;

                    break;
                }
            }

            if (!footer)
            {
                void* vp = (HYP_ALLOC_ALIGNED(AlignAs(sizeof(PageBody) + sizeof(PageFooter), PageSize), PageSize));

                if (!vp)
                {
                    HYP_FAIL("Failed to allocate page!!");
                }

                AssertDebug(reinterpret_cast<uintptr_t>(vp) % uintptr_t(PageSize) == 0);

                body = new (vp) PageBody;
                footer = new (static_cast<char*>(vp) + sizeof(PageBody)) PageFooter;

                m_pages.PushBack(body);
            }
        }

        AssertDebug(footer->used_indices.Count() < MaxElementsPerPage());

        void* ptr = AllocInPage(body, size, alignment);
        AssertDebug(ptr);

        // sanity
        volatile PageFooter* footer2 = GetFooterFromPointer(ptr);
        AssertDebug(footer2 == footer);

        ++m_size;

        return ptr;
    }

    void* Alloc()
    {
        return Alloc(m_elem_size, m_elem_alignment);
    }

    void Free(void* ptr)
    {
        AssertDebug(ptr != nullptr);
        AssertDebug(m_size != 0);

        PageBody* body = GetBodyFromPointer(ptr);

        uint32 idx = IndexForElement(body, ptr);
        AssertDebug(idx < MaxElementsPerPage());

        body->GetFooter()->used_indices.Set(idx, false);

        // ptr->~T();

        --m_size;
    }

    HYP_FORCE_INLINE uint32 Size() const
    {
        return m_size;
    }

private:
    HYP_FORCE_INLINE static PageBody* GetBodyFromPointer(void* ptr)
    {
        static constexpr uintptr_t mask = ~(uintptr_t(PageSize) - 1);
        return reinterpret_cast<PageBody*>((reinterpret_cast<uintptr_t>(ptr) & mask));
    }

    HYP_FORCE_INLINE static PageFooter* GetFooterFromPointer(void* ptr)
    {
        static constexpr uintptr_t mask = ~(uintptr_t(PageSize) - 1);
        return reinterpret_cast<PageFooter*>((reinterpret_cast<uintptr_t>(ptr) & mask) + sizeof(PageBody));
    }

    HYP_FORCE_INLINE static PageFooter* GetFooterFromBody(PageBody* body)
    {
        return reinterpret_cast<PageFooter*>(reinterpret_cast<uintptr_t>(body) + sizeof(PageBody));
    }

    HYP_FORCE_INLINE void* AllocInPage(PageBody* body, uint32 size, uint32 alignment)
    {
        AssertDebug(body);
        AssertDebug(size <= m_elem_size);
        AssertDebug(alignment <= m_elem_alignment);

        PageFooter* footer = reinterpret_cast<PageFooter*>(reinterpret_cast<uintptr_t>(body) + sizeof(PageBody));

        if (footer->used_indices.Count() == MaxElementsPerPage())
        {
            // need to alloc new page
            return nullptr;
        }

        uint32 elem_index = ~0u;

        for (uint32 i = 0; i < MaxElementsPerPage(); ++i)
        {
            if (!footer->used_indices.Get(i))
            {
                elem_index = i;
                footer->used_indices.Set(i, true);
                break;
            }
        }

        AssertDebug(elem_index != ~0u);

        void* ptr = reinterpret_cast<void*>(HYP_ALIGN_ADDRESS(GetStorageAddress(body) + (m_elem_size * elem_index), alignment));
        AssertDebug(uintptr_t(ptr) < (reinterpret_cast<uintptr_t>(GetStorageAddress(body)) + (MaxElementsPerPage() * m_elem_size)));

        return ptr;
    }

    void FreeMemory(bool keep_pages = false)
    {
        for (SizeType i = m_pages.Size(); i != 0; i--)
        {
            PageBody* body = m_pages[i - 1];
            PageFooter* footer = body->GetFooter();

            // void* ptr_aligned = reinterpret_cast<void*>(HYP_ALIGN_ADDRESS(body->storage.GetPointer(), m_elem_alignment));

            Bitset::BitIndex bit_index = Bitset::not_found;
            while ((bit_index = footer->used_indices.LastSetBitIndex()) != Bitset::not_found)
            {
                // (ptr_aligned + bit_index)->~T();
                footer->used_indices.Set(bit_index, false);
            }

            if (!keep_pages)
            {
                // Only have to destruct footer; PageBody is trivial and we destruct each element
                footer->~PageFooter();

                // Free the page (body is the start of the allocation)
                HYP_FREE_ALIGNED(body);
            }
        }

        if (!keep_pages)
        {
            m_pages.Clear();
        }

        m_size = 0;
    }

    HYP_FORCE_INLINE uintptr_t GetStorageAddress(PageBody* body) const
    {
        return reinterpret_cast<uintptr_t>(HYP_ALIGN_ADDRESS(reinterpret_cast<uintptr_t>(body) + offsetof(PageBody, storage), m_elem_alignment));
    }

    HYP_FORCE_INLINE uint32 IndexForElement(PageBody* body, const void* ptr) const
    {
        return (reinterpret_cast<uintptr_t>(ptr) - GetStorageAddress(body)) / m_elem_size;
    }

    HYP_FORCE_INLINE void* ElementAtIndex(PageBody* body, uint32 idx) const
    {
        return reinterpret_cast<void*>(GetStorageAddress(body) + (idx * m_elem_size));
    }

    Array<PageBody*, InlineAllocator<32>> m_pages;
    uint32 m_size;
    uint32 m_elem_size;
    uint32 m_elem_alignment;

    HYP_FORCE_INLINE const uint32 MaxElementsPerPage() const
    {
        return PageSize / m_elem_size;
    }
};

} // namespace memory

template <uint32 PageSize = 16>
using Pool = memory::Pool<PageSize>;

} // namespace hyperion

#endif
