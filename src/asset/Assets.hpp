#ifndef HYPERION_V2_ASSETS_H
#define HYPERION_V2_ASSETS_H

#include <asset/AssetBatch.hpp>
#include <asset/AssetLoader.hpp>
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

class AssetManager
{
    friend struct AssetBatch;
    friend class AssetLoader;

public:
    AssetManager();
    AssetManager(const AssetManager &other) = delete;
    AssetManager &operator=(const AssetManager &other) = delete;
    ~AssetManager() = default;

    const FilePath &GetBasePath() const
        { return m_base_path; }

    void SetBasePath(const FilePath &base_path)
        { m_base_path = base_path; }

    template <class Loader, class ... Formats>
    void Register(Formats &&... formats)
    {
        static_assert(std::is_base_of_v<AssetLoaderBase, Loader>,
            "Loader must be a derived class of AssetLoaderBase!");

        // Threads::AssertOnThread(THREAD_GAME);

        const FixedArray<String, sizeof...(formats)> format_strings {
            String(formats)...
        };

        for (auto &str : format_strings) {
            m_loaders[str] = UniquePtr<Loader>::Construct();
        }
    }
    

    /*! \brief Load a single asset in a synchronous fashion. The resulting object will have a
        type corresponding to the provided template type.
        If any errors occur, an empty result is returned.
        Node -> NodeProxy
        T -> Handle<T> */
    template <class T>
    auto Load(const String &path) -> typename AssetLoaderWrapper<NormalizedType<T>>::CastedType
    {
        LoaderResult result;

        auto value = Load<T>(path, result);

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
    auto Load(const String &path, LoaderResult &out_result) -> typename AssetLoaderWrapper<NormalizedType<T>>::CastedType
    {
        const String extension(StringUtil::ToLower(StringUtil::GetExtension(path.Data())).c_str());

        if (extension.Empty()) {
            out_result = { LoaderResult::Status::ERR_NO_LOADER, "File has no extension; cannot determine loader to use" };

            return AssetLoaderWrapper<NormalizedType<T>>::EmptyResult();
        }

        AssetLoaderBase *loader = nullptr;

        { // find loader for the requested type
            const auto it = m_loaders.Find(extension);

            if (it != m_loaders.End()) {
                loader = it->second.Get();
            } else {
                for (auto &loader_it : m_loaders) {
                    if (String(StringUtil::ToLower(path.Data()).c_str()).EndsWith(loader_it.first)) {
                        loader = loader_it.second.Get();
                        break;
                    }
                }
            }
        }

        if (loader == nullptr) {
            out_result = { LoaderResult::Status::ERR_NO_LOADER, "No registered loader for the given type" };

            return AssetLoaderWrapper<NormalizedType<T>>::EmptyResult();
        }

        return AssetLoaderWrapper<NormalizedType<T>>(*loader)
            .Load(*this, path, out_result);
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
        AssetBatch batch = CreateBatch();

        UInt path_index = 0;

        for (const auto &path : paths_array) {
            batch.Add<T>(String::ToString(path_index++), path);
        }

        batch.LoadAsync();

        auto results = batch.AwaitResults();

        SizeType index = 0u;
        AssertThrow(results.Size() == results_array.Size());

        for (auto &it : results) {
            results_array[index++] = it.second.Get<T>();
        }

        return results_array;
    }

    AssetBatch CreateBatch()
    {
        return AssetBatch(this);
    }

private:
    void RegisterDefaultLoaders();

    ObjectPool &GetObjectPool();

    FilePath m_base_path;
    FlatMap<String, UniquePtr<AssetLoaderBase>> m_loaders;
};

class AssetLoader : public AssetLoaderBase
{
protected:
    static inline auto GetTryFilepaths(const FilePath &filepath, const FilePath &original_filepath)
    {
        const FilePath current_path = FilePath::Current();

        FixedArray<FilePath, 3> paths {
            FilePath::Relative(original_filepath, current_path),
            FilePath::Relative(filepath, current_path),
            filepath
        };

        return paths;
    }

public:
    virtual ~AssetLoader() = default;

    virtual LoadedAsset Load(AssetManager &asset_manager, const String &path) const override final
    {
        LoadedAsset asset;
        asset.result = LoaderResult {
            LoaderResult::Status::ERR_NOT_FOUND,
            "File could not be found"
        };

        const FilePath original_filepath(path);
        const FilePath filepath = GetRebasedFilepath(asset_manager, original_filepath);
        const auto paths = GetTryFilepaths(filepath, original_filepath);

        for (const auto &path : paths) {
            BufferedReader<HYP_READER_DEFAULT_BUFFER_SIZE> reader;

            if (!path.Open(reader)) {
                // could not open... try next path
                DebugLog(
                    LogType::Warn,
                    "Could not open file at path : %s, trying next path...\n",
                    path.Data()
                );

                continue;
            }

            LoaderState state {
                &asset_manager,
                path.Data(),
                std::move(reader)
            };

            asset = LoadAsset(state);

            if (asset.result) {
                break; // stop searching when value result is found
            }
        }

        return asset;
    }

protected:
    virtual LoadedAsset LoadAsset(LoaderState &state) const = 0;

    FilePath GetRebasedFilepath(const AssetManager &asset_manager, const FilePath &filepath) const
    {
        const FilePath relative_filepath = FilePath::Relative(filepath, FilePath::Current());

        if (asset_manager.GetBasePath().Any()) {
            return FilePath::Join(
                asset_manager.GetBasePath().Data(),
                relative_filepath.Data()
            );
        }

        return relative_filepath;
    }
};

} // namespace hyperion::v2

#endif
