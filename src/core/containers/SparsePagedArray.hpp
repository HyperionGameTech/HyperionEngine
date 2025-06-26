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
 *  Another useful feature of this container: pointers to elements will not be invalidated as the container grows and shrinks.
 *  (as long as the elements themselves being pointed to are not removed from the container) */
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

    using KeyType = SizeType;
    using ValueType = T;

    template <class Derived, bool IsConst>
    struct IteratorBase
    {
        std::conditional_t<IsConst, const SparsePagedArray*, SparsePagedArray*> array;
        uint32 page;
        uint32 elem;

        template <class ArrayType>
        IteratorBase(ArrayType* array, uint32 page_index, uint32 element_index)
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

        Derived& operator++()
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

            return static_cast<Derived&>(*this);
        }

        Derived operator++(int)
        {
            Derived tmp = static_cast<Derived&>(*this);
            ++(*this);
            return tmp;
        }

        bool operator==(const Derived& other) const
        {
            return page == other.page && elem == other.elem;
        }

        bool operator!=(const Derived& other) const
        {
            return !(*this == other);
        }
    };

    struct Iterator : IteratorBase<Iterator, false>
    {
        template <class ArrayType>
        Iterator(ArrayType* array, uint32 page_index, uint32 element_index)
            : IteratorBase<Iterator, false>(array, page_index, element_index)
        {
        }

        Iterator(const Iterator& other)
            : IteratorBase<Iterator, false>(other.array, other.page, other.elem)
        {
        }

        Iterator& operator=(const Iterator& other)
        {
            if (this == &other)
            {
                return *this;
            }

            AssertDebug(other.array == this->array);

            this->page = other.page;
            this->elem = other.elem;

            return *this;
        }
    };

    struct ConstIterator : IteratorBase<ConstIterator, true>
    {
        template <class ArrayType>
        ConstIterator(const ArrayType* array, uint32 page_index, uint32 element_index)
            : IteratorBase<ConstIterator, true>(array, page_index, element_index)
        {
        }

        template <class OtherIteratorType, bool OtherIsConst>
        ConstIterator(const IteratorBase<OtherIteratorType, OtherIsConst>& other)
            : IteratorBase<ConstIterator, true>(other.array, other.page, other.elem)
        {
        }

        template <class OtherIteratorType, bool OtherIsConst>
        ConstIterator& operator=(const IteratorBase<OtherIteratorType, OtherIsConst>& other)
        {
            if (this == &other)
            {
                return *this;
            }

            AssertDebug(other.array == this->array);

            this->page = other.page;
            this->elem = other.elem;

            return *this;
        }
    };

    SparsePagedArray()
    {
    }

    SparsePagedArray(std::initializer_list<KeyValuePair<KeyType, T>> initializer_list)
    {
        for (const auto& item : initializer_list)
        {
            Set(item.first, item.second);
        }
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

    /*! \brief Counts the total number of set elements in each valid page.
     *  \note This is different than Array::Size(). Don't use this method to check if an index is valid the same way you would with a standard array.
     *  Because this is a sparse array type, indices that are still < Count() may be invalid! Additionally, you may have indices that are far >= Count().
     *  Only use this as a means of tracking the number of elements set! */
    HYP_FORCE_INLINE SizeType Count() const
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

        Page* page = GetOrAllocatePage(page_index);

        if (!page->initialized_bits.Test(element_index))
        {
            page->storage.ConstructElement(element_index);
            page->initialized_bits.Set(element_index, true);
        }

        return page->storage.GetPointer()[element_index];
    }

    T& Get(SizeType index)
    {
        const SizeType page_index = PageIndex(index);
        const SizeType element_index = ElementIndex(index);

        AssertDebug(m_valid_pages.Test(page_index));

        Page* page = m_pages[page_index];
        AssertDebug(page != nullptr);

        AssertDebug(page->initialized_bits.Test(element_index));

        return page->storage.GetPointer()[element_index];
    }

    const T& Get(SizeType index) const
    {
        return const_cast<SparsePagedArray&>(*this).Get(index);
    }

    T* TryGet(SizeType index)
    {
        const SizeType page_index = PageIndex(index);
        const SizeType element_index = ElementIndex(index);

        if (!m_valid_pages.Test(page_index))
        {
            return nullptr;
        }

        AssertDebug(m_pages[page_index] != nullptr);

        if (!m_pages[page_index]->initialized_bits.Test(element_index))
        {
            return nullptr;
        }

        return &m_pages[page_index]->storage.GetPointer()[element_index];
    }

    const T* TryGet(SizeType index) const
    {
        return const_cast<SparsePagedArray&>(*this).TryGet(index);
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

    // Don't worry about the const casts -- we return ConstIterator if this is const this will be const again

    HYP_DEF_STL_BEGIN_END(Iterator(const_cast<SparsePagedArray*>(this), 0, 0), Iterator(const_cast<SparsePagedArray*>(this), m_pages.Size(), PageSize));

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
            AssertDebugMsg(page_index < UINT32_MAX, "Invalid page index requested - will cause OOM crash!");

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
