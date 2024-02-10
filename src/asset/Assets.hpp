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

        for (auto &str : format_strings) {
            m_loaders[str] = {
                TypeID::ForType<Loader>(),
                UniquePtr<Loader>::Construct()
            };
        }

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

        AssetLoaderBase *loader = GetLoader(path);

        if (!loader) {
            DebugLog(LogType::Warn, "No registered loader for path: %s\n", path.Data());

            out_result = { LoaderResult::Status::ERR_NO_LOADER, "No registered loader for the path" };

            return AssetLoaderWrapper<Normalized>::EmptyResult();
        }

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

    /*! \brief Load multple objects of the same type. All assets will be loaded asynchronously
        from one another, but the method is still synchronous, so there is no need to wait on anything
        after calling this method. All assets will be returned in the resulting array in the order that their
        respective filepaths were provided in. */
    template <class T, class ... Paths>
    auto LoadMany(const String &first_path, Paths &&... paths) -> FixedArray<typename AssetLoaderWrapper<NormalizedType<T>>::CastedType, sizeof...(paths) + 1>
    {
        FixedArray<typename AssetLoaderWrapper<NormalizedType<T>>::CastedType, sizeof...(paths) + 1> results_array;
        std::vector<String> paths_array { first_path, std::forward<Paths>(paths)... };
        auto batch = CreateBatch();

        uint path_index = 0;

        for (const auto &path : paths_array) {
            batch->Add(String::ToString(path_index++), path);
        }

        batch->LoadAsync();

        auto results = batch->AwaitResults();

        SizeType index = 0u;
        AssertThrow(results.Size() == results_array.Size());

        for (auto &it : results) {
            results_array[index++] = it.second.Get<T>();
        }

        return results_array;
    }

    RC<AssetBatch> CreateBatch()
        { return RC<AssetBatch>::Construct(this); }

    AssetCache *GetAssetCache()
        { return m_asset_cache.Get(); }

    const AssetCache *GetAssetCache() const
        { return m_asset_cache.Get(); }

    template <class Loader>
    UniquePtr<ProcessAssetFunctorBase> CreateProcessAssetFunctor(const String &key, const String &path, AssetBatchCallbacks *callbacks_ptr)
    {
        auto it = m_functor_factories.Find<Loader>();
        AssertThrow(it != m_functor_factories.End());

        return it->second(key, path, callbacks_ptr);
    }

    UniquePtr<ProcessAssetFunctorBase> CreateProcessAssetFunctor(TypeID loader_type_id, const String &key, const String &path, AssetBatchCallbacks *callbacks_ptr)
    {
        auto it = m_functor_factories.Find(loader_type_id);
        AssertThrow(it != m_functor_factories.End());

        return it->second(key, path, callbacks_ptr);
    }

    UniquePtr<ProcessAssetFunctorBase> CreateProcessAssetFunctor(const String &key, const String &path, AssetBatchCallbacks *callbacks_ptr)
    {
        const String extension(StringUtil::ToLower(StringUtil::GetExtension(path.Data())).c_str());

        if (extension.Empty()) {
            DebugLog(LogType::Warn, "No extension found for path: %s\n", path.Data());

            return nullptr;
        }

        TypeID loader_type_id = TypeID::ForType<void>();

        const auto it = m_loaders.Find(extension);

        if (it != m_loaders.End()) {
            loader_type_id = it->second.first;
        } else {
            for (auto &loader_it : m_loaders) {
                if (String(StringUtil::ToLower(path.Data()).c_str()).EndsWith(loader_it.first)) {
                    loader_type_id = loader_it.second.first;
                    break;
                }
            }
        }

        // no loader found
        if (loader_type_id == TypeID::ForType<void>()) {
            DebugLog(LogType::Warn, "No loader found for path: %s\n", path.Data());

            return nullptr;
        }

        // use the loader type id to create the functor
        return CreateProcessAssetFunctor(loader_type_id, key, path, callbacks_ptr);
    }

private:
    AssetLoaderBase *GetLoader(const FilePath &path);

    void RegisterDefaultLoaders();

    ObjectPool &GetObjectPool();

    UniquePtr<AssetCache>                                       m_asset_cache;

    FilePath                                                    m_base_path;

    FlatMap<String, Pair<TypeID, UniquePtr<AssetLoaderBase>>>   m_loaders;
    TypeMap<ProcessAssetFunctorFactory>                         m_functor_factories;
};

} // namespace hyperion::v2

#endif
