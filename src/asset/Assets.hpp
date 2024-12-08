/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ASSETS_HPP
#define HYPERION_ASSETS_HPP

#include <asset/AssetLoader.hpp>
#include <asset/AssetCache.hpp>

#include <util/fs/FsUtil.hpp>

#include <core/Base.hpp>

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

struct ProcessAssetFunctorBase;

template <class T>
struct ProcessAssetFunctor;

using AssetLoadFlags = uint32;

enum AssetLoadFlagBits : AssetLoadFlags
{
    ASSET_LOAD_FLAGS_NONE           = 0x0,
    ASSET_LOAD_FLAGS_CACHE_READ     = 0x1,
    ASSET_LOAD_FLAGS_CACHE_WRITE    = 0x2
};

struct AssetLoaderDefinition
{
    const StringView<StringType::ANSI>  name;
    TypeID                              loader_type_id;
    TypeID                              result_type_id;
    FlatSet<String>                     extensions;
    UniquePtr<AssetLoaderBase>          loader;

    HYP_FORCE_INLINE bool HandlesResultType(TypeID type_id) const
    {
        return result_type_id == type_id;
    }

    HYP_FORCE_INLINE bool HandlesExtension(const String &filepath) const
    {
        return extensions.FindIf([&filepath](const String &extension)
        {
            return filepath.EndsWith(extension);
        }) != extensions.End();
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
    RENAMED = 3
};

HYP_CLASS()
class AssetCollector : public HypObject<AssetCollector>
{
    HYP_OBJECT_BODY(AssetCollector);

public:
    // Necessary for script bindings
    AssetCollector() = default;

    AssetCollector(const FilePath &base_path)
        : m_base_path(base_path)
    {
    }

    HYP_API ~AssetCollector();

    HYP_METHOD()
    HYP_FORCE_INLINE const FilePath &GetBasePath() const
        { return m_base_path; }

    HYP_METHOD()
    HYP_API void NotifyAssetChanged(const FilePath &path, AssetChangeType change_type);

    HYP_API void Init();

    HYP_METHOD(Scriptable)
    bool IsWatching() const;

    HYP_METHOD(Scriptable)
    void StartWatching();

    HYP_METHOD(Scriptable)
    void StopWatching();

    HYP_METHOD(Scriptable)
    void OnAssetChanged(const FilePath &path, AssetChangeType change_type);

private:
    bool IsWatching_Impl() const { return false; }
    void StartWatching_Impl() { }
    void StopWatching_Impl() { }
    void OnAssetChanged_Impl(const FilePath &path, AssetChangeType change_type) { }

    FilePath                                            m_base_path;
};

HYP_CLASS()
class AssetManager : public HypObject<AssetManager>
{
    friend class AssetBatch;
    friend class AssetLoader;

    HYP_OBJECT_BODY(AssetManager);

public:
    using ProcessAssetFunctorFactory = std::add_pointer_t<UniquePtr<ProcessAssetFunctorBase>(const String & /* key */, const String & /* path */, AssetBatchCallbacks * /* callbacks */)>;

    static constexpr bool asset_cache_enabled = false;

    HYP_API static const Handle<AssetManager> &GetInstance();

    HYP_API AssetManager();
    AssetManager(const AssetManager &other)                 = delete;
    AssetManager &operator=(const AssetManager &other)      = delete;
    AssetManager(AssetManager &&other) noexcept             = delete;
    AssetManager &operator=(AssetManager &&other) noexcept  = delete;
    ~AssetManager()                                         = default;

    HYP_METHOD()
    FilePath GetBasePath() const;

    HYP_METHOD()
    void SetBasePath(const FilePath &base_path);

    HYP_METHOD()
    Handle<AssetCollector> GetBaseAssetCollector() const;

    HYP_METHOD()
    void AddAssetCollector(const Handle<AssetCollector> &asset_collector);

    HYP_METHOD()
    void RemoveAssetCollector(const Handle<AssetCollector> &asset_collector);

    const Handle<AssetCollector> &FindAssetCollector(ProcRef<bool, const Handle<AssetCollector> &>) const;

    template <class Loader, class ResultType, class... Formats>
    void Register(Formats &&... formats)
    {
        static_assert(std::is_base_of_v<AssetLoaderBase, Loader>,
            "Loader must be a derived class of AssetLoaderBase!");

        // Threads::AssertOnThread(ThreadName::THREAD_GAME);

        const FixedArray<String, sizeof...(formats)> format_strings {
            String(formats)...
        };

        static const auto loader_type_name = TypeNameWithoutNamespace<Loader>();

        m_loaders.PushBack(AssetLoaderDefinition {
            loader_type_name,
            TypeID::ForType<Loader>(),
            TypeID::ForType<ResultType>(),
            FlatSet<String>(format_strings.Begin(), format_strings.End()),
            UniquePtr<Loader>::Construct()
        });

        m_functor_factories.Set<Loader>([](const String &key, const String &path, AssetBatchCallbacks *callbacks_ptr) -> UniquePtr<ProcessAssetFunctorBase>
        {
            return MakeUnique<ProcessAssetFunctor<ResultType>>(key, path, callbacks_ptr);
        });
    }

    /*! \brief Load a single asset in a synchronous fashion. The resulting object will have a
        type corresponding to the provided template type.
        Node -> NodeProxy
        T -> Handle<T> */
    template <class T>
    HYP_NODISCARD Asset<NormalizedType<T>> Load(const String &path, AssetLoadFlags flags = ASSET_LOAD_FLAGS_CACHE_READ | ASSET_LOAD_FLAGS_CACHE_WRITE)
    {
        using Normalized = NormalizedType<T>;

        // AssetCachePool<Normalized> *cache_pool = m_asset_cache->GetPool<Normalized>();

        // if (asset_cache_enabled && (flags & ASSET_LOAD_FLAGS_CACHE_READ) && cache_pool->Has(path)) {
        //     out_result = { };

        //     return cache_pool->Get(path);
        // }

        const AssetLoaderDefinition *loader_definition = GetLoader(path, TypeID::ForType<Normalized>());

        if (!loader_definition) {
            return Asset<NormalizedType<T>> {
                { LoaderResult::Status::ERR_NO_LOADER, "No registered loader for the given path" }
            };
        }

        AssetLoaderBase *loader = loader_definition->loader.Get();
        AssertThrow(loader != nullptr);

        return Asset<NormalizedType<T>>(loader->Load(*this, path));
    }

    HYP_API RC<AssetBatch> CreateBatch();

    HYP_FORCE_INLINE AssetCache *GetAssetCache() const
        { return m_asset_cache.Get(); }

    void Init();
    void Update(GameCounter::TickUnit delta);

    HYP_FORCE_INLINE HashCode GetHashCode() const
        { HYP_NOT_IMPLEMENTED(); }
    
    Delegate<void, const Handle<AssetCollector> &>  OnAssetCollectorAdded;
    Delegate<void, const Handle<AssetCollector> &>  OnAssetCollectorRemoved;
    Delegate<void, const Handle<AssetCollector> &>  OnBaseAssetCollectorChanged;

private:
    /*! \internal Called from AssetBatch on LoadAsync() */
    HYP_API void AddPendingBatch(const RC<AssetBatch> &batch);

    HYP_API const AssetLoaderDefinition *GetLoader(const FilePath &path, TypeID desired_type_id = TypeID::Void());

    HYP_API UniquePtr<ProcessAssetFunctorBase> CreateProcessAssetFunctor(TypeID loader_type_id, const String &key, const String &path, AssetBatchCallbacks *callbacks_ptr);

    template <class Loader>
    UniquePtr<ProcessAssetFunctorBase> CreateProcessAssetFunctor(const String &key, const String &path, AssetBatchCallbacks *callbacks_ptr)
        { return CreateProcessAssetFunctor(TypeID::ForType<Loader>(), key, path, callbacks_ptr); }

    UniquePtr<ProcessAssetFunctorBase> CreateProcessAssetFunctor(const String &key, const String &path, AssetBatchCallbacks *callbacks_ptr)
    {
        const AssetLoaderDefinition *loader_definition = GetLoader(path);

        if (!loader_definition) {
            DebugLog(LogType::Error, "No registered loader for path: %s\n", path.Data());

            return nullptr;
        }

        return CreateProcessAssetFunctor(loader_definition->loader_type_id, key, path, callbacks_ptr);
    }

    void RegisterDefaultLoaders();

    UniquePtr<AssetCache>               m_asset_cache;

    Array<Handle<AssetCollector>>       m_asset_collectors;
    WeakHandle<AssetCollector>          m_base_asset_collector;
    mutable Mutex                       m_asset_collectors_mutex;

    Array<AssetLoaderDefinition>        m_loaders;
    TypeMap<ProcessAssetFunctorFactory> m_functor_factories;

    Array<RC<AssetBatch>>               m_pending_batches;
    Mutex                               m_pending_batches_mutex;
    AtomicVar<uint32>                   m_num_pending_batches;
    Array<RC<AssetBatch>>               m_completed_batches;
};

} // namespace hyperion

#endif
