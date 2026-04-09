/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#ifdef HYP_TESTS

#include <Core/profiling/Profile.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/SparsePagedArray.hpp>
#include <Core/containers/HashMap.hpp>
#include <Core/containers/FlatMap.hpp>
#include <Core/containers/HashSet.hpp>
#include <Core/containers/FlatSet.hpp>

#include <Core/memory/pool/Pool.hpp>

#include <Core/memory/allocator/ArenaAllocator.hpp>

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

    const int index = static_cast<int>(size);

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

void ProfileArrayInsertion()
{
    const Dataset& data = ActiveDataset();

    Array<uint32, TestAllocator> array;

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        array.PushBack(data.keys[i]);
    }

    Consume(array.Size());
}

void ProfileArrayIteration()
{
    const Dataset& data = ActiveDataset();

    Array<uint32, TestAllocator> array;
    array.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        array.PushBack(data.keys[i]);
    }

    uint64 sum = 0;
    for (size_t i = 0; i < array.Size(); ++i)
    {
        sum += array[i];
    }

    Consume(sum);
}

void ProfileArrayFind()
{
    const Dataset& data = ActiveDataset();

    Array<uint32, TestAllocator> array;
    array.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        array.PushBack(data.keys[i]);
    }

    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (array.Find(data.lookupKeys[i]) != array.End())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileArrayRemoval()
{
    const Dataset& data = ActiveDataset();

    Array<uint32, TestAllocator> array;
    array.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        array.PushBack(data.keys[i]);
    }

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        array.Erase(data.removalKeys[i]);
    }

    Consume(array.Size());
}

void ProfileSparsePagedArrayInsertion()
{
    const Dataset& data = ActiveDataset();

    SparsePagedArray<uint32, 64, TestAllocator> array;

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        array.Set(data.sparseIndices[i], data.keys[i]);
    }

    Consume(array.Count());
}

void ProfileSparsePagedArrayIteration()
{
    const Dataset& data = ActiveDataset();

    SparsePagedArray<uint32, 64, TestAllocator> array;

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        array.Set(data.sparseIndices[i], data.keys[i]);
    }

    uint64 sum = 0;
    for (const auto &value : array)
    {
        sum += value;
    }

    Consume(sum);
}

void ProfileSparsePagedArrayFind()
{
    const Dataset& data = ActiveDataset();

    SparsePagedArray<uint32, 64, TestAllocator> array;

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        array.Set(data.sparseIndices[i], data.keys[i]);
    }

    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (array.TryGet(data.lookupIndices[i]) != nullptr)
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileSparsePagedArrayRemoval()
{
    const Dataset& data = ActiveDataset();

    SparsePagedArray<uint32, 64, TestAllocator> array;

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        array.Set(data.sparseIndices[i], data.keys[i]);
    }

    for (size_t i = 0; i < data.removalIndices.Size(); ++i)
    {
        array.EraseAt(data.removalIndices[i]);
    }

    Consume(array.Count());
}

void ProfileHashMapInsertion()
{
    const Dataset& data = ActiveDataset();

    HashMap<uint32, uint32, TestAllocator> map;
    map.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.Set(data.keys[i], static_cast<uint32>(i));
    }

    Consume(map.Size());
}

void ProfileHashMapDynamicInsertion()
{
    const Dataset& data = ActiveDataset();

    HashMap<uint32, uint32, TestAllocator> map;
    map.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.Set(data.keys[i], static_cast<uint32>(i));
    }

    Consume(map.Size());
}

void ProfileHashMapIteration()
{
    const Dataset& data = ActiveDataset();

    HashMap<uint32, uint32, TestAllocator> map;
    map.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.Set(data.keys[i], static_cast<uint32>(i));
    }

    uint64 sum = 0;
    for (const auto &item : map)
    {
        sum += item.second;
    }

    Consume(sum);
}

void ProfileHashMapDynamicIteration()
{
    const Dataset& data = ActiveDataset();

    HashMap<uint32, uint32, TestAllocator> map;
    map.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.Set(data.keys[i], static_cast<uint32>(i));
    }

    uint64 sum = 0;
    for (const auto &item : map)
    {
        sum += item.second;
    }

    Consume(sum);
}

void ProfileHashMapFind()
{
    const Dataset& data = ActiveDataset();

    HashMap<uint32, uint32, TestAllocator> map;
    map.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.Set(data.keys[i], static_cast<uint32>(i));
    }

    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (map.Find(data.lookupKeys[i]) != map.End())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileHashMapDynamicFind()
{
    const Dataset& data = ActiveDataset();

    HashMap<uint32, uint32, TestAllocator> map;
    map.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.Set(data.keys[i], static_cast<uint32>(i));
    }

    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (map.Find(data.lookupKeys[i]) != map.End())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileHashMapRemoval()
{
    const Dataset& data = ActiveDataset();

    HashMap<uint32, uint32, TestAllocator> map;
    map.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.Set(data.keys[i], static_cast<uint32>(i));
    }

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        map.Erase(data.removalKeys[i]);
    }

    Consume(map.Size());
}

void ProfileHashMapDynamicRemoval()
{
    const Dataset& data = ActiveDataset();

    HashMap<uint32, uint32, TestAllocator> map;
    map.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.Set(data.keys[i], static_cast<uint32>(i));
    }

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        map.Erase(data.removalKeys[i]);
    }

    Consume(map.Size());
}

void ProfileFlatMapInsertion()
{
    const Dataset& data = ActiveDataset();

    FlatMap<uint32, uint32> map;
    map.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.Set(data.keys[i], static_cast<uint32>(i));
    }

    Consume(map.Size());
}

void ProfileFlatMapIteration()
{
    const Dataset& data = ActiveDataset();

    FlatMap<uint32, uint32> map;
    map.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.Set(data.keys[i], static_cast<uint32>(i));
    }

    uint64 sum = 0;
    for (const auto &item : map)
    {
        sum += item.second;
    }

    Consume(sum);
}

void ProfileFlatMapFind()
{
    const Dataset& data = ActiveDataset();

    FlatMap<uint32, uint32> map;
    map.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.Set(data.keys[i], static_cast<uint32>(i));
    }

    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (map.Find(data.lookupKeys[i]) != map.End())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileFlatMapRemoval()
{
    const Dataset& data = ActiveDataset();

    FlatMap<uint32, uint32> map;
    map.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.Set(data.keys[i], static_cast<uint32>(i));
    }

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        map.Erase(data.removalKeys[i]);
    }

    Consume(map.Size());
}

void ProfileHashSetInsertion()
{
    const Dataset& data = ActiveDataset();

    HashSet<uint32, TestAllocator> set;
    set.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.Insert(data.keys[i]);
    }

    Consume(set.Size());
}

void ProfileHashSetDynamicInsertion()
{
    const Dataset& data = ActiveDataset();

    HashSet<uint32, TestAllocator> set;
    set.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.Insert(data.keys[i]);
    }

    Consume(set.Size());
}

void ProfileHashSetIteration()
{
    const Dataset& data = ActiveDataset();

    HashSet<uint32, TestAllocator> set;
    set.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.Insert(data.keys[i]);
    }

    uint64 sum = 0;
    for (const auto &item : set)
    {
        sum += item;
    }

    Consume(sum);
}

void ProfileHashSetDynamicIteration()
{
    const Dataset& data = ActiveDataset();

    HashSet<uint32, TestAllocator> set;
    set.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.Insert(data.keys[i]);
    }

    uint64 sum = 0;
    for (const auto &item : set)
    {
        sum += item;
    }

    Consume(sum);
}

void ProfileHashSetFind()
{
    const Dataset& data = ActiveDataset();

    HashSet<uint32, TestAllocator> set;
    set.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.Insert(data.keys[i]);
    }

    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (set.Find(data.lookupKeys[i]) != set.End())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileHashSetDynamicFind()
{
    const Dataset& data = ActiveDataset();

    HashSet<uint32, TestAllocator> set;
    set.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.Insert(data.keys[i]);
    }

    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (set.Find(data.lookupKeys[i]) != set.End())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileHashSetRemoval()
{
    const Dataset& data = ActiveDataset();

    HashSet<uint32, TestAllocator> set;
    set.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.Insert(data.keys[i]);
    }

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        set.Erase(data.removalKeys[i]);
    }

    Consume(set.Size());
}

void ProfileHashSetDynamicRemoval()
{
    const Dataset& data = ActiveDataset();

    HashSet<uint32, TestAllocator> set;
    set.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.Insert(data.keys[i]);
    }

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        set.Erase(data.removalKeys[i]);
    }

    Consume(set.Size());
}

void ProfileFlatSetInsertion()
{
    const Dataset& data = ActiveDataset();

    FlatSet<uint32> set;
    set.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.Insert(data.keys[i]);
    }

    Consume(set.Size());
}

void ProfileFlatSetIteration()
{
    const Dataset& data = ActiveDataset();

    FlatSet<uint32> set;
    set.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.Insert(data.keys[i]);
    }

    uint64 sum = 0;
    for (const auto &item : set)
    {
        sum += item;
    }

    Consume(sum);
}

void ProfileFlatSetFind()
{
    const Dataset& data = ActiveDataset();

    FlatSet<uint32> set;
    set.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.Insert(data.keys[i]);
    }

    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (set.Find(data.lookupKeys[i]) != set.End())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileFlatSetRemoval()
{
    const Dataset& data = ActiveDataset();

    FlatSet<uint32> set;
    set.Reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.Insert(data.keys[i]);
    }

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        set.Erase(data.removalKeys[i]);
    }

    Consume(set.Size());
}

void ProfileStdVectorInsertion()
{
    const Dataset& data = ActiveDataset();

    std::vector<uint32> vec;
    vec.reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        vec.push_back(data.keys[i]);
    }

    Consume(vec.size());
}

void ProfileStdVectorIteration()
{
    const Dataset& data = ActiveDataset();

    std::vector<uint32> vec;
    vec.reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        vec.push_back(data.keys[i]);
    }

    uint64 sum = 0;
    for (size_t i = 0; i < vec.size(); ++i)
    {
        sum += vec[i];
    }

    Consume(sum);
}

void ProfileStdVectorFind()
{
    const Dataset& data = ActiveDataset();

    std::vector<uint32> vec;
    vec.reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        vec.push_back(data.keys[i]);
    }

    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (std::find(vec.begin(), vec.end(), data.lookupKeys[i]) != vec.end())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileStdVectorRemoval()
{
    const Dataset& data = ActiveDataset();

    std::vector<uint32> vec;
    vec.reserve(data.elementCount);

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        vec.push_back(data.keys[i]);
    }

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

void ProfileStdUnorderedMapInsertion()
{
    const Dataset& data = ActiveDataset();

    std::unordered_map<uint32, uint32> map;
    map.reserve(static_cast<size_t>(data.elementCount));

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.emplace(data.keys[i], static_cast<uint32>(i));
    }

    Consume(map.size());
}

void ProfileStdUnorderedMapIteration()
{
    const Dataset& data = ActiveDataset();

    std::unordered_map<uint32, uint32> map;
    map.reserve(static_cast<size_t>(data.elementCount));

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.emplace(data.keys[i], static_cast<uint32>(i));
    }

    uint64 sum = 0;
    for (const auto &item : map)
    {
        sum += item.second;
    }

    Consume(sum);
}

void ProfileStdUnorderedMapFind()
{
    const Dataset& data = ActiveDataset();

    std::unordered_map<uint32, uint32> map;
    map.reserve(static_cast<size_t>(data.elementCount));

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.emplace(data.keys[i], static_cast<uint32>(i));
    }

    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (map.find(data.lookupKeys[i]) != map.end())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileStdUnorderedMapRemoval()
{
    const Dataset& data = ActiveDataset();

    std::unordered_map<uint32, uint32> map;
    map.reserve(static_cast<size_t>(data.elementCount));

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.emplace(data.keys[i], static_cast<uint32>(i));
    }

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        map.erase(data.removalKeys[i]);
    }

    Consume(map.size());
}

void ProfileStdMapInsertion()
{
    const Dataset& data = ActiveDataset();

    std::map<uint32, uint32> map;

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.emplace(data.keys[i], static_cast<uint32>(i));
    }

    Consume(map.size());
}

void ProfileStdMapIteration()
{
    const Dataset& data = ActiveDataset();

    std::map<uint32, uint32> map;

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.emplace(data.keys[i], static_cast<uint32>(i));
    }

    uint64 sum = 0;
    for (const auto &item : map)
    {
        sum += item.second;
    }

    Consume(sum);
}

void ProfileStdMapFind()
{
    const Dataset& data = ActiveDataset();

    std::map<uint32, uint32> map;

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.emplace(data.keys[i], static_cast<uint32>(i));
    }

    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (map.find(data.lookupKeys[i]) != map.end())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileStdMapRemoval()
{
    const Dataset& data = ActiveDataset();

    std::map<uint32, uint32> map;

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        map.emplace(data.keys[i], static_cast<uint32>(i));
    }

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        map.erase(data.removalKeys[i]);
    }

    Consume(map.size());
}

void ProfileStdUnorderedSetInsertion()
{
    const Dataset& data = ActiveDataset();

    std::unordered_set<uint32> set;
    set.reserve(static_cast<size_t>(data.elementCount));

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.insert(data.keys[i]);
    }

    Consume(set.size());
}

void ProfileStdUnorderedSetIteration()
{
    const Dataset& data = ActiveDataset();

    std::unordered_set<uint32> set;
    set.reserve(static_cast<size_t>(data.elementCount));

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.insert(data.keys[i]);
    }

    uint64 sum = 0;
    for (const auto &item : set)
    {
        sum += item;
    }

    Consume(sum);
}

void ProfileStdUnorderedSetFind()
{
    const Dataset& data = ActiveDataset();

    std::unordered_set<uint32> set;
    set.reserve(static_cast<size_t>(data.elementCount));

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.insert(data.keys[i]);
    }

    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (set.find(data.lookupKeys[i]) != set.end())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileStdUnorderedSetRemoval()
{
    const Dataset& data = ActiveDataset();

    std::unordered_set<uint32> set;
    set.reserve(static_cast<size_t>(data.elementCount));

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.insert(data.keys[i]);
    }

    for (size_t i = 0; i < data.removalKeys.Size(); ++i)
    {
        set.erase(data.removalKeys[i]);
    }

    Consume(set.size());
}

void ProfileStdSetInsertion()
{
    const Dataset& data = ActiveDataset();

    std::set<uint32> set;

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.insert(data.keys[i]);
    }

    Consume(set.size());
}

void ProfileStdSetIteration()
{
    const Dataset& data = ActiveDataset();

    std::set<uint32> set;

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.insert(data.keys[i]);
    }

    uint64 sum = 0;
    for (const auto &item : set)
    {
        sum += item;
    }

    Consume(sum);
}

void ProfileStdSetFind()
{
    const Dataset& data = ActiveDataset();

    std::set<uint32> set;

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.insert(data.keys[i]);
    }

    uint64 hits = 0;
    for (size_t i = 0; i < data.lookupCount; ++i)
    {
        if (set.find(data.lookupKeys[i]) != set.end())
        {
            ++hits;
        }
    }

    Consume(hits);
}

void ProfileStdSetRemoval()
{
    const Dataset& data = ActiveDataset();

    std::set<uint32> set;

    for (size_t i = 0; i < data.elementCount; ++i)
    {
        set.insert(data.keys[i]);
    }

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
        { "SparsePagedArray", &ProfileSparsePagedArrayInsertion },
        { "HashMap (Pooled)", &ProfileHashMapInsertion },
        { "HashMap (Dynamic)", &ProfileHashMapDynamicInsertion },
        { "HashSet (Pooled)", &ProfileHashSetInsertion },
        { "HashSet (Dynamic)", &ProfileHashSetDynamicInsertion },
        { "std::vector", &ProfileStdVectorInsertion },
        { "std::unordered_map", &ProfileStdUnorderedMapInsertion },
        { "std::map", &ProfileStdMapInsertion },
        { "std::unordered_set", &ProfileStdUnorderedSetInsertion },
        { "std::set", &ProfileStdSetInsertion }
    };

    const SectionEntry iterationEntries[] = {
        { "Array", &ProfileArrayIteration },
        { "SparsePagedArray", &ProfileSparsePagedArrayIteration },
        { "HashMap (Pooled)", &ProfileHashMapIteration },
        { "HashMap (Dynamic)", &ProfileHashMapDynamicIteration },
        { "HashSet (Pooled)", &ProfileHashSetIteration },
        { "HashSet (Dynamic)", &ProfileHashSetDynamicIteration },
        { "std::vector", &ProfileStdVectorIteration },
        { "std::unordered_map", &ProfileStdUnorderedMapIteration },
        { "std::map", &ProfileStdMapIteration },
        { "std::unordered_set", &ProfileStdUnorderedSetIteration },
        { "std::set", &ProfileStdSetIteration }
    };

    const SectionEntry findEntries[] = {
        { "Array", &ProfileArrayFind },
        { "SparsePagedArray", &ProfileSparsePagedArrayFind },
        { "HashMap (Pooled)", &ProfileHashMapFind },
        { "HashMap (Dynamic)", &ProfileHashMapDynamicFind },
        { "HashSet (Pooled)", &ProfileHashSetFind },
        { "HashSet (Dynamic)", &ProfileHashSetDynamicFind },
        { "std::vector", &ProfileStdVectorFind },
        { "std::unordered_map", &ProfileStdUnorderedMapFind },
        { "std::map", &ProfileStdMapFind },
        { "std::unordered_set", &ProfileStdUnorderedSetFind },
        { "std::set", &ProfileStdSetFind }
    };

    const SectionEntry removalEntries[] = {
        { "Array", &ProfileArrayRemoval },
        { "SparsePagedArray", &ProfileSparsePagedArrayRemoval },
        { "HashMap (Pooled)", &ProfileHashMapRemoval },
        { "HashMap (Dynamic)", &ProfileHashMapDynamicRemoval },
        { "HashSet (Pooled)", &ProfileHashSetRemoval },
        { "HashSet (Dynamic)", &ProfileHashSetDynamicRemoval },
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