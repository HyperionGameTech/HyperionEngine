/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <asset/AssetPath.hpp>
#include <asset/AssetTypes.hpp>
#include <asset/AssetBucket.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/filesystem/FilePath.hpp>

#include <Core/containers/HashSet.hpp>

#include <Core/utilities/StringView.hpp>
#include <Core/utilities/ForEach.hpp>
#include <Core/utilities/Result.hpp>

#include <Core/threading/SharedMutex.hpp>
#include <Core/threading/SchedulerFwd.hpp>

#include <Core/memory/resource/Resource.hpp>

#include <Core/logging/LoggerFwd.hpp>

#include <Core/Constants.hpp>
#include <Core/Defines.hpp>

#include <Core/utilities/ClockTimer.hpp>

#include <scripting/ScriptableDelegate.hpp>

#include <Core/utilities/GlobalContext.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

HYP_API extern Pool* g_assetPool;
using AssetAllocator = AllocatorInstance<Pool, &g_assetPool>;

class AssetRegistry;
class AssetObject;
struct BoxedValue;
class ByteWriter;
class BlobStorage;
class MemoryMappedFile;

enum class AssetRegistryId : uint32;

extern StringHash AssetDesc_KeyByFunction(const AssetDesc& assetDesc);
extern StringHash AssetObject_KeyByFunction(const Handle<AssetObject>& assetObject);

using AssetDescSet = IntrusiveMap<AssetDesc, &AssetDesc_KeyByFunction, AssetAllocator>;

// Maps from AssetDesc id -> AssetObject handle
using AssetObjectCache = SparsePagedArray<Handle<AssetObject>, 64, AssetAllocator>;

class AssetBucketData
{
public:
    AssetRegistryId registryId : 3;
    uint32 bucketIndex : 29;
    
    AssetDescSet assetDescs;
    AssetObjectCache assetObjectCache;
    Bitset dirtyIndices;
    Bitset usedIndices;
    SharedMutex mtx;

    AssetBucketData()
    {
        // reserve index 0 for invalid
        usedIndices.Set(0, true);
    }

    AssetBucketData(const AssetBucketData& other) = delete;
    AssetBucketData& operator=(const AssetBucketData& other) = delete;

    AssetBucketData(AssetBucketData&& other) noexcept = delete;
    AssetBucketData& operator=(AssetBucketData&& other) noexcept = delete;

    void MarkDirty(uint32 index);

    void SetAsset(
        AssetDesc& assetDesc,
        const Handle<AssetObject>& assetObject);

    /*! \brief Get a unique asset name within this bucket by appending an incrementing number to the base name until an unused name is found.
     *   The returned AssetDesc has info about the allocated index/slot + the unique name that was generated.
     *   \param comparator If provided, returning true from the comparator will indicate equality between assets,
     *   meaning that new names will stop being generated from that point on, and the function will return. */
    void AllocateUniqueAssetName(
        ANSIStringView inAssetName,
        AssetDesc& outAssetDesc,
        const ProcRef<bool(const AssetDesc&)>& comparator = nullptr);

    bool GetAssetDesc(StringHash nameHash, AssetDesc& outAssetDesc) const;
};

HYP_CLASS()
class HYP_API AssetRegistry final : public ObjectBase
{
    HYP_OBJECT_BODY(AssetRegistry);
    
    AssetRegistry() = default;

public:
    static Pool* GetAllocator() { return g_assetPool; }

    AssetRegistry(AssetRegistryId registryId, const FilePath& rootPath);

    AssetRegistry(const AssetRegistry& other) = delete;
    AssetRegistry& operator=(const AssetRegistry& other) = delete;

    AssetRegistry(AssetRegistry&& other) noexcept = delete;
    AssetRegistry& operator=(AssetRegistry&& other) noexcept = delete;

    ~AssetRegistry();

    HYP_FORCE_INLINE AssetRegistryId GetRegistryId() const
    {
        return m_registryId;
    }

    HYP_METHOD()
    FilePath GetRootPath() const;

    HYP_METHOD()
    void SetRootPath(const FilePath& rootPath);
    
    /// Begin new assetbucket based stuff
    Handle<AssetObject> GetAsset(const AssetBucket& bucket, StringHash name);

    void MarkAssetDirty(const AssetObject& assetObject);

    uint32 GetBucketAssetDescs(uint32 bucketIndex, Array<AssetDesc>& outDescs) const;

    void PutAsset(const Handle<AssetObject>& asset);
    void PutAsset(const AssetBucket& bucket, const Handle<AssetObject>& asset);

    void PutAssetUnique(const Handle<AssetObject>& asset);
    void PutAssetUnique(const AssetBucket& bucket, const Handle<AssetObject>& asset);

    void PutAssetsDeep(const Handle<AssetObject>& targetAsset);

    void RemoveAsset(const Handle<AssetObject>& asset);
    void RemoveAsset(const AssetBucket& bucket, StringHash name);

    void LoadAssetDescs();
    void SaveDirtyAssets();

    void RemoveCached();
    void RemoveCached(const AssetBucket& bucket);

    /// End new assetbucket based stuff

    FilePath GetManifestPath(const AssetPath& assetPath) const;

    BlobStorage& GetBlobStorage();

    HYP_FORCE_INLINE bool HasBlobStorage() const
    {
        return m_blobStorage != nullptr;
    }

    void Initialize();
    void Shutdown();

    /*! \brief Called by AssetManager to perform enqueued tasks that mutate the registry. */
    void Update();

private:
    void InitBlobStorage(const FilePath& blobStorageDir);

    template <class Func, class FutureType = void>
    void PostTask(Func&& fn, Task<FutureType>* outFuture = nullptr);

    void SaveBlobCache(bool async);

    AssetRegistryId m_registryId;
    FilePath m_rootPath;

    bool m_isInitialized;

    SharedMutex m_mutex;

    // timer for when we should prune transient packages
    ClockTimer m_pruneTimer;
    threading::TaskBatch* m_pruneTaskBatch;

    // timer for saving blob cache data
    ClockTimer m_saveBlobCacheTimer;
    threading::TaskBatch* m_saveBlobCacheBatch;

    FixedArray<AssetBucketData, MaxAssetBuckets> m_assetBucketData;

    Scheduler* m_scheduler;

    BlobStorage* m_blobStorage;

    DelegateHandler m_onEngineShutdown;
};

/*! \brief Context struct used with GlobalContext to allow scope-based overriding of the current AssetRegistry. */
struct AssetRegistryContext
{
    Handle<AssetRegistry> registry;
};

HYP_API Handle<AssetRegistry> GetCurrentAssetRegistry();

HYP_API void PushCurrentAssetRegistry(const Handle<AssetRegistry>& registry);
HYP_API void PopCurrentAssetRegistry();

HYP_API Handle<AssetRegistry> GetEngineAssetRegistry();
HYP_API void SetEngineAssetRegistry(const Handle<AssetRegistry>& registry);

} // namespace Hyperion
