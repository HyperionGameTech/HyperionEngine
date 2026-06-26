/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Core/Profiling/Profile.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>
#include <Core/Memory/Allocator/ArenaAllocator.hpp>
#include <Core/Memory/Allocator/SlabAllocator.hpp>
#include <Core/Memory/Pool/Pool.hpp>

#include <Core/Threading/Thread.hpp>

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <cstdio>
#include <cstdlib>

namespace Hyperion {
namespace tests {
namespace profiling {

namespace {

constexpr size_t LargeCount = 10000;
constexpr size_t MaxBatchPtrs = 256;

volatile uint64 g_sink = 0;

void Consume(uint64 value)
{
    g_sink += value;
}

// ---- DynamicAllocator profiles ----

DynamicAllocator g_dyn;

static constexpr size_t allocSizes[] = { 16, 32, 64, 128, 256, 512, 1024, 2048, 4096 };
static constexpr size_t numAllocSizes = sizeof(allocSizes) / sizeof(allocSizes[0]);

static constexpr size_t overAlignSizes[] = { 64, 128, 256 };
static constexpr size_t overAlignments[] = { 64, 128, 4096 };
static constexpr size_t numOverAlign = sizeof(overAlignSizes) / sizeof(overAlignSizes[0]);

void ProfileDynamicAllocFreeSmall()
{
    for (size_t i = 0; i < LargeCount; ++i)
    {
        void* ptr = g_dyn.Allocate(32, alignof(uint64));
        Consume(reinterpret_cast<UIntPtr>(ptr));
        g_dyn.Free(ptr);
    }
}

void ProfileDynamicAllocFreeLarge()
{
    for (size_t i = 0; i < LargeCount; ++i)
    {
        void* ptr = g_dyn.Allocate(4096, alignof(uint64));
        Consume(reinterpret_cast<UIntPtr>(ptr));
        g_dyn.Free(ptr);
    }
}

void ProfileDynamicBatchAllocFreeSmall()
{
    void* ptrs[MaxBatchPtrs];

    for (size_t i = 0; i < LargeCount / MaxBatchPtrs; ++i)
    {
        for (size_t j = 0; j < MaxBatchPtrs; ++j)
            ptrs[j] = g_dyn.Allocate(32, alignof(uint64));

        Consume(reinterpret_cast<UIntPtr>(ptrs[0]));

        for (size_t j = 0; j < MaxBatchPtrs; ++j)
            g_dyn.Free(ptrs[j]);
    }
}

void ProfileDynamicAllocFreeMixed()
{
    for (size_t i = 0; i < LargeCount; ++i)
    {
        size_t size = allocSizes[i % numAllocSizes];
        void* ptr = g_dyn.Allocate(size, alignof(uint64));
        Consume(reinterpret_cast<UIntPtr>(ptr));
        g_dyn.Free(ptr);
    }
}

void ProfileDynamicAllocFreeOverAligned()
{
    for (size_t i = 0; i < LargeCount; ++i)
    {
        size_t size = overAlignSizes[i % numOverAlign];
        size_t align = overAlignments[i % numOverAlign];
        void* ptr = g_dyn.Allocate(size, align);
        Consume(reinterpret_cast<UIntPtr>(ptr));
        g_dyn.Free(ptr);
    }
}

// ---- ThreadAllocator profiles ----

ThreadAllocator g_thr;

void ProfileThreadAllocFreeSmall()
{
    for (size_t i = 0; i < LargeCount; ++i)
    {
        void* ptr = g_thr.Allocate(32, alignof(uint64));
        Consume(reinterpret_cast<UIntPtr>(ptr));
        g_thr.Free(ptr);
    }
}

void ProfileThreadAllocFreeLarge()
{
    for (size_t i = 0; i < LargeCount; ++i)
    {
        void* ptr = g_thr.Allocate(4096, alignof(uint64));
        Consume(reinterpret_cast<UIntPtr>(ptr));
        g_thr.Free(ptr);
    }
}

void ProfileThreadBatchAllocFreeSmall()
{
    void* ptrs[MaxBatchPtrs];

    for (size_t i = 0; i < LargeCount / MaxBatchPtrs; ++i)
    {
        for (size_t j = 0; j < MaxBatchPtrs; ++j)
            ptrs[j] = g_thr.Allocate(32, alignof(uint64));

        Consume(reinterpret_cast<UIntPtr>(ptrs[0]));

        for (size_t j = 0; j < MaxBatchPtrs; ++j)
            g_thr.Free(ptrs[j]);
    }
}

void ProfileThreadAllocFreeMixed()
{
    for (size_t i = 0; i < LargeCount; ++i)
    {
        size_t size = allocSizes[i % numAllocSizes];
        void* ptr = g_thr.Allocate(size, alignof(uint64));
        Consume(reinterpret_cast<UIntPtr>(ptr));
        g_thr.Free(ptr);
    }
}

void ProfileThreadAllocFreeOverAligned()
{
    for (size_t i = 0; i < LargeCount; ++i)
    {
        size_t size = overAlignSizes[i % numOverAlign];
        size_t align = overAlignments[i % numOverAlign];
        void* ptr = g_thr.Allocate(size, align);
        Consume(reinterpret_cast<UIntPtr>(ptr));
        g_thr.Free(ptr);
    }
}

// ---- Pool profiles ----

static constexpr size_t PoolBlockSize = 1ull << 20;

Pool* g_testPool = nullptr;

void InitTestPool()
{
    if (!g_testPool)
        g_testPool = new Pool(PoolBlockSize, PF_NONE, ThreadId::Current());
}

void ProfilePoolAllocFreeSmall()
{
    InitTestPool();

    for (size_t i = 0; i < LargeCount; ++i)
    {
        void* ptr = g_testPool->Allocate(32, alignof(uint64));
        Consume(reinterpret_cast<UIntPtr>(ptr));
        g_testPool->Free(ptr);
    }
}

void ProfilePoolAllocFreeLarge()
{
    InitTestPool();

    for (size_t i = 0; i < LargeCount; ++i)
    {
        void* ptr = g_testPool->Allocate(4096, alignof(uint64));
        Consume(reinterpret_cast<UIntPtr>(ptr));
        g_testPool->Free(ptr);
    }
}

void ProfilePoolBatchAllocFreeSmall()
{
    InitTestPool();

    void* ptrs[MaxBatchPtrs];

    for (size_t i = 0; i < LargeCount / MaxBatchPtrs; ++i)
    {
        for (size_t j = 0; j < MaxBatchPtrs; ++j)
            ptrs[j] = g_testPool->Allocate(32, alignof(uint64));

        Consume(reinterpret_cast<UIntPtr>(ptrs[0]));

        for (size_t j = 0; j < MaxBatchPtrs; ++j)
            g_testPool->Free(ptrs[j]);
    }
}

void ProfilePoolAllocFreeMixed()
{
    InitTestPool();

    for (size_t i = 0; i < LargeCount; ++i)
    {
        size_t size = allocSizes[i % numAllocSizes];
        void* ptr = g_testPool->Allocate(size, alignof(uint64));
        Consume(reinterpret_cast<UIntPtr>(ptr));
        g_testPool->Free(ptr);
    }
}

void ProfilePoolAllocFreeOverAligned()
{
    InitTestPool();

    for (size_t i = 0; i < LargeCount; ++i)
    {
        size_t size = overAlignSizes[i % numOverAlign];
        size_t align = overAlignments[i % numOverAlign];
        void* ptr = g_testPool->Allocate(size, align);
        Consume(reinterpret_cast<UIntPtr>(ptr));
        g_testPool->Free(ptr);
    }
}

// ---- Arena profiles ----

static constexpr size_t ArenaBlockSize = 1ull << 20;

Arena g_testArena(ArenaBlockSize);

void ProfileArenaAllocSmall()
{
    for (size_t i = 0; i < LargeCount; ++i)
    {
        void* ptr = g_testArena.Allocate(32, alignof(uint64));
        Consume(reinterpret_cast<UIntPtr>(ptr));
    }

    g_testArena.Reset();
}

void ProfileArenaAllocLarge()
{
    for (size_t i = 0; i < LargeCount; ++i)
    {
        void* ptr = g_testArena.Allocate(4096, alignof(uint64));
        Consume(reinterpret_cast<UIntPtr>(ptr));
    }

    g_testArena.Reset();
}

void ProfileArenaAllocMixed()
{
    for (size_t i = 0; i < LargeCount; ++i)
    {
        size_t size = allocSizes[i % numAllocSizes];
        void* ptr = g_testArena.Allocate(size, alignof(uint64));
        Consume(reinterpret_cast<UIntPtr>(ptr));
    }

    g_testArena.Reset();
}

// ---- SlabAllocator profiles ----

static constexpr size_t SlabBlockSize = 64;
static constexpr size_t SlabLargeBlockSize = 4096;
static constexpr uint32 SlabBlocksPerSlab = 256;

SlabAllocator g_slabSmall(SlabBlockSize, 16, SlabBlocksPerSlab, AF_NONE, ThreadId::Current());
SlabAllocator g_slabLarge(SlabLargeBlockSize, 16, SlabBlocksPerSlab, AF_NONE, ThreadId::Current());

void ProfileSlabAllocFreeSmall()
{
    for (size_t i = 0; i < LargeCount; ++i)
    {
        void* ptr = g_slabSmall.Allocate(SlabBlockSize, 16);
        Consume(reinterpret_cast<UIntPtr>(ptr));
        g_slabSmall.Free(ptr);
    }
}

void ProfileSlabBatchAllocFreeSmall()
{
    void* ptrs[MaxBatchPtrs];

    for (size_t i = 0; i < LargeCount / MaxBatchPtrs; ++i)
    {
        for (size_t j = 0; j < MaxBatchPtrs; ++j)
            ptrs[j] = g_slabSmall.Allocate(SlabBlockSize, 16);

        Consume(reinterpret_cast<UIntPtr>(ptrs[0]));

        for (size_t j = 0; j < MaxBatchPtrs; ++j)
            g_slabSmall.Free(ptrs[j]);
    }
}

void ProfileSlabAllocFreeLarge()
{
    for (size_t i = 0; i < LargeCount; ++i)
    {
        void* ptr = g_slabLarge.Allocate(SlabLargeBlockSize, 16);
        Consume(reinterpret_cast<UIntPtr>(ptr));
        g_slabLarge.Free(ptr);
    }
}

void ProfileSlabBatchAllocFreeLarge()
{
    void* ptrs[MaxBatchPtrs];

    for (size_t i = 0; i < LargeCount / MaxBatchPtrs; ++i)
    {
        for (size_t j = 0; j < MaxBatchPtrs; ++j)
            ptrs[j] = g_slabLarge.Allocate(SlabLargeBlockSize, 16);

        Consume(reinterpret_cast<UIntPtr>(ptrs[0]));

        for (size_t j = 0; j < MaxBatchPtrs; ++j)
            g_slabLarge.Free(ptrs[j]);
    }
}

struct SectionEntry
{
    const char *label;
    Profile::ProfileFunction function;
};

template <size_t Count>
void RunSection(const char *title, const SectionEntry (&entries)[Count], size_t runsPer, size_t numIterations, size_t runsPerIteration)
{
    Array<Profile> profiles;
    profiles.Reserve(Count);

    for (size_t i = 0; i < Count; ++i)
        profiles.EmplaceBack(entries[i].function);

    Array<Profile *> profilePtrs;
    profilePtrs.Reserve(Count);

    for (size_t i = 0; i < Count; ++i)
        profilePtrs.PushBack(&profiles[i]);

    Array<double> results = Profile::RunInterleved(std::move(profilePtrs), runsPer, numIterations, runsPerIteration);

    std::printf("%s\n", title);
    std::printf("--------\n");

    for (size_t i = 0; i < Count; ++i)
        std::printf("%-30s : %.6f s\n", entries[i].label, results[i]);

    std::printf("\n");
}

} // namespace

HYP_EXPORT void PrintAllocatorProfiling(size_t runsPer = 5, size_t numIterations = 50, size_t runsPerIteration = 10)
{
    const SectionEntry sequentialEntries[] = {
        { "Dynamic   32B alloc+free",     &ProfileDynamicAllocFreeSmall },
        { "Thread    32B alloc+free",     &ProfileThreadAllocFreeSmall },
        { "Pool      32B alloc+free",     &ProfilePoolAllocFreeSmall },
        { "Dynamic   4KB alloc+free",     &ProfileDynamicAllocFreeLarge },
        { "Thread    4KB alloc+free",     &ProfileThreadAllocFreeLarge },
        { "Pool      4KB alloc+free",     &ProfilePoolAllocFreeLarge },
        { "Dynamic   mixed alloc+free",   &ProfileDynamicAllocFreeMixed },
        { "Thread    mixed alloc+free",   &ProfileThreadAllocFreeMixed },
        { "Pool      mixed alloc+free",   &ProfilePoolAllocFreeMixed },
    };

    const SectionEntry batchEntries[] = {
        { "Dynamic 32B batch alloc+free",  &ProfileDynamicBatchAllocFreeSmall },
        { "Thread   32B batch alloc+free", &ProfileThreadBatchAllocFreeSmall },
        { "Pool     32B batch alloc+free", &ProfilePoolBatchAllocFreeSmall },
    };

    const SectionEntry overAlignedEntries[] = {
        { "Dynamic over-aligned", &ProfileDynamicAllocFreeOverAligned },
        { "Thread  over-aligned", &ProfileThreadAllocFreeOverAligned },
        { "Pool    over-aligned", &ProfilePoolAllocFreeOverAligned },
    };

    const SectionEntry slabEntries[] = {
        { "Slab 64B alloc+free",     &ProfileSlabAllocFreeSmall },
        { "Slab 4KB alloc+free",     &ProfileSlabAllocFreeLarge },
    };

    const SectionEntry slabBatchEntries[] = {
        { "Slab 64B batch alloc+free",  &ProfileSlabBatchAllocFreeSmall },
        { "Slab 4KB batch alloc+free",  &ProfileSlabBatchAllocFreeLarge },
    };

    const SectionEntry arenaEntries[] = {
        { "Arena 32B alloc",    &ProfileArenaAllocSmall },
        { "Arena 4KB alloc",    &ProfileArenaAllocLarge },
        { "Arena mixed alloc",  &ProfileArenaAllocMixed },
    };

    std::printf("=== Allocator profiling (N=%zu) ===\n\n", LargeCount);

    RunSection("Sequential alloc+free", sequentialEntries, runsPer, numIterations, runsPerIteration);
    RunSection("Batch alloc+free", batchEntries, runsPer, numIterations, runsPerIteration);
    RunSection("Over-aligned alloc+free", overAlignedEntries, runsPer, numIterations, runsPerIteration);
    RunSection("Slab alloc+free", slabEntries, runsPer, numIterations, runsPerIteration);
    RunSection("Slab batch alloc+free", slabBatchEntries, runsPer, numIterations, runsPerIteration);
    RunSection("Arena alloc (no free)", arenaEntries, runsPer, numIterations, runsPerIteration);
}

} // namespace profiling
} // namespace tests
} // namespace Hyperion

#endif
