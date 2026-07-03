/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Bitset.hpp>

#include <Core/Utilities/BitField.hpp>

#include <Core/Memory/Allocator/Allocator.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Defines.hpp>

#include <Core/Types.hpp>

namespace Hyperion {
namespace containers {

/*! \brief SparsePagedArray defines a container that stores elements in pages of a fixed size, allowing for sparse storage.
 *  \details This container is useful when you have a large number of elements, but only a small subset of them are initialized.
 *  Another useful feature of this container: pointers to elements will not be invalidated as the container grows and shrinks.
 *  (as long as the elements themselves being pointed to are not removed from the container) */
template <class T, uint32 PageSize, class AllocatorType = DynamicAllocator>
class SparsePagedArray : public ContainerBase<SparsePagedArray<T, PageSize, AllocatorType>, size_t>
{
    static_assert(MathUtil::IsPowerOfTwo(PageSize), "PageSize must be power of two!");

protected:
    struct Page
    {
        ValueStorage<T, PageSize, alignof(T)> storage;
        BitField<PageSize> initializedBits;

        Page()
            : initializedBits {}
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
                for (size_t bit : initializedBits)
                {
                    storage.GetPointer()[bit].~T();
                }
            }
        }
    };

    static constexpr size_t IdealBlockSizeBytes = 16 * 1024; // 16 KiB
    static constexpr size_t PagesPerBlock = (sizeof(Page) + IdealBlockSizeBytes - 1) / IdealBlockSizeBytes;

    struct PageBlock
    {
        static constexpr size_t BlockSize = sizeof(Page) * PagesPerBlock;
        alignas(Page) ubyte data[BlockSize];

        HYP_FORCE_INLINE Page* GetPage(uint32 index)
        {
            return reinterpret_cast<Page*>(data + index * sizeof(Page));
        }
    };

public:
    static constexpr bool isContiguous = false;

    using KeyType = size_t;
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
                    size_t next = self->array->m_pages[p]->initializedBits.NextOneBit(e);
                    if (next < PageSize)
                    {
                        self->page = p;
                        self->elem = static_cast<uint32>(next);
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
#if HYP_DEBUG_MODE
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
#if HYP_DEBUG_MODE
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
                    size_t next = array->m_pages[page]->initializedBits.NextOneBit(elem + 1);
                    if (next < PageSize)
                    {
                        elem = static_cast<uint32>(next);
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

        Derived operator+(size_t offset) const
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

            size_t remaining = offset;

            while (remaining > 0)
            {
                if (result.page >= uint32(array->m_pages.Size()))
                {
                    // Already at end
                    return result;
                }

                if (result.page != ~0u && array->m_validPages.Test(result.page))
                {
                    const BitField<PageSize>& bits = array->m_pages[result.page]->initializedBits;

                    // Count how many set bits are after current elem in this page
                    uint32 bitsInPage = 0;
                    size_t nextBit = bits.NextOneBit(result.elem + 1);

                    while (nextBit < PageSize && bitsInPage < remaining)
                    {
                        ++bitsInPage;

                        if (bitsInPage == remaining)
                        {
                            result.elem = nextBit;
                            return result;
                        }

                        nextBit = bits.NextOneBit(nextBit + 1);
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
        : m_pages(),
          m_validPages(),
          m_pageBlocks(),
          m_nextPageSlot(0)
    {
    }

    template <bool ConditionalEnable = HasDefaultAllocatorInstance<AllocatorType>, typename = std::enable_if_t<ConditionalEnable>>
    SparsePagedArray(std::initializer_list<KeyValuePair<KeyType, T>> initializerList)
        : m_pages(),
          m_validPages(),
          m_pageBlocks(),
          m_nextPageSlot(0)
    {
        for (const auto& item : initializerList)
        {
            Set(item.first, item.second);
        }
    }

    SparsePagedArray(const SparsePagedArray& other)
        : m_pages(),
          m_validPages(other.m_validPages),
          m_pageBlocks(),
          m_nextPageSlot(0)
    {
        m_pages.ResizeZeroed(other.m_pages.Size());

        for (Bitset::BitIndex bit : other.m_validPages)
        {
            HYP_CORE_ASSERT(bit < other.m_pages.Size());

            const size_t blockIndex = m_nextPageSlot / PagesPerBlock;
            const size_t slotIndex = m_nextPageSlot % PagesPerBlock;

            if (blockIndex >= m_pageBlocks.Size())
            {
                PageBlock* block = (PageBlock*)GetAllocator()->Allocate(sizeof(PageBlock), alignof(PageBlock));
                m_pageBlocks.PushBack(block);
            }

            Page* page = new (m_pageBlocks[blockIndex]->GetPage(uint32(slotIndex))) Page();
            ++m_nextPageSlot;

            m_pages[bit] = page;

            for (size_t elem : other.m_pages[bit]->initializedBits)
            {
                new (page->storage.GetPointer() + elem) T(other.m_pages[bit]->storage.GetPointer()[elem]);
            }
        }
    }

    SparsePagedArray& operator=(const SparsePagedArray& other)
    {
        if (this == &other)
        {
            return *this;
        }

        Clear(true);

        m_validPages = other.m_validPages;

        m_pages.ResizeZeroed(other.m_pages.Size());

        for (Bitset::BitIndex bit : other.m_validPages)
        {
            HYP_CORE_ASSERT(bit < other.m_pages.Size());

            const size_t blockIndex = m_nextPageSlot / PagesPerBlock;
            const size_t slotIndex = m_nextPageSlot % PagesPerBlock;

            if (blockIndex >= m_pageBlocks.Size())
            {
                PageBlock* block = (PageBlock*)GetAllocator()->Allocate(sizeof(PageBlock), alignof(PageBlock));
                m_pageBlocks.PushBack(block);
            }

            Page* page = new (m_pageBlocks[blockIndex]->GetPage(uint32(slotIndex))) Page();
            ++m_nextPageSlot;

            m_pages[bit] = page;

            for (size_t elem : other.m_pages[bit]->initializedBits)
            {
                new (page->storage.GetPointer() + elem) T(other.m_pages[bit]->storage.GetPointer()[elem]);
            }
        }

        return *this;
    }

    SparsePagedArray(SparsePagedArray&& other) noexcept
        : m_pages(std::move(other.m_pages)),
          m_validPages(std::move(other.m_validPages)),
          m_pageBlocks(std::move(other.m_pageBlocks)),
          m_nextPageSlot(other.m_nextPageSlot)
    {
        other.m_nextPageSlot = 0;
    }

    SparsePagedArray& operator=(SparsePagedArray&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        Clear(true);

        m_pages = std::move(other.m_pages);
        m_validPages = std::move(other.m_validPages);
        m_pageBlocks = std::move(other.m_pageBlocks);
        m_nextPageSlot = other.m_nextPageSlot;
        other.m_nextPageSlot = 0;

        return *this;
    }

    ~SparsePagedArray()
    {
        for (size_t i = 0; i < m_nextPageSlot; i++)
        {
            const size_t blockIndex = i / PagesPerBlock;
            const size_t slotIndex = i % PagesPerBlock;

            m_pageBlocks[blockIndex]->GetPage(uint32(slotIndex))->~Page();
        }

        for (auto* block : m_pageBlocks)
        {
            GetAllocator()->Free(block);
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
    HYP_FORCE_INLINE size_t Count() const
    {
        size_t size = 0;

        for (Bitset::BitIndex bit : m_validPages)
        {
            size += m_pages[bit]->initializedBits.CountOnes();
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
        HYP_CORE_ASSERT(lastPage->initializedBits.CountOnes() > 0);

        size_t lastIndex = lastPage->initializedBits.LastOneBit();
        HYP_CORE_ASSERT(lastIndex < PageSize);

        return lastPage->storage.GetPointer()[lastIndex];
    }

    HYP_FORCE_INLINE const T& Back() const
    {
        HYP_CORE_ASSERT(m_validPages.Count() != 0);

        Page* lastPage = m_pages[m_validPages.LastSetBitIndex()];

        HYP_CORE_ASSERT(lastPage != nullptr);
        HYP_CORE_ASSERT(lastPage->initializedBits.CountOnes() > 0);

        size_t lastIndex = lastPage->initializedBits.LastOneBit();
        HYP_CORE_ASSERT(lastIndex < PageSize);

        return lastPage->storage.GetPointer()[lastIndex];
    }

    HYP_FORCE_INLINE bool HasIndex(size_t index) const
    {
        const size_t pageIndex = PageIndex(index);

        if (!m_validPages.Test(pageIndex))
        {
            return false;
        }

        return m_pages[pageIndex]->initializedBits.Test(ElementIndex(index));
    }

    T& operator[](size_t index)
    {
        const size_t pageIndex = PageIndex(index);

        Page* page = GetOrAllocatePage(pageIndex);

        const size_t elementIndex = ElementIndex(index);

        if (!page->initializedBits.Test(elementIndex))
        {
            page->storage.ConstructElement(elementIndex);
            page->initializedBits.Set(elementIndex, true);
        }

        return page->storage.GetPointer()[elementIndex];
    }

    HYP_FORCE_INLINE T& Get(size_t index)
    {
        HYP_CORE_ASSERT(HasIndex(index), "Index %zu is not initialized in SparsePagedArray!", index);

        const size_t pageIndex = PageIndex(index);

        Page* page = m_pages[pageIndex];

        return *(page->storage.GetPointer() + ElementIndex(index));
    }

    HYP_FORCE_INLINE const T& Get(size_t index) const
    {
        HYP_CORE_ASSERT(HasIndex(index), "Index %zu is not initialized in SparsePagedArray!", index);

        const size_t pageIndex = PageIndex(index);

        Page* page = m_pages[pageIndex];

        return *(page->storage.GetPointer() + ElementIndex(index));
    }

    HYP_FORCE_INLINE T* TryGet(size_t index)
    {
        const size_t pageIndex = PageIndex(index);

        if (!m_validPages.Test(pageIndex))
        {
            return nullptr;
        }

        Page* page = m_pages[pageIndex];

        const size_t elementIndex = ElementIndex(index);

        if (!page->initializedBits.Test(elementIndex))
        {
            return nullptr;
        }

        return page->storage.GetPointer() + elementIndex;
    }

    HYP_FORCE_INLINE const T* TryGet(size_t index) const
    {
        return const_cast<SparsePagedArray&>(*this).TryGet(index);
    }

    Iterator Set(size_t index, const T& value)
    {
        size_t pageIndex = PageIndex(index);
        size_t elementIndex = ElementIndex(index);

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

    Iterator Set(size_t index, T&& value)
    {
        size_t pageIndex = PageIndex(index);
        size_t elementIndex = ElementIndex(index);

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
    Iterator Emplace(size_t index, Args&&... args)
    {
        size_t pageIndex = PageIndex(index);
        size_t elementIndex = ElementIndex(index);

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

        return EraseAt(size_t(iter.page * PageSize + iter.elem), freeMemory);
    }

    Iterator EraseAt(size_t index, bool freeMemory = true)
    {
        size_t pageIndex = PageIndex(index);
        size_t elementIndex = ElementIndex(index);

        if (!m_validPages.Test(pageIndex) || !m_pages[pageIndex]->initializedBits.Test(elementIndex))
        {
            // element does not exist, nothing to erase
            // note: iterator will automatically skip to next
            return Iterator(this, pageIndex, elementIndex);
        }

        Page* page = m_pages[pageIndex];
        page->storage.DestructElement(elementIndex);
        page->initializedBits.Set(elementIndex, false);

        if (freeMemory && page->initializedBits.CountOnes() == 0)
        {
            // no elems remaining, remove the page reference.
            // page memory remains in the pool block; reclaimed at Clear/destruction.
            m_validPages.Set(pageIndex, false);

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

    size_t IndexOf(ConstIterator iter) const
    {
        AssertDebug(iter.array == this, "Iterator does not belong to this SparsePagedArray!");

        if (iter.elem >= PageSize || iter.page >= m_pages.Size())
        {
            return size_t(-1);
        }

        return size_t(iter.page * PageSize + iter.elem);
    }

    template <class Predicate>
    ConstIterator FindIf(Predicate&& predicate) const
    {
        return const_cast<SparsePagedArray&>(*this).FindIf(std::forward<Predicate>(predicate));
    }

    void Clear(bool freeMemory = true)
    {
        if (freeMemory)
        {
            for (size_t i = 0; i < m_nextPageSlot; i++)
            {
                const size_t blockIndex = i / PagesPerBlock;
                const size_t slotIndex = i % PagesPerBlock;

                m_pageBlocks[blockIndex]->GetPage(uint32(slotIndex))->~Page();
            }

            for (auto* block : m_pageBlocks)
            {
                GetAllocator()->Free(block);
            }

            m_pageBlocks.Clear();
            m_nextPageSlot = 0;
            m_validPages.Clear();
            m_pages.Clear();
        }
        else
        {
            for (Bitset::BitIndex bit : m_validPages)
            {
                HYP_CORE_ASSERT(bit < m_pages.Size());

                Page* page = m_pages[bit];
                HYP_CORE_ASSERT(page != nullptr);

                for (size_t elem : page->initializedBits)
                {
                    HYP_CORE_ASSERT(elem < PageSize);
                    page->storage.DestructElement(elem);
                }

                page->initializedBits = BitField<PageSize> {};
            }
        }
    }

    // Don't worry about the const casts -- we return ConstIterator if this is const this will be const again

    HYP_DEF_STL_BEGIN_END(Iterator(const_cast<SparsePagedArray*>(this), 0, 0), Iterator(const_cast<SparsePagedArray*>(this), m_pages.Size(), PageSize));

    /*! \brief Pre-allocate storage for the given number of pages.
     *  Allocates page blocks and resizes the page pointer array to avoid
     *  repeated reallocations during insertion. Pages are constructed on-demand
     *  when elements are actually inserted. */
    void ReservePages(size_t numPages)
    {
        const size_t blocksNeeded = (numPages + PagesPerBlock - 1) / PagesPerBlock;

        while (m_pageBlocks.Size() < blocksNeeded)
        {
            PageBlock* block = (PageBlock*)GetAllocator()->Allocate(sizeof(PageBlock), alignof(PageBlock));
            m_pageBlocks.PushBack(block);
        }

        if (m_pages.Size() < numPages)
        {
            m_pages.Resize(numPages);
        }
    }

    /*! \brief Pre-allocate storage for all pages needed to hold indices up to \p maxIndex (inclusive). */
    void Reserve(size_t maxIndex)
    {
        ReservePages(PageIndex(maxIndex) + 1);
    }

protected:
    static constexpr uint64 PageSizeBits = MathUtil::FastLog2_Pow2(PageSize);

    HYP_FORCE_INLINE static constexpr size_t PageIndex(size_t index)
    {
        return index >> PageSizeBits;
    }

    HYP_FORCE_INLINE static constexpr size_t ElementIndex(size_t index)
    {
        return index & (PageSize - 1);
    }

    Page* GetOrAllocatePage(size_t pageIndex)
    {
        if (!m_validPages.Test(pageIndex))
        {
            HYP_CORE_ASSERT(pageIndex < UINT32_MAX, "Invalid page index requested - will cause OOM crash!");

            if (m_pages.Size() <= pageIndex)
            {
                m_pages.Resize(pageIndex + 1);
            }

            const size_t blockIndex = m_nextPageSlot / PagesPerBlock;
            const size_t slotIndex = m_nextPageSlot % PagesPerBlock;

            if (blockIndex >= m_pageBlocks.Size())
            {
                PageBlock* block = (PageBlock*)GetAllocator()->Allocate(sizeof(PageBlock), alignof(PageBlock));
                m_pageBlocks.PushBack(block);
            }

            Page* page = new (m_pageBlocks[blockIndex]->GetPage(uint32(slotIndex))) Page();
            ++m_nextPageSlot;

            m_pages[pageIndex] = page;
            m_validPages.Set(pageIndex, true);
        }

        return m_pages[pageIndex];
    }

    HYP_FORCE_INLINE static AllocatorType* GetAllocator()
    {
        return GetDefaultAllocatorInstance<AllocatorType>();
    }

    Array<Page*, AllocatorType> m_pages;
    TBitset<AllocatorType> m_validPages;
    Array<PageBlock*, AllocatorType> m_pageBlocks;
    size_t m_nextPageSlot = 0;
};

} // namespace containers

template <class T, uint32 PageSize = 16, class AllocatorType = DynamicAllocator>
using SparsePagedArray = containers::SparsePagedArray<T, PageSize, AllocatorType>;

} // namespace Hyperion
