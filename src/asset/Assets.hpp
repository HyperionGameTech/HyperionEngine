/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_ASSETS_HPP
#define HYPERION_ASSETS_HPP

#include <asset/AssetBatch.hpp>
#include <asset/AssetLoader.hpp>
#include <asset/AssetCache.hpp>
#include <asset/BufferedByteReader.hpp>

#include <util/fs/FsUtil.hpp>

#include <core/Core.hpp>
#include <core/Defines.hpp>
#include <core/threading/Threads.hpp>
#include <core/threading/TaskSystem.hpp>
#include <core/logging/LoggerFwd.hpp>
#include <core/ObjectPool.hpp>

#include <GameCounter.hpp>

#include <scene/Node.hpp>

#include <Constants.hpp>

#include <algorithm>
#include <type_traits>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

class AssetCache;

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

class AssetManager
{
    friend struct AssetBatch;
    friend class AssetLoader;

public:
    using ProcessAssetFunctorFactory = std::add_pointer_t<UniquePtr<ProcessAssetFunctorBase>(const String & /* key */, const String & /* path */, AssetBatchCallbacks * /* callbacks */)>;

    static constexpr bool asset_cache_enabled = false;

    HYP_API static AssetManager *GetInstance();

    HYP_API AssetManager();
    AssetManager(const AssetManager &other)                 = delete;
    AssetManager &operator=(const AssetManager &other)      = delete;
    AssetManager(AssetManager &&other) noexcept             = delete;
    AssetManager &operator=(AssetManager &&other) noexcept  = delete;
    ~AssetManager()                                         = default;

    const FilePath &GetBasePath() const
        { return m_base_path; }

    void SetBasePath(const FilePath &base_path)
        { m_base_path = base_path; }

    template <class Loader, class ResultType, class ... Formats>
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
            return UniquePtr<ProcessAssetFunctorBase>(new ProcessAssetFunctor<ResultType>(key, path, callbacks_ptr));
        });
    }

    /*! \brief Load a single asset in a synchronous fashion. The resulting object will have a
        type corresponding to the provided template type.
        Node -> NodeProxy
        T -> Handle<T> */
    template <class T>
    HYP_NODISCARD
    LoadedAssetInstance<NormalizedType<T>> Load(const String &path, AssetLoadFlags flags = ASSET_LOAD_FLAGS_CACHE_READ | ASSET_LOAD_FLAGS_CACHE_WRITE)
    {
        using Normalized = NormalizedType<T>;

        // AssetCachePool<Normalized> *cache_pool = m_asset_cache->GetPool<Normalized>();

        // if (asset_cache_enabled && (flags & ASSET_LOAD_FLAGS_CACHE_READ) && cache_pool->Has(path)) {
        //     out_result = { };

        //     return cache_pool->Get(path);
        // }

        const AssetLoaderDefinition *loader_definition = GetLoader(path, TypeID::ForType<Normalized>());

        if (!loader_definition) {
            return LoadedAssetInstance<NormalizedType<T>> {
                { LoaderResult::Status::ERR_NO_LOADER, "No registered loader for the given path" }
            };
        }

        AssetLoaderBase *loader = loader_definition->loader.Get();
        AssertThrow(loader != nullptr);

        return LoadedAssetInstance<NormalizedType<T>>(loader->Load(*this, path));
    }

    HYP_API RC<AssetBatch> CreateBatch();

    HYP_FORCE_INLINE AssetCache *GetAssetCache() const
        { return m_asset_cache.Get(); }

    HYP_API void Update(GameCounter::TickUnit delta);

private:
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

    FilePath                            m_base_path;

    Array<AssetLoaderDefinition>        m_loaders;
    TypeMap<ProcessAssetFunctorFactory> m_functor_factories;

    Array<RC<AssetBatch>>               m_pending_batches;
    Mutex                               m_pending_batches_mutex;
    AtomicVar<uint32>                   m_num_pending_batches;
    Array<RC<AssetBatch>>               m_completed_batches;
};

} // namespace hyperion

#endif
