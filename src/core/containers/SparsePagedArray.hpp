/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/Bitset.hpp>

#include <core/memory/allocator/Allocator.hpp>

#include <core/math/MathUtil.hpp>

#include <core/Defines.hpp>

#include <core/Types.hpp>

namespace Hyperion {
namespace containers {

/*! \brief SparsePagedArray defines a container that stores elements in pages of a fixed size, allowing for sparse storage.
 *  \details This container is useful when you have a large number of elements, but only a small subset of them are initialized.
 *  Another useful feature of this container: pointers to elements will not be invalidated as the container grows and shrinks.
 *  (as long as the elements themselves being pointed to are not removed from the container) */
template <class T, uint32 PageSize, class AllocatorType = DynamicAllocator>
class SparsePagedArray : public ContainerBase<SparsePagedArray<T, PageSize, AllocatorType>, SizeType>
{
    static_assert(MathUtil::IsPowerOfTwo(PageSize), "PageSize must be power of two!");

protected:
    struct Page
    {
        ValueStorage<T, PageSize, alignof(T)> storage;
        TBitset<AllocatorType> initializedBits;

        explicit Page(AllocatorType* pAllocator)
            : initializedBits(pAllocator)
        {
        }

        // No move/copy operations, Page is stored as a pointer so these are unnecessary and
        // would only introduce complexity.
        Page(const Page& other) = delete;
        Page& operator=(Page& other) = delete;

        Page(Page&& other) noexcept = delete;
        Page& operator=(Page&& other) noexcept = delete;

        ~Page()
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                for (Bitset::BitIndex bit : initializedBits)
                {
                    storage.GetPointer()[bit].~T();
                }
            }
        }
    };

public:
    static constexpr bool isContiguous = false;

    using KeyType = SizeType;
    using ValueType = T;

    template <class Derived, bool IsConst>
    struct IteratorBase
    {
        std::conditional_t<IsConst, const SparsePagedArray*, SparsePagedArray*> array;
        uint32 page;
        uint32 elem;
        bool lazyInit;

        HYP_FORCE_INLINE IteratorBase()
            : array(nullptr),
              page(~0u),
              elem(PageSize),
              lazyInit(false)
        {
        }

        template <class ArrayType>
        HYP_FORCE_INLINE IteratorBase(ArrayType* a, uint32 pageIndex, uint32 elementIndex)
            : array(a),
              page(pageIndex),
              elem(elementIndex),
              lazyInit(true)
        {
            HYP_CORE_ASSERT(array != nullptr);

            if (page >= uint32(array->m_pages.Size()))
            {
                page = uint32(array->m_pages.Size());
                elem = PageSize;
                lazyInit = false;
            }
        }

        void EnsureReady() const
        {
            if (HYP_LIKELY(!lazyInit))
            {
                return;
            }

            IteratorBase* self = const_cast<IteratorBase*>(this);

            uint32 p = self->page;
            uint32 e = self->elem;

            while (p != ~0u)
            {
                if (self->array->m_validPages.Test(p))
                {
                    Bitset::BitIndex next = self->array->m_pages[p]->initializedBits.NextSetBitIndex(e);
                    if (next < PageSize)
                    {
                        self->page = p;
                        self->elem = next;
                        self->lazyInit = false;
                        return;
                    }
                }

                p = self->array->m_validPages.NextSetBitIndex(p + 1);

                if (p == ~0u)
                {
                    self->page = uint32(self->array->m_pages.Size());
                    self->elem = PageSize;
                    self->lazyInit = false;
                    return;
                }

                e = 0;
            }

            self->page = uint32(self->array->m_pages.Size());
            self->elem = PageSize;
            self->lazyInit = false;
        }

        HYP_FORCE_INLINE T& operator*() const
        {
            EnsureReady();
#ifdef HYP_DEBUG_MODE
            HYP_CORE_ASSERT(array->m_validPages.Test(page));
            Page* pg = array->m_pages[page];
            HYP_CORE_ASSERT(pg && pg->initializedBits.Test(elem));
            return pg->storage.GetPointer()[elem];
#else
            return array->m_pages[page]->storage.GetPointer()[elem];
#endif
        }

        HYP_FORCE_INLINE T* operator->() const
        {
            EnsureReady();
#ifdef HYP_DEBUG_MODE
            HYP_CORE_ASSERT(array->m_validPages.Test(page));
            Page* pg = array->m_pages[page];
            HYP_CORE_ASSERT(pg && pg->initializedBits.Test(elem));
            return pg->storage.GetPointer() + elem;
#else
            return array->m_pages[page]->storage.GetPointer() + elem;
#endif
        }

        Derived& operator++()
        {
            if (HYP_UNLIKELY(lazyInit))
            {
                EnsureReady();
            }

            while (true)
            {
                if (page != ~0u && array->m_validPages.Test(page))
                {
                    Bitset::BitIndex next = array->m_pages[page]->initializedBits.NextSetBitIndex(elem + 1);
                    if (next < PageSize)
                    {
                        elem = next;
                        break;
                    }
                }

                page = array->m_validPages.NextSetBitIndex(page + 1);

                if (page == ~0u)
                {
                    page = uint32(array->m_pages.Size());
                    elem = PageSize;
                    break;
                }

                elem = ~0u; // so elem+1 == 0 on next loop
            }

            return static_cast<Derived&>(*this);
        }

        Derived operator++(int)
        {
            Derived tmp = static_cast<Derived&>(*this);
            ++(*this);
            return tmp;
        }

        Derived operator+(SizeType offset) const
        {
            if (offset == 0)
            {
                return static_cast<const Derived&>(*this);
            }

            if (HYP_UNLIKELY(lazyInit))
            {
                EnsureReady();
            }

            Derived result;
            result.array = array;
            result.page = page;
            result.elem = elem;
            result.lazyInit = false;

            SizeType remaining = offset;

            while (remaining > 0)
            {
                if (result.page >= uint32(array->m_pages.Size()))
                {
                    // Already at end
                    return result;
                }

                if (result.page != ~0u && array->m_validPages.Test(result.page))
                {
                    const Bitset& bits = array->m_pages[result.page]->initializedBits;

                    // Count how many set bits are after current elem in this page
                    uint32 bitsInPage = 0;
                    Bitset::BitIndex nextBit = bits.NextSetBitIndex(result.elem + 1);

                    while (nextBit < PageSize && bitsInPage < remaining)
                    {
                        ++bitsInPage;
                        if (bitsInPage == remaining)
                        {
                            result.elem = nextBit;
                            return result;
                        }
                        nextBit = bits.NextSetBitIndex(nextBit + 1);
                    }

                    remaining -= bitsInPage;
                }

                // Move to next valid page
                result.page = array->m_validPages.NextSetBitIndex(result.page + 1);

                if (result.page == ~0u)
                {
                    // Reached the end
                    result.page = uint32(array->m_pages.Size());
                    result.elem = PageSize;
                    return result;
                }

                // Start at beginning of new page
                result.elem = ~0u; // Will be incremented to 0 when searching
            }

            return result;
        }

        HYP_FORCE_INLINE bool operator==(const Derived& other) const
        {
            HYP_CORE_ASSERT(array == other.array); // comparing iterators from different containers, bad!

            if (HYP_UNLIKELY(lazyInit))
            {
                EnsureReady();
            }

            if (HYP_UNLIKELY(other.lazyInit))
            {
                other.EnsureReady();
            }

            return page == other.page && elem == other.elem;
        }

        HYP_FORCE_INLINE bool operator!=(const Derived& other) const
        {
            return !(*this == other);
        }

        HYP_FORCE_INLINE bool operator<(const Derived& other) const
        {
            HYP_CORE_ASSERT(array == other.array); // comparing iterators from different containers, bad!

            if (HYP_UNLIKELY(lazyInit))
            {
                EnsureReady();
            }

            if (HYP_UNLIKELY(other.lazyInit))
            {
                other.EnsureReady();
            }

            if (page < other.page)
            {
                return true;
            }
            else if (page > other.page)
            {
                return false;
            }
            else
            {
                return elem < other.elem;
            }
        }
    };

    struct Iterator : IteratorBase<Iterator, false>
    {
        Iterator() = default;

        template <class ArrayType>
        HYP_FORCE_INLINE Iterator(ArrayType* array, uint32 pageIndex, uint32 elementIndex)
            : IteratorBase<Iterator, false>(array, pageIndex, elementIndex)
        {
        }

        HYP_FORCE_INLINE Iterator(const Iterator& other)
            : IteratorBase<Iterator, false>(other.array, other.page, other.elem)
        {
        }

        Iterator& operator=(const Iterator& other) = default;
    };

    struct ConstIterator : IteratorBase<ConstIterator, true>
    {
        ConstIterator() = default;

        template <class ArrayType>
        ConstIterator(const ArrayType* array, uint32 pageIndex, uint32 elementIndex)
            : IteratorBase<ConstIterator, true>(array, pageIndex, elementIndex)
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

            this->array = other.array;
            this->page = other.page;
            this->elem = other.elem;

            return *this;
        }
    };
    
    template <bool ConditionalEnable = HasDefaultAllocatorInstance<AllocatorType>, typename = std::enable_if_t<ConditionalEnable>>
    HYP_FORCE_INLINE SparsePagedArray()
        : m_pAllocator(GetDefaultAllocatorInstance<AllocatorType>()),
          m_pages(m_pAllocator),
          m_validPages(m_pAllocator)
    {
        HYP_CORE_ASSERT(m_pAllocator != nullptr);
    }

    explicit SparsePagedArray(AllocatorType* pAllocator)
        : m_pAllocator(pAllocator),
          m_pages(m_pAllocator),
          m_validPages(m_pAllocator)
    {
        HYP_CORE_ASSERT(m_pAllocator != nullptr);
    }
    
    template <bool ConditionalEnable = HasDefaultAllocatorInstance<AllocatorType>, typename = std::enable_if_t<ConditionalEnable>>
    SparsePagedArray(std::initializer_list<KeyValuePair<KeyType, T>> initializerList)
        : m_pAllocator(GetDefaultAllocatorInstance<AllocatorType>()),
          m_pages(m_pAllocator),
          m_validPages(m_pAllocator)
    {
        HYP_CORE_ASSERT(m_pAllocator != nullptr);

        for (const auto& item : initializerList)
        {
            Set(item.first, item.second);
        }
    }

    SparsePagedArray(const SparsePagedArray& other)
        : m_pAllocator(other.m_pAllocator),
          m_pages(m_pAllocator),
          m_validPages(other.m_validPages)
    {
        m_pages.ResizeZeroed(other.m_pages.Size());

        for (Bitset::BitIndex bit : other.m_validPages)
        {
            HYP_CORE_ASSERT(bit < other.m_pages.Size());

            m_pages[bit] = (Page*)m_pAllocator->Allocate(sizeof(Page), alignof(Page));
            new (m_pages[bit]) Page(m_pAllocator);

            for (Bitset::BitIndex elem : other.m_pages[bit]->initializedBits)
            {
                new (m_pages[bit]->storage.GetPointer() + elem) T(other.m_pages[bit]->storage.GetPointer()[elem]);
            }
        }
    }

    SparsePagedArray& operator=(const SparsePagedArray& other)
    {
        if (this == &other)
        {
            return *this;
        }

        for (Bitset::BitIndex bit : m_validPages)
        {
            HYP_CORE_ASSERT(bit < m_pages.Size());

            m_pages[bit]->~Page();
            m_pAllocator->Free(m_pages[bit]);

            m_pages[bit] = nullptr;
        }

        m_validPages = other.m_validPages;

        m_pages = Array<Page*, AllocatorType>(m_pAllocator, 0);
        m_pages.ResizeZeroed(other.m_pages.Size());

        for (Bitset::BitIndex bit : other.m_validPages)
        {
            HYP_CORE_ASSERT(bit < other.m_pages.Size());

            m_pages[bit] = (Page*)m_pAllocator->Allocate(sizeof(Page), alignof(Page));
            new (m_pages[bit]) Page(m_pAllocator);

            for (Bitset::BitIndex elem : other.m_pages[bit]->initializedBits)
            {
                new (m_pages[bit]->storage.GetPointer() + elem) T(other.m_pages[bit]->storage.GetPointer()[elem]);
            }
        }

        return *this;
    }

    SparsePagedArray(SparsePagedArray&& other) noexcept
        : m_pAllocator(other.m_pAllocator),
          m_pages(std::move(other.m_pages)),
          m_validPages(std::move(other.m_validPages))
    {
    }

    SparsePagedArray& operator=(SparsePagedArray&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        for (Bitset::BitIndex bit : m_validPages)
        {
            HYP_CORE_ASSERT(bit < m_pages.Size());

            m_pages[bit]->~Page();
            m_pAllocator->Free(m_pages[bit]);

            m_pages[bit] = nullptr;
        }

        m_pages = std::move(other.m_pages);
        m_validPages = std::move(other.m_validPages);

        return *this;
    }

    ~SparsePagedArray()
    {
        for (Bitset::BitIndex bit : m_validPages)
        {
            HYP_CORE_ASSERT(bit < m_pages.Size());

            m_pages[bit]->~Page();
            m_pAllocator->Free(m_pages[bit]);

            m_pages[bit] = nullptr;
        }
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return m_validPages.Count() == 0;
    }

    HYP_FORCE_INLINE bool Any() const
    {
        return m_validPages.Count() != 0;
    }

    /*! \brief Counts the total number of set elements in each valid page.
     *  \note This is different than Array::Size(). Don't use this method to check if an index is valid the same way you would with a standard array.
     *  Because this is a sparse array type, indices that are still < Count() may be invalid! Additionally, you may have indices that are far >= Count().
     *  Only use this as a means of tracking the number of elements set! */
    HYP_FORCE_INLINE SizeType Count() const
    {
        SizeType size = 0;

        for (Bitset::BitIndex bit : m_validPages)
        {
            size += m_pages[bit]->initializedBits.Count();
        }

        return size;
    }

    HYP_FORCE_INLINE T& Front()
    {
        HYP_CORE_ASSERT(m_validPages.Count() != 0);

        return *Begin();
    }

    HYP_FORCE_INLINE const T& Front() const
    {
        HYP_CORE_ASSERT(m_validPages.Count() != 0);

        return *Begin();
    }

    HYP_FORCE_INLINE T& Back()
    {
        HYP_CORE_ASSERT(m_validPages.Count() != 0);

        Page* lastPage = m_pages[m_validPages.LastSetBitIndex()];

        HYP_CORE_ASSERT(lastPage != nullptr);
        HYP_CORE_ASSERT(lastPage->initializedBits.Count() > 0);

        SizeType lastIndex = lastPage->initializedBits.LastSetBitIndex();
        HYP_CORE_ASSERT(lastIndex < PageSize);

        return lastPage->storage.GetPointer()[lastIndex];
    }

    HYP_FORCE_INLINE const T& Back() const
    {
        HYP_CORE_ASSERT(m_validPages.Count() != 0);

        Page* lastPage = m_pages[m_validPages.LastSetBitIndex()];

        HYP_CORE_ASSERT(lastPage != nullptr);
        HYP_CORE_ASSERT(lastPage->initializedBits.Count() > 0);

        SizeType lastIndex = lastPage->initializedBits.LastSetBitIndex();
        HYP_CORE_ASSERT(lastIndex < PageSize);

        return lastPage->storage.GetPointer()[lastIndex];
    }

    HYP_FORCE_INLINE bool HasIndex(SizeType index) const
    {
        const SizeType pageIndex = PageIndex(index);

        if (!m_validPages.Test(pageIndex))
        {
            return false;
        }

        return m_pages[pageIndex]->initializedBits.Test(ElementIndex(index));
    }

    T& operator[](SizeType index)
    {
        const SizeType pageIndex = PageIndex(index);

        Page* page = GetOrAllocatePage(pageIndex);

        const SizeType elementIndex = ElementIndex(index);

        if (!page->initializedBits.Test(elementIndex))
        {
            page->storage.ConstructElement(elementIndex);
            page->initializedBits.Set(elementIndex, true);
        }

        return page->storage.GetPointer()[elementIndex];
    }

    HYP_FORCE_INLINE T& Get(SizeType index)
    {
        HYP_CORE_ASSERT(HasIndex(index), "Index %zu is not initialized in SparsePagedArray!", index);

        const SizeType pageIndex = PageIndex(index);

        Page* page = m_pages[pageIndex];

        return *(page->storage.GetPointer() + ElementIndex(index));
    }

    HYP_FORCE_INLINE const T& Get(SizeType index) const
    {
        HYP_CORE_ASSERT(HasIndex(index), "Index %zu is not initialized in SparsePagedArray!", index);

        const SizeType pageIndex = PageIndex(index);

        Page* page = m_pages[pageIndex];

        return *(page->storage.GetPointer() + ElementIndex(index));
    }

    HYP_FORCE_INLINE T* TryGet(SizeType index)
    {
        const SizeType pageIndex = PageIndex(index);

        if (!m_validPages.Test(pageIndex))
        {
            return nullptr;
        }

        Page* page = m_pages[pageIndex];

        const SizeType elementIndex = ElementIndex(index);

        if (!page->initializedBits.Test(elementIndex))
        {
            return nullptr;
        }

        return page->storage.GetPointer() + elementIndex;
    }

    HYP_FORCE_INLINE const T* TryGet(SizeType index) const
    {
        return const_cast<SparsePagedArray&>(*this).TryGet(index);
    }

    Iterator Set(SizeType index, const T& value)
    {
        SizeType pageIndex = PageIndex(index);
        SizeType elementIndex = ElementIndex(index);

        Page* page = GetOrAllocatePage(pageIndex);
        HYP_CORE_ASSERT(page != nullptr);

        if constexpr (std::is_copy_assignable_v<T>)
        {
            if (page->initializedBits.Test(elementIndex))
            {
                // copy assign instead of reconstructing
                page->storage.GetPointer()[elementIndex] = value;
                return Iterator(this, pageIndex, elementIndex);
            }
        }
        else if (!std::is_trivially_destructible_v<T> && page->initializedBits.Test(elementIndex))
        {
            page->storage.DestructElement(elementIndex);
        }

        page->storage.ConstructElement(elementIndex, value);
        page->initializedBits.Set(elementIndex, true);

        return Iterator(this, pageIndex, elementIndex);
    }

    Iterator Set(SizeType index, T&& value)
    {
        SizeType pageIndex = PageIndex(index);
        SizeType elementIndex = ElementIndex(index);

        Page* page = GetOrAllocatePage(pageIndex);
        HYP_CORE_ASSERT(page != nullptr);

        if constexpr (std::is_move_assignable_v<T>)
        {
            if (page->initializedBits.Test(elementIndex))
            {
                // move assign instead of reconstructing
                page->storage.GetPointer()[elementIndex] = std::move(value);
                return Iterator(this, pageIndex, elementIndex);
            }
        }
        else if (!std::is_trivially_destructible_v<T> && page->initializedBits.Test(elementIndex))
        {
            page->storage.DestructElement(elementIndex);
        }

        page->storage.ConstructElement(elementIndex, std::move(value));
        page->initializedBits.Set(elementIndex, true);

        return Iterator(this, pageIndex, elementIndex);
    }

    template <class... Args>
    Iterator Emplace(SizeType index, Args&&... args)
    {
        SizeType pageIndex = PageIndex(index);
        SizeType elementIndex = ElementIndex(index);

        Page* page = GetOrAllocatePage(pageIndex);
        HYP_CORE_ASSERT(page != nullptr);

        if (page->initializedBits.Test(elementIndex))
        {
            page->storage.DestructElement(elementIndex);
        }

        page->storage.ConstructElement(elementIndex, std::forward<Args>(args)...);
        page->initializedBits.Set(elementIndex, true);

        return Iterator(this, pageIndex, elementIndex);
    }

    Iterator Erase(ConstIterator iter, bool freeMemory = true)
    {
        if (iter == End())
        {
            return End();
        }

        return EraseAt(SizeType(iter.page * PageSize + iter.elem), freeMemory);
    }

    Iterator EraseAt(SizeType index, bool freeMemory = true)
    {
        SizeType pageIndex = PageIndex(index);
        SizeType elementIndex = ElementIndex(index);

        if (!m_validPages.Test(pageIndex) || !m_pages[pageIndex]->initializedBits.Test(elementIndex))
        {
            // element does not exist, nothing to erase
            // note: iterator will automatically skip to next
            return Iterator(this, pageIndex, elementIndex);
        }

        Page* page = m_pages[pageIndex];
        page->storage.DestructElement(elementIndex);
        page->initializedBits.Set(elementIndex, false);

        if (freeMemory && page->initializedBits.Count() == 0)
        {
            // no elems remaining, delete the page
            m_validPages.Set(pageIndex, false);

            page->~Page();
            m_pAllocator->Free(page);

            m_pages[pageIndex] = nullptr;
        }

        // Note: if page deleted, iterator will automatically skip to next element or END if none
        return Iterator(this, pageIndex, elementIndex + 1);
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

    SizeType IndexOf(ConstIterator iter) const
    {
        AssertDebug(iter.array == this, "Iterator does not belong to this SparsePagedArray!");

        if (iter.elem >= PageSize || iter.page >= m_pages.Size())
        {
            return SizeType(-1);
        }

        return SizeType(iter.page * PageSize + iter.elem);
    }

    template <class Predicate>
    ConstIterator FindIf(Predicate&& predicate) const
    {
        return const_cast<SparsePagedArray&>(*this).FindIf(std::forward<Predicate>(predicate));
    }

    void Clear(bool freeMemory = true)
    {
        for (Bitset::BitIndex bit : m_validPages)
        {
            HYP_CORE_ASSERT(bit < m_pages.Size());

            Page* page = m_pages[bit];
            HYP_CORE_ASSERT(page != nullptr);

            if (freeMemory)
            {
                page->~Page();
                m_pAllocator->Free(page);

                m_pages[bit] = nullptr;
            }
            else
            {
                // Just destruct the elements in the page, without freeing the allocated memory.
                // we want to reuse this page later, to avoid additional reallocs!
                for (Bitset::BitIndex elemBit : page->initializedBits)
                {
                    HYP_CORE_ASSERT(elemBit < PageSize);
                    page->storage.DestructElement(elemBit);
                }

                page->initializedBits.Clear();
            }
        }

        if (freeMemory)
        {
            m_validPages.Clear();
            m_pages.Clear();
        }
    }

    // Don't worry about the const casts -- we return ConstIterator if this is const this will be const again

    HYP_DEF_STL_BEGIN_END(Iterator(const_cast<SparsePagedArray*>(this), 0, 0), Iterator(const_cast<SparsePagedArray*>(this), m_pages.Size(), PageSize));

protected:
    static constexpr uint64 PageSizeBits = MathUtil::FastLog2_Pow2(PageSize);

    HYP_FORCE_INLINE static constexpr SizeType PageIndex(SizeType index)
    {
        return index >> PageSizeBits;
    }

    HYP_FORCE_INLINE static constexpr SizeType ElementIndex(SizeType index)
    {
        return index & (PageSize - 1);
    }

    Page* GetOrAllocatePage(SizeType pageIndex)
    {
        if (!m_validPages.Test(pageIndex))
        {
            HYP_CORE_ASSERT(pageIndex < UINT32_MAX, "Invalid page index requested - will cause OOM crash!");

            if (m_pages.Size() <= pageIndex)
            {
                m_pages.Resize(pageIndex + 1);
            }

            m_pages[pageIndex] = (Page*)m_pAllocator->Allocate(sizeof(Page), alignof(Page));
            new (m_pages[pageIndex]) Page(m_pAllocator);

            m_validPages.Set(pageIndex, true);
        }

        return m_pages[pageIndex];
    }

    AllocatorType* const m_pAllocator;
    Array<Page*, AllocatorType> m_pages;
    TBitset<AllocatorType> m_validPages;
};

} // namespace containers

template <class T, uint32 PageSize = 16, class AllocatorType = DynamicAllocator>
using SparsePagedArray = containers::SparsePagedArray<T, PageSize, AllocatorType>;

} // namespace Hyperion
