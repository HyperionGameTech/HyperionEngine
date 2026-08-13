/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#ifdef HYP_TESTS

#include <Core/Profiling/Profile.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/SlimArray.hpp>
#include <Core/Containers/SparsePagedArray.hpp>
#include <Core/Containers/Map.hpp>
#include <Core/Containers/FlatMap.hpp>
#include <Core/Containers/Set.hpp>
#include <Core/Containers/FlatSet.hpp>

#include <Core/Memory/Pool/Pool.hpp>

#include <Core/Memory/Allocator/ArenaAllocator.hpp>

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Hyperion {
namespace tests {
namespace profiling {

#define USE_CUSTOM_ALLOCATOR

#ifdef USE_CUSTOM_ALLOCATOR

static Pool s_testPool { 1ull << 18 };
static Pool* s_pTestPool = &s_testPool;
using TestAllocator = AllocatorInstance<Pool, &s_pTestPool>;

//static Arena s_testArena { 1ull << 18 };
//static Arena* s_pTestArena = &s_testArena;
//using TestAllocator = AllocatorInstance<Arena, &s_pTestArena>;

#else

using TestAllocator = DynamicAllocator;

#endif

namespace {

constexpr size_t SmallElementCount = 128;
constexpr size_t SmallLookupCount = 64;
constexpr size_t LargeElementCount = 10000;
constexpr size_t LargeLookupCount = 1000;
constexpr size_t SparseStride = 3;

volatile uint64 g_sink = 0;

struct Dataset
{
    const char* label = "";
    size_t elementCount = 0;
    size_t lookupCount = 0;
    Array<uint32> keys;
    Array<uint32> lookupKeys;
    Array<size_t> sparseIndices;
    Array<size_t> lookupIndices;
    Array<uint32> removalKeys;
    Array<size_t> removalIndices;
};

enum class DatasetSize
{
    Small = 0,
    Large = 1
};

const Dataset *g_dataset = nullptr;

HYP_FORCE_INLINE uint32 NextRandom(uint32 &state)
{
    state = state * 1664525u + 1013904223u;

    return state;
}

void Consume(uint64 value)
{
    g_sink += value;
}

Dataset BuildDataset(const char* label, size_t elementCount, size_t lookupCount, uint32 seed, uint32 lookupSeed)
{
    Dataset dataset;
    dataset.label = label;
    dataset.elementCount = elementCount;
    dataset.lookupCount = lookupCount;

    dataset.keys.ResizeUninitialized(elementCount);
    dataset.lookupKeys.ResizeUninitialized(lookupCount);
    dataset.sparseIndices.ResizeUninitialized(elementCount);
    dataset.lookupIndices.ResizeUninitialized(lookupCount);
    dataset.removalKeys.ResizeUninitialized(elementCount / 2);
    dataset.removalIndices.ResizeUninitialized(elementCount / 2);

    uint32 state = seed;
    for (size_t i = 0; i < elementCount; ++i)
    {
        dataset.keys[i] = NextRandom(state);
        dataset.sparseIndices[i] = i * SparseStride + (i & 3);
    }

    state = lookupSeed;
    for (size_t i = 0; i < lookupCount; ++i)
    {
        dataset.lookupKeys[i] = NextRandom(state);
        dataset.lookupIndices[i] = i * SparseStride + (i & 3);
    }

    for (size_t i = 0; i < dataset.removalKeys.Size(); ++i)
    {
        dataset.removalKeys[i] = dataset.keys[i];
        dataset.removalIndices[i] = dataset.sparseIndices[i];
    }

    return dataset;
}

Dataset& GetDataset(DatasetSize size)
{
    static Dataset datasets[2];
    static bool initialized[2] = { false, false };

    const int index = int(size);

    if (!initialized[index])
    {
        if (size == DatasetSize::Small)
        {
            datasets[index] = BuildDataset("Small", SmallElementCount, SmallLookupCount, 0x123456u, 0x9e3779b9u);
        }
        else
        {
            datasets[index] = BuildDataset("Large", LargeElementCount, LargeLookupCount, 0xdeadbeefu, 0x1234abcd);
        }

        initialized[index] = true;
    }

    return datasets[index];
}

const Dataset& ActiveDataset()
{
    return *g_dataset;
}

void ProfileArrayInsertion(bool setupOnly)
{
    if (setupOnly) return;

    const Dataset& data = ActiveDataset();

    Array<uint32, TestAllocator> array;

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        array.PushBack(data.keys[i]);
    }

    Consume(array.Size());
}

void ProfileArrayIteration(bool setupOnly)
{
    static Array<uint32, TestAllocator> s_array;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        Array<uint32, TestAllocator> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.PushBack(data.keys[i]);
        }

        s_array = std::move(temp);

        return;
    }

    uint64 sum = 0;
    for (size_t i = 0; i < s_array.Size(); ++i)
    {
        sum += s_array[i];
    }

    Consume(sum);
}

void ProfileArrayFind(bool setupOnly)
{
    static Array<uint32, TestAllocator> s_array;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        Array<uint32, TestAllocator> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.PushBack(data.keys[i]);
        }

        s_array = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (s_array.Find(data.lookupKeys[i]) != s_array.End())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileArrayRemoval(bool setupOnly)
{
    static Array<uint32, TestAllocator> s_source;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        Array<uint32, TestAllocator> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.PushBack(data.keys[i]);
        }

        s_source = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    Array<uint32, TestAllocator> array = s_source;

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        array.Erase(data.removalKeys[i]);
    }

    Consume(array.Size());
}

void ProfileSlimArrayInsertion(bool setupOnly)
{
    if (setupOnly) return;

    const Dataset& data = ActiveDataset();

    SlimArray<uint32, TestAllocator> array;

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        array.PushBack(data.keys[i]);
    }

    Consume(array.Size());
}

void ProfileSlimArrayIteration(bool setupOnly)
{
    static SlimArray<uint32, TestAllocator> s_array;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        SlimArray<uint32, TestAllocator> temp;
        temp.Reserve(uint32(data.elementCount));

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.PushBack(data.keys[i]);
        }

        s_array = std::move(temp);

        return;
    }

    uint64 sum = 0;
    for (size_t i = 0; i < s_array.Size(); ++i)
    {
        sum += s_array[i];
    }

    Consume(sum);
}

void ProfileSlimArrayFind(bool setupOnly)
{
    static SlimArray<uint32, TestAllocator> s_array;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        SlimArray<uint32, TestAllocator> temp;
        temp.Reserve(uint32(data.elementCount));

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.PushBack(data.keys[i]);
        }

        s_array = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (s_array.Find(data.lookupKeys[i]) != s_array.End())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileSlimArrayRemoval(bool setupOnly)
{
    static SlimArray<uint32, TestAllocator> s_source;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        SlimArray<uint32, TestAllocator> temp;
        temp.Reserve(uint32(data.elementCount));

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.PushBack(data.keys[i]);
        }

        s_source = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    SlimArray<uint32, TestAllocator> array = s_source;

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        array.Erase(data.removalKeys[i]);
    }

    Consume(array.Size());
}

void ProfileSparsePagedArrayInsertion(bool setupOnly)
{
    if (setupOnly) return;

    const Dataset& data = ActiveDataset();

    SparsePagedArray<uint32, 64, TestAllocator> array;

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        array.Set(data.sparseIndices[i], data.keys[i]);
    }

    Consume(array.Count());
}

void ProfileSparsePagedArrayIteration(bool setupOnly)
{
    static SparsePagedArray<uint32, 64, TestAllocator> s_array;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        SparsePagedArray<uint32, 64, TestAllocator> temp;

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Set(data.sparseIndices[i], data.keys[i]);
        }

        s_array = std::move(temp);

        return;
    }

    uint64 sum = 0;
    for (const auto &value : s_array)
    {
        sum += value;
    }

    Consume(sum);
}

void ProfileSparsePagedArrayFind(bool setupOnly)
{
    static SparsePagedArray<uint32, 64, TestAllocator> s_array;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        SparsePagedArray<uint32, 64, TestAllocator> temp;

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Set(data.sparseIndices[i], data.keys[i]);
        }

        s_array = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (s_array.TryGet(data.lookupIndices[i]) != nullptr)
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileSparsePagedArrayRemoval(bool setupOnly)
{
    static SparsePagedArray<uint32, 64, TestAllocator> s_source;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        SparsePagedArray<uint32, 64, TestAllocator> temp;

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Set(data.sparseIndices[i], data.keys[i]);
        }

        s_source = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    SparsePagedArray<uint32, 64, TestAllocator> array = s_source;

    for (size_t i = 0; i < data.removalIndices.Size(); ++i)
    {
        array.EraseAt(data.removalIndices[i]);
    }

    Consume(array.Count());
}

void ProfileHashMapInsertion(bool setupOnly)
{
    if (setupOnly) return;

    const Dataset& data = ActiveDataset();

    Map<uint32, uint32, TestAllocator> map;
    map.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.Set(data.keys[i], uint32(i));
    }

    Consume(map.Size());
}

void ProfileHashMapDynamicInsertion(bool setupOnly)
{
    if (setupOnly) return;

    const Dataset& data = ActiveDataset();

    Map<uint32, uint32, TestAllocator> map;
    map.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.Set(data.keys[i], uint32(i));
    }

    Consume(map.Size());
}

void ProfileHashMapIteration(bool setupOnly)
{
    static Map<uint32, uint32, TestAllocator> s_map;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        Map<uint32, uint32, TestAllocator> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Set(data.keys[i], uint32(i));
        }

        s_map = std::move(temp);

        return;
    }

    uint64 sum = 0;
    for (const auto &item : s_map)
    {
        sum += item.second;
    }

    Consume(sum);
}

void ProfileHashMapDynamicIteration(bool setupOnly)
{
    static Map<uint32, uint32, TestAllocator> s_map;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        Map<uint32, uint32, TestAllocator> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Set(data.keys[i], uint32(i));
        }

        s_map = std::move(temp);

        return;
    }

    uint64 sum = 0;
    for (const auto &item : s_map)
    {
        sum += item.second;
    }

    Consume(sum);
}

void ProfileHashMapFind(bool setupOnly)
{
    static Map<uint32, uint32, TestAllocator> s_map;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        Map<uint32, uint32, TestAllocator> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Set(data.keys[i], uint32(i));
        }

        s_map = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (s_map.Find(data.lookupKeys[i]) != s_map.End())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileHashMapDynamicFind(bool setupOnly)
{
    static Map<uint32, uint32, TestAllocator> s_map;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        Map<uint32, uint32, TestAllocator> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Set(data.keys[i], uint32(i));
        }

        s_map = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (s_map.Find(data.lookupKeys[i]) != s_map.End())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileHashMapRemoval(bool setupOnly)
{
    static Map<uint32, uint32, TestAllocator> s_source;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        Map<uint32, uint32, TestAllocator> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Set(data.keys[i], uint32(i));
        }

        s_source = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    Map<uint32, uint32, TestAllocator> map = s_source;

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        map.Erase(data.removalKeys[i]);
    }

    Consume(map.Size());
}

void ProfileHashMapDynamicRemoval(bool setupOnly)
{
    static Map<uint32, uint32, TestAllocator> s_source;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        Map<uint32, uint32, TestAllocator> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Set(data.keys[i], uint32(i));
        }

        s_source = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    Map<uint32, uint32, TestAllocator> map = s_source;

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        map.Erase(data.removalKeys[i]);
    }

    Consume(map.Size());
}

void ProfileFlatMapInsertion(bool setupOnly)
{
    if (setupOnly) return;

    const Dataset& data = ActiveDataset();

    FlatMap<uint32, uint32> map;
    map.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.Set(data.keys[i], uint32(i));
    }

    Consume(map.Size());
}

void ProfileFlatMapIteration(bool setupOnly)
{
    static FlatMap<uint32, uint32> s_map;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        FlatMap<uint32, uint32> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Set(data.keys[i], uint32(i));
        }

        s_map = std::move(temp);

        return;
    }

    uint64 sum = 0;
    for (const auto &item : s_map)
    {
        sum += item.second;
    }

    Consume(sum);
}

void ProfileFlatMapFind(bool setupOnly)
{
    static FlatMap<uint32, uint32> s_map;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        FlatMap<uint32, uint32> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Set(data.keys[i], uint32(i));
        }

        s_map = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (s_map.Find(data.lookupKeys[i]) != s_map.End())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileFlatMapRemoval(bool setupOnly)
{
    static FlatMap<uint32, uint32> s_source;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        FlatMap<uint32, uint32> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Set(data.keys[i], uint32(i));
        }

        s_source = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    FlatMap<uint32, uint32> map = s_source;

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        map.Erase(data.removalKeys[i]);
    }

    Consume(map.Size());
}

void ProfileHashSetInsertion(bool setupOnly)
{
    if (setupOnly) return;

    const Dataset& data = ActiveDataset();

    Set<uint32, TestAllocator> set;
    set.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.Insert(data.keys[i]);
    }

    Consume(set.Size());
}

void ProfileHashSetDynamicInsertion(bool setupOnly)
{
    if (setupOnly) return;

    const Dataset& data = ActiveDataset();

    Set<uint32, TestAllocator> set;
    set.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.Insert(data.keys[i]);
    }

    Consume(set.Size());
}

void ProfileHashSetIteration(bool setupOnly)
{
    static Set<uint32, TestAllocator> s_set;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        Set<uint32, TestAllocator> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Insert(data.keys[i]);
        }

        s_set = std::move(temp);

        return;
    }

    uint64 sum = 0;
    for (const auto &item : s_set)
    {
        sum += item;
    }

    Consume(sum);
}

void ProfileHashSetDynamicIteration(bool setupOnly)
{
    static Set<uint32, TestAllocator> s_set;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        Set<uint32, TestAllocator> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Insert(data.keys[i]);
        }

        s_set = std::move(temp);

        return;
    }

    uint64 sum = 0;
    for (const auto &item : s_set)
    {
        sum += item;
    }

    Consume(sum);
}

void ProfileHashSetFind(bool setupOnly)
{
    static Set<uint32, TestAllocator> s_set;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        Set<uint32, TestAllocator> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Insert(data.keys[i]);
        }

        s_set = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (s_set.Find(data.lookupKeys[i]) != s_set.End())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileHashSetDynamicFind(bool setupOnly)
{
    static Set<uint32, TestAllocator> s_set;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        Set<uint32, TestAllocator> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Insert(data.keys[i]);
        }

        s_set = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (s_set.Find(data.lookupKeys[i]) != s_set.End())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileHashSetRemoval(bool setupOnly)
{
    static Set<uint32, TestAllocator> s_source;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        Set<uint32, TestAllocator> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Insert(data.keys[i]);
        }

        s_source = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    Set<uint32, TestAllocator> set = s_source;

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        set.Erase(data.removalKeys[i]);
    }

    Consume(set.Size());
}

void ProfileHashSetDynamicRemoval(bool setupOnly)
{
    static Set<uint32, TestAllocator> s_source;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        Set<uint32, TestAllocator> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Insert(data.keys[i]);
        }

        s_source = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    Set<uint32, TestAllocator> set = s_source;

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        set.Erase(data.removalKeys[i]);
    }

    Consume(set.Size());
}

void ProfileFlatSetInsertion(bool setupOnly)
{
    if (setupOnly) return;

    const Dataset& data = ActiveDataset();

    FlatSet<uint32> set;
    set.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.Insert(data.keys[i]);
    }

    Consume(set.Size());
}

void ProfileFlatSetIteration(bool setupOnly)
{
    static FlatSet<uint32> s_set;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        FlatSet<uint32> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Insert(data.keys[i]);
        }

        s_set = std::move(temp);

        return;
    }

    uint64 sum = 0;
    for (const auto &item : s_set)
    {
        sum += item;
    }

    Consume(sum);
}

void ProfileFlatSetFind(bool setupOnly)
{
    static FlatSet<uint32> s_set;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        FlatSet<uint32> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Insert(data.keys[i]);
        }

        s_set = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (s_set.Find(data.lookupKeys[i]) != s_set.End())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileFlatSetRemoval(bool setupOnly)
{
    static FlatSet<uint32> s_source;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        FlatSet<uint32> temp;
        temp.Reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.Insert(data.keys[i]);
        }

        s_source = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    FlatSet<uint32> set = s_source;

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        set.Erase(data.removalKeys[i]);
    }

    Consume(set.Size());
}

void ProfileStdVectorInsertion(bool setupOnly)
{
    if (setupOnly) return;

    const Dataset& data = ActiveDataset();

    std::vector<uint32> vec;
    vec.reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        vec.push_back(data.keys[i]);
    }

    Consume(vec.size());
}

void ProfileStdVectorIteration(bool setupOnly)
{
    static std::vector<uint32> s_vec;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        std::vector<uint32> temp;
        temp.reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.push_back(data.keys[i]);
        }

        s_vec = std::move(temp);

        return;
    }

    uint64 sum = 0;
    for (size_t i = 0; i < s_vec.size(); ++i)
    {
        sum += s_vec[i];
    }

    Consume(sum);
}

void ProfileStdVectorFind(bool setupOnly)
{
    static std::vector<uint32> s_vec;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        std::vector<uint32> temp;
        temp.reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.push_back(data.keys[i]);
        }

        s_vec = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (std::find(s_vec.begin(), s_vec.end(), data.lookupKeys[i]) != s_vec.end())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileStdVectorRemoval(bool setupOnly)
{
    static std::vector<uint32> s_source;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        std::vector<uint32> temp;
        temp.reserve(data.elementCount);

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.push_back(data.keys[i]);
        }

        s_source = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    std::vector<uint32> vec = s_source;

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        const auto it = std::find(vec.begin(), vec.end(), data.removalKeys[i]);
        if (it != vec.end())
        {
            vec.erase(it);
        }
    }

    Consume(vec.size());
}

void ProfileStdUnorderedMapInsertion(bool setupOnly)
{
    if (setupOnly) return;

    const Dataset& data = ActiveDataset();

    std::unordered_map<uint32, uint32> map;
    map.reserve(static_cast<size_t>(data.elementCount));

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.emplace(data.keys[i], uint32(i));
    }

    Consume(map.size());
}

void ProfileStdUnorderedMapIteration(bool setupOnly)
{
    static std::unordered_map<uint32, uint32> s_map;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        std::unordered_map<uint32, uint32> temp;
        temp.reserve(static_cast<size_t>(data.elementCount));

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.emplace(data.keys[i], uint32(i));
        }

        s_map = std::move(temp);

        return;
    }

    uint64 sum = 0;
    for (const auto &item : s_map)
    {
        sum += item.second;
    }

    Consume(sum);
}

void ProfileStdUnorderedMapFind(bool setupOnly)
{
    static std::unordered_map<uint32, uint32> s_map;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        std::unordered_map<uint32, uint32> temp;
        temp.reserve(static_cast<size_t>(data.elementCount));

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.emplace(data.keys[i], uint32(i));
        }

        s_map = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (s_map.find(data.lookupKeys[i]) != s_map.end())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileStdUnorderedMapRemoval(bool setupOnly)
{
    static std::unordered_map<uint32, uint32> s_source;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        std::unordered_map<uint32, uint32> temp;
        temp.reserve(static_cast<size_t>(data.elementCount));

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.emplace(data.keys[i], uint32(i));
        }

        s_source = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    std::unordered_map<uint32, uint32> map = s_source;

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        map.erase(data.removalKeys[i]);
    }

    Consume(map.size());
}

void ProfileStdMapInsertion(bool setupOnly)
{
    if (setupOnly) return;

    const Dataset& data = ActiveDataset();

    std::map<uint32, uint32> map;

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.emplace(data.keys[i], uint32(i));
    }

    Consume(map.size());
}

void ProfileStdMapIteration(bool setupOnly)
{
    static std::map<uint32, uint32> s_map;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        std::map<uint32, uint32> temp;

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.emplace(data.keys[i], uint32(i));
        }

        s_map = std::move(temp);

        return;
    }

    uint64 sum = 0;
    for (const auto &item : s_map)
    {
        sum += item.second;
    }

    Consume(sum);
}

void ProfileStdMapFind(bool setupOnly)
{
    static std::map<uint32, uint32> s_map;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        std::map<uint32, uint32> temp;

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.emplace(data.keys[i], uint32(i));
        }

        s_map = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (s_map.find(data.lookupKeys[i]) != s_map.end())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileStdMapRemoval(bool setupOnly)
{
    static std::map<uint32, uint32> s_source;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        std::map<uint32, uint32> temp;

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.emplace(data.keys[i], uint32(i));
        }

        s_source = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    std::map<uint32, uint32> map = s_source;

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        map.erase(data.removalKeys[i]);
    }

    Consume(map.size());
}

void ProfileStdUnorderedSetInsertion(bool setupOnly)
{
    if (setupOnly) return;

    const Dataset& data = ActiveDataset();

    std::unordered_set<uint32> set;
    set.reserve(static_cast<size_t>(data.elementCount));

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.insert(data.keys[i]);
    }

    Consume(set.size());
}

void ProfileStdUnorderedSetIteration(bool setupOnly)
{
    static std::unordered_set<uint32> s_set;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        std::unordered_set<uint32> temp;
        temp.reserve(static_cast<size_t>(data.elementCount));

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.insert(data.keys[i]);
        }

        s_set = std::move(temp);

        return;
    }

    uint64 sum = 0;
    for (const auto &item : s_set)
    {
        sum += item;
    }

    Consume(sum);
}

void ProfileStdUnorderedSetFind(bool setupOnly)
{
    static std::unordered_set<uint32> s_set;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        std::unordered_set<uint32> temp;
        temp.reserve(static_cast<size_t>(data.elementCount));

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.insert(data.keys[i]);
        }

        s_set = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (s_set.find(data.lookupKeys[i]) != s_set.end())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileStdUnorderedSetRemoval(bool setupOnly)
{
    static std::unordered_set<uint32> s_source;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        std::unordered_set<uint32> temp;
        temp.reserve(static_cast<size_t>(data.elementCount));

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.insert(data.keys[i]);
        }

        s_source = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    std::unordered_set<uint32> set = s_source;

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        set.erase(data.removalKeys[i]);
    }

    Consume(set.size());
}

void ProfileStdSetInsertion(bool setupOnly)
{
    if (setupOnly) return;

    const Dataset& data = ActiveDataset();

    std::set<uint32> set;

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.insert(data.keys[i]);
    }

    Consume(set.size());
}

void ProfileStdSetIteration(bool setupOnly)
{
    static std::set<uint32> s_set;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        std::set<uint32> temp;

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.insert(data.keys[i]);
        }

        s_set = std::move(temp);

        return;
    }

    uint64 sum = 0;
    for (const auto &item : s_set)
    {
        sum += item;
    }

    Consume(sum);
}

void ProfileStdSetFind(bool setupOnly)
{
    static std::set<uint32> s_set;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        std::set<uint32> temp;

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.insert(data.keys[i]);
        }

        s_set = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (s_set.find(data.lookupKeys[i]) != s_set.end())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileStdSetRemoval(bool setupOnly)
{
    static std::set<uint32> s_source;

    if (setupOnly)
    {
        const Dataset& data = ActiveDataset();

        std::set<uint32> temp;

        for (size_t i = 0; i < data.elementCount; ++i)
        {
            temp.insert(data.keys[i]);
        }

        s_source = std::move(temp);

        return;
    }

    const Dataset& data = ActiveDataset();
    std::set<uint32> set = s_source;

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        set.erase(data.removalKeys[i]);
    }

    Consume(set.size());
}

struct SectionEntry
{
    const char *label;
    Profile::ProfileFunction function;
};

template <size_t Count>
void RunSection(const char *title, const SectionEntry (&entries)[Count], const Dataset& dataset, size_t runsPer, size_t numIterations, size_t runsPerIteration)
{
    g_dataset = &dataset;

    for (size_t i = 0; i < Count; ++i)
    {
        entries[i].function(true);
    }

    Array<Profile> profiles;
    profiles.Reserve(Count);

    for (size_t i = 0; i < Count; ++i)
    {
        profiles.EmplaceBack(entries[i].function);
    }

    Array<Profile *> profilePtrs;
    profilePtrs.Reserve(Count);

    for (size_t i = 0; i < Count; ++i)
    {
        profilePtrs.PushBack(&profiles[i]);
    }

    Array<double> results = Profile::RunInterleved(std::move(profilePtrs), runsPer, numIterations, runsPerIteration);

    std::printf("%s\n", title);
    std::printf("--------\n");

    for (size_t i = 0; i < Count; ++i)
    {
        std::printf("%-22s : %.6f s\n", entries[i].label, results[i]);
    }

    std::printf("\n");
}

void PrintDatasetHeader(const Dataset& dataset)
{
    std::printf("=== %s dataset (N=%zu, lookups=%zu) ===\n\n", dataset.label, dataset.elementCount, dataset.lookupCount);
}

} // namespace

HYP_EXPORT void PrintContainerProfiling(size_t runsPer = 5, size_t numIterations = 50, size_t runsPerIteration = 10)
{
    const Dataset &small = GetDataset(DatasetSize::Small);
    const Dataset &large = GetDataset(DatasetSize::Large);

    const SectionEntry insertionEntries[] = {
        { "Array", &ProfileArrayInsertion },
        { "SlimArray", &ProfileSlimArrayInsertion },
        { "SparsePagedArray", &ProfileSparsePagedArrayInsertion },
        { "Map (Pooled)", &ProfileHashMapInsertion },
        { "Map (Dynamic)", &ProfileHashMapDynamicInsertion },
        { "HashSet (Pooled)", &ProfileHashSetInsertion },
        { "HashSet (Dynamic)", &ProfileHashSetDynamicInsertion },
        { "FlatMap", &ProfileFlatMapInsertion },
        { "FlatSet", &ProfileFlatSetInsertion },
        { "std::vector", &ProfileStdVectorInsertion },
        { "std::unordered_map", &ProfileStdUnorderedMapInsertion },
        { "std::map", &ProfileStdMapInsertion },
        { "std::unordered_set", &ProfileStdUnorderedSetInsertion },
        { "std::set", &ProfileStdSetInsertion }
    };

    const SectionEntry iterationEntries[] = {
        { "Array", &ProfileArrayIteration },
        { "SlimArray", &ProfileSlimArrayIteration },
        { "SparsePagedArray", &ProfileSparsePagedArrayIteration },
        { "Map (Pooled)", &ProfileHashMapIteration },
        { "Map (Dynamic)", &ProfileHashMapDynamicIteration },
        { "HashSet (Pooled)", &ProfileHashSetIteration },
        { "HashSet (Dynamic)", &ProfileHashSetDynamicIteration },
        { "FlatMap", &ProfileFlatMapIteration },
        { "FlatSet", &ProfileFlatSetIteration },
        { "std::vector", &ProfileStdVectorIteration },
        { "std::unordered_map", &ProfileStdUnorderedMapIteration },
        { "std::map", &ProfileStdMapIteration },
        { "std::unordered_set", &ProfileStdUnorderedSetIteration },
        { "std::set", &ProfileStdSetIteration }
    };

    const SectionEntry findEntries[] = {
        { "Array", &ProfileArrayFind },
        { "SlimArray", &ProfileSlimArrayFind },
        { "SparsePagedArray", &ProfileSparsePagedArrayFind },
        { "Map (Pooled)", &ProfileHashMapFind },
        { "Map (Dynamic)", &ProfileHashMapDynamicFind },
        { "HashSet (Pooled)", &ProfileHashSetFind },
        { "HashSet (Dynamic)", &ProfileHashSetDynamicFind },
        { "FlatMap", &ProfileFlatMapFind },
        { "FlatSet", &ProfileFlatSetFind },
        { "std::vector", &ProfileStdVectorFind },
        { "std::unordered_map", &ProfileStdUnorderedMapFind },
        { "std::map", &ProfileStdMapFind },
        { "std::unordered_set", &ProfileStdUnorderedSetFind },
        { "std::set", &ProfileStdSetFind }
    };

    const SectionEntry removalEntries[] = {
        { "Array", &ProfileArrayRemoval },
        { "SlimArray", &ProfileSlimArrayRemoval },
        { "SparsePagedArray", &ProfileSparsePagedArrayRemoval },
        { "Map (Pooled)", &ProfileHashMapRemoval },
        { "Map (Dynamic)", &ProfileHashMapDynamicRemoval },
        { "HashSet (Pooled)", &ProfileHashSetRemoval },
        { "HashSet (Dynamic)", &ProfileHashSetDynamicRemoval },
        { "FlatMap", &ProfileFlatMapRemoval },
        { "FlatSet", &ProfileFlatSetRemoval },
        { "std::vector", &ProfileStdVectorRemoval },
        { "std::unordered_map", &ProfileStdUnorderedMapRemoval },
        { "std::map", &ProfileStdMapRemoval },
        { "std::unordered_set", &ProfileStdUnorderedSetRemoval },
        { "std::set", &ProfileStdSetRemoval }
    };

    PrintDatasetHeader(small);
    RunSection("Insertion", insertionEntries, small, runsPer, numIterations, runsPerIteration);
    RunSection("Iteration", iterationEntries, small, runsPer, numIterations, runsPerIteration);
    RunSection("Find by key", findEntries, small, runsPer, numIterations, runsPerIteration);
    RunSection("Removal", removalEntries, small, runsPer, numIterations, runsPerIteration);

    PrintDatasetHeader(large);
    RunSection("Insertion", insertionEntries, large, runsPer, numIterations, runsPerIteration);
    RunSection("Iteration", iterationEntries, large, runsPer, numIterations, runsPerIteration);
    RunSection("Find by key", findEntries, large, runsPer, numIterations, runsPerIteration);
    RunSection("Removal", removalEntries, large, runsPer, numIterations, runsPerIteration);
}

} // namespace profiling
} // namespace tests
} // namespace Hyperion

#endif
