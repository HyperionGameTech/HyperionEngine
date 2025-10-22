/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <editor/EditorPickCache.hpp>

#include <core/containers/FlatMap.hpp>
#include <core/containers/FlatSet.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/HashSet.hpp>

#include <rendering/RenderCollection.hpp>
#include <rendering/Mesh.hpp>

#include <asset/MeshAsset.hpp>

#include <util/GameCounter.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

static constexpr int MinResidency = 1;
static constexpr int MaxResidency = 10;

// amount of headroom to leave in the memory pool before we start evicting entries
constexpr SizeType IdealHeadroom = 8 * 1024 * 1024;

struct EditorPickCacheImpl
{
    using Cache = HashMap<const Mesh*, EditorPickCacheEntry, NodeAllocator<EpcAllocator>>;
    using ResidencySet = FlatSet<typename Cache::Iterator>;

    Cache cache;
    FlatMap<int, ResidencySet> residencyMap;
    RenderProxyList renderProxyList { /* isShared */ false, /* useRefCounting */ false };
    LockstepGameCounter residencyUpdateCounter { 1000.0 }; // to track time until we update residencies
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

    const int fc = RenderApi_GetFrameCounter();

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
    : m_pImpl(MakePimpl<EditorPickCacheImpl>())
{
    for (int i = MinResidency; i <= MaxResidency; ++i)
    {
        m_pImpl->residencyMap.Emplace(i);
    }
}

EditorPickCache::~EditorPickCache()
{
}

RenderProxyList& EditorPickCache::GetRenderProxyList() const
{
    return m_pImpl->renderProxyList;
}

bool EditorPickCache::HasEntry(const Mesh* mesh) const
{
    return m_pImpl->cache.Contains(mesh);
}

void EditorPickCache::PutEntry(const Mesh* mesh)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    const uint32 fc = RenderApi_GetFrameCounter();

    auto it = m_pImpl->cache.Find(mesh);
    if (it != m_pImpl->cache.End())
    {
        const int currResidency = it->second.residency;

        // update last frame visible
        it->second.frameVisible = fc;

        const int newResidency = ComputeResidency(it->second);

        if (newResidency != currResidency)
        {
            // update residency map
            auto resIt = m_pImpl->residencyMap.Find(currResidency);
            if (resIt != m_pImpl->residencyMap.End())
            {
                auto& arr = resIt->second;
                arr.Erase(it);
            }

            it->second.residency = newResidency;

            m_pImpl->residencyMap[newResidency].Insert(it);
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
        return;
    }

    const MeshDesc& meshDesc = mesh->GetAsset()->GetMeshDesc();
    const MeshData& meshData = *mesh->GetAsset()->GetMeshData();

    EditorPickCacheEntry entry {};
    entry.frameVisible = fc;
    entry.residency = ComputeResidency(entry);

    entry.positions.Resize(meshData.vertexData.Size());
    for (SizeType i = 0; i < meshData.vertexData.Size(); ++i)
    {
        entry.positions[i] = meshData.vertexData[i].position;
    }

    entry.indices.Resize(meshData.indexData.Size() / sizeof(uint32));
    Memory::MemCpy(entry.indices.Data(), meshData.indexData.Data(), meshData.indexData.Size());

    auto insertResult = m_pImpl->cache.Insert({ mesh, std::move(entry) });
    m_pImpl->residencyMap[insertResult.first->second.residency].Insert(insertResult.first);
}

void EditorPickCache::RemoveEntry(const Mesh* mesh)
{
    auto it = m_pImpl->cache.Find(mesh);
    if (it != m_pImpl->cache.End())
    {
        const int residency = it->second.residency;

        auto resIt = m_pImpl->residencyMap.Find(residency);
        if (resIt != m_pImpl->residencyMap.End())
        {
            auto& arr = resIt->second;
            arr.Erase(it);
        }

        m_pImpl->cache.Erase(it);
    }
}

void EditorPickCache::Clear()
{
    m_pImpl->cache.Clear();
    m_pImpl->residencyMap.Clear();
}

void EditorPickCache::Update(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    m_pImpl->residencyUpdateCounter.NextTick();

    if (!m_pImpl->residencyUpdateCounter.Waiting())
    {
        // update residencies

        for (auto& [residency, entrySet] : m_pImpl->residencyMap)
        {
            for (auto it = entrySet.Begin(); it != entrySet.End();)
            {
                EditorPickCacheEntry& entry = (*it)->second;

                const int newResidency = ComputeResidency(entry);

                if (newResidency != residency)
                {
                    // update residency map
                    it = entrySet.Erase(it);

                    entry.residency = newResidency;
                    m_pImpl->residencyMap[newResidency].Insert(*it);
                }
                else
                {
                    ++it;
                }
            }
        }

        m_pImpl->residencyUpdateCounter.Reset();
    }

    const SizeType maxMemoryUsageBytes = (64 * 1024 * 1024);

    if (g_editorPickCachePool->GetMemoryMetrics()[MemoryMetrics::MM_BYTES_USED] <= maxMemoryUsageBytes - IdealHeadroom)
    {
        // we have enough headroom, no need to evict entries
        return;
    }

    SizeType currentMemoryUsageBytes = 0; // @TODO: Start at current allocated amount, sort elemsn by size, and subtract until we reach the target

    for (int residency = MaxResidency; residency >= MinResidency; --residency)
    {
        auto resIt = m_pImpl->residencyMap.Find(residency);
        if (resIt == m_pImpl->residencyMap.End())
        {
            continue;
        }

        auto& entrySet = resIt->second;

        for (auto it = entrySet.Begin(); it != entrySet.End();)
        {
            const EditorPickCacheEntry& entry = (*it)->second;

            const SizeType entryMemoryUsageBytes = entry.positions.ByteSize() + entry.indices.ByteSize();

            if (currentMemoryUsageBytes + entryMemoryUsageBytes > maxMemoryUsageBytes)
            {
                // evict this entry
                it = entrySet.Erase(it);
                m_pImpl->cache.Erase(*it);
            }
            else
            {
                currentMemoryUsageBytes += entryMemoryUsageBytes;
                ++it;
            }
        }
    }
}

} // namespace hyperion
