/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <asset/AssetLoader.hpp>

#include <Core/reflection/Handle.hpp>
#include <Core/reflection/ObjectFwd.hpp>

#include <Core/functional/Delegate.hpp>

#include <Core/filesystem/FsUtil.hpp>
#include <Core/filesystem/FilePath.hpp>

#include <Core/logging/LoggerFwd.hpp>

#include <Core/Constants.hpp>
#include <Core/Defines.hpp>

#include <algorithm>
#include <type_traits>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

class AssetCache;
class AssetBatch;
struct AssetBatchCallbacks;
class AssetRegistry;

struct ProcessAssetFunctorBase;

namespace threading {

class TaskThreadPool;

} // namespace threading

using threading::TaskThreadPool;

template <class T>
struct ProcessAssetFunctor;

using AssetLoadFlags = uint32;

CORE_API extern const Class* GetClass(const TypeId& typeId);

enum AssetLoadFlagBits : AssetLoadFlags
{
    ASSET_LOAD_FLAGS_NONE = 0x0,
    ASSET_LOAD_FLAGS_CACHE_READ = 0x1,
    ASSET_LOAD_FLAGS_CACHE_WRITE = 0x2
};

HYP_STRUCT()
struct AssetLoaderDefinition
{
    HYP_STRUCT_BODY(AssetLoaderDefinition);

    TypeId loaderTypeId;
    TypeId resultTypeId;
    const Class* resultClass = nullptr;
    FlatSet<String> extensions;
    Handle<AssetLoaderBase> loader;

    HYP_FORCE_INLINE bool HandlesResultType(TypeId typeId) const
    {
        return resultTypeId == typeId
            || (resultClass != nullptr && IsA(resultClass, GetClass(typeId)));
    }

    HYP_FORCE_INLINE bool HandlesExtension(const String& filepath) const
    {
        return extensions.FindIf([&filepath](const String& extension)
                   {
                       return filepath.EndsWith(extension);
                   })
            != extensions.End();
    }

    HYP_FORCE_INLINE bool IsWildcardExtensionLoader() const
    {
        return extensions.Empty() || extensions.Find("*") != extensions.End();
    }
};

HYP_ENUM()
enum class AssetChangeType : uint32
{
    CHANGED = 0,
    CREATED = 1,
    DELETED = 2,
    RENAMED = 3,

    MAX
};

HYP_CLASS()
class AssetCollector final : public ObjectBase
{
    HYP_OBJECT_BODY(AssetCollector);

public:
    // Necessary for script bindings
    AssetCollector() = default;

    AssetCollector(const FilePath& basePath)
        : m_basePath(basePath)
    {
    }

    ENGINE_API ~AssetCollector();

    HYP_METHOD(Property = "BasePath", Serialize = true)
    HYP_FORCE_INLINE const FilePath& GetBasePath() const
    {
        return m_basePath;
    }

    HYP_METHOD(Property = "BasePath", Serialize = true)
    HYP_FORCE_INLINE void SetBasePath(const FilePath& basePath)
    {
        m_basePath = basePath;
    }

    HYP_METHOD()
    ENGINE_API void NotifyAssetChanged(const FilePath& path, AssetChangeType changeType);

    HYP_METHOD(Scriptable)
    bool IsWatching() const;

    HYP_METHOD(Scriptable)
    void StartWatching();

    HYP_METHOD(Scriptable)
    void StopWatching();

    HYP_METHOD(Scriptable)
    void OnAssetChanged(const FilePath& path, AssetChangeType changeType);

private:
    bool IsWatching_Impl() const
    {
        return false;
    }

    void StartWatching_Impl()
    {
    }

    void StopWatching_Impl()
    {
    }

    void OnAssetChanged_Impl(const FilePath& path, AssetChangeType changeType)
    {
    }

    FilePath m_basePath;
};

class AssetManagerThreadPool;

HYP_CLASS()
class AssetManager final : public ObjectBase
{
    friend class AssetBatch;
    friend class AssetLoaderBase;

    HYP_OBJECT_BODY(AssetManager);

public:
    typedef UniquePtr<ProcessAssetFunctorBase> (*ProcessAssetFunctorFactory)(
        const String& batchIdentifier,
        const String& key,
        const String& path,
        AssetBatchCallbacks* callbacks,
        AssetLoadHint hint);

    static constexpr bool assetCacheEnabled = false;

    HYP_METHOD()
    ENGINE_API static const Handle<AssetManager>& GetInstance();

    ENGINE_API AssetManager();
    AssetManager(const AssetManager& other) = delete;
    AssetManager& operator=(const AssetManager& other) = delete;
    AssetManager(AssetManager&& other) noexcept = delete;
    AssetManager& operator=(AssetManager&& other) noexcept = delete;
    ENGINE_API ~AssetManager();

    ENGINE_API TaskThreadPool* GetThreadPool() const;

    HYP_METHOD()
    FilePath GetBasePath() const;

    HYP_METHOD()
    void SetBasePath(const FilePath& basePath);

    HYP_METHOD()
    Handle<AssetCollector> GetBaseAssetCollector() const;

    void ForEachAssetCollector(const ProcRef<void(const Handle<AssetCollector>&)>& callback) const;

    HYP_METHOD()
    void AddAssetCollector(const Handle<AssetCollector>& assetCollector);

    HYP_METHOD()
    void RemoveAssetCollector(const Handle<AssetCollector>& assetCollector);

    const Handle<AssetCollector>& FindAssetCollector(ProcRef<bool(const Handle<AssetCollector>&)>) const;

    template <class Loader, class ResultType, class... Formats>
    void Register(Formats&&... formats)
    {
        static_assert(std::is_base_of_v<AssetLoaderBase, Loader>,
            "Loader must be a derived class of AssetLoaderBase!");

        // AssertOnThread(g_simThread);

        const FixedArray<String, sizeof...(formats)> formatStrings {
            String(formats)...
        };

        AssetLoaderDefinition& assetLoaderDefinition = m_loaders.EmplaceBack();
        assetLoaderDefinition.loaderTypeId = TypeId::ForType<Loader>();
        assetLoaderDefinition.resultTypeId = TypeId::ForType<ResultType>();
        assetLoaderDefinition.resultClass = GetClass(TypeId::ForType<ResultType>());
        assetLoaderDefinition.extensions = FlatSet<String>(formatStrings.Begin(), formatStrings.End());
        assetLoaderDefinition.loader = MakeHandle<Loader>();

        m_functorFactories.Set<Loader>([](
            const String& batchIdentifier,
            const String& key,
            const String& path,
            AssetBatchCallbacks* callbacksPtr,
            AssetLoadHint hint) -> UniquePtr<ProcessAssetFunctorBase>
            {
                return MakeUnique<ProcessAssetFunctor<ResultType>>(batchIdentifier, key, path, callbacksPtr, hint);
            });
    }

    /*! \brief Load a single asset synchronously
     *  \param typeId The TypeId of asset to load
     *  \param path The path to the asset
     *  \param batchIdentifier Optional string identifier used to group assets together once they're imported.
     *  \param hint Optional hint to pass to the loader.
     *  \return The result of the load operation */
    HYP_NODISCARD AssetLoadResult Load(
        const TypeId& typeId,
        const String& path,
        const String& batchIdentifier = String::empty,
        AssetLoadHint hint = AssetLoadHint::NoHint);

    /*! \brief Load a single asset synchronously
     *  \tparam T The type of asset to load
     *  \param path The path to the asset
     *  \param batchIdentifier Optional identifier string to group assets together once they're imported.
     *  \param hint Optional hint to pass to the loader.
     *  \return The result of the load operation */
    template <class T>
    HYP_NODISCARD TAssetLoadResult<T> Load(
        const String& path,
        const String& batchIdentifier = String::empty,
        AssetLoadHint hint = AssetLoadHint::NoHint)
    {
        const AssetLoaderDefinition* loaderDefinition = GetLoaderDefinition(path, TypeId::ForType<T>());

        if (!loaderDefinition)
        {
            return HYP_MAKE_ERROR(AssetLoadError, "No registered loader for the given path", AssetLoadError::ERR_NO_LOADER);
        }

        AssetLoaderBase* loader = loaderDefinition->loader.Get();
        Assert(loader != nullptr);

        return TAssetLoadResult<T>(loader->Load(*this, path, batchIdentifier, hint));
    }

    ENGINE_API const AssetLoaderDefinition* GetLoaderDefinition(const FilePath& path, TypeId desiredTypeId = TypeId::Void());

    ENGINE_API AssetBatch* CreateBatch(const String& identifier = String::empty);

    HYP_METHOD()
    ENGINE_API Handle<AssetRegistry> GetAssetRegistry() const;

    void Initialize();

    void Update(float delta);

    Delegate<void, const Handle<AssetCollector>&> OnAssetCollectorAdded;
    Delegate<void, const Handle<AssetCollector>&> OnAssetCollectorRemoved;
    Delegate<void, const Handle<AssetCollector>&> OnBaseAssetCollectorChanged;

private:
    /*! \internal Called from AssetBatch on LoadAsync() */
    ENGINE_API void AddPendingBatch(AssetBatch* batch);

    ENGINE_API UniquePtr<ProcessAssetFunctorBase> CreateProcessAssetFunctor(TypeId loaderTypeId,
        const String& batchIdentifier,
        const String& key,
        const String& path,
        AssetBatchCallbacks* callbacksPtr,
        AssetLoadHint hint = AssetLoadHint::NoHint);

    template <class Loader>
    UniquePtr<ProcessAssetFunctorBase> CreateProcessAssetFunctor(
        const String& batchIdentifier,
        const String& key,
        const String& path,
        AssetBatchCallbacks* callbacksPtr,
        AssetLoadHint hint = AssetLoadHint::NoHint)
    {
        return CreateProcessAssetFunctor(TypeId::ForType<Loader>(), batchIdentifier, key, path, callbacksPtr, hint);
    }

    UniquePtr<ProcessAssetFunctorBase> CreateProcessAssetFunctor(
        const String& batchIdentifier,
        const String& key,
        const String& path,
        AssetBatchCallbacks* callbacksPtr,
        AssetLoadHint hint = AssetLoadHint::NoHint)
    {
        const AssetLoaderDefinition* loaderDefinition = GetLoaderDefinition(path);

        if (!loaderDefinition)
        {
            // no registered loader for the path
            return nullptr;
        }

        return CreateProcessAssetFunctor(loaderDefinition->loaderTypeId, batchIdentifier, key, path, callbacksPtr, hint);
    }

    void RegisterDefaultLoaders();

    UniquePtr<AssetManagerThreadPool> m_threadPool;

    Array<Handle<AssetCollector>> m_assetCollectors;
    WeakHandle<AssetCollector> m_baseAssetCollector;
    mutable Mutex m_assetCollectorsMutex;

    Array<AssetLoaderDefinition> m_loaders;
    TypeMap<ProcessAssetFunctorFactory> m_functorFactories;

    Array<AssetBatch*> m_pendingBatches;
    Mutex m_pendingBatchesMutex;
    AtomicVar<uint32> m_numPendingBatches;
    Array<AssetBatch*> m_completedBatches;
};

} // namespace Hyperion
