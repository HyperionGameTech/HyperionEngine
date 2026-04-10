// #define FX_PAGED_ARRAY_USE_MALLOC 1


// #include "FxMemPool.hpp"

// #include "../FxDefines.hpp"
// #include "../FxPanic.hpp"
// #include "../FxTypes.hpp"

// #include <Math/FxMathUtil.hpp>

// // Align allocations to 16 bytes
// static constexpr uint32 scAlignToBytes = 16;

// // Allocates enough nodes given all allocations are approximately `scPredictedAllocSize`. This is
// // just to prevent additional calls to `malloc` during calls to our `Alloc` functions.
// static constexpr uint32 scPredictedAllocSize = 512;

// void FxMemPoolPage::Create(uint64 size)
// {
// #ifdef FX_MEMPOOL_USE_ATOMIC_LOCKING
//     FxSpinThreadGuard guard(&mInUse);
// #endif

//     if (mMem || mCapacity) {
//         FxPanic("FxMemPool", "Memory pool has already been initialized!");
//     }

//     mCapacity = size;

//     void* allocated_ptr = std::malloc(mCapacity);
//     if (allocated_ptr == nullptr) {
//         FxPanic("FxMemPool", "Error allocating buffer for memory pool!");
//     }

//     mMem = static_cast<uint8*>(allocated_ptr);

//     // This should allocate enough items to avoid resizing the paged arrays / linked list
//     const uint32 num_mem_blocks = size / scPredictedAllocSize;
//     mMemBlocks.Create(num_mem_blocks);

//     FxLogDebug("Allocated {} mem block slots!", num_mem_blocks);
// }

// static inline uint8* GetAlignedPtr(uint8* ptr)
// {
//     return reinterpret_cast<uint8*>(FxMath::AlignValue<scAlignToBytes>(reinterpret_cast<uintptr_t>(ptr)));
// }


// #ifdef FX_MEMPOOL_NEXT_FIT

// /**
//  * @brief
//  * Allocates a new memory block in the page using a similar to next-fit method.
//  *
//  * This function searches for an open block of memory by first going to the end of the
//  * last allocation point. If there is no room left in the page, we then find the first gap
//  * between allocations.
//  *
//  * In almost every case this is multiple times faster than MSVC's new and delete!
//  *
//  * @param requested_size The size of the buffer that we want to allocate
//  * @return The memory block's node in the pool
//  */
// auto FxMemPoolPage::AllocateMemory(uint64 requested_size) -> FxMPLinkedList<FxMemPoolPage::MemBlock>::Node*
// {
//     // #ifdef FX_MEMPOOL_USE_ATOMIC_LOCKING
//     FxSpinThreadGuard guard(&mInUse);
//     // #endif

//     auto* node = mMemBlocks.Head;

//     uint8* new_block_ptr = mMem;

//     const uint64 aligned_size = FxMath::AlignValue<scAlignToBytes>(requested_size);

//     // If there are no allocations currently, allocate at start and exit early
//     if (!mMemBlocks.Tail) {
//         // Create a new block entry for the allocation
//         FxMemPoolPage::MemBlock new_block { .Size = aligned_size, .Start = GetAlignedPtr(new_block_ptr) };

//         auto* new_node = mMemBlocks.InsertFirst(std::move(new_block));

//         return new_node;
//     }

//     // There are previous allocations, so we can start at the end of the last one.

//     // Note that we will not track block sizes that are freed(with mFreedTotalSize) when they are at the end of
//     // the page since we will hold very limited info on those blocks. We cannot determine whether the block has
//     // been freed or if its not been allocated at all.

//     const FxMemPoolPage::MemBlock& last_block = mMemBlocks.Tail->Data;
//     new_block_ptr = last_block.Start + last_block.Size;

//     const uint64 bytes_used_in_page = static_cast<uint64>(new_block_ptr - mMem);

//     // Our potential allocation fits, return our entry!
//     if (bytes_used_in_page < FxMath::AlignValue<scAlignToBytes>(mCapacity)) {
//         // Create a new block entry for the allocation
//         FxMemPoolPage::MemBlock new_block { .Size = aligned_size, .Start = GetAlignedPtr(new_block_ptr) };

//         auto* new_node = mMemBlocks.InsertLast(std::move(new_block));

//         return new_node;
//     }

//     // Check to see if there is unallocated space at the start of the page.
//     // If our node is the first node and does not match the start of our buffer, there is a gap there.
//     if (node->Prev == nullptr && node == mMemBlocks.Head && node->Data.Start != mMem) {
//         const uint64 gap_size = (node->Data.Start) - mMem;

//         // There is a gap large enough, we can get our memory block here.

//         // The idea here is to put the allocation as close as we can to the current first allocation in the buffer,
//         // at the very end of the gap. Since we already have this allocation, this will save us from having to
//         iterate
//         // on previous allocations if another future allocation can fit in the gap at the start as well.
//         //
//         // This won't make a meaningful difference in everyday performance, but it will save a few cycles in future
//         // allocations.
//         //
//         // +----------+===========+-----------+------          +===========+----------+------------+------
//         // | GAP      | NEW HEAD  | PREV HEAD | ...      VS.   | NEW HEAD  | GAP      | PREV HEAD  | ...
//         // +----------+===========+-----------+------          +===========+----------+------------+------

//         if (gap_size >= aligned_size) {
//             FxMemPoolPage::MemBlock new_block { .Size = aligned_size, .Start = node->Data.Start - aligned_size };

//             mFreedTotalSize -= aligned_size;

//             return mMemBlocks.InsertFirst(std::move(new_block));
//         }
//     }

//     // If there have been no previous blocks freed, exit out as there will(SHOULD) be no gaps
//     if (!HasFreeSpace<true>()) {
//         return nullptr;
//     }

//     // There are no fun tricks left, we need to search for first fit now.

// #ifdef FX_MEMPOOL_WARN_SLOW_ALLOC
//     FxLogWarning("Using slow allocation path in FxMemPool!");
// #endif

//     // Since there are blocks allocated and there is not enough space for our allocations at the end,
//     // we should last-resort check to see if any blocks previously allocated in the page have freed up.
//     // Since there is a higher chance of the earlier blocks being freed, we will check there first.

//     // Walk through currently allocated blocks. If there is a gap between the nodes, check for a large enough space
//     // for our allocation.

//     while (node != nullptr) {
//         FxMemPoolPage::MemBlock& block = node->Data;

//         // If there is no node next, break as we won't find a gap. This means we have reached the end of the page
//         // as we have already checked for allocation space at the end.
//         if (node->Next == nullptr) {
//             break;
//         }

//         FxMemPoolPage::MemBlock& next_block = node->Next->Data;

//         uint8* current_block_end = block.Start + block.Size;
//         const int64 gap_size = next_block.Start - current_block_end;

//         // The gap is large enough for our buffer, take it
//         if (gap_size >= aligned_size) {
//             // We have an allocation at home (._.)

//             FxMemPoolPage::MemBlock new_block { .Size = aligned_size, .Start = GetAlignedPtr(current_block_end) };

//             mFreedTotalSize -= aligned_size;

//             // Track that the block is now allocated
//             return mMemBlocks.InsertAfterNode(new_block, node);
//         }

//         node = node->Next;
//     }

//     // There is no space remaining in the memory pool page, we return null here so the memory
//     // pool can allocate a new page
//     return nullptr;
// }
// #endif

// auto FxMemPoolPage::GetNodeFromPtr(void* ptr) const -> FxMPLinkedList<FxMemPoolPage::MemBlock>::Node*
// {
//     FxSpinThreadGuard guard(&mInUse);

//     // Start from the tail as most allocations being freed will be recent ones.
//     auto* node = mMemBlocks.Tail;

//     while (node != nullptr) {
//         if (node->Data.Start == ptr) {
//             return node;
//         }

//         if (node->Prev == mMemBlocks.Tail || node->Next == mMemBlocks.Head || node->Prev == node) {
//             FX_BREAKPOINT;
//         }

//         node = node->Prev;
//     }
//     return nullptr;
// }

// FxMemPoolLegacy& FxMemPoolLegacy::GetGlobalPool()
// {
//     static FxMemPoolLegacy global_pool;
//     return global_pool;
// }

// #ifdef FX_MEMPOOL_TRACK_STATISTICS
// #define ALLOC_UPDATE_STATS \
//     if (mBytesInUse > mStats.BytesAllocatedPeak) { \
//         mStats.BytesAllocatedPeak = mBytesInUse; \
//     } \
//     ++mStats.TotalAllocs;
// #else
// #define ALLOC_UPDATE_STATS
// #endif

// #define ALLOC_RETURN_NEW_ALLOCATION                                                                                    \
//     mBytesInUse += node->Data.Size;                                                                                    \
//     ALLOC_UPDATE_STATS;                                                                                                \
//     return node;


// auto FxMemPoolLegacy::AllocateMemory(uint64 requested_size) -> FxMPLinkedList<FxMemPoolPage::MemBlock>::Node*
// {
//     FxSpinThreadGuard guard(&mbInUse);

//     auto* node = mpCurrentPage->AllocateMemory(requested_size);

//     // Check all pages except the current page.
//     const uint32 pool_pages_count = mPoolPages.Size() - 1;

//     // If our node was successfully allocated, return it!
//     if (node != nullptr) {
//         ALLOC_RETURN_NEW_ALLOCATION;
//     }

//     // If we could not allocate any memory in the current page, try to allocate in one of the previous pages.
//     for (int i = 0; i < pool_pages_count; i++) {
//         FxMemPoolPage& page = mPoolPages[i];

//         // If the page has free space, try to allocate in the page.
//         if (page.HasFreeSpace()) {
//             node = page.AllocateMemory(requested_size);
//         }

//         // If the allocation was successful, exit with our new block
//         if (node != nullptr) {
//             ALLOC_RETURN_NEW_ALLOCATION;
//         }
//     }

//     // There are no previous pages that have free space, allocate a new one!

//     if (node == nullptr) {
//         printf("Allocating new MemPool page!\n");
//         AllocateNewPage();
//         node = mpCurrentPage->AllocateMemory(requested_size);
//     }

//     // If we have a fresh memory block and that is still too small, we can assume
//     // the memory block is larger than page size and fail.
//     if (node == nullptr) {
//         FxPanic("FxMemPool", "Allocation is too large for memory pool page size!");
//     }

//     ALLOC_RETURN_NEW_ALLOCATION;
// }


// void* FxMemPoolPage::Realloc(void* ptr, uint32 new_size)
// {
//     auto* node = GetNodeFromPtr(ptr);

//     // Adjust the count of bytes allocated
//     // int64 change_in_size = static_cast<int64>(new_size) - node->Data.Size;

//     FxMemPoolPage::MemBlock& block = node->Data;

//     // Node is at end of block list, simply increase size
//     if (node->Next == nullptr) {
//         node->Data.Size = FxMath::AlignValue<scAlignToBytes>(new_size);
//         return node->Data.Start;
//     }
//     // There is a node after this, check the gap

//     FxMemPoolPage::MemBlock& next_block = node->Next->Data;
//     intptr_t gap_size = next_block.Start - (block.Start + block.Size);

//     // There is enough space between the nodes, reallocate in place
//     if (gap_size >= new_size) {
//         node->Data.Size = FxMath::AlignValue<scAlignToBytes>(new_size);
//         return node->Data.Start;
//     }

//     // Worst case scenario, allocate a new block and copy to there.
//     void* new_mem = Alloc<void>(new_size);
//     std::memcpy(new_mem, block.Start, block.Size);

//     // Remove the previous allocation from the memory pool
//     mMemBlocks.DeleteNode(node);

//     // Return the new block of memory
//     return new_mem;
// }


// void FxMemPoolPage::PrintAllocations() const
// {
//     auto* node = mMemBlocks.Head;

//     FxLogDebug("");
//     FxLogDebug("=== MemStat [size={:d}, start={:p}, end={:p}, freed={:d}] ===", mCapacity,
//                reinterpret_cast<void*>(mMem), reinterpret_cast<void*>(mMem + mCapacity), mFreedTotalSize);

//     while (node != nullptr) {
//         FxMemPoolPage::MemBlock& block = node->Data;

//         if (node->Next) {
//             FxMemPoolPage::MemBlock& next_block = node->Next->Data;

//             uint8* current_block_end = block.Start + block.Size;
//             const uint64 gap_size = next_block.Start - current_block_end;

//             if (gap_size) {
//                 FxLogDebug("MemGap  [size={:d}, ptr={:p}]", gap_size, reinterpret_cast<void*>(current_block_end));
//             }
//         }

//         if (node->Prev == nullptr && node == mMemBlocks.Head && node->Data.Start != mMem) {
//             FxMemPoolPage::MemBlock& block = node->Data;

//             uint64 gap_size = block.Start - mMem;
//             if (gap_size) {
//                 FxLogDebug("MemGap  [size={:d}, ptr={:p}]", gap_size, reinterpret_cast<void*>(mMem));
//             }
//         }

//         FxLogDebug("MemAlloc[size={:d}, ptr={:p}]", block.Size, reinterpret_cast<void*>(block.Start));

//         node = node->Next;
//     }
//     FxLogDebug("");
// }

// void FxMemPoolPage::Destroy()
// {
//     std::free(mMem);
//     mCapacity = 0;
// }


// ////////////////////////////
// // FxMemPool Functions
// ////////////////////////////

// void FxMemPoolLegacy::Create(uint64 page_size, uint64 size_unit)
// {
//     FxSpinThreadGuard guard(&mbInUse);
//     {
//         mPageCapacity = page_size * size_unit;

//         mPoolPages.Create(8);
// #ifdef FX_MEMPOOL_DEBUG_CHECK_THREAD_OWNERSHIP
//         mCreatedThreadId = std::this_thread::get_id();
// #endif
//     }

//     AllocateNewPage();
// }

// void FxMemPoolLegacy::PrintAllocations()
// {
//     FxMemPoolLegacy& pool = GetGlobalPool();

//     int32 index = 0;

// #ifdef FX_MEMPOOL_TRACK_STATISTICS
//     printf("=== MemPool Stats ===\n");
//     printf("-> Total Allocs : %u\n", pool.mStats.TotalAllocs);
//     printf("-> Total Frees  : %u\n", pool.mStats.TotalFrees);
//     printf("=====================\n");
// #endif
//     for (const auto& page : pool.mPoolPages) {
//         printf("=== MemPool Page %d ===\n", ++index);
//         page.PrintAllocations();
//     }
// }

// void* FxMemPoolLegacy::AllocRaw(uint32 size, FxMemPoolLegacy* pool)
// {
//     if (pool == nullptr) {
//         pool = &GetGlobalPool();
//     }

//     // Note: `AllocateMemory()` locks the atomic, so we do not need to do it here or in `Alloc<>()`.

//     FxMemPoolPage::MemBlock& block = pool->AllocateMemory(size)->Data;

//     return static_cast<void*>(block.Start);
// }

// bool FxMemPoolLegacy::IsPtrInPage(void* ptr, FxMemPoolPage* page) const
// {
//     const uint8* start_ptr = page->mMem;
//     const uint8* end_ptr = start_ptr + mPageCapacity;

//     // If the ptr is within the bounds of the page, return the page
//     if (ptr >= start_ptr && ptr <= end_ptr) {
//         return true;
//     }

//     return false;
// }

// FxMemPoolPage* FxMemPoolLegacy::FindPtrInPage(void* ptr)
// {
//     // Iterate in reverse as most allocations being freed are likely to be closer to the most recent allocation.
//     size_t num_pages = mPoolPages.Size();

//     if (num_pages == 0) {
//         FxLogError("Cannot find ptr in page when there are no pages in memory pool!");
//     }

//     for (int32 i = num_pages - 1; i >= 0; --i) {
//         FxMemPoolPage* page = &mPoolPages[i];

//         if (IsPtrInPage(ptr, page)) {
//             return page;
//         }
//     }

//     // The ptr was not found in any pages
//     return nullptr;
// }

// void FxMemPoolLegacy::AllocateNewPage()
// {
//     FxMemPoolPage* page = mPoolPages.Insert();

//     mpCurrentPage = page;
//     mpCurrentPage->Create(mPageCapacity);

//     FxLogInfo("Allocating a new memory page!");
// }

// void FxMemPoolLegacy::FreeRaw(void* ptr, FxMemPoolLegacy* pool)
// {
//     if (pool == nullptr) {
//         pool = &GetGlobalPool();
//     }

//     if (ptr == nullptr) {
//         return;
//     }

//     // If there are no pages the memory has 100% been freed already. freeing a ptr
//     // after the mempool is freed is super jank, but fine I guess lol
//     if (pool->mPoolPages.IsEmpty()) {
// #ifdef FX_BUILD_DEBUG
//         FxLogWarning("Attempting to free a ptr {:p} after mempool is destroyed", ptr);
//         FX_BREAKPOINT;
// #endif
//         return;
//     }

//     // Find the page that contains the allocation
//     FxMemPoolPage* page = pool->FindPtrInPage(ptr);

//     if (page == nullptr) {
//         FxLogError("FxMemPool::Free: Pointer {:p} not found! Has it been freed already?", ptr);

// #ifdef FX_BUILD_DEBUG
//         FX_BREAKPOINT;
// #endif

//         return;
//     }

//     const uint32 bytes_freed = page->Free(ptr);
//     pool->mBytesInUse -= bytes_freed;
// }
