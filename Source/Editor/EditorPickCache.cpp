/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorPickCache.hpp>
#include <Editor/EditorMemory.hpp>

#include <Core/Containers/SparsePagedArray.hpp>

#include <Rendering/RendererMain.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/Mesh.hpp>

#include <Core/Utilities/ClockTimer.hpp>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

extern uint32 GetFrameCounter();

static constexpr int MinResidency = 1;
static constexpr int MaxResidency = 10;

constexpr size_t IdealHeadroom = 1 * 1024 * 1024;
constexpr size_t MaxMemoryUsageBytes = 256 * 1024 * 1024;

static constexpr size_t BlockSize = (16 * 1024 * 1024);

Pool s_editorPickCachePool { BlockSize, PF_NONE };
HYP_EXPORT Pool* g_editorPickCachePool = &s_editorPickCachePool;

// @TOOD Would be nice to compute screenspace size for each object using viewport camera and use that as part of residency computation?
// so small objects are worth less, and if less than some pixel threshold, discard it entirely.

struct EditorPickCacheImpl
{
    // uses SparsePagedArray so iterators remain valid even when entries are evicted
    using Cache = SparsePagedArray<EditorPickCacheEntry, 32, EditorAllocator>;
    using ResidencySet = TBitset<EditorAllocator>;

    Cache cache;
    FixedArray<ResidencySet, MaxResidency + 1> residencyMap;
    RenderProxyList renderProxyList { /* isShared */ false, /* useRefCounting */ false };

    ClockTimer timer { 1.0 }; // 1 tick per second

    EditorPickCacheImpl()
    {
    }
};

static int ComputeResidency(const EditorPickCacheEntry& entry)
{
    // @TODO Just keep a timestamp. Don't need to assume based on frame count.
    const uint32 fc = GetFrameCounter();

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
    m_impl.Reset();
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

void EditorPickCache::PutEntry(const Mesh* mesh, bool invalidate)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!mesh)
    {
        HYP_LOG(Editor, Error, "Cannot add null mesh to editor pick cache");

        return;
    }

    AssertDebug(std::is_final_v<Mesh> || mesh->InstanceClass() == Mesh::StaticClass());

    const uint32 fc = GetFrameCounter();

    bool existsButInvalidating = false;

    if (m_impl->cache.HasIndex(mesh->Id().ToIndex()))
    {
        if (invalidate)
        {
            existsButInvalidating = true;
        }
        else
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

            return; // already exists and we aren't invalidating.
        }
    }

#if 0 // TEMP: Disabling for now as we figure out dropping data at end of all read scopes

    auto resGuard = mesh->GetReadScope();

    if (!resGuard)
    {
        HYP_LOG(Editor, Error, "Failed to get resource handle for mesh asset {} (id: {}), cannot add to editor pick cache", mesh->GetName(), mesh->Id());

        return;
    }

    const MeshDesc& meshDesc = mesh->GetMeshDesc();

    const VertexArrayView vertexData = mesh->GetVertexData(0);
    const Span<const ubyte> indexData = mesh->GetIndexData(0);

    const uint32 indexSize = GpuElemTypeSize(meshDesc.meshAttributes.indexBufferElemType);
    const size_t numIndices = indexData.Size() / indexSize;

    const size_t bytesNeeded = (vertexData.vertexCount * sizeof(Vec3f)) + numIndices * indexSize;

    if (!HasFreeSpace(bytesNeeded) && !EvictEntries(bytesNeeded))
    {
        HYP_LOG_ONCE(Editor, Error, "Not enough headroom in editor pick cache; cannot add mesh {} (id: {}) to editor pick cache", mesh->GetName(), mesh->Id());

        return;
    }

    EditorPickCacheEntry& entry = m_impl->cache[mesh->Id().ToIndex()];
    entry.mesh = MakeWeakRef(mesh);
    entry.frameVisible = fc;

    const int newResidency = ComputeResidency(entry);
    
    if (existsButInvalidating)
    {
        const int oldResidency = entry.residency;
        
        if (oldResidency != newResidency)
        {
            m_impl->residencyMap[oldResidency].Set(mesh->Id().ToIndex(), false);
        }
    }

    entry.residency = newResidency;

    entry.positions.Resize(vertexData.vertexCount);

    const size_t vertexSizeInFloats = vertexData.layoutDesc.VertexSize() / sizeof(float);

    for (size_t i = 0; i < vertexData.vertexCount; ++i)
    {
        const float* floatDataOffset = vertexData.floatData + (i * vertexSizeInFloats);
        const TVertexPacket<VT_Position>* packet = reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset);

        entry.positions[i].x = packet->posX;
        entry.positions[i].y = packet->posY;
        entry.positions[i].z = packet->posZ;
    }

    // @TODO fix for non-uint32 indices
    Assert(indexSize == 4);

    entry.indices.Resize(numIndices);
    Memory::Copy(entry.indices.Data(), indexData.Data(), numIndices * indexSize);

    m_impl->residencyMap[newResidency].Set(mesh->Id().ToIndex(), true);
#endif
}

void EditorPickCache::RemoveEntry(const Mesh* mesh)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!mesh)
    {
        HYP_LOG(Editor, Error, "Cannot remove null mesh from editor pick cache");

        return;
    }

    if (m_impl->cache.HasIndex(mesh->Id().ToIndex()))
    {
        RemoveEntryAtIndex(mesh->Id().ToIndex());
    }
}

void EditorPickCache::RemoveEntryAtIndex(uint32 index)
{
    AssertDebug(m_impl->cache.HasIndex(index));

    EditorPickCacheEntry& entry = m_impl->cache.Get(index);

    m_impl->residencyMap[entry.residency].Set(index, false);

    m_impl->cache.EraseAt(index);
}

EditorPickCacheEntry* EditorPickCache::GetEntry(const Mesh* mesh)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!mesh)
    {
        HYP_LOG(Editor, Error, "Cannot get entry for null mesh from editor pick cache");

        return nullptr;
    }

    if (m_impl->cache.HasIndex(mesh->Id().ToIndex()))
    {
        EditorPickCacheEntry& entry = m_impl->cache.Get(mesh->Id().ToIndex());

        // Ids are recycled and entries are only reaped on a timer in Update(), so this index can
        // still hold a destroyed mesh's data. Drop it; the caller falls back to live mesh data.
        if (entry.mesh.GetUnsafe() != mesh)
        {
            RemoveEntryAtIndex(mesh->Id().ToIndex());

            return nullptr;
        }

        return &entry;
    }

    return nullptr;
}

void EditorPickCache::Clear()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    m_impl->cache.Clear();

    for (auto& it : m_impl->residencyMap)
    {
        it.Clear();
    }
}

bool EditorPickCache::EvictEntries(size_t bytesNeeded)
{
    const MemoryMetrics metrics = g_editorPickCachePool->GetMemoryMetrics();

    if (metrics[MemoryMetrics::MM_BYTES_USED] + bytesNeeded <= MaxMemoryUsageBytes)
    {
        // we have enough headroom, no need to evict entries
        return true;
    }

    size_t currentMemoryUsageBytes = metrics[MemoryMetrics::MM_BYTES_USED];

    const size_t targetMemoryUsageBytes = bytesNeeded <= MaxMemoryUsageBytes
        ? MaxMemoryUsageBytes - bytesNeeded
        : 0;

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
                const size_t aSize = a.first->positions.ByteSize() + a.first->indices.ByteSize();
                const size_t bSize = b.first->positions.ByteSize() + b.first->indices.ByteSize();

                return aSize > bSize;
            });

        for (const Pair<EditorPickCacheEntry*, uint32>& pair : sortedEntries)
        {
            if (currentMemoryUsageBytes <= targetMemoryUsageBytes)
            {
                break;
            }

            // evict
            const size_t infoSize = pair.first->positions.ByteSize() + pair.first->indices.ByteSize();
            currentMemoryUsageBytes -= infoSize;

            entrySet.Set(pair.second, false);
            m_impl->cache.EraseAt(pair.second);
        }
    }

    return currentMemoryUsageBytes <= targetMemoryUsageBytes;
}

bool EditorPickCache::HasFreeSpace(size_t bytes)
{
    const MemoryMetrics metrics = g_editorPickCachePool->GetMemoryMetrics();

    return metrics[MemoryMetrics::MM_BYTES_USED] + bytes <= MaxMemoryUsageBytes;
}

void EditorPickCache::Update(float delta)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (m_impl->timer.Waiting())
    {
        return;
    }

    m_impl->timer.NextTick();

    for (int residency = MinResidency; residency <= MaxResidency; ++residency)
    {
        auto& entrySet = m_impl->residencyMap[residency];

        for (Bitset::BitIndex bit : entrySet)
        {
            AssertDebug(m_impl->cache.HasIndex(bit));
            EditorPickCacheEntry& entry = m_impl->cache.Get(bit);

            if (!entry.mesh.Lock())
            {
                RemoveEntryAtIndex(bit);

                continue;
            }

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

    HYP_LOG(Editor, Verbose, "Memory usage after editor pick cache update: {} bytes",
        g_editorPickCachePool->GetMemoryMetrics()[MemoryMetrics::MM_BYTES_USED]);
}

} // namespace Hyperion
