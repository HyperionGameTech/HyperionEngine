/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Asset/AssetPath.hpp>
#include <Asset/AssetTypes.hpp>
#include <Asset/AssetBucket.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Containers/Set.hpp>
#include <Core/Containers/SparsePagedArray.hpp>

#include <Core/Utilities/StringView.hpp>
#include <Core/Utilities/ForEach.hpp>
#include <Core/Utilities/Result.hpp>

#include <Core/Threading/SharedMutex.hpp>
#include <Core/Threading/SchedulerFwd.hpp>

#include <Core/Resource/Resource.hpp>

#include <Core/Logging/LoggerFwd.hpp>

#include <Core/Constants.hpp>
#include <Core/Defines.hpp>

#include <Core/Utilities/ClockTimer.hpp>

#include <Scripting/ScriptableDelegate.hpp>

#include <Core/Utilities/GlobalContext.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Assets);

ENGINE_API extern Pool* g_assetPool;
using AssetAllocator = AllocatorInstance<Pool, &g_assetPool>;

class AssetRegistry;
class AssetBucketData;
class AssetObject;
struct BoxedValue;
class ByteWriter;
class BlobStorage;

enum class AssetRegistryId : uint32;

extern StringHash AssetDesc_KeyByFunction(const AssetDesc& assetDesc);
extern StringHash AssetObject_KeyByFunction(const Handle<AssetObject>& assetObject);

using AssetDescSet = HashTable<AssetDesc, &AssetDesc_KeyByFunction, AssetAllocator>;

// Maps from AssetDesc id -> AssetObject handle
using AssetObjectCache = SparsePagedArray<Handle<AssetObject>, 256, AssetAllocator>;

HYP_CLASS()
class ENGINE_API AssetRegistry final : public ObjectBase
{
    HYP_OBJECT_BODY(AssetRegistry);

    AssetRegistry() = default;

public:
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

    template <class T>
    Handle<T> GetAsset(const AssetBucket& bucket, StringHash name)
    {
        if (Handle<AssetObject> asset = GetAsset(bucket, name); asset.IsValid())
        {
            if (Handle<T> assetCasted = DynamicCast<T>(asset); assetCasted.IsValid())
            {
                return assetCasted;
            }
        }

        return Handle<T>();
    }

    void MarkAssetDirty(const AssetObject& assetObject);

    uint32 GetBucketAssetDescs(uint32 bucketIndex, Array<AssetDesc>& outDescs) const;

    void PutAsset(const Handle<AssetObject>& asset);
    void PutAsset(const AssetBucket& bucket, const Handle<AssetObject>& asset);

    void PutAssetUnique(const Handle<AssetObject>& asset);
    void PutAssetUnique(const AssetBucket& bucket, const Handle<AssetObject>& asset);

    void PutAssetsDeep(const Handle<AssetObject>& targetAsset, bool overwriteExisting = false);

    void RemoveAsset(const Handle<AssetObject>& asset);
    void RemoveAsset(const AssetBucket& bucket, StringHash name);

    bool LoadAssetDescs();
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

    AssetBucketData* m_assetBucketData;

    Scheduler* m_scheduler;

    BlobStorage* m_blobStorage;

    DelegateHandler m_onEngineShutdown;
};

/*! \brief Context struct used with GlobalContext to allow scope-based overriding of the current AssetRegistry. */
struct AssetRegistryContext
{
    Handle<AssetRegistry> registry;
};

/*! \brief Context struct used with GlobalContext to suppress MarkDirty() calls while an AssetObject is being deserialized. */
struct AssetLoadingContext
{
};

ENGINE_API Handle<AssetRegistry> GetCurrentAssetRegistry();

ENGINE_API void PushAssetRegistry(const Handle<AssetRegistry>& registry);
ENGINE_API void PopAssetRegistry(const AssetRegistry* registry);

ENGINE_API void ClearAssetRegistryStack();

ENGINE_API Handle<AssetRegistry> GetEngineAssetRegistry();
ENGINE_API void SetEngineAssetRegistry(const Handle<AssetRegistry>& registry);

#ifdef HYP_EDITOR
ENGINE_API Handle<AssetRegistry> GetEditorAssetRegistry();
ENGINE_API void SetEditorAssetRegistry(const Handle<AssetRegistry>& registry);
#endif // HYP_EDITOR

} // namespace Hyperion
