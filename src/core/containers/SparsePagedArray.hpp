/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SPARSE_PAGED_ARRAY_HPP
#define HYPERION_SPARSE_PAGED_ARRAY_HPP

#include <core/containers/Array.hpp>
#include <core/containers/Bitset.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {
namespace containers {

/*! \brief SparsePagedArray defines a container that stores elements in pages of a fixed size, allowing for sparse storage.
 *  \details This container is useful when you have a large number of elements, but only a small subset of them are initialized.
 *  Another useful feature of this container: pointers to elements will become dangling, even if the container grows and shrinks.
 *  (as long as the elements themselves are not removed) */
template <class T, uint32 PageSize>
class SparsePagedArray : public ContainerBase<SparsePagedArray<T, PageSize>, SizeType>
{
    struct Page
    {
        ValueStorageArray<T, PageSize, alignof(T)> storage;
        Bitset initialized_bits;

        Page() = default;

        // No move/copy operations, Page is stored as a pointer so these are unnecessary and
        // would only introduce complexity.
        Page(const Page& other) = delete;
        Page& operator=(Page& other) = delete;
        Page(Page&& other) noexcept = delete;
        Page& operator=(Page&& other) noexcept = delete;

        ~Page()
        {
            for (Bitset::BitIndex bit : initialized_bits)
            {
                storage.GetPointer()[bit].~T();
            }
        }
    };

public:
    static constexpr bool is_contiguous = false;

    struct Iterator
    {
        SparsePagedArray* array;
        uint32 page;
        uint32 elem;

        Iterator(SparsePagedArray* array, uint32 page_index, uint32 element_index)
            : array(array),
              page(page_index),
              elem(element_index)
        {
            AssertDebug(array);

            if (page >= uint32(array->m_pages.Size()))
            {
                page = uint32(array->m_pages.Size());
                elem = PageSize;

                return;
            }

            while (page != ~0u)
            {
                if (array->m_valid_pages.Test(page))
                {
                    Bitset::BitIndex next_bit = array->m_pages[page]->initialized_bits.NextSetBitIndex(elem);

                    if (next_bit < PageSize)
                    {
                        elem = next_bit;

                        break;
                    }
                }

                page = array->m_valid_pages.NextSetBitIndex(page + 1);

                // no valid pages remaining after this
                if (page == ~0u)
                {
                    page = uint32(array->m_pages.Size());
                    elem = PageSize;

                    return;
                }

                // reset for next iter
                elem = 0;
            }

            // do
            // {
            //     if (*pg)
            //     {
            //         Bitset::BitIndex next_bit = (*pg)->initialized_bits.NextSetBitIndex(elem);

            //         if (next_bit < PageSize)
            //         {
            //             elem = next_bit;

            //             break;
            //         }
            //     }

            //     ++pg;
            //     elem = 0;
            // }
            // while (pg != array->m_pages.End());
        }

        T& operator*() const
        {
            AssertDebug(page < array->m_pages.Size());
            AssertDebug(elem < PageSize);

            return array->m_pages[page]->storage.GetPointer()[elem];
        }

        T* operator->() const
        {
            AssertDebug(page < array->m_pages.Size());
            AssertDebug(elem < PageSize);

            return &array->m_pages[page]->storage.GetPointer()[elem];
        }

        Iterator& operator++()
        {
            AssertDebug(page < array->m_pages.Size());

            while (true)
            {
                if (page != ~0u && array->m_valid_pages.Test(page))
                {
                    Bitset::BitIndex next_bit = array->m_pages[page]->initialized_bits.NextSetBitIndex(elem + 1);

                    if (next_bit < PageSize)
                    {
                        elem = next_bit;

                        break;
                    }
                }

                // try to move to next page
                page = array->m_valid_pages.NextSetBitIndex(page + 1);

                // No more valid pages
                if (page == ~0u)
                {
                    page = uint32(array->m_pages.Size());
                    elem = PageSize;
                    break;
                }

                // Flip back around for next page (~0u + 1 == 0)
                elem = ~0u;
            }

            return *this;
        }

        Iterator operator++(int)
        {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const Iterator& other) const
        {
            return page == other.page && elem == other.elem;
        }

        bool operator!=(const Iterator& other) const
        {
            return !(*this == other);
        }
    };

    // No real difference between ConstIterator and Iterator in this case
    using ConstIterator = Iterator;

    SparsePagedArray()
    {
    }

    SparsePagedArray(const SparsePagedArray& other) = delete;
    SparsePagedArray& operator=(const SparsePagedArray& other) = delete;

    SparsePagedArray(SparsePagedArray&& other) noexcept
        : m_pages(std::move(other.m_pages)),
          m_valid_pages(std::move(other.m_valid_pages))
    {
    }

    SparsePagedArray& operator=(SparsePagedArray&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        for (Bitset::BitIndex bit : m_valid_pages)
        {
            AssertDebug(bit < m_pages.Size());

            delete m_pages[bit];
            m_pages[bit] = nullptr;
        }

        m_pages = std::move(other.m_pages);
        m_valid_pages = std::move(other.m_valid_pages);

        return *this;
    }

    ~SparsePagedArray()
    {
        for (Bitset::BitIndex bit : m_valid_pages)
        {
            AssertDebug(bit < m_pages.Size());

            delete m_pages[bit];
            m_pages[bit] = nullptr;
        }
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return m_valid_pages.Count() == 0;
    }

    HYP_FORCE_INLINE bool Any() const
    {
        return m_valid_pages.Count() != 0;
    }

    HYP_FORCE_INLINE SizeType Size() const
    {
        SizeType size = 0;

        for (Bitset::BitIndex bit : m_valid_pages)
        {
            size += m_pages[bit]->initialized_bits.Count();
        }

        return size;
    }

    HYP_FORCE_INLINE T& Front()
    {
        AssertDebug(m_valid_pages.Count() != 0);

        return *Begin();
    }

    HYP_FORCE_INLINE const T& Front() const
    {
        AssertDebug(m_valid_pages.Count() != 0);

        return *Begin();
    }

    HYP_FORCE_INLINE T& Back()
    {
        AssertDebug(m_valid_pages.Count() != 0);

        Page* last_page = m_pages[m_valid_pages.LastSetBitIndex()];

        AssertDebug(last_page != nullptr);
        AssertDebug(last_page->initialized_bits.Count() > 0);

        SizeType last_index = last_page->initialized_bits.LastSetBitIndex();
        AssertDebug(last_index < PageSize);

        return last_page->storage.GetPointer()[last_index];
    }

    HYP_FORCE_INLINE const T& Back() const
    {
        AssertDebug(m_valid_pages.Count() != 0);

        Page* last_page = m_pages[m_valid_pages.LastSetBitIndex()];

        AssertDebug(last_page != nullptr);
        AssertDebug(last_page->initialized_bits.Count() > 0);

        SizeType last_index = last_page->initialized_bits.LastSetBitIndex();
        AssertDebug(last_index < PageSize);

        return last_page->storage.GetPointer()[last_index];
    }

    HYP_FORCE_INLINE bool HasIndex(SizeType index) const
    {
        const SizeType page_index = PageIndex(index);
        const SizeType element_index = ElementIndex(index);

        if (!m_valid_pages.Test(page_index))
        {
            return false;
        }

        AssertDebug(m_pages[page_index] != nullptr);

        return m_pages[page_index]->initialized_bits.Test(element_index);
    }

    T& operator[](SizeType index)
    {
        const SizeType page_index = PageIndex(index);
        const SizeType element_index = ElementIndex(index);

        AssertDebug(page_index < m_pages.Size());
        AssertDebug(m_pages[page_index] != nullptr);

        return m_pages[page_index]->storage.GetPointer()[element_index];
    }

    const T& operator[](SizeType index) const
    {
        return const_cast<SparsePagedArray&>(*this)[index];
    }

    Iterator Set(SizeType index, const T& value)
    {
        SizeType page_index = PageIndex(index);
        SizeType element_index = ElementIndex(index);

        Page* page = GetOrAllocatePage(page_index);
        AssertDebug(page != nullptr);

        if (page->initialized_bits.Test(element_index))
        {
            page->storage.DestructElement(element_index);
        }

        page->storage.ConstructElement(element_index, value);
        page->initialized_bits.Set(element_index, true);

        return Iterator(this, page_index, element_index);
    }

    Iterator Set(SizeType index, T&& value)
    {
        SizeType page_index = PageIndex(index);
        SizeType element_index = ElementIndex(index);

        Page* page = GetOrAllocatePage(page_index);
        AssertDebug(page != nullptr);

        if (page->initialized_bits.Test(element_index))
        {
            page->storage.DestructElement(element_index);
        }

        page->storage.ConstructElement(element_index, std::move(value));
        page->initialized_bits.Set(element_index, true);

        return Iterator(this, page_index, element_index);
    }

    template <class... Args>
    Iterator Emplace(SizeType index, Args&&... args)
    {
        SizeType page_index = PageIndex(index);
        SizeType element_index = ElementIndex(index);

        Page* page = GetOrAllocatePage(page_index);
        AssertDebug(page != nullptr);

        if (page->initialized_bits.Test(element_index))
        {
            page->storage.DestructElement(element_index);
        }

        page->storage.ConstructElement(element_index, std::forward<Args>(args)...);
        page->initialized_bits.Set(element_index, true);

        return Iterator(this, page_index, element_index);
    }

    Iterator EraseAt(SizeType index)
    {
        SizeType page_index = PageIndex(index);
        SizeType element_index = ElementIndex(index);

        if (!m_valid_pages.Test(page_index) || !m_pages[page_index]->initialized_bits.Test(element_index))
        {
            // Element does not exist, nothing to erase
            // note: iterator will automatically skip to next
            return Iterator(this, page_index, element_index);
        }

        Page* page = m_pages[page_index];
        page->storage.DestructElement(element_index);
        page->initialized_bits.Set(element_index, false);

        if (page->initialized_bits.Count() == 0)
        {
            // no elems remaining, delete the page
            m_valid_pages.Set(page_index, false);

            delete page;
            m_pages[page_index] = nullptr;
        }

        // Note: if page deleted, iterator will automatically skip to next element or END if none
        return Iterator(this, page_index, element_index + 1);
    }

    Iterator Find(const T& value)
    {
        for (auto it = Begin(); it != End(); ++it)
        {
            if (*it == value)
            {
                return it;
            }
        }

        return End();
    }

    ConstIterator Find(const T& value) const
    {
        return const_cast<SparsePagedArray&>(*this).Find(value);
    }

    template <class Predicate>
    Iterator FindIf(Predicate&& predicate)
    {
        for (auto it = Begin(); it != End(); ++it)
        {
            if (predicate(*it))
            {
                return it;
            }
        }

        return End();
    }

    template <class Predicate>
    ConstIterator FindIf(Predicate&& predicate) const
    {
        return const_cast<SparsePagedArray&>(*this).FindIf(std::forward<Predicate>(predicate));
    }

    void Clear(bool delete_pages = true)
    {
        for (Bitset::BitIndex bit : m_valid_pages)
        {
            AssertDebug(bit < m_pages.Size());

            Page* page = m_pages[bit];
            AssertDebug(page != nullptr);

            if (delete_pages)
            {
                delete page;
                m_pages[bit] = nullptr;
            }
            else
            {
                // Just destruct the elements in the page, without freeing the allocated memory.
                // we want to reuse this page later, to avoid additional reallocs!
                for (Bitset::BitIndex elem_bit : page->initialized_bits)
                {
                    AssertDebug(elem_bit < PageSize);
                    page->storage.DestructElement(elem_bit);
                }

                page->initialized_bits.Clear();
            }
        }

        m_valid_pages.Clear();

        if (delete_pages)
        {
            m_pages.Clear();
        }
    }

    HYP_DEF_STL_BEGIN_END(Iterator(this, 0, 0), Iterator(this, m_pages.Size(), PageSize));

private:
    HYP_FORCE_INLINE static constexpr SizeType PageIndex(SizeType index)
    {
        return index / PageSize;
    }

    HYP_FORCE_INLINE static constexpr SizeType ElementIndex(SizeType index)
    {
        return index % PageSize;
    }

    Page* GetOrAllocatePage(SizeType page_index)
    {
        if (!m_valid_pages.Test(page_index))
        {
            if (m_pages.Size() <= page_index)
            {
                m_pages.Resize(page_index + 1);
            }

            m_pages[page_index] = new Page;
            m_valid_pages.Set(page_index, true);
        }

        return m_pages[page_index];
    }

    Array<Page*, InlineAllocator<8>> m_pages;
    Bitset m_valid_pages;
};

} // namespace containers

template <class T, uint32 PageSize = 16>
using SparsePagedArray = containers::SparsePagedArray<T, PageSize>;

} // namespace hyperion

#endif
