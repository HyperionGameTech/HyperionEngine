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
#include <Core/Threading/ThreadSignal.hpp>
#include <Core/Threading/SchedulerFwd.hpp>

#include <Core/Resource/Resource.hpp>

#include <Core/Logging/LoggerFwd.hpp>

#include <Core/Constants.hpp>
#include <Core/Defines.hpp>

#include <Core/Utilities/ClockTimer.hpp>

#include <Scripting/ScriptableDelegate.hpp>

#include <Core/Utilities/GlobalContext.hpp>

#include <Core/Functional/Proc.hpp>

namespace Hyperion {

using functional::ProcRef;

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Assets);

ENGINE_API extern Pool* g_assetPool;
using AssetAllocator = AllocatorInstance<Pool, &g_assetPool>;

class AssetRegistry;
class AssetBucketData;
class AssetObject;
struct BoxedValue;
class ByteWriter;

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

    void SyncAssetName(const AssetBucket& bucket, Name oldName, Name newName);

    bool LoadAssetDescs();

    void SaveDirtyAssets();
    bool HasDirtyAssets() const;

    void RemoveCached();
    void RemoveCached(const AssetBucket& bucket);

    /// End new assetbucket based stuff

    FilePath GetManifestPath(const AssetPath& assetPath) const;

    /*! \param outSyncContentTask if not nullptr, will attempt to download cache from url pointed to at CacheServer CLI arg
     *      The Task itself will be set to a task that on completes indicates the download has completed. */
    void Initialize(Task<void>* outSyncContentTask = nullptr);
    void Shutdown();

    /*! \brief Called by AssetManager to perform enqueued tasks that mutate the registry. */
    void Update();

    static void WalkAssetDeep(const BoxedValue& target, const ProcRef<void(const Handle<AssetObject>&)>& onAssetFound);

private:
    template <class Func, class FutureType = void>
    void PostTask(Func&& fn, Task<FutureType>* outFuture = nullptr);

    AssetRegistryId m_registryId;
    FilePath m_rootPath;

    bool m_isInitialized;
    bool m_isSyncingCache;

    ThreadSignal m_cacheSyncComplete;

    SharedMutex m_mutex;

    // timer for when we should prune transient packages
    ClockTimer m_pruneTimer;
    threading::TaskBatch* m_pruneTaskBatch;

    AssetBucketData* m_assetBucketData;

    Scheduler* m_scheduler;

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

//-- helpers

static constexpr AssetRegistryId GetAssetRegistryIndex(StringHash hash)
{
    constexpr HashCode::ValueType GameRegistryHash = ("Game"_sh).hashCode;
    constexpr HashCode::ValueType EngineRegistryHash = ("Engine"_sh).hashCode;
    constexpr HashCode::ValueType EditorRegistryHash = ("Editor"_sh).hashCode;

    switch (hash.hashCode)
    {
    case GameRegistryHash:
        return AssetRegistryId::Game;
    case EngineRegistryHash:
        return AssetRegistryId::Engine;
    case EditorRegistryHash:
        return AssetRegistryId::Editor;
    }

    return AssetRegistryId::Game;
}

static constexpr const char* GetAssetRegistryName(AssetRegistryId registryId)
{
    switch (registryId)
    {
    case AssetRegistryId::Game:
        return "Game";
    case AssetRegistryId::Engine:
        return "Engine";
    case AssetRegistryId::Editor:
        return "Editor";
    }

    return "Game";
}

//--

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
