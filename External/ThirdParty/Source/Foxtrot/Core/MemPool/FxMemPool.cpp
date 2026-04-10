/*
** Two Level Segregated Fit memory allocator, version 3.1.
** Written by Matthew Conte
**	http://tlsf.baisoku.org
**
** Based on the original documentation by Miguel Masmano:
**	http://www.gii.upv.es/tlsf/main/docs
**
** This implementation was written to the specification
** of the document, therefore no GPL restrictions apply.
**
** Copyright (c) 2006-2016, Matthew Conte
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of the copyright holder nor the
**       names of its contributors may be used to endorse or promote products
**       derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL MATTHEW CONTE BE LIABLE FOR ANY
** DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
** LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
** ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "FxMemPool.hpp"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Core/FxBitUtil.hpp>
#include <Core/FxPanic.hpp>
#include <Math/FxMathUtil.hpp>

#define tlsf_decl inline

/// The total alignment and alignment of each allocation from the memory pool.
/// Default is 16 to allow for aligned loads and stores on ARM and modern x64.
static constexpr uint32 scAlignmentSize = 8;

/// The amount of bits to shift to get `scAlignmentSize`. Used in TLSF to get block sizes and index counts
static constexpr uint32 scAlignmentShift = (scAlignmentSize == 16 ? 4 : (scAlignmentSize == 8 ? 3 : 2));
static_assert((1 << scAlignmentShift) == scAlignmentSize);


using Status = FxMemPool::Status;

struct MemBlock;

FX_FORCE_INLINE static void* BlockToPtr(MemBlock* block);
FX_FORCE_INLINE static const void* BlockToPtr(const MemBlock* block);

static void* align_ptr(const void* ptr, size_t align);

/* Public constants: may be modified. */
enum tlsf_public
{
    /* log2 of number of linear subdivisions of block sizes. Larger
    ** values require more memory in the control structure. Values of
    ** 4 or 5 are typical.
    */
    SL_INDEX_COUNT_LOG2 = 5,
};

enum tlsf_private
{
    /*
    ** TODO: We can increase this to support larger sizes, at the expense
    ** of more overhead in the TLSF structure.
    */
    FL_INDEX_MAX = 32,

    SL_INDEX_COUNT = (1 << SL_INDEX_COUNT_LOG2),
    FL_INDEX_SHIFT = (SL_INDEX_COUNT_LOG2 + scAlignmentShift),
    FL_INDEX_COUNT = (FL_INDEX_MAX - FL_INDEX_SHIFT + 1),

    SMALL_BLOCK_SIZE = (1 << FL_INDEX_SHIFT),
};


/*
** Set assert macro, if it has not been provided by the user.
*/
#if !defined(tlsf_assert)
#define tlsf_assert assert
#endif


static_assert(sizeof(int) * CHAR_BIT == 32);
static_assert(sizeof(size_t) * CHAR_BIT >= 32);
static_assert(sizeof(size_t) * CHAR_BIT <= 64);

/* SL_INDEX_COUNT must be <= number of bits in sl_bitmap's storage type. */
static_assert(sizeof(unsigned int) * CHAR_BIT >= SL_INDEX_COUNT);

/* Ensure we've properly tuned our sizes. */
static_assert(scAlignmentSize == SMALL_BLOCK_SIZE / SL_INDEX_COUNT);

/*
** Data structures and associated constants.
*/


/*
** Since block sizes are always at least a multiple of 4, the two least
** significant bits of the size field are used to store the block status:
** - bit 0: whether block is busy or free
** - bit 1: whether previous block is busy or free
*/
static constexpr size_t scBlockFreeBit = 1 << 0;
static constexpr size_t scBlockPrevFreeBit = 1 << 1;


/*
** Block header structure.
**
** There are several implementation subtleties involved:
** - The prev_phys_block field is only valid if the previous block is free.
** - The prev_phys_block field is actually stored at the end of the
**   previous block. It appears at the beginning of this structure only to
**   simplify the implementation.
** - The next_free / prev_free fields are only valid if the block is free.
*/
struct alignas(8) MemBlock
{
public:
    size_t GetSize() const { return Size & ~(scBlockFreeBit | scBlockPrevFreeBit); }
    void SetSize(size_t size)
    {
        const uint32 flags = Size & (scBlockFreeBit | scBlockPrevFreeBit);
        Size = size | flags;
    }

    bool IsLast() const { return GetSize() == 0; }
    bool IsFree() const { return static_cast<bool>(Size & scBlockFreeBit); }
    void SetFree() { Size |= scBlockFreeBit; }
    void SetUsed() { Size &= ~scBlockFreeBit; }
    bool IsPrevFree() const { return static_cast<bool>(Size & scBlockPrevFreeBit); }
    void SetPrevFree() { Size |= scBlockPrevFreeBit; }
    void SetPrevUsed() { Size &= ~scBlockPrevFreeBit; }

    MemBlock* GetPrev()
    {
        tlsf_assert(IsPrevFree() && "previous block must be free");
        return pPrevPhysBlock;
    }

    /* Return location of next existing block. */
    MemBlock* GetNext() const;

    /* Link a new block with its physical neighbor, return the neighbor. */
    MemBlock* LinkToNextBlock()
    {
        MemBlock* next = GetNext();
        next->pPrevPhysBlock = this;
        return next;
    }

    void MarkFree()
    {
        /* Link the block to the next block, first. */
        MemBlock* next = LinkToNextBlock();

        next->SetPrevFree();
        SetFree();
    }

    void MarkUsed()
    {
        MemBlock* next = GetNext();
        next->SetPrevUsed();
        SetUsed();
    }

    bool CanSplit(uint64 size) const { return GetSize() >= sizeof(MemBlock) + size; }

    /* Split a block into two, the second of which is free. */
    MemBlock* Split(uint64 size);

public:
    /* Points to the previous physical block. */
    MemBlock* pPrevPhysBlock;

    /* The size of this block, excluding the block header. */
    uint64 Size;

    /* Next and previous free blocks. */
    MemBlock* pNextFree;
    MemBlock* pPrevFree;
};


/*
** TLSF utility functions. In most cases, these are direct translations of
** the documentation found in the white paper.
*/

static void mapping_insert(size_t size, int* fli, int* sli)
{
    int fl, sl;
    if (size < SMALL_BLOCK_SIZE) {
        /* Store small blocks in first list. */
        fl = 0;
        sl = static_cast<int32>(size) / (SMALL_BLOCK_SIZE / SL_INDEX_COUNT);
    }
    else {
        uint32 x = FxBit::FindLastOne64(size);
        fl = static_cast<int32>(x);
        if (x == FxBit::scBitNotFound) {
            fl = -1;
        }

        sl = static_cast<int32>(size >> (fl - SL_INDEX_COUNT_LOG2)) ^ (1 << SL_INDEX_COUNT_LOG2);
        fl -= (FL_INDEX_SHIFT - 1);
    }
    *fli = fl;
    *sli = sl;
}

/* This version rounds up to the next block size (for allocations) */
static void mapping_search(size_t size, int* fli, int* sli)
{
    if (size >= SMALL_BLOCK_SIZE) {
        uint32 bit = FxBit::FindLastOne64(size);
        int zbit = bit;
        if (bit == FxBit::scBitNotFound) {
            zbit = -1;
        }

        const size_t round = (1 << (zbit - SL_INDEX_COUNT_LOG2)) - 1;
        size += round;
    }
    mapping_insert(size, fli, sli);
}

struct alignas(16) ControlBlock
{
public:
    /* Remove a given block from the free list. */
    void RemoveBlockFromFreeList(MemBlock* block)
    {
        int fl, sl;
        mapping_insert(block->GetSize(), &fl, &sl);
        RemoveFreeBlock(block, fl, sl);
    }

    /* Insert a given block into the free list. */
    void AddBlockToFreeList(MemBlock* block)
    {
        int fl, sl;
        mapping_insert(block->GetSize(), &fl, &sl);
        InsertFreeBlock(block, fl, sl);
    }

    /* Remove a free block from the free list.*/
    void RemoveFreeBlock(MemBlock* block, int fl, int sl)
    {
        MemBlock* prev = block->pPrevFree;
        MemBlock* next = block->pNextFree;
        tlsf_assert(prev && "prev_free field can not be null");
        tlsf_assert(next && "next_free field can not be null");
        next->pPrevFree = prev;
        prev->pNextFree = next;

        /* If this block is the head of the free list, set new head. */
        if (ppBlocks[fl][sl] == block) {
            ppBlocks[fl][sl] = next;

            /* If the new head is null, clear the bitmap. */
            if (next == &NullBlock) {
                SlBitmap[fl] &= ~(1U << sl);

                /* If the second bitmap is now empty, clear the fl bitmap. */
                if (!SlBitmap[fl]) {
                    FlBitmap &= ~(1U << fl);
                }
            }
        }
    }

    /* Insert a free block into the free block list. */
    void InsertFreeBlock(MemBlock* block, int fl, int sl)
    {
        MemBlock* current = ppBlocks[fl][sl];
        FxAssertMsg(current != nullptr, "Free list cannot have a null entry");
        FxAssertMsg(block != nullptr, "Cannot insert a null entry into the free list");
        block->pNextFree = current;
        block->pPrevFree = &NullBlock;
        current->pPrevFree = block;

        void* btp = BlockToPtr(block);
        void* align_btp = FxMath::AlignPtr<void*, scAlignmentSize>(btp);

        tlsf_assert(btp == align_btp && "block not aligned properly");
        /*
        ** Insert the new block at the head of the list, and mark the first-
        ** and second-level bitmaps appropriately.
        */
        ppBlocks[fl][sl] = block;
        FlBitmap |= (1U << fl);
        SlBitmap[fl] |= (1U << sl);
    }

public:
    /* Empty lists point at this block to indicate they are free. */
    MemBlock NullBlock;

    /* Bitmaps for free lists. */
    unsigned int FlBitmap;
    unsigned int SlBitmap[FL_INDEX_COUNT];

    /* Head of free lists. */
    MemBlock* ppBlocks[FL_INDEX_COUNT][SL_INDEX_COUNT];
};
/*
** The size of the block header exposed to used blocks is the size field.
** The prev_phys_block field is stored *inside* the previous free block.
*/
static constexpr size_t scBlockHeaderSize = sizeof(uint64);

/* User data starts directly after the size field in a used block. */
static constexpr size_t scBlockStartOffset = offsetof(MemBlock, Size) + sizeof(size_t);

/*
** A free block must be large enough to store its header minus the size of
** the prev_phys_block field, and no larger than the number of addressable
** bits for FL_INDEX.
*/
static const size_t block_size_min = sizeof(MemBlock) - sizeof(MemBlock*);
static const size_t block_size_max = 1llu << FL_INDEX_MAX;


/* A type used for casting when doing pointer arithmetic. */
typedef ptrdiff_t tlsfptr_t;

/*
** block_header_t member functions.
*/

FX_FORCE_INLINE static MemBlock* BlockFromPtr(void* ptr)
{
    return reinterpret_cast<MemBlock*>(reinterpret_cast<uint8*>(ptr) - scBlockStartOffset);
}

FX_FORCE_INLINE static const MemBlock* BlockFromPtr(const void* ptr)
{
    return reinterpret_cast<const MemBlock*>(reinterpret_cast<const uint8*>(ptr) - scBlockStartOffset);
}

FX_FORCE_INLINE static void* BlockToPtr(MemBlock* block)
{
    return reinterpret_cast<void*>(reinterpret_cast<uint8*>(block) + scBlockStartOffset);
}

FX_FORCE_INLINE static const void* BlockToPtr(const MemBlock* block)
{
    return reinterpret_cast<const void*>(reinterpret_cast<const uint8*>(block) + scBlockStartOffset);
}

/* Return location of next block after block of given size. */
static MemBlock* GetBlock(const void* ptr, size_t size)
{
    return reinterpret_cast<MemBlock*>(reinterpret_cast<uintptr_t>(ptr) + size);
}


static size_t align_up(size_t x, size_t align)
{
    tlsf_assert(0 == (align & (align - 1)) && "must align to a power of two");
    return (x + (align - 1)) & ~(align - 1);
}

static size_t align_down(size_t x, size_t align)
{
    tlsf_assert(0 == (align & (align - 1)) && "must align to a power of two");
    return x - (x & (align - 1));
}

static void* align_ptr(const void* ptr, size_t align)
{
    const uintptr_t aligned = (reinterpret_cast<uintptr_t>(ptr) + (align - 1)) & ~(align - 1);
    tlsf_assert(0 == (align & (align - 1)) && "must align to a power of two");
    return reinterpret_cast<void*>(aligned);
}

MemBlock* MemBlock::GetNext() const
{
    MemBlock* next = GetBlock(BlockToPtr(this), GetSize() - scBlockHeaderSize);
    tlsf_assert(!IsLast());
    return next;
}

/*
** Adjust an allocation size to be aligned to word size, and no smaller
** than internal minimum.
*/
static size_t adjust_request_size(size_t size, size_t align)
{
    size_t adjust = 0;
    if (size) {
        const size_t aligned = FxMath::AlignValue(size, align);

        /* aligned sized must not exceed block_size_max or we'll go out of bounds on sl_bitmap */
        if (aligned < block_size_max) {
            adjust = std::max(aligned, block_size_min);
        }
    }
    return adjust;
}


static MemBlock* search_suitable_block(ControlBlock* control, int* fli, int* sli)
{
    int fl = *fli;
    int sl = *sli;

    /*
    ** First, search for a block in the list associated with the given
    ** fl/sl index.
    */
    uint32 sl_map = control->SlBitmap[fl] & (~0U << sl);

    if (!sl_map) {
        /* No block exists. Search in the next largest first-level list. */
        const uint32 fl_map = control->FlBitmap & (~0U << (fl + 1));

        if (!fl_map) {
            /* No free blocks available, memory has been exhausted. */
            FxLogError("No free blocks available");
            return nullptr;
        }

        uint32 x = FxBit::FindFirstOne32(fl_map);
        fl = static_cast<uint32>(x);
        if (x == FxBit::scBitNotFound) {
            fl = -1;
        }

        *fli = fl;
        sl_map = control->SlBitmap[fl];
    }

    tlsf_assert(sl_map && "internal error - second level bitmap is null");
    uint32 x = FxBit::FindFirstOne32(sl_map);
    sl = static_cast<int>(x);
    if (x == FxBit::scBitNotFound) {
        sl = -1;
    }

    *sli = sl;

    /* Return the first block in the free list. */
    return control->ppBlocks[fl][sl];
}


MemBlock* MemBlock::Split(uint64 size)
{
    /* Calculate the amount of space left in the remaining block. */
    MemBlock* remaining = GetBlock(BlockToPtr(this), size - scBlockHeaderSize);

    const size_t remain_size = GetSize() - (size + scBlockHeaderSize);


    tlsf_assert((BlockToPtr(remaining) == FxMath::AlignPtr<void*, scAlignmentSize>(BlockToPtr(remaining))) &&
                "remaining block not aligned properly");

    tlsf_assert(GetSize() == remain_size + size + scBlockHeaderSize);
    remaining->SetSize(remain_size);
    tlsf_assert(remaining->GetSize() >= block_size_min && "block split with invalid size");

    SetSize(size);
    remaining->MarkFree();

    return remaining;
}


/* Absorb a free block's storage into an adjacent previous free block. */
static MemBlock* block_absorb(MemBlock* prev, MemBlock* block)
{
    tlsf_assert(!prev->IsLast() && "previous block can't be last");
    /* Note: Leaves flags untouched. */
    prev->Size += block->GetSize() + scBlockHeaderSize;
    prev->LinkToNextBlock();
    return prev;
}

/* Merge a just-freed block with an adjacent previous free block. */
static MemBlock* block_merge_prev(ControlBlock* control, MemBlock* block)
{
    if (block->IsPrevFree()) {
        MemBlock* prev = block->GetPrev();
        tlsf_assert(prev && "prev physical block can't be null");
        tlsf_assert(prev->IsFree() && "prev block is not free though marked as such");
        control->RemoveBlockFromFreeList(prev);
        block = block_absorb(prev, block);
    }

    return block;
}

/* Merge a just-freed block with an adjacent free block. */
static MemBlock* block_merge_next(ControlBlock* control, MemBlock* block)
{
    MemBlock* next = block->GetNext();
    tlsf_assert(next && "next physical block can't be null");

    if (next->IsFree()) {
        tlsf_assert(!(block->IsLast()) && "previous block can't be last");

        control->RemoveBlockFromFreeList(next);
        block = block_absorb(block, next);
    }

    return block;
}

/* Trim any trailing block space off the end of a block, return to pool. */
static void block_trim_free(ControlBlock* control, MemBlock* block, size_t size)
{
    tlsf_assert((block->IsFree()) && "block must be free");
    if (block->CanSplit(size)) {
        MemBlock* remaining_block = block->Split(size);
        block->LinkToNextBlock();

        remaining_block->SetPrevFree();
        control->AddBlockToFreeList(remaining_block);
    }
}

/* Trim any trailing block space off the end of a used block, return to pool. */
static void block_trim_used(ControlBlock* control, MemBlock* block, size_t size)
{
    tlsf_assert(!(block->IsFree()) && "block must be used");
    if (block->CanSplit(size)) {
        /* If the next block is free, we must coalesce. */
        MemBlock* remaining_block = block->Split(size);
        remaining_block->SetPrevUsed();

        remaining_block = block_merge_next(control, remaining_block);
        control->AddBlockToFreeList(remaining_block);
    }
}

static MemBlock* block_trim_free_leading(ControlBlock* control, MemBlock* block, size_t size)
{
    MemBlock* remaining_block = block;
    if (block->CanSplit(size)) {
        /* We want the 2nd block. */
        remaining_block = block->Split(size - scBlockHeaderSize);
        remaining_block->SetPrevFree();

        block->LinkToNextBlock();
        control->AddBlockToFreeList(block);
    }

    return remaining_block;
}

static MemBlock* block_locate_free(ControlBlock* control, size_t size)
{
    int fl = 0, sl = 0;
    MemBlock* block = nullptr;

    if (size) {
        mapping_search(size, &fl, &sl);

        /*
        ** mapping_search can futz with the size, so for excessively large sizes it can sometimes wind up
        ** with indices that are off the end of the block array.
        ** So, we protect against that here, since this is the only callsite of mapping_search.
        ** Note that we don't need to check sl, since it comes from a modulo operation that guarantees it's always in
        *range.
        */
        if (fl < FL_INDEX_COUNT) {
            block = search_suitable_block(control, &fl, &sl);
        }
    }

    if (block) {
        tlsf_assert((block->GetSize()) >= size);
        control->RemoveFreeBlock(block, fl, sl);
    }

    return block;
}

static void* block_prepare_used(ControlBlock* control, MemBlock* block, size_t size)
{
    void* p = 0;
    if (block) {
        tlsf_assert(size && "size must be non-zero");
        block_trim_free(control, block, size);
        block->MarkUsed();
        p = BlockToPtr(block);
    }
    return p;
}

/* Clear structure and point all empty lists at the null block. */
static void MakeControlBlock(ControlBlock* control)
{
    control->NullBlock.pNextFree = &control->NullBlock;
    control->NullBlock.pPrevFree = &control->NullBlock;

    control->FlBitmap = 0;
    for (int i = 0; i < FL_INDEX_COUNT; ++i) {
        control->SlBitmap[i] = 0;
        for (int j = 0; j < SL_INDEX_COUNT; ++j) {
            control->ppBlocks[i][j] = &control->NullBlock;
        }
    }
}

/*
** Debugging utilities.
*/

typedef struct integrity_t
{
    int prev_status;
    int status;
} integrity_t;

#define tlsf_insist(x)                                                                                                 \
    {                                                                                                                  \
        tlsf_assert(x);                                                                                                \
        if (!(x)) {                                                                                                    \
            status--;                                                                                                  \
        }                                                                                                              \
    }

static void integrity_walker(void* ptr, size_t size, int used, void* user)
{
    MemBlock* block = BlockFromPtr(ptr);
    integrity_t* integ = reinterpret_cast<integrity_t*>(user);
    const bool this_prev_status = block->IsPrevFree();
    const bool this_status = block->IsFree();

    const size_t this_block_size = block->GetSize();

    int status = 0;
    (void)used;
    tlsf_insist((integ->prev_status == this_prev_status) && "prev status incorrect");
    tlsf_insist((size == this_block_size) && "block size incorrect");

    integ->prev_status = this_status;
    integ->status += status;
}

Status FxMemPool::CheckIntegrity()
{
    int i, j;

    ControlBlock* control = GetControlBlock();
    Status status = scPoolOk;

    /* Check that the free lists and bitmaps are accurate. */
    for (i = 0; i < FL_INDEX_COUNT; ++i) {
        for (j = 0; j < SL_INDEX_COUNT; ++j) {
            const int fl_map = control->FlBitmap & (1U << i);
            const int sl_list = control->SlBitmap[i];
            const int sl_map = sl_list & (1U << j);
            const MemBlock* block = control->ppBlocks[i][j];

            /* Check that first- and second-level lists agree. */
            if (!fl_map) {
                tlsf_insist(!sl_map && "second-level map must be null");
            }

            if (!sl_map) {
                tlsf_insist(block == &control->NullBlock && "block list must be null");
                continue;
            }

            /* Check that there is at least one free block. */
            tlsf_insist(sl_list && "no free blocks in second-level map");
            tlsf_insist(block != &control->NullBlock && "block should not be null");

            while (block != &control->NullBlock) {
                int fli, sli;
                tlsf_insist(block->IsFree() && "block should be free");
                tlsf_insist(!block->IsPrevFree() && "blocks should have coalesced");
                tlsf_insist(!block->GetNext()->IsFree() && "blocks should have coalesced");
                tlsf_insist(block->GetNext()->IsPrevFree() && "block should be free");
                tlsf_insist(block->GetSize() >= block_size_min && "block not minimum size");

                mapping_insert(block->GetSize(), &fli, &sli);
                tlsf_insist(fli == i && sli == j && "block size indexed in wrong list");
                block = block->pNextFree;
            }
        }
    }

    return status;
}

#undef tlsf_insist

static void default_walker(void* ptr, size_t size, int used, void* user)
{
    (void)user;
    printf("\t%p %s size: %x (%p)\n", ptr, used ? "used" : "free", (unsigned int)size, BlockFromPtr(ptr));
}

void FxMemPool::WalkPool(pool_t pool, WalkerFunc walker, void* user)
{
    WalkerFunc pool_walker = walker ? walker : default_walker;
    MemBlock* block = GetBlock(pool, -(int)scBlockHeaderSize);

    while (block && !block->IsLast()) {
        pool_walker(BlockToPtr(block), block->GetSize(), !block->IsFree(), user);
        block = block->GetNext();
    }
}

int FxMemPool::CheckPool(pool_t pool)
{
    /* Check that the blocks are physically correct. */
    integrity_t integ = { 0, 0 };
    WalkPool(pool, integrity_walker, &integ);

    return integ.status;
}


size_t tlsf_scAlignment(void) { return scAlignmentSize; }

size_t tlsf_block_size_min(void) { return block_size_min; }

size_t tlsf_block_size_max(void) { return block_size_max; }

/*
** Overhead of the TLSF structures in a given memory block passed to
** tlsf_add_pool, equal to the overhead of a free block and the
** sentinel block.
*/
static constexpr size_t scPoolOverhead = 4 * scBlockHeaderSize;
static constexpr size_t scAllocOverhead = scBlockHeaderSize;

pool_t FxMemPool::AddPool(void* mem, size_t bytes)
{
    MemBlock* block;
    MemBlock* next;


    const size_t pool_bytes = align_down(bytes - scPoolOverhead, scAlignmentSize);

    if (((ptrdiff_t)mem % scAlignmentSize) != 0) {
        printf("tlsf_add_pool: Memory must be aligned by %u bytes.\n", (unsigned int)scAlignmentSize);
        return 0;
    }

    FxLogInfo("Pool bytes: {}, BSM: {}, BSMX: {}", pool_bytes, block_size_min, block_size_max);

    if (pool_bytes < block_size_min || pool_bytes > block_size_max) {
        printf("tlsf_add_pool: Memory size must be between 0x%x and 0x%x00 bytes.\n",
               (unsigned int)(scPoolOverhead + block_size_min),
               (unsigned int)((scPoolOverhead + block_size_max) / 256));

        return 0;
    }

    /*
    ** Create the main free block. Offset the start of the block slightly
    ** so that the prev_phys_block field falls outside of the pool -
    ** it will never be used.
    */
    block = GetBlock(mem, -scBlockHeaderSize);

    block->SetSize(pool_bytes);
    block->SetFree();
    block->SetPrevUsed();

    GetControlBlock()->AddBlockToFreeList(block);

    /* Split the block to create a zero-size sentinel block. */
    next = block->LinkToNextBlock();
    next->SetSize(0);
    next->SetUsed();

    next->SetPrevFree();

    return mem;
}

void FxMemPool::RemovePool(pool_t pool)
{
    ControlBlock* control = GetControlBlock();
    MemBlock* block = GetBlock(pool, -(int)scBlockHeaderSize);

    int fl = 0, sl = 0;

    FxAssert((block->IsFree()));
    FxAssert(!block->GetNext()->IsFree());
    FxAssert(block->GetNext()->GetSize() == 0);

    mapping_insert((block->GetSize()), &fl, &sl);
    control->RemoveFreeBlock(block, fl, sl);
}


/*
** TLSF main interface.
*/

#if DEBUG

int tlsf_ffs(uint32 value)
{
    uint32 v = FxBit::FindFirstOne32(value);
    if (v == FxBit::scBitNotFound) {
        return -1;
    }
    return static_cast<int>(v);
}

int tlsf_fls(uint32 value)
{
    uint32 v = FxBit::FindLastOne32(value);
    if (v == FxBit::scBitNotFound) {
        return -1;
    }
    return static_cast<int>(v);
}


int tlsf_fls_sizet(uint64 value)
{
    uint32 v = FxBit::FindLastOne64(value);
    if (v == FxBit::scBitNotFound) {
        return -1;
    }
    return static_cast<int>(v);
}

int test_ffs_fls()
{
    /* Verify ffs/fls work properly. */
    int rv = 0;
    rv += (tlsf_ffs(0) == -1) ? 0 : 0x1;
    rv += (tlsf_fls(0) == -1) ? 0 : 0x2;
    rv += (tlsf_ffs(1) == 0) ? 0 : 0x4;
    rv += (tlsf_fls(1) == 0) ? 0 : 0x8;
    rv += (tlsf_ffs(0x80000000) == 31) ? 0 : 0x10;
    rv += (tlsf_ffs(0x80008000) == 15) ? 0 : 0x20;
    rv += (tlsf_fls(0x80000008) == 31) ? 0 : 0x40;
    rv += (tlsf_fls(0x7FFFFFFF) == 30) ? 0 : 0x80;

    rv += (tlsf_fls_sizet(0x80000000) == 31) ? 0 : 0x100;
    rv += (tlsf_fls_sizet(0x100000000) == 32) ? 0 : 0x200;
    rv += (tlsf_fls_sizet(0xffffffffffffffff) == 63) ? 0 : 0x400;

    if (rv) {
        printf("test_ffs_fls: %x ffs/fls tests failed.\n", rv);
    }
    return rv;
}
#endif

void FxMemPool::Create(uint64 size)
{
    void* ptr = std::malloc(size);
    FxAssertMsg(ptr != nullptr, "Could not allocate memory pool!");

    CreateFromPtr(ptr);
    AddPool(reinterpret_cast<void*>(reinterpret_cast<uint8*>(ptr) + sizeof(ControlBlock)), size - sizeof(ControlBlock));

    pMemory = ptr;
}

tlsf_t FxMemPool::CreateFromPtr(void* allocated_buffer)
{
#ifdef DEBUG
    if (test_ffs_fls()) {
        return 0;
    }
#endif

    if ((reinterpret_cast<ptrdiff_t>(allocated_buffer) % scAlignmentSize) != 0) {
        printf("tlsf_create: Memory must be aligned to %u bytes.\n", (unsigned int)scAlignmentSize);
        return 0;
    }

    pMemory = reinterpret_cast<tlsf_t>(allocated_buffer);
    MakeControlBlock(GetControlBlock());

    return pMemory;
}

tlsf_t FxMemPool::CreateWithPool(void* mem, size_t bytes)
{
    CreateFromPtr(mem);
    AddPool((char*)mem + sizeof(ControlBlock), bytes - sizeof(ControlBlock));
    return pMemory;
}

ControlBlock* FxMemPool::GetControlBlock() { return reinterpret_cast<ControlBlock*>(pMemory); }


pool_t FxMemPool::GetPool()
{
    return reinterpret_cast<pool_t>(reinterpret_cast<uint8*>(pMemory) + sizeof(ControlBlock));
}

void* FxMemPool::AllocRaw(size_t size)
{
    std::lock_guard<std::mutex> guard(mMutex);

    ControlBlock* control = GetControlBlock();

    const size_t adjust = adjust_request_size(size, scAlignmentSize);

    MemBlock* block = block_locate_free(control, adjust);
    return block_prepare_used(control, block, adjust);
}

void* FxMemPool::AlignedAllocRaw(uint32 alignment, size_t size)
{
    std::lock_guard<std::mutex> guard(mMutex);

    ControlBlock* control = GetControlBlock();

    const size_t adjust = adjust_request_size(size, scAlignmentSize);

    /*
    ** We must allocate an additional minimum block size bytes so that if
    ** our free block will leave an alignment gap which is smaller, we can
    ** trim a leading free block and release it back to the pool. We must
    ** do this because the previous physical block is in use, therefore
    ** the prev_phys_block field is not valid, and we can't simply adjust
    ** the size of that block.
    */
    const size_t gap_minimum = sizeof(MemBlock);
    const size_t size_with_gap = adjust_request_size(adjust + alignment + gap_minimum, alignment);

    /*
    ** If alignment is less than or equals base alignment, we're done.
    ** If we requested 0 bytes, return null, as tlsf_malloc(0) does.
    */
    const size_t aligned_size = (adjust && alignment > scAlignmentSize) ? size_with_gap : adjust;

    MemBlock* block = block_locate_free(control, aligned_size);

    /* This can't be a static assert. */
    tlsf_assert(sizeof(MemBlock) == block_size_min + scBlockHeaderSize);

    if (block) {
        void* ptr = BlockToPtr(block);
        const void* aligned = FxMath::AlignPtr(ptr, alignment);
        size_t gap = static_cast<size_t>(reinterpret_cast<uintptr_t>(aligned) - reinterpret_cast<uintptr_t>(ptr));

        /* If gap size is too small, offset to next aligned boundary. */
        if (gap && gap < gap_minimum) {
            const uint32 gap_remain = gap_minimum - gap;
            const uint32 offset = std::max(gap_remain, alignment);
            const void* next_aligned = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(aligned) + offset);

            aligned = FxMath::AlignPtr(next_aligned, alignment);
            gap = static_cast<size_t>(reinterpret_cast<uintptr_t>(aligned) - reinterpret_cast<uintptr_t>(ptr));
        }

        if (gap) {
            tlsf_assert(gap >= gap_minimum && "gap size too small");
            block = block_trim_free_leading(control, block, gap);
        }
    }

    return block_prepare_used(control, block, adjust);
}

void FxMemPool::FreeRaw(void* ptr)
{
    if (!ptr) {
        return;
    }

    std::lock_guard<std::mutex> guard(mMutex);

    ControlBlock* control = GetControlBlock();

    MemBlock* block = BlockFromPtr(ptr);
    tlsf_assert(!(block->IsFree()) && "block already marked as free");
    block->MarkFree();
    block = block_merge_prev(control, block);
    block = block_merge_next(control, block);
    control->AddBlockToFreeList(block);
}

/*
** The TLSF block information provides us with enough information to
** provide a reasonably intelligent implementation of realloc, growing or
** shrinking the currently allocated block as required.
**
** This routine handles the somewhat esoteric edge cases of realloc:
** - a non-zero size with a null pointer will behave like malloc
** - a zero size with a non-null pointer will behave like free
** - a request that cannot be satisfied will leave the original buffer
**   untouched
** - an extended buffer size will leave the newly-allocated area with
**   contents undefined
*/
void* FxMemPool::ReallocRaw(void* ptr, size_t size)
{
    std::lock_guard<std::mutex> guard(mMutex);

    ControlBlock* control = GetControlBlock();
    void* p = 0;

    /* Zero-size requests are treated as free. */
    if (ptr && size == 0) {
        FreeRaw(ptr);
    }
    /* Requests with NULL pointers are treated as malloc. */
    else if (!ptr) {
        p = AllocRaw(size);
    }
    else {
        MemBlock* block = BlockFromPtr(ptr);
        MemBlock* next = block->GetNext();

        const size_t cursize = block->GetSize();
        const size_t combined = cursize + next->GetSize() + scBlockHeaderSize;
        const size_t adjust = adjust_request_size(size, scAlignmentSize);

        tlsf_assert(!(block->IsFree()) && "block already marked as free");

        /*
        ** If the next block is used, or when combined with the current
        ** block, does not offer enough space, we must reallocate and copy.
        */
        if (adjust > cursize && (!next->IsFree() || adjust > combined)) {
            p = AllocRaw(size);
            if (p) {
                const size_t minsize = std::min(cursize, size);
                memcpy(p, ptr, minsize);
                FreeRaw(ptr);
            }
        }
        else {
            /* Do we need to expand to the next block? */
            if (adjust > cursize) {
                block_merge_next(control, block);
                block->MarkUsed();
            }

            /* Trim the resulting block and return the original pointer. */
            block_trim_used(control, block, adjust);
            p = ptr;
        }
    }

    return p;
}


void FxMemPool::Destroy()
{
    if (!pMemory) {
        return;
    }

    std::free(pMemory);
    pMemory = nullptr;
}
