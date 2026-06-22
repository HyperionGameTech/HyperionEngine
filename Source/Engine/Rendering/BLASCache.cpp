/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/BLASCache.hpp>
#include <Rendering/MeshBlasBuilder.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/MaterialInstance.hpp>

#include <Core/Threading/Mutex.hpp>
#include <Core/Containers/Map.hpp>

#include <Scene/Entity.hpp>

namespace Hyperion {

static uint64 MakeBLASKey(Span<const ObjIdBase> ids)
{
    HashCode hc;

    for (const ObjIdBase& id : ids)
    {
        hc.Add(id.GetHashCode());
    }

    return hc.GetHashCode().Value();
}

struct Entry
{
    GpuBlas* blas;
    uint32 lastUsedFrame;
};

using MeshEntityIdToKeyMap = SparsePagedArray<uint64, 128, RenderAllocator>;

class BLASCacheImpl
{
public:
    BLASCacheImpl()
        : cleanupIterator(meshEntityIdToKey.End())
    {
    }

    ~BLASCacheImpl()
    {
        for (auto& pair : map)
        {
            EnqueueDeletion(pair.second.blas);
        }

        map.Clear();
    }

    TMap<uint64, Entry, RenderAllocator> map;
    MeshEntityIdToKeyMap meshEntityIdToKey;

    typename MeshEntityIdToKeyMap::Iterator cleanupIterator;
};

BLASCache::BLASCache()
    : m_impl(MakePimplWithAllocator<BLASCacheImpl, RenderAllocator>())
{
}

BLASCache::~BLASCache() = default;

void BLASCache::GetOrCreateBLAS(
    Entity* entity, Mesh* mesh, MaterialInstance* material,
    uint64& outNewKey, uint64& outOldKey,
    GpuBlas*& outBlas)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    outNewKey = 0;
    outOldKey = 0;
    outBlas = nullptr;

    if (!entity || !mesh)
    {
        return;
    }

    AssertDebug(entity->InstanceClass() == Entity::StaticClass()); // needed since we use ToIndex() - if we ever want to change this, we need subclassImpls as used elsewhere.

    ObjIdBase ids[] = { entity->Id(), mesh->Id(), material ? material->Id() : ObjId<MaterialInstance>() };

    const uint64 newKey = MakeBLASKey(Span<const ObjIdBase>(ids, ids + std::size(ids)));

    outNewKey = newKey;

    if (m_impl->meshEntityIdToKey.HasIndex(entity->Id().ToIndex()))
    {
        const uint64 oldKey = m_impl->meshEntityIdToKey.Get(entity->Id().ToIndex());

        outOldKey = oldKey;

        if (newKey == oldKey)
        {
            Entry& entry = m_impl->map[oldKey];
            entry.lastUsedFrame = GetFrameCounter();

            outBlas = entry.blas;

            return;
        }

        auto it = m_impl->map.Find(oldKey);
        Assert(it != m_impl->map.End());

        EnqueueDeletion(it->second.blas);

        m_impl->map.Erase(it);
    }

    auto it = m_impl->map.Find(newKey);

    if (it != m_impl->map.End())
    {
        Entry& entry = it->second;

        if (!entry.blas)
        {
            return;
        }

        // Check if material changed - if so, we need to rebuild
        const bool materialsDiffer = entry.blas->GetMaterial() != material;

        if (!materialsDiffer)
        {
            outBlas = entry.blas;

            entry.lastUsedFrame = GetFrameCounter();

            return;
        }

        // Material changed or BLAS is null, need to rebuild
        EnqueueDeletion(entry.blas);
        entry.blas = nullptr;
    }

    it = m_impl->map.Emplace(newKey).first;
    Assert(it->second.blas == nullptr);

    GpuBlasRef gpuBlas = MeshBlasBuilder::Build(mesh, material);
    CheckResult(gpuBlas->Create());

    // Build new BLAS
    Entry& entry = it->second;

    entry = {};
    entry.blas = gpuBlas.Release();
    entry.lastUsedFrame = GetFrameCounter();

    outBlas = entry.blas;
}

void BLASCache::RunCleanupCycle(int maxIter)
{
    HYP_SCOPE;

#if 0
    m_impl->cleanupIterator = typename MeshEntityIdToKeyMap::Iterator(
        &m_impl->meshEntityIdToKey,
        m_impl->cleanupIterator.page,
        m_impl->cleanupIterator.elem);

    if (m_impl->cleanupIterator == m_impl->meshEntityIdToKey.End())
    {
        m_impl->cleanupIterator = m_impl->meshEntityIdToKey.Begin();
    }

    int numIterations = 0;
    const uint32 frameCounter = GetFrameCounter();

    while (numIterations < maxIter && m_impl->cleanupIterator != m_impl->meshEntityIdToKey.End())
    {
        uint64 key = *m_impl->cleanupIterator;

        ++numIterations;

        Entry& entry = m_impl->map[key];

        if (frameCounter - entry.lastUsedFrame > 100)
        {
            EnqueueDeletion(entry.blas);

            m_impl->map.Erase(key);
            m_impl->cleanupIterator = m_impl->meshEntityIdToKey.Erase(m_impl->cleanupIterator);

            continue;
        }

        ++m_impl->cleanupIterator;
    }
#endif
}

} // namespace Hyperion
