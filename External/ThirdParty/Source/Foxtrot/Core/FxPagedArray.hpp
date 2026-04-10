#pragma once

#include <Core/FxPanic.hpp>
#include <Core/FxTypes.hpp>
#include <Core/MemPool/FxMemPool.hpp>
#include <FxEngine.hpp>


// There are going to likely be some instances where there are static variables that create paged arrays. Since they are
// static and likely destroyed AFTER the memory pool, we will only allocate or free if the pool exists. This will not
// leak any memory since the block has already been freed.
#define FX_PAGED_ARRAY_ALLOC(type_, size_) (gEnginePool ? gEnginePool->Alloc<type_>(size_) : nullptr)

#define FX_PAGED_ARRAY_FREE(type_, ptr_)                                                                               \
    if (gEnginePool) {                                                                                                 \
        gEnginePool->Free<type_>(ptr_);                                                                                \
    }


// The size that a paged array is initialized to if any modifying functions are called before `.Init()`.
#define FX_PAGED_ARRAY_AUTO_INIT_SIZE 32

/**
 * @brief A dynamic array of objects, with objects preallocated with a user-defined size.
 *
 * To use this class with a different allocator, include `FxPagedArrayImpl.hpp` with `FX_PAGED_ARRAY_ALLOC` and
 * `FX_PAGED_ARRAY_FREE` to the custom allocator functions.
 *
 * @tparam TElementType The type of the object that will be stored.
 *
 */
template <typename TElementType>
class FxPagedArray
{
public:
    struct Page
    {
        TElementType* pData = nullptr;

        /** The number of elements in use in `Data` */
        uint32 Size;

        Page* pNext;
        Page* pPrev;

    public:
        void Destroy()
        {
            if (!pData) {
                return;
            }

            // Call the destructor for each item in the page
            if constexpr (std::is_destructible_v<TElementType>) {
                for (uint32 i = 0; i < Size; i++) {
                    pData[i].~TElementType();
                }
            }

            // Since we have already called the destructors on each object, we don't want to call the
            // destructor on the ptr of the object. Free the page without calling the destructor
            FX_PAGED_ARRAY_FREE(void, (reinterpret_cast<void*>(pData)));
            pData = nullptr;
        }
    };

public:
    struct Iterator
    {
        Iterator(Page* current_page, uint32 index) : mCurrentPage(current_page), mCurrentIndex(index) {}

        Iterator& operator++()
        {
            ++mCurrentIndex;

            if (mCurrentIndex > mCurrentPage->Size) {
                mCurrentPage = mCurrentPage->pNext;
                mCurrentIndex = 0;
            }

            return *this;
        }

        Iterator operator++(int)
        {
            Iterator iterator = *this;
            ++*this;

            return iterator;
        }

        Iterator& operator--()
        {
            if (mCurrentIndex == 0) {
                mCurrentPage = mCurrentPage->pPrev;
                mCurrentIndex = mCurrentPage->Size;
            }

            --mCurrentIndex;

            return *this;
        }

        TElementType& operator*() { return mCurrentPage->pData[mCurrentIndex]; }
        const TElementType& operator*() const { return mCurrentPage->pData[mCurrentIndex]; }

        bool operator!=(const Iterator& other)
        {
            return mCurrentPage && (mCurrentPage != other.mCurrentPage || mCurrentIndex != other.mCurrentIndex);
        }

        Page* mCurrentPage = nullptr;
        uint32 mCurrentIndex = 0;
    };

    FxPagedArray() = default;

    FxPagedArray(const FxPagedArray& other) { (*this) = other; }

    /**
     * @brief Creates a new paged array with a page capacity of `page_node_capacity`.
     */
    FxPagedArray(uint32 page_node_capacity) { Create(page_node_capacity); }

    FxPagedArray(FxPagedArray&& other) { (*this) = std::move(other); }

    FxPagedArray& operator=(const FxPagedArray& other)
    {
        pFirstPage = other.pFirstPage;
        pCurrentPage = other.pCurrentPage;

        PageNodeCapacity = other.PageNodeCapacity;
        CurrentPageIndex = other.CurrentPageIndex;

        TrackedSize = other.TrackedSize;

        return *this;
    }

    FxPagedArray& operator=(FxPagedArray&& other)
    {
        pFirstPage = other.pFirstPage;
        pCurrentPage = other.pCurrentPage;

        PageNodeCapacity = other.PageNodeCapacity;
        CurrentPageIndex = other.CurrentPageIndex;

        TrackedSize = other.TrackedSize;

        other.pFirstPage = nullptr;
        other.pCurrentPage = nullptr;

        other.PageNodeCapacity = 0;
        other.CurrentPageIndex = 0;

        other.TrackedSize = 0;

        return *this;
    }

    Iterator begin() const { return Iterator(pFirstPage, 0); }

    Iterator end() const { return Iterator(pCurrentPage, pCurrentPage ? pCurrentPage->Size : 0); }

    size_t GetCalculatedSize() const
    {
        size_t size = 0;
        Page* page = pFirstPage;

        while (page != nullptr) {
            size += page->Size;
            page = page->pNext;
        }

        FxAssert(size == TrackedSize);

        return size;
    }

    inline size_t Size() const
    {
        // return GetCalculatedSize();
        return TrackedSize;
    }

    void Create(uint32 page_node_capacity = 32)
    {
        // We are already allocated, free the previous pages
        // TODO: replace this with some reallocs!
        if (pFirstPage || pCurrentPage) {
            Destroy();
        }

        PageNodeCapacity = page_node_capacity;

        pFirstPage = AllocateNewPage(nullptr, nullptr);
        pCurrentPage = pFirstPage;
    }

    bool IsInited() const { return (pFirstPage != nullptr && pCurrentPage != nullptr); }

    TElementType* Insert()
    {
        InitializedIfNeeded();

        // Create a new item and initialize it
        TElementType* element = &pCurrentPage->pData[pCurrentPage->Size];

        if constexpr (std::is_constructible_v<TElementType>) {
            ::new (element) TElementType;
        }

        // Move to the next index
        ++pCurrentPage->Size;

        ++TrackedSize;

        // There are no free slots left in the page, allocate a new page
        if (pCurrentPage->Size >= PageNodeCapacity) {
            // Since the size will be N + 1, decrement by one
            --pCurrentPage->Size;

            Page* new_page = AllocateNewPage(pCurrentPage, nullptr);

            pCurrentPage->pNext = new_page;
            pCurrentPage = new_page;

            FxLogDebug("Allocating new page for FxPagedArray");
        }

        return element;
    }

    TElementType& Insert(const TElementType& element)
    {
        InitializedIfNeeded();

        TElementType* new_element = &pCurrentPage->pData[pCurrentPage->Size];

        if constexpr (std::is_copy_constructible_v<TElementType>) {
            ::new (new_element) TElementType(element);
        }
        else {
            memcpy(new_element, &element, sizeof(TElementType));
        }

        ++pCurrentPage->Size;

        ++TrackedSize;

        // There are no free slots left in the page, allocate a new page
        if (pCurrentPage->Size >= PageNodeCapacity) {
            // Since the size will be N + 1, decrement by one
            --pCurrentPage->Size;

            Page* new_page = AllocateNewPage(pCurrentPage, nullptr);

            pCurrentPage->pNext = new_page;
            pCurrentPage = new_page;

            FxLogDebug("Allocating new page for FxPagedArray");
        }

        return *new_element;
    }

    TElementType& Insert(TElementType&& element)
    {
        InitializedIfNeeded();

        TElementType* new_element = &pCurrentPage->pData[pCurrentPage->Size];

        if constexpr (std::is_move_constructible_v<TElementType>) {
            ::new (new_element) TElementType(std::move(element));
        }
        else {
            memcpy(new_element, &element, sizeof(TElementType));
        }

        ++pCurrentPage->Size;

        ++TrackedSize;

        // There are no free slots left in the page, allocate a new page
        if (pCurrentPage->Size >= PageNodeCapacity) {
            // Since the size will be N + 1, decrement by one
            --pCurrentPage->Size;

            Page* new_page = AllocateNewPage(pCurrentPage, nullptr);

            pCurrentPage->pNext = new_page;
            pCurrentPage = new_page;

            FxLogDebug("Allocating new page for FxPagedArray");
        }

        return *new_element;
    }

    Page* FindPageForElement(TElementType* value)
    {
        Page* current_page = pCurrentPage;

        while (current_page != nullptr) {
            const void* data_start_ptr = reinterpret_cast<char*>(current_page->pData);
            const void* data_end_ptr = data_start_ptr + (sizeof(TElementType) * PageNodeCapacity);

            const char* value_u8 = reinterpret_cast<char*>(value);

            // If the value is within the bounds of this pages' pointers, then we know it is in this page.
            if (value_u8 >= data_start_ptr && value_u8 <= data_end_ptr) {
                return current_page;
            }
        }
        return nullptr;
    }

    TElementType& GetLast() { return pCurrentPage->pData[pCurrentPage->Size - 1]; }

    TElementType* RemoveLast()
    {
        // If there are no pages remaining, return null
        if (pCurrentPage == nullptr) {
            return nullptr;
        }

        SizeCheck(pCurrentPage->Size);

        TElementType* element = &GetLast();

        // If there are no items left in the page and there is another page before this one, switch
        // to that page.
        if (pCurrentPage->Size == 0 && pCurrentPage->pPrev) {
            Page* page_to_remove = pCurrentPage;

            pCurrentPage = page_to_remove->pPrev;
            pCurrentPage->pNext = nullptr;

            if (page_to_remove != nullptr) {
                page_to_remove->Destroy();
            }

            FX_PAGED_ARRAY_FREE(Page, page_to_remove);

            --CurrentPageIndex;

            return element;
        }

        if (pCurrentPage->Size == 0 && pCurrentPage->pPrev == nullptr) {
            return nullptr;
        }

        --pCurrentPage->Size;

        --TrackedSize;

        return element;
    }

    void Clear()
    {
        Destroy();
        Create(PageNodeCapacity);
    }

    inline bool IsEmpty() const
    {
        if (pFirstPage == nullptr || pCurrentPage == nullptr) {
            return true;
        }

        if (pCurrentPage == pFirstPage && pCurrentPage->Size == 0) {
            return true;
        }

        return false;
    }


    TElementType& operator[](size_t index) const { return Get(index); }

    TElementType& Get(size_t index) const
    {
        Page* page = nullptr;
        const uint32 dest_page_index = (index / PageNodeCapacity);
        uint32 start_index = 0;

        // If the page index is closer to the end, start from the last page
        if ((CurrentPageIndex - dest_page_index) < dest_page_index) {
            page = pCurrentPage;
            start_index = CurrentPageIndex;

            while (start_index != dest_page_index) {
                page = page->pPrev;
                --start_index;
            }
        }
        // Start from the first page
        else {
            page = pFirstPage;

            while (start_index != dest_page_index) {
                page = page->pNext;
                ++start_index;
            }
        }

        const uint32 item_index = index - (dest_page_index * PageNodeCapacity);

        return page->pData[item_index];
    }


    void Destroy()
    {
        TrackedSize = 0;
        pCurrentPage = nullptr;

        if (pFirstPage == nullptr || bDoNotDestroy) {
            return;
        }

        Page* current_page = pFirstPage;

        while (current_page != nullptr) {
            Page* next_page = current_page->pNext;

            if (current_page != nullptr) {
                current_page->Destroy();
            }

            FX_PAGED_ARRAY_FREE(Page, current_page);

            current_page = next_page;
        }


        pFirstPage = nullptr;
    }

    ~FxPagedArray()
    {
        if (pFirstPage == nullptr) {
            return;
        }

        Destroy();
    }

private:
    Page* AllocateNewPage(Page* prev, Page* next)
    {
        // Allocate and initialize the page object
        Page* page = FX_PAGED_ARRAY_ALLOC(Page, sizeof(Page));
        if (page == nullptr) {
            FxPanic("FxPagedArray", "Memory error allocating page");
            return nullptr;
        }

        page->Size = 0;
        page->pNext = next;
        page->pPrev = prev;

        // Allocate the buffer of nodes in the page
        void* allocated_nodes = FX_PAGED_ARRAY_ALLOC(TElementType, (sizeof(TElementType) * PageNodeCapacity));

        if (allocated_nodes == nullptr) {
            FxPanic("FxPagedArray", "Memory error allocating page data");
            return nullptr;
        }

        page->pData = static_cast<TElementType*>(allocated_nodes);

        ++CurrentPageIndex;

        return page;
    }

    FX_FORCE_INLINE void SizeCheck(uint32 size) const
    {
        if (size > PageNodeCapacity) {
            FxPanic("FxPagedArray", "The current size of a page is greater than the allocated page node capacity!");
        }
    }

    /**
     * @brief Checks if the paged array is initialized. If not, this function automatically initialized it.
     */
    FX_FORCE_INLINE void InitializedIfNeeded()
    {
        if (IsInited()) {
            return;
        }

#ifdef FX_WARN_PAGED_ARRAY_AUTO_INIT
        FxLogWarning("FxPagedArray was not previously initialized, auto initializing!");
#endif

        Create(FX_PAGED_ARRAY_AUTO_INIT_SIZE);
    }

public:
    /// The number of objects that are allocated in a page.
    uint32 PageNodeCapacity = 0;

    Page* pFirstPage = nullptr;
    Page* pCurrentPage = nullptr;
    int32 CurrentPageIndex = 0;

    /// If true, this array will not be automatically freed.
    bool bDoNotDestroy = false;


    /// The number of inserted objects
    uint32 TrackedSize = 0;
};
