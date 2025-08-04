/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetLoader.hpp>
#include <asset/AssetCache.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <core/Handle.hpp>

#include <core/object/HypObject.hpp>

#include <core/functional/Delegate.hpp>

#include <core/Defines.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <GameCounter.hpp>

#include <scene/Node.hpp>

#include <Constants.hpp>

#include <algorithm>
#include <type_traits>

namespace hyperion {

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

extern HYP_API const HypClass* GetClass(TypeId typeId);

enum AssetLoadFlagBits : AssetLoadFlags
{
    ASSET_LOAD_FLAGS_NONE = 0x0,
    ASSET_LOAD_FLAGS_CACHE_READ = 0x1,
    ASSET_LOAD_FLAGS_CACHE_WRITE = 0x2
};

HYP_STRUCT()
struct AssetLoaderDefinition
{
    TypeId loaderTypeId;
    TypeId resultTypeId;
    const HypClass* resultHypClass = nullptr;
    FlatSet<String> extensions;
    Handle<AssetLoaderBase> loader;

    HYP_FORCE_INLINE bool HandlesResultType(TypeId typeId) const
    {
        return resultTypeId == typeId
            || (resultHypClass != nullptr && IsA(resultHypClass, GetClass(typeId)));
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
        return extensions.Empty() || extensions.Contains("*");
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
class AssetCollector final : public HypObjectBase
{
    HYP_OBJECT_BODY(AssetCollector);

public:
    // Necessary for script bindings
    AssetCollector() = default;

    AssetCollector(const FilePath& basePath)
        : m_basePath(basePath)
    {
    }

    HYP_API ~AssetCollector();

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
    HYP_API void NotifyAssetChanged(const FilePath& path, AssetChangeType changeType);

    HYP_METHOD(Scriptable)
    bool IsWatching() const;

    HYP_METHOD(Scriptable)
    void StartWatching();

    HYP_METHOD(Scriptable)
    void StopWatching();

    HYP_METHOD(Scriptable)
    void OnAssetChanged(const FilePath& path, AssetChangeType changeType);

private:
    HYP_API void Init() override;

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
class AssetManager final : public HypObjectBase
{
    friend class AssetBatch;
    friend class AssetLoaderBase;

    HYP_OBJECT_BODY(AssetManager);

public:
    using ProcessAssetFunctorFactory = std::add_pointer_t<UniquePtr<ProcessAssetFunctorBase>(const String& /* key */, const String& /* path */, AssetBatchCallbacks* /* callbacks */)>;

    static constexpr bool assetCacheEnabled = false;

    HYP_METHOD()
    HYP_API static const Handle<AssetManager>& GetInstance();

    HYP_API AssetManager();
    AssetManager(const AssetManager& other) = delete;
    AssetManager& operator=(const AssetManager& other) = delete;
    AssetManager(AssetManager&& other) noexcept = delete;
    AssetManager& operator=(AssetManager&& other) noexcept = delete;
    HYP_API ~AssetManager();

    HYP_API TaskThreadPool* GetThreadPool() const;

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

        // Threads::AssertOnThread(g_gameThread);

        const FixedArray<String, sizeof...(formats)> formatStrings {
            String(formats)...
        };

        AssetLoaderDefinition& assetLoaderDefinition = m_loaders.EmplaceBack();
        assetLoaderDefinition.loaderTypeId = TypeId::ForType<Loader>();
        assetLoaderDefinition.resultTypeId = TypeId::ForType<ResultType>();
        assetLoaderDefinition.resultHypClass = GetClass(TypeId::ForType<ResultType>());
        assetLoaderDefinition.extensions = FlatSet<String>(formatStrings.Begin(), formatStrings.End());
        assetLoaderDefinition.loader = CreateObject<Loader>();

        m_functorFactories.Set<Loader>([](const String& key, const String& path, AssetBatchCallbacks* callbacksPtr) -> UniquePtr<ProcessAssetFunctorBase>
            {
                return MakeUnique<ProcessAssetFunctor<ResultType>>(key, path, callbacksPtr);
            });
    }

    /*! \brief Load a single asset synchronously
     *  \param typeId The TypeId of asset to load
     *  \param path The path to the asset
     *  \param flags Flags to control the loading process
     *  \return The result of the load operation */
    HYP_NODISCARD AssetLoadResult Load(const TypeId& typeId, const String& path, AssetLoadFlags flags = ASSET_LOAD_FLAGS_CACHE_READ | ASSET_LOAD_FLAGS_CACHE_WRITE)
    {
        const AssetLoaderDefinition* loaderDefinition = GetLoaderDefinition(path, typeId);

        if (!loaderDefinition)
        {
            return HYP_MAKE_ERROR(AssetLoadError, "No registered loader for the given path", AssetLoadError::ERR_NO_LOADER);
        }

        const Handle<AssetLoaderBase>& loader = loaderDefinition->loader;
        Assert(loader.IsValid());

        return AssetLoadResult(loader->Load(*this, path));
    }

    /*! \brief Load a single asset synchronously
     *  \tparam T The type of asset to load
     *  \param path The path to the asset
     *  \param flags Flags to control the loading process
     *  \return The result of the load operation */
    template <class T>
    HYP_NODISCARD TAssetLoadResult<T> Load(const String& path, AssetLoadFlags flags = ASSET_LOAD_FLAGS_CACHE_READ | ASSET_LOAD_FLAGS_CACHE_WRITE)
    {
        const AssetLoaderDefinition* loaderDefinition = GetLoaderDefinition(path, TypeId::ForType<T>());

        if (!loaderDefinition)
        {
            return HYP_MAKE_ERROR(AssetLoadError, "No registered loader for the given path", AssetLoadError::ERR_NO_LOADER);
        }

        AssetLoaderBase* loader = loaderDefinition->loader.Get();
        Assert(loader != nullptr);

        return TAssetLoadResult<T>(loader->Load(*this, path));
    }

    HYP_API const AssetLoaderDefinition* GetLoaderDefinition(const FilePath& path, TypeId desiredTypeId = TypeId::Void());

    HYP_API RC<AssetBatch> CreateBatch();

    HYP_FORCE_INLINE const Handle<AssetRegistry>& GetAssetRegistry() const
    {
        return m_assetRegistry;
    }

    HYP_FORCE_INLINE AssetCache* GetAssetCache() const
    {
        return m_assetCache.Get();
    }

    void Update(float delta);

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HYP_NOT_IMPLEMENTED();
    }

    Delegate<void, const Handle<AssetCollector>&> OnAssetCollectorAdded;
    Delegate<void, const Handle<AssetCollector>&> OnAssetCollectorRemoved;
    Delegate<void, const Handle<AssetCollector>&> OnBaseAssetCollectorChanged;

private:
    void Init() override;

    /*! \internal Called from AssetBatch on LoadAsync() */
    HYP_API void AddPendingBatch(const RC<AssetBatch>& batch);

    HYP_API UniquePtr<ProcessAssetFunctorBase> CreateProcessAssetFunctor(TypeId loaderTypeId, const String& key, const String& path, AssetBatchCallbacks* callbacksPtr);

    template <class Loader>
    UniquePtr<ProcessAssetFunctorBase> CreateProcessAssetFunctor(const String& key, const String& path, AssetBatchCallbacks* callbacksPtr)
    {
        return CreateProcessAssetFunctor(TypeId::ForType<Loader>(), key, path, callbacksPtr);
    }

    UniquePtr<ProcessAssetFunctorBase> CreateProcessAssetFunctor(const String& key, const String& path, AssetBatchCallbacks* callbacksPtr)
    {
        const AssetLoaderDefinition* loaderDefinition = GetLoaderDefinition(path);

        if (!loaderDefinition)
        {
            DebugLog(LogType::Error, "No registered loader for path: %s\n", path.Data());

            return nullptr;
        }

        return CreateProcessAssetFunctor(loaderDefinition->loaderTypeId, key, path, callbacksPtr);
    }

    void RegisterDefaultLoaders();

    Handle<AssetRegistry> m_assetRegistry;

    UniquePtr<AssetCache> m_assetCache;

    UniquePtr<AssetManagerThreadPool> m_threadPool;

    Array<Handle<AssetCollector>> m_assetCollectors;
    WeakHandle<AssetCollector> m_baseAssetCollector;
    mutable Mutex m_assetCollectorsMutex;

    Array<AssetLoaderDefinition> m_loaders;
    TypeMap<ProcessAssetFunctorFactory> m_functorFactories;

    Array<RC<AssetBatch>> m_pendingBatches;
    Mutex m_pendingBatchesMutex;
    AtomicVar<uint32> m_numPendingBatches;
    Array<RC<AssetBatch>> m_completedBatches;
};

} // namespace hyperion

