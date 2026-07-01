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

#include <Core/Utilities/IndexAllocator.hpp>

#include <Scene/Entity.hpp>

namespace Hyperion {

template <size_t N>
static HYP_FORCE_INLINE uint64 MakeBLASKey(const ObjIdBase (&ids)[N])
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
using EntryMap = TMap<uint64, Entry, RenderAllocator>;

struct StorageIdAndRefCount
{
    uint32 storageId;
    uint32 refCount;
};
using StorageIdMap = TMap<uint64, StorageIdAndRefCount, RenderAllocator>;

using EntityToKeyMap = TMap<Entity*, uint64, RenderAllocator>;

class BLASCacheImpl
{
public:
    BLASCacheImpl() = default;

    ~BLASCacheImpl()
    {
        for (auto& pair : entryMap)
        {
            EnqueueDeletion(pair.second.blas);
        }

        entryMap.Clear();
    }

    EntryMap entryMap;
    EntityToKeyMap entityToKey;
    StorageIdMap blasKeyToStorageId;

    IndexAllocator storageIndexAllocator;
};

BLASCache::BLASCache()
    : m_impl(MakePimplWithAllocator<BLASCacheImpl, RenderAllocator>())
{
}

BLASCache::~BLASCache() = default;

BottomLevelAS* BLASCache::TryGetBLAS(Entity* entity, uint64* pOutKey)
{
    if (!entity)
    {
        return nullptr;
    }

    AssertDebug(entity->InstanceClass() == Entity::StaticClass());

    auto entityToKeyIt = m_impl->entityToKey.Find(entity);
    if (entityToKeyIt == m_impl->entityToKey.End())
    {
        return false;
    }

    const uint64 key = entityToKeyIt->second;

    if (pOutKey)
    {
        *pOutKey = key;
    }

    if (key == 0)
    {
        return nullptr;
    }

    auto it = m_impl->entryMap.Find(key);

    if (it == m_impl->entryMap.End())
    {
        return nullptr;
    }

    return it->second.blas;
}

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

    const uint64 newKey = MakeBLASKey({ entity->Id(), mesh->Id(), material ? material->Id() : ObjId<Material>() });
    outNewKey = newKey;

    auto entityToKeyIt = m_impl->entityToKey.Find(entity);

    if (entityToKeyIt != m_impl->entityToKey.End())
    {
        const uint64 oldKey = entityToKeyIt->second;

        outOldKey = oldKey;

        if (newKey == oldKey)
        {
            Entry& entry = m_impl->entryMap[oldKey];
            entry.lastUsedFrame = GetFrameCounter();

            outBlas = entry.blas;

            return;
        }

        auto it = m_impl->entryMap.Find(oldKey);
        Assert(it != m_impl->entryMap.End());

        EnqueueDeletion(it->second.blas);

        m_impl->entryMap.Erase(it);

        // update in Mesh Entity -> Key Map
        entityToKeyIt->second = newKey;
    }
    else
    {
        entityToKeyIt = m_impl->entityToKey.Emplace(entity, newKey).first;
    }

    auto it = m_impl->entryMap.Find(newKey);

    if (it != m_impl->entryMap.End())
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

    it = m_impl->entryMap.Emplace(newKey).first;
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

    const uint32 newId = m_impl->storageIndexAllocator.Allocate();

    map[key] = { newId, 1 };

    return newId;
}

bool BLASCache::ReleaseStorageIdForBLASKey(uint64 key, uint32& outStorageId, uint32& outNewRefCount)
{
    AssertOnThread(g_renderThread);

    Assert(key != 0);

    auto& map = m_impl->blasKeyToStorageId;

    auto it = map.Find(key);
    AssertDebug(it != map.End());

    if (it == map.End())
    {
        return false;
    }

    const uint32 storageId = it->second.storageId;
    Assert(storageId != InvalidStorageId);

    outStorageId = storageId;
    outNewRefCount = --it->second.refCount;

    if (outNewRefCount == 0)
    {
        m_impl->storageIndexAllocator.Free(storageId);

        map.Erase(it);
    }

    return true;
}

void BLASCache::RunCleanupCycle(int)
{
    HYP_SCOPE;

    const uint32 frameCounter = GetFrameCounter();

    for (auto it = m_impl->entityToKey.Begin(); it != m_impl->entityToKey.End();)
    {
        uint64 key = it->second;
        Entry& entry = m_impl->entryMap[key];

        if (frameCounter - entry.lastUsedFrame > 100)
        {
            EnqueueDeletion(entry.blas);

            m_impl->entryMap.Erase(key);

            it = m_impl->entityToKey.Erase(it);

            continue;
        }

        ++it;
    }
}

} // namespace Hyperion
