/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/ContainerBase.hpp>

#include <Core/Utilities/BitField.hpp>
#include <Core/Utilities/Pair.hpp>

#include <Core/Memory/Allocator/Allocator.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {
namespace containers {

template <class T, size_t NumElementsPerPage>
struct SparseArrayPage
{
    ValueStorage<T> data[NumElementsPerPage];
    BitField<NumElementsPerPage> states;

    SparseArrayPage* next;
    size_t pageIndex;
};

template <class T, class AllocatorType = DynamicAllocator, size_t NumElementsPerPage = 256>
class SparseArray : public ContainerBase<SparseArray<T, AllocatorType, NumElementsPerPage>, size_t>
{
    using Page = SparseArrayPage<T, NumElementsPerPage>;

public:
    static constexpr bool isContiguous = false;

    using Base = ContainerBase<SparseArray<T, AllocatorType, NumElementsPerPage>, size_t>;
    using ValueType = T;

    // number of bits to shift to use when calculating absolute index for a given page index.
    // amounts to the same as (page * NumElementsPerPage)
    static constexpr size_t PageBitShiftCount = MathUtil::FastLog2_Pow2(NumElementsPerPage);

    template <bool IsConst>
    struct IteratorBase
    {
        using ArrayType = std::conditional_t<IsConst, const SparseArray, SparseArray>;

        using PointerType = std::conditional_t<IsConst, const T*, T*>;
        using ReferenceType = std::conditional_t<IsConst, const T&, T&>;

        struct BeginTag
        {
        };

        ArrayType* target;
        Page* page;
        size_t elemIndex;

        HYP_FORCE_INLINE IteratorBase()
            : target(nullptr),
              page(nullptr),
              elemIndex(NumElementsPerPage)
        {
        }

        HYP_FORCE_INLINE IteratorBase(ArrayType* target, Page* page, size_t elemIndex)
            : target(target),
              page(page),
              elemIndex(elemIndex)
        {
        }

        HYP_FORCE_INLINE IteratorBase(ArrayType* target, BeginTag)
            : target(target),
              page(nullptr),
              elemIndex(NumElementsPerPage)
        {
            if (target != nullptr)
            {
                page = const_cast<Page*>(target->m_pages);
                Advance(0);
            }
        }

        HYP_FORCE_INLINE ReferenceType operator*() const
        {
            HYP_CORE_ASSERT(target != nullptr);
            HYP_CORE_ASSERT(page != nullptr);
            HYP_CORE_ASSERT(page->states.Test(elemIndex));

            return reinterpret_cast<ReferenceType>(page->data[elemIndex]);
        }

        HYP_FORCE_INLINE PointerType operator->() const
        {
            return &(**this);
        }

        HYP_FORCE_INLINE IteratorBase& operator++()
        {
            Advance(elemIndex + 1);

            return *this;
        }

        HYP_FORCE_INLINE IteratorBase operator++(int)
        {
            IteratorBase tmp = *this;
            ++(*this);

            return tmp;
        }

        HYP_FORCE_INLINE bool operator==(const IteratorBase& other) const
        {
            return target == other.target
                && page == other.page
                && elemIndex == other.elemIndex;
        }

        HYP_FORCE_INLINE bool operator!=(const IteratorBase& other) const
        {
            return !(*this == other);
        }

        void Advance(size_t startElem)
        {
            HYP_CORE_ASSERT(target != nullptr);

            size_t currElemIndex = startElem;

            while (page != nullptr)
            {
                if (currElemIndex < NumElementsPerPage)
                {
                    const size_t next = page->states.NextOneBit(currElemIndex);

                    if (next != SIZE_MAX)
                    {
                        elemIndex = next;

                        return;
                    }
                }

                page = page->next;
                currElemIndex = 0;
            }

            elemIndex = NumElementsPerPage;
        }
    };

    struct ConstIterator : IteratorBase<true>
    {
        using IteratorBase<true>::IteratorBase;
    };

    struct Iterator : IteratorBase<false>
    {
        using IteratorBase<false>::IteratorBase;

        HYP_FORCE_INLINE operator ConstIterator() const
        {
            return ConstIterator(this->target, this->page, this->elemIndex);
        }
    };

    SparseArray()
        : m_pages(nullptr)
    {
    }

    SparseArray(const SparseArray& other) = delete;
    SparseArray& operator=(const SparseArray& other) = delete;

    SparseArray(SparseArray&& other) noexcept
        : m_pages(other.m_pages)
    {
        other.m_pages = nullptr;
    }

    SparseArray& operator=(SparseArray&& other) noexcept
    {
        Clear(true);

        m_pages = other.m_pages;
        other.m_pages = nullptr;

        return *this;
    }

    ~SparseArray()
    {
        Clear(true);
    }

    HYP_FORCE_INLINE T& Get(size_t index)
    {
        HYP_CORE_ASSERT(HasIndex(index), "Index {} is not initialized in SparseArray!", index);

        size_t pageIndex = (index / NumElementsPerPage);
        size_t elemIndex = (index % NumElementsPerPage);

        Page& page = *TryGetPage(pageIndex);

        return reinterpret_cast<T&>(page.data[elemIndex]);
    }

    HYP_FORCE_INLINE const T& Get(size_t index) const
    {
        return const_cast<SparseArray*>(this)->Get(index);
    }

    HYP_FORCE_INLINE T& GetOrCreate(size_t index)
    {
        size_t pageIndex = (index / NumElementsPerPage);
        size_t elemIndex = (index % NumElementsPerPage);

        Page& page = *GetOrCreatePage(pageIndex);

        if (!page.states.Test(elemIndex))
        {
            new (&page.data[elemIndex]) T;
            page.states.Set(elemIndex, true);
        }

        return reinterpret_cast<T&>(page.data[elemIndex]);
    }

    HYP_FORCE_INLINE T& GetUnchecked(size_t index)
    {
        size_t pageIndex = (index / NumElementsPerPage);
        size_t elemIndex = (index % NumElementsPerPage);

        Page& page = *TryGetPage(pageIndex);
        return reinterpret_cast<T&>(page.data[elemIndex]);
    }

    HYP_FORCE_INLINE const T& GetUnchecked(size_t index) const
    {
        return const_cast<SparseArray*>(this)->GetUnchecked(index);
    }

    HYP_FORCE_INLINE T* TryGet(size_t index)
    {
        size_t pageIndex = (index / NumElementsPerPage);
        size_t elemIndex = (index % NumElementsPerPage);

        Page* page = TryGetPage(pageIndex);

        if (!page)
        {
            return nullptr;
        }

        return (page->states.Test(elemIndex) ? reinterpret_cast<T*>(&page->data[elemIndex]) : nullptr);
    }

    HYP_FORCE_INLINE const T* TryGet(size_t index) const
    {
        return const_cast<SparseArray*>(this)->TryGet(index);
    }

    template <class... Args>
    T& Emplace(size_t index, Args&&... args)
    {
        size_t pageIndex = (index / NumElementsPerPage);
        size_t elemIndex = (index % NumElementsPerPage);

        Page& page = *GetOrCreatePage(pageIndex);

        if (page.states.Test(elemIndex))
        {
            if constexpr (std::is_move_assignable_v<T>)
            {
                // Follow std::vector's logic. construct at a different place then move into the location
                reinterpret_cast<T&>(page.data[elemIndex]) = T(std::forward<Args>(args)...);
            }
            else
            {
                reinterpret_cast<T&>(page.data[elemIndex]).~T();
                new (&page.data[elemIndex]) T(std::forward<Args>(args)...);
            }
        }
        else
        {
            new (&page.data[elemIndex]) T(std::forward<Args>(args)...);
            page.states.Set(elemIndex, true);
        }

        return reinterpret_cast<T&>(page.data[elemIndex]);
    }

    void Set(size_t index, const T& value)
    {
        size_t pageIndex = (index / NumElementsPerPage);
        size_t elemIndex = (index % NumElementsPerPage);

        Page& page = *GetOrCreatePage(pageIndex);

        if (page.states.Test(elemIndex))
        {
            reinterpret_cast<T&>(page.data[elemIndex]) = value;
        }
        else
        {
            new (&page.data[elemIndex]) T(value);
            page.states.Set(elemIndex, true);
        }
    }

    void Set(size_t index, T&& value)
    {
        size_t pageIndex = (index / NumElementsPerPage);
        size_t elemIndex = (index % NumElementsPerPage);

        Page& page = *GetOrCreatePage(pageIndex);

        if (page.states.Test(elemIndex))
        {
            reinterpret_cast<T&>(page.data[elemIndex]) = std::move(value);
        }
        else
        {
            new (&page.data[elemIndex]) T(std::move(value));
            page.states.Set(elemIndex, true);
        }
    }

    /// Remove element at index \p{index} from the container, freeing the page if there are no more elements in it
    void Delete(size_t index)
    {
        const size_t pageIndex = (index / NumElementsPerPage);
        const size_t elemIndex = (index % NumElementsPerPage);

        Page** link = &m_pages;
        Page* curr = m_pages;

        while (curr != nullptr && curr->pageIndex < pageIndex)
        {
            link = &curr->next;
            curr = curr->next;
        }

        if (curr == nullptr || curr->pageIndex != pageIndex || !curr->states.Test(elemIndex))
        {
            return;
        }

        reinterpret_cast<T&>(curr->data[elemIndex]).~T();
        curr->states.Set(elemIndex, false);

        if (curr->states.CountOnes() == 0)
        {
            *link = curr->next;
            GetAllocator().Free(curr);
        }
    }

    /// Finds the absolute index of the iterator, SIZE_MAX on failure.
    HYP_FORCE_INLINE size_t IndexOf(const ConstIterator& iter) const
    {
        if (iter.target != this || iter.page == nullptr || iter.elemIndex >= NumElementsPerPage)
        {
            return SIZE_MAX;
        }

        return (iter.page->pageIndex << PageBitShiftCount) + iter.elemIndex;
    }

    /// Finds the absolute index of element \p{value}. Returns SIZE_MAX on failure.
    /// More hefty than IndexOf() with an iterator param. Searches through pages to find a page with matching base
    /// address and calculates it from that.
    size_t IndexOf(const T& value) const
    {
        const uintptr_t elemAddress = reinterpret_cast<uintptr_t>(std::addressof(value));

        constexpr size_t pageStorageSizeBytes = (1ull << PageBitShiftCount) * sizeof(T);

        for (Page* currPage = m_pages; currPage != nullptr; currPage = currPage->next)
        {
            const uintptr_t baseAddress = reinterpret_cast<uintptr_t>(&currPage->data[0]);

            if (elemAddress < baseAddress || elemAddress >= baseAddress + pageStorageSizeBytes)
            {
                continue; // pointer not in this page
            }

            return (currPage->pageIndex << PageBitShiftCount) + ((elemAddress - baseAddress) / sizeof(T));
        }

        return SIZE_MAX;
    }

    bool Any() const
    {
        for (Page* currPage = m_pages; currPage != nullptr; currPage = currPage->next)
        {
            if (currPage->states.CountOnes() != 0)
            {
                return true;
            }
        }

        return false;
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return !Any();
    }

    /// Count valid elements
    size_t Count() const
    {
        size_t count = 0;

        for (Page* currPage = m_pages; currPage != nullptr; currPage = currPage->next)
        {
            count += currPage->states.CountOnes();
        }

        return count;
    }

    bool HasIndex(size_t index) const
    {
        size_t pageIndex = (index / NumElementsPerPage);
        size_t elemIndex = (index % NumElementsPerPage);

        const Page* page = TryGetPage(pageIndex);

        if (!page)
        {
            return false;
        }

        return page->states.Test(elemIndex);
    }

    Iterator Erase(const ConstIterator& iter)
    {
        if (HYP_UNLIKELY(iter == End()))
        {
            return End();
        }

        Iterator iter2 = reinterpret_cast<const Iterator&>(iter);
        ++iter2;

        const size_t absoluteIndex = (iter.page->pageIndex << PageBitShiftCount) + iter.elemIndex;

        Delete(absoluteIndex);

        return iter2;
    }

    void Clear(bool freeMemory = true)
    {
        Page* currPage = m_pages;

        while (currPage != nullptr)
        {
            Page* next = currPage->next;

            for (size_t bitIndex : currPage->states)
            {
                reinterpret_cast<T&>(currPage->data[bitIndex]).~T();
            }

            if (freeMemory)
            {
                GetAllocator().Free(currPage);
            }
            else
            {
                Memory::Zero(&currPage->states, sizeof(currPage->states));
            }

            currPage = next;
        }

        if (freeMemory)
        {
            m_pages = nullptr;
        }
    }

    HYP_DEF_STL_BEGIN_END(
        Iterator(const_cast<SparseArray*>(this), typename Iterator::BeginTag()),
        Iterator(const_cast<SparseArray*>(this), nullptr, NumElementsPerPage))

private:
    static HYP_FORCE_INLINE AllocatorType& GetAllocator()
    {
        return *GetDefaultAllocatorInstance<AllocatorType>();
    }

    /// Finds the page for \p{pageIndex}, creating (and splicing into the sorted page list) one if it
    /// doesn't already exist. Only ever allocates the single page being requested.
    HYP_NODISCARD Page* GetOrCreatePage(size_t pageIndex)
    {
        Page** link = &m_pages;
        Page* curr = m_pages;

        while (curr != nullptr && curr->pageIndex < pageIndex)
        {
            link = &curr->next;
            curr = curr->next;
        }

        if (curr != nullptr && curr->pageIndex == pageIndex)
        {
            return curr;
        }

        Page* newPage = (Page*)GetAllocator().Allocate(sizeof(Page), alignof(Page));
        HYP_CORE_ASSERT(newPage != nullptr);

        Memory::Zero(newPage, sizeof(Page));

        newPage->pageIndex = pageIndex;
        newPage->next = curr;
        *link = newPage;

        return newPage;
    }

    /// Finds the page for \p{pageIndex}, or nullptr if no such page is resident
    Page* TryGetPage(size_t pageIndex)
    {
        for (Page* curr = m_pages; curr != nullptr; curr = curr->next)
        {
            if (curr->pageIndex == pageIndex)
            {
                return curr;
            }

            if (curr->pageIndex > pageIndex)
            {
                return nullptr;
            }
        }

        return nullptr;
    }

    HYP_FORCE_INLINE const Page* TryGetPage(size_t pageIndex) const
    {
        return const_cast<SparseArray*>(this)->TryGetPage(pageIndex);
    }

    Page* m_pages; // head of a linked list of pages, sorted ascending by pageIndex
};

} // namespace containers

using containers::SparseArray;

} // namespace Hyperion
