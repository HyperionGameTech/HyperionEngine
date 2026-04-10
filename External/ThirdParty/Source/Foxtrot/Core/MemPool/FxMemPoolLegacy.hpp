// #pragma once

// #include "../FxDefines.hpp"
// #include "../FxTypes.hpp"

// #define FX_PAGED_ARRAY_USE_MALLOC 1
// #include "FxMPLinkedList.hpp"

// #include <type_traits>

// // NOTE: Option macros are defined in FxDefines.hpp

// #ifdef FX_MEMPOOL_USE_ATOMIC_LOCKING
// #include <atomic>
// #endif

// #if !defined(FX_MEMPOOL_USE_ATOMIC_LOCKING) && !defined(FX_MEMPOOL_DEBUG_CHECK_THREAD_OWNERSHIP)
// // If we are not using atomic locking, check for thread ownership to prevent race conditions
// #define FX_MEMPOOL_DEBUG_CHECK_THREAD_OWNERSHIP
// #endif

// #ifdef FX_MEMPOOL_DEBUG_CHECK_THREAD_OWNERSHIP
// #include <iostream>
// #include <thread>
// #endif

// struct FxMemPoolStatistics
// {
//     uint32 TotalAllocs = 0;
//     uint32 TotalFrees = 0;

//     uint64 BytesAllocatedPeak = 0;
// };

// class FxMemPoolPage
// {
// private:
//     struct MemBlock
//     {
//         uint64 Size = 0;
//         uint8* Start = nullptr;
//     };
//     friend class FxMemPoolLegacy;

// public:
//     FxMemPoolPage() = default;

//     FxMemPoolPage(FxMemPoolPage&& other) noexcept
//     {
//         mCapacity = other.mCapacity;
//         mMem = other.mMem;
//         mMemBlocks = std::move(other.mMemBlocks);

//         other.mMem = nullptr;
//         other.mCapacity = 0;
//     }

//     void Create(uint64 size);

//     /** Allocates a block of memory */
//     template <typename ElementType, typename... Args>
//     ElementType* Alloc(uint32 size, Args... args)
//     {
// #ifdef FX_MEMPOOL_USE_ATOMIC_LOCKING
//         FxSpinThreadGuard guard(&mInUse);
// #endif
//         MemBlock& block = AllocateMemory(size)->Data;

//         return static_cast<ElementType*>(block.Start);
//     }

//     void* Realloc(void* ptr, uint32 new_size);

//     template <typename Type>
//     Type* Realloc(Type* ptr, uint32 new_size)
//     {
// #ifdef FX_MEMPOOL_USE_ATOMIC_LOCKING
//         FxSpinThreadGuard guard(&mInUse);
// #endif
//         return static_cast<Type*>(Realloc(static_cast<void*>(ptr), new_size));
//     }

//     /**
//      * @brief Frees an allocated pointer from a memory pool page.
//      * @returns The size of the allocation that was freed.
//      */
//     template <typename ElementType>
//     uint32 Free(ElementType* ptr)
//     {
//         if (ptr == nullptr) {
//             return 0;
//         }


//         auto* node = GetNodeFromPtr(static_cast<void*>(ptr));
//         if (node == nullptr) {
//             FxLogError("FxMemPoolPage::Free: Could not find ptr {:p} in memory page!", ptr);
//             FX_BREAKPOINT;
//             return 0;
//         }

// #ifdef FX_MEMPOOL_USE_ATOMIC_LOCKING
//         FxSpinThreadGuard guard(&mInUse);
// #endif

//         const uint32 block_size = node->Data.Size;

//         // Do not mark as a freed block when we are at the end of the page as it is ambiguous
//         // on whether the end is a previously freed block or has not been previously allocated.
//         // We don't want to waste cycles on pages that have nothing to give us!

//         // There is a check in `HasFreeSpace()` that covers the case of freed space at the end.

//         if (node != mMemBlocks.Tail) {
//             mFreedTotalSize += block_size;
//         }

//         mMemBlocks.DeleteNode(node);

//         return block_size;
//     }

//     template <bool TOnlyCheckPreviouslyFreedBlocks = false>
//     bool HasFreeSpace() const
//     {
//         if (mFreedTotalSize > 0) {
//             return true;
//         }

//         // For `AllocateMemory()` where it already tests for free memory at the end of the page. We can
//         // skip the extraneous checks
//         if constexpr (TOnlyCheckPreviouslyFreedBlocks) {
//             return false;
//         }

//         // Checks if there is space remaining at the end of the page
//         const auto& final_block = mMemBlocks.Tail->Data;
//         uint64 current_page_size = static_cast<uint64>((final_block.Start + final_block.Size) - mMem);

//         if (current_page_size < mCapacity) {
//             return true;
//         }

//         return false;
//     }

//     void PrintAllocations() const;

//     void Destroy();
//     ~FxMemPoolPage() { Destroy(); }

//     inline bool IsInited() const { return (mMem != nullptr && mCapacity > 0); }

// private:
//     auto AllocateMemory(uint64 requested_size) -> FxMPLinkedList<FxMemPoolPage::MemBlock>::Node*;
//     auto GetNodeFromPtr(void* ptr) const -> FxMPLinkedList<FxMemPoolPage::MemBlock>::Node*;

//     inline void CheckInited()
//     {
//         if (!IsInited()) {
//             FxPanic("FxMemoryPage", "Page has not been initialized!");
//         }
//     }

// private:
//     /**
//      * @brief The allocated capacity of the page
//      */
//     uint64 mCapacity = 0;

//     /**
//      * @brief The total amount of bytes that have been freed in the page
//      */
//     int64 mFreedTotalSize = 0;

//     /**
//      * @brief The page's allocated memory
//      */
//     uint8* mMem = nullptr;

//     FxMPLinkedList<MemBlock> mMemBlocks;

//     mutable FxAtomicFlag mInUse {};
// };

// class FxMemPoolLegacy
// {
// public:
//     FxMemPoolLegacy() = default;

//     static FxMemPoolLegacy& GetGlobalPool();

//     /**
//      * Creates a new memory pool with paging.
//      * @param page_capacity The size of each page in `size_unit` units.
//      * @param size_unit The unit to multiply the `page_capacity` by (ex: `FxUnitMebibyte`)
//      */
//     void Create(uint64 page_size, uint64 size_unit);

//     /**
//      * Allocates a raw memory block on a memory pool.
//      * @param pool The memory pool to allocate the memory block from. If nullptr, the global pool is used.
//      */
//     static void* AllocRaw(uint32 size, FxMemPoolLegacy* pool = nullptr);

//     /**
//      * Allocates a memory block on the global memory pool.
//      * @param args The arguments to construct the object with.
//      */
//     template <typename Type, typename... Args>
//     static Type* Alloc(uint32 size, Args&&... args)
//     {
//         Type* ptr = static_cast<Type*>(AllocRaw(size, nullptr));

//         // Use a placement new if the object is constructable with the provided arguments
//         if constexpr (std::is_constructible_v<Type, Args...>) {
//             ::new (ptr) Type(std::forward<Args>(args)...);
//         }
//         // else {
//         // printf("Not constructable %s\n", typeid(Type).name());
//         // }

//         return ptr;
//     }

//     /**
//      * Allocates memory on a given memory pool.
//      * @param pool The memory pool to allocate the memory block from.
//      * @param args The arguments to construct the object with.
//      */
//     template <typename Type, typename... Args>
//     static Type* AllocInPool(uint32 size, FxMemPoolLegacy* pool, Args&&... args)
//     {
//         Type* ptr = static_cast<Type*>(AllocRaw(size, pool));

//         // Use a placement new if the object is constructable with the provided arguments
//         if constexpr (std::is_constructible_v<Type, Args...>) {
//             ::new (ptr) Type(std::forward<Args>(args)...);
//         }

//         return ptr;
//     }

//     /**
//      * @brief Gets the total capacity in bytes of the memory pool.
//      */
//     FX_FORCE_INLINE uint64 GetTotalCapacity() const { return (mPageCapacity * mPoolPages.Size()); }

//     /**
//      * @brief Calculates the total amount of bytes in use across all pages in the memory pool.
//      */
//     FX_FORCE_INLINE uint64 GetTotalUsed() const { return mBytesInUse; }

//     template <typename Type>
//     static void Free(Type* ptr, FxMemPoolLegacy* pool = nullptr)
//     {
//         if (pool == nullptr) {
//             pool = &GetGlobalPool();
//         }

// #ifdef FX_MEMPOOL_DEBUG_CHECK_THREAD_OWNERSHIP
//         const auto& this_id = std::this_thread::get_id();

//         if (this_id != pool->mCreatedThreadId) {
//             FxLogWarning("Attempting to free memory from a different thread!");
//             std::cout << "Thread ids: " << pool->mCreatedThreadId << ", " << std::this_thread::get_id() << "\n";
//         }
// #endif
//         if constexpr (std::is_destructible_v<Type>) {
//             ptr->~Type();
//         }

//         FxSpinThreadGuard guard(&pool->mbInUse);

// #ifdef FX_MEMPOOL_TRACK_STATISTICS
//         ++pool->mStats.TotalFrees;
// #endif

//         FreeRaw(static_cast<void*>(ptr), pool);
//     }

//     const FxMemPoolStatistics& GetStatistics() const { return mStats; }

//     bool IsEmpty() const { return mPoolPages.IsEmpty(); }

//     static void PrintAllocations();

// private:
//     auto AllocateMemory(uint64 requested_size) -> FxMPLinkedList<FxMemPoolPage::MemBlock>::Node*;

//     FxMemPoolPage* FindPtrInPage(void* ptr);
//     inline bool IsPtrInPage(void* ptr, FxMemPoolPage* page) const;

//     void AllocateNewPage();


//     /** Frees an allocated pointer on the global memory pool */
//     static void FreeRaw(void* ptr, FxMemPoolLegacy* pool = nullptr);

// private:
//     FxMemPoolPage* mpCurrentPage = nullptr;
//     uint64 mPageCapacity = 0;

//     /**
//      * @brief The total amount of bytes in use by memory blocks
//      */
//     uint64 mBytesInUse = 0;

//     FxPagedArray<FxMemPoolPage> mPoolPages;

//     FxAtomicFlag mbInUse {};

// #ifdef FX_MEMPOOL_DEBUG_CHECK_THREAD_OWNERSHIP
//     std::thread::id mCreatedThreadId;
// #endif

//     FxMemPoolStatistics mStats;
// };

// #undef FX_PAGED_ARRAY_USE_MALLOC
