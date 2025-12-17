/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <EditorPch.hpp>

#include <editor/EditorPickCache.hpp>

#include <core/containers/SparsePagedArray.hpp>

#include <rendering/RenderCollection.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/Mesh.hpp>

#include <asset/MeshAsset.hpp>

#include <util/GameCounter.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

namespace RenderApi {
HYP_API extern uint32 GetFrameCounter();
} // namespace RenderApi

static constexpr int MinResidency = 1;
static constexpr int MaxResidency = 10;

// amount of headroom to leave in the memory pool before we start forcefully evicting entries
constexpr SizeType IdealHeadroom = 1 * 1024 * 1024;

struct EditorPickCacheImpl
{
    // uses SparsePagedArray so iterators remain valid even when entries are evicted
    using Cache = SparsePagedArray<EditorPickCacheEntry, 32, EpcAllocator>;

    using ResidencySet = Bitset;

    Cache cache;
    FixedArray<ResidencySet, MaxResidency + 1> residencyMap;
    RenderProxyList renderProxyList { g_editorPickCachePool, /* isShared */ false, /* useRefCounting */ false };

    LockstepGameCounter updateCounter { 1.0 }; // per second

    EditorPickCacheImpl()
    {
    }
};

static int ComputeResidency(const EditorPickCacheEntry& entry)
{
    // Simple residency computation based on number of vertices and indices
    const SizeType vertexCount = entry.positions.Size();
    const SizeType indexCount = entry.indices.Size();

    if (vertexCount == 0 || indexCount == 0)
    {
        return 0;
    }

    const int fc = RenderApi::GetFrameCounter();

    int residency = MinResidency;

    // scale residency based on how recently the mesh was visible
    // the more recent, the higher the residency
    const int64 framesSinceVisible = int64(fc) - int64(entry.frameVisible);

    if (framesSinceVisible < 60) // visible within last second (assuming 60fps)
    {
        residency += 5;
    }
    else if (framesSinceVisible < 300) // visible within last 5 seconds
    {
        residency += 2;
    }
    else if (framesSinceVisible < 600) // visible within last 10 seconds
    {
        residency += 1;
    }

    residency = MathUtil::Clamp(residency, MinResidency, MaxResidency);

    return residency;
}

EditorPickCache::EditorPickCache()
    : m_impl(MakePimpl<EditorPickCacheImpl>())
{
}

EditorPickCache::~EditorPickCache()
{
}

RenderProxyList& EditorPickCache::GetRenderProxyList() const
{
    return m_impl->renderProxyList;
}

bool EditorPickCache::HasEntry(const Mesh* mesh) const
{
    if (!mesh)
    {
        return false;
    }

    return m_impl->cache.HasIndex(mesh->Id().ToIndex());
}

void EditorPickCache::PutEntry(const Mesh* mesh)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    if (!mesh)
    {
        HYP_LOG(Editor, Error, "Cannot add null mesh to editor pick cache");

        return;
    }

    AssertDebug(std::is_final_v<Mesh> || mesh->InstanceClass() == Mesh::StaticClass());

    const uint32 fc = RenderApi::GetFrameCounter();

    if (m_impl->cache.HasIndex(mesh->Id().ToIndex()))
    {
        EditorPickCacheEntry& entry = m_impl->cache[mesh->Id().ToIndex()];

        const int currResidency = entry.residency;

        // update last frame visible
        entry.frameVisible = fc;

        const int newResidency = ComputeResidency(entry);

        if (newResidency != currResidency)
        {
            // update residency map
            AssertDebug(currResidency < m_impl->residencyMap.Size() && newResidency < m_impl->residencyMap.Size());

            auto& arr = m_impl->residencyMap[currResidency];
            arr.Set(mesh->Id().ToIndex(), false);

            entry.residency = newResidency;

            m_impl->residencyMap[newResidency].Set(mesh->Id().ToIndex(), true);
        }

        return; // already exists
    }

    // Load the stuff in
    if (!mesh->GetAsset())
    {
        HYP_LOG(Editor, Error, "No asset for mesh {} (id: {}), cannot add to editor pick cache", mesh->GetName(), mesh->Id());

        return;
    }

    ResourceHandle resourceHandle(*mesh->GetAsset()->GetResource());

    if (!resourceHandle)
    {
        HYP_LOG(Editor, Error, "Failed to get resource handle for mesh asset {} (id: {}), cannot add to editor pick cache", mesh->GetName(), mesh->Id());

        return;
    }

    const MeshDesc& meshDesc = mesh->GetAsset()->GetMeshDesc();
    const MeshData& meshData = *mesh->GetAsset()->GetMeshData();

    // make sure we have enough memory
    if (!EvictEntries(meshData.vertexData.ByteSize() + meshData.indexData.Size() + IdealHeadroom))
    {
        HYP_LOG(Editor, Error, "Failed to evict enough entries to add mesh {} (id: {}) to editor pick cache", mesh->GetName(), mesh->Id());

        return;
    }

    EditorPickCacheEntry entry {};
    entry.frameVisible = fc;

    entry.positions.Resize(meshData.vertexData.Size());
    for (SizeType i = 0; i < meshData.vertexData.Size(); ++i)
    {
        entry.positions[i] = meshData.vertexData[i].position;
    }

    entry.indices.Resize(meshData.indexData.Size() / sizeof(uint32));
    Memory::MemCpy(entry.indices.Data(), meshData.indexData.Data(), meshData.indexData.Size());

    entry.residency = ComputeResidency(entry);

    auto& set = m_impl->residencyMap[entry.residency];
    auto iter = m_impl->cache.Emplace(mesh->Id().ToIndex(), std::move(entry));
    set.Set(mesh->Id().ToIndex(), true);
}

void EditorPickCache::RemoveEntry(const Mesh* mesh)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    if (!mesh)
    {
        HYP_LOG(Editor, Error, "Cannot remove null mesh from editor pick cache");

        return;
    }

    if (m_impl->cache.HasIndex(mesh->Id().ToIndex()))
    {
        EditorPickCacheEntry& entry = m_impl->cache[mesh->Id().ToIndex()];

        const int residency = entry.residency;

        auto& arr = m_impl->residencyMap[residency];
        arr.Set(mesh->Id().ToIndex(), false);

        m_impl->cache.EraseAt(mesh->Id().ToIndex());
    }
}

EditorPickCacheEntry* EditorPickCache::GetEntry(const Mesh* mesh)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    if (!mesh)
    {
        HYP_LOG(Editor, Error, "Cannot get entry for null mesh from editor pick cache");

        return nullptr;
    }

    if (m_impl->cache.HasIndex(mesh->Id().ToIndex()))
    {
        return &m_impl->cache.Get(mesh->Id().ToIndex());
    }

    return nullptr;
}

void EditorPickCache::Clear()
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    m_impl->cache.Clear();

    for (auto& it : m_impl->residencyMap)
    {
        it.Clear();
    }
}

bool EditorPickCache::EvictEntries(SizeType bytesNeeded)
{
    const MemoryMetrics metrics = g_editorPickCachePool->GetMemoryMetrics();

    if (metrics[MemoryMetrics::MM_BYTES_USED] <= MaxMemoryUsageBytes - bytesNeeded)
    {
        // we have enough headroom, no need to evict entries
        return true;
    }

    SizeType currentMemoryUsageBytes = metrics[MemoryMetrics::MM_BYTES_USED];
    const SizeType targetMemoryUsageBytes = MaxMemoryUsageBytes - bytesNeeded;

    for (int residency = MinResidency; residency <= MaxResidency; ++residency)
    {
        if (currentMemoryUsageBytes <= targetMemoryUsageBytes)
        {
            break;
        }

        auto& entrySet = m_impl->residencyMap[residency];

        Array<Pair<EditorPickCacheEntry*, uint32>> sortedEntries;
        sortedEntries.Reserve(entrySet.Count());

        for (Bitset::BitIndex bit : entrySet)
        {
            AssertDebug(m_impl->cache.HasIndex(bit));

            sortedEntries.EmplaceBack(&m_impl->cache.Get(bit), bit);
        }

        // sort by size in this bucket (largest first)
        std::sort(sortedEntries.Begin(), sortedEntries.End(), [](const Pair<EditorPickCacheEntry*, uint32>& a, const Pair<EditorPickCacheEntry*, uint32>& b)
            {
                const SizeType aSize = a.first->positions.ByteSize() + a.first->indices.ByteSize();
                const SizeType bSize = b.first->positions.ByteSize() + b.first->indices.ByteSize();

                return aSize > bSize;
            });

        for (const Pair<EditorPickCacheEntry*, uint32>& pair : sortedEntries)
        {
            if (currentMemoryUsageBytes <= targetMemoryUsageBytes)
            {
                break;
            }

            // evict
            const SizeType infoSize = pair.first->positions.ByteSize() + pair.first->indices.ByteSize();
            currentMemoryUsageBytes -= infoSize;

            entrySet.Set(pair.second, false);
            m_impl->cache.EraseAt(pair.second);
        }
    }

    return currentMemoryUsageBytes <= targetMemoryUsageBytes;
}

void EditorPickCache::Update(float delta)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    if (m_impl->updateCounter.Waiting())
    {
        return;
    }

    m_impl->updateCounter.NextTick();

    // update residencies
    for (int residency = MinResidency; residency <= MaxResidency; ++residency)
    {
        auto& entrySet = m_impl->residencyMap[residency];

        for (Bitset::BitIndex bit : entrySet)
        {
            AssertDebug(m_impl->cache.HasIndex(bit));
            EditorPickCacheEntry& entry = m_impl->cache.Get(bit);

            const int newResidency = ComputeResidency(entry);

            if (newResidency != residency)
            {
                // update residency map
                entrySet.Set(bit, false);

                entry.residency = newResidency;

                m_impl->residencyMap[newResidency].Set(bit, true);
            }
        }
    }

    const MemoryMetrics metrics = g_editorPickCachePool->GetMemoryMetrics();

    if (metrics[MemoryMetrics::MM_BYTES_USED] <= MaxMemoryUsageBytes - IdealHeadroom)
    {
        // we have enough headroom, no need to evict entries
        return;
    }

    if (!EvictEntries(IdealHeadroom))
    {
        HYP_LOG(Editor, Warning, "Failed to evict enough entries to maintain ideal headroom in editor pick cache (used: {} bytes, ideal: {} bytes)",
            g_editorPickCachePool->GetMemoryMetrics()[MemoryMetrics::MM_BYTES_USED],
            MaxMemoryUsageBytes - IdealHeadroom);
    }

    HYP_LOG(Editor, Debug, "Memory usage after editor pick cache update: {} bytes",
        g_editorPickCachePool->GetMemoryMetrics()[MemoryMetrics::MM_BYTES_USED]);
}

} // namespace hyperion
