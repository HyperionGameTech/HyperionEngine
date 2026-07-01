/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/BLASCache.hpp>
#include <Rendering/BLASBuilder.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Utilities/IdGenerator.hpp>

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
    BottomLevelAS* blas;
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

    struct StorageIdAndRefCount
    {
        uint32 storageId;
        uint32 refCount;
    };

    TFlatMap<uint64, StorageIdAndRefCount, RenderAllocator> blasKeyToStorageId;

    IdGenerator storageIdGenerator;

    typename MeshEntityIdToKeyMap::Iterator cleanupIterator;
};

BLASCache::BLASCache()
    : m_impl(MakePimplWithAllocator<BLASCacheImpl, RenderAllocator>())
{
}

BLASCache::~BLASCache() = default;

void BLASCache::GetOrCreateBLAS(
    Entity* entity, Mesh* mesh, Material* material,
    uint64& outNewKey, uint64& outOldKey,
    BottomLevelAS*& outBlas)
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

    ObjIdBase ids[] = { entity->Id(), mesh->Id(), material ? material->Id() : ObjId<Material>() };

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

    BottomLevelASRef blas = BLASBuilder::Build(mesh, material);
    CheckResult(blas->Create());

    // Build new BLAS
    Entry& entry = it->second;

    entry = {};
    entry.blas = blas.Release();
    entry.lastUsedFrame = GetFrameCounter();

    outBlas = entry.blas;
}

uint32 BLASCache::TranslateBLASKeyToStorageId(uint64 key) const
{
    AssertOnThread(g_renderThread);

    auto& map = m_impl->blasKeyToStorageId;

    auto it = map.Find(key);

    if (it == map.End())
    {
        return InvalidStorageId;
    }

    return it->second.storageId;
}

HYP_NODISCARD uint32 BLASCache::AllocateStorageId(uint64 key)
{
    AssertOnThread(g_renderThread);

    auto& map = m_impl->blasKeyToStorageId;

    auto it = map.Find(key);

    if (it != map.End())
    {
        ++it->second.refCount;
        return it->second.storageId;
    }

    const uint32 newId = m_impl->storageIdGenerator.Next() - 1;

    map[key] = { newId, 1 };

    return newId;
}

bool BLASCache::ReleaseStorageIdForBLASKey(uint64 key, uint32& outStorageId, uint32& outNewRefCount)
{
    AssertOnThread(g_renderThread);

    auto& map = m_impl->blasKeyToStorageId;

    auto it = map.Find(key);
    AssertDebug(it != map.End());

    if (it == map.End())
    {
        return false;
    }

    outStorageId = it->second.storageId;
    outNewRefCount = --it->second.refCount;

    if (outNewRefCount == 0)
    {
        const uint32 storageId = it->second.storageId;

        m_impl->storageIdGenerator.ReleaseId(storageId + 1);

        map.Erase(it);
    }

    return true;
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
