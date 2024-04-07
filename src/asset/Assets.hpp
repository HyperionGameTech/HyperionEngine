#ifndef HYPERION_V2_ASSETS_H
#define HYPERION_V2_ASSETS_H

#include <asset/AssetBatch.hpp>
#include <asset/AssetLoader.hpp>
#include <asset/AssetCache.hpp>
#include <util/fs/FsUtil.hpp>
#include <scene/Node.hpp>

#include <core/Core.hpp>
#include <core/Containers.hpp>
#include <util/Defines.hpp>
#include <Threads.hpp>
#include <Constants.hpp>
#include <core/ObjectPool.hpp>

#include <TaskSystem.hpp>

#include <string>
#include <unordered_map>
#include <thread>
#include <algorithm>
#include <type_traits>
#include <asset/BufferedByteReader.hpp>

namespace hyperion::v2 {

class AssetCache;

using AssetLoadFlags = uint32;

enum AssetLoadFlagBits : AssetLoadFlags
{
    ASSET_LOAD_FLAGS_NONE = 0x0,
    ASSET_LOAD_FLAGS_CACHE_READ = 0x1,
    ASSET_LOAD_FLAGS_CACHE_WRITE = 0x2
};

struct AssetLoaderDefinition
{
    const StringView            name;
    TypeID                      loader_type_id;
    TypeID                      result_type_id;
    FlatSet<String>             extensions;
    UniquePtr<AssetLoaderBase>  loader;

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool HandlesResultType(TypeID type_id) const
    {
        return result_type_id == type_id;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsWildcardExtensionLoader() const
    {
        return extensions.Empty() || extensions.Contains("*");
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool HandlesExtension(const String &filepath) const
    {
        return extensions.FindIf([&filepath](const String &extension)
        {
            return filepath.EndsWith(extension);
        }) != extensions.End();
    }
};

class AssetManager
{
    friend struct AssetBatch;
    friend class AssetLoader;

public:
    using ProcessAssetFunctorFactory = std::add_pointer_t<UniquePtr<ProcessAssetFunctorBase>(const String & /* key */, const String & /* path */, AssetBatchCallbacks * /* callbacks */)>;

    static const bool asset_cache_enabled;

    AssetManager();
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

        // Threads::AssertOnThread(THREAD_GAME);

        const FixedArray<String, sizeof...(formats)> format_strings {
            String(formats)...
        };

        static const auto loader_type_name = TypeName<Loader>();

        DebugLog(LogType::Debug, "Loader %s handles formats: ", loader_type_name.Data());

        for (const auto &format : format_strings) {
            DebugLog(LogType::Debug, "%s ", format.Data());
        }

        DebugLog(LogType::Debug, "\n");

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
        If any errors occur, an empty result is returned.
        Node -> NodeProxy
        T -> Handle<T> */
    template <class T>
    auto Load(const String &path, AssetLoadFlags flags = ASSET_LOAD_FLAGS_CACHE_READ | ASSET_LOAD_FLAGS_CACHE_WRITE) -> typename AssetLoaderWrapper<NormalizedType<T>>::CastedType
    {
        LoaderResult result;

        auto value = Load<T>(path, result, flags);

        if (result.status != LoaderResult::Status::OK) {
            return AssetLoaderWrapper<NormalizedType<T>>::EmptyResult();
        }

        return value;
    }

    /*! \brief Load a single asset in a synchronous fashion. The resulting object will have a
        type corresponding to the provided template type.
        Node -> NodeProxy
        T -> Handle<T> */
    template <class T>
    auto Load(const String &path, LoaderResult &out_result, AssetLoadFlags flags = ASSET_LOAD_FLAGS_CACHE_READ | ASSET_LOAD_FLAGS_CACHE_WRITE) -> typename AssetLoaderWrapper<NormalizedType<T>>::CastedType
    {
        using Normalized = NormalizedType<T>;

        AssetCachePool<Normalized> *cache_pool = m_asset_cache->GetPool<Normalized>();

        if (asset_cache_enabled && (flags & ASSET_LOAD_FLAGS_CACHE_READ) && cache_pool->Has(path)) {
            out_result = { };

            DebugLog(LogType::Info, "%s: Load from cache\n", path.Data());

            return cache_pool->Get(path);
        }

        const AssetLoaderDefinition *loader_definition = GetLoader(path, TypeID::ForType<Normalized>());

        if (!loader_definition) {
            out_result = { LoaderResult::Status::ERR_NO_LOADER, "No registered loader for the given path" };

            return AssetLoaderWrapper<Normalized>::EmptyResult();
        }

        AssetLoaderBase *loader = loader_definition->loader.Get();
        AssertThrow(loader != nullptr);

        const auto result = AssetLoaderWrapper<Normalized>(*loader)
            .Load(*this, path, out_result);

        if (asset_cache_enabled && (flags & ASSET_LOAD_FLAGS_CACHE_WRITE)) {
            cache_pool->Set(path, result);
        }

        return result;
    }

    template <class T>
    auto LoadForBatch(const String &path) -> typename AssetLoaderWrapper<NormalizedType<T>>::ResultType
    {
        return Load<T>(path);
    }

    RC<AssetBatch> CreateBatch()
        { return RC<AssetBatch>::Construct(this); }

    AssetCache *GetAssetCache()
        { return m_asset_cache.Get(); }

    const AssetCache *GetAssetCache() const
        { return m_asset_cache.Get(); }

private:
    UniquePtr<ProcessAssetFunctorBase> CreateProcessAssetFunctor(TypeID loader_type_id, const String &key, const String &path, AssetBatchCallbacks *callbacks_ptr);

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

    const AssetLoaderDefinition *GetLoader(const FilePath &path, TypeID desired_type_id = TypeID::void_type_id);

    void RegisterDefaultLoaders();

    ObjectPool &GetObjectPool();

    UniquePtr<AssetCache>                                           m_asset_cache;

    FilePath                                                        m_base_path;

    Array<AssetLoaderDefinition>                                    m_loaders;
    TypeMap<ProcessAssetFunctorFactory>                             m_functor_factories;
};

} // namespace hyperion::v2

#endif
