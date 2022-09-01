#ifndef HYPERION_V2_ASSETS_H
#define HYPERION_V2_ASSETS_H

#include "model_loaders/OBJModelLoader.hpp"
#include "material_loaders/MTLMaterialLoader.hpp"
#include "model_loaders/OgreXMLModelLoader.hpp"
#include "skeleton_loaders/OgreXMLSkeletonLoader.hpp"
#include "texture_loaders/TextureLoader.hpp"
#include "audio_loaders/WAVAudioLoader.hpp"
#include "script_loaders/HypscriptLoader.hpp"
#include "../scene/Node.hpp"

#include <core/lib/StaticMap.hpp>
#include <util/fs/FsUtil.hpp>
#include <util/Defines.hpp>
#include <Constants.hpp>

#include <string>
#include <unordered_map>
#include <thread>
#include <algorithm>

namespace hyperion::v2 {

class Engine;

class QueuedAsset {
public:
    enum Priority {
        LOWEST,
        LOW,
        MEDIUM,
        HIGH,
        HIGHEST,
    };

    QueuedAsset(const std::string &filename, const Priority priority)
    { 
        m_filename = filename;
        m_priority = priority;


    };

    void SetPriority(const Priority priority) { m_priority = priority; }
    float GetNiceness() { 
        float niceness = (float)m_data_size / 100.0f;
        uint8_t pr = Priority::HIGHEST - m_priority;
        /* The lower the niceness, the higher the priority! */
        return niceness + pr;
    }

private:
    Priority m_priority;
    std::string m_filename;
    uint64_t m_data_size = 0;
};

class AssetPool {
public:
    AssetPool() { }
    ~AssetPool() { }
private:

    std::vector<QueuedAsset> enqueued_assets;
    std::thread pool_thread;
};

class Assets {
    template <class T>
    struct ConstructedAsset {
        std::unique_ptr<T> object;
        LoaderResult       result;
    };

    struct HandleAssetFunctorBase {
        std::string filepath;

        LoaderFormat GetResourceFormat() const
        {
            constexpr StaticMap<const char *, LoaderFormat, 15> extensions {
                std::make_pair(".obj",          LoaderFormat::OBJ_MODEL),
                std::make_pair(".mtl",          LoaderFormat::MTL_MATERIAL_LIBRARY),
                std::make_pair(".mesh.xml",     LoaderFormat::OGRE_XML_MODEL),
                std::make_pair(".skeleton.xml", LoaderFormat::OGRE_XML_SKELETON),
                std::make_pair(".png",          LoaderFormat::TEXTURE_2D),
                std::make_pair(".jpg",          LoaderFormat::TEXTURE_2D),
                std::make_pair(".jpeg",         LoaderFormat::TEXTURE_2D),
                std::make_pair(".tga",          LoaderFormat::TEXTURE_2D),
                std::make_pair(".bmp",          LoaderFormat::TEXTURE_2D),
                std::make_pair(".psd",          LoaderFormat::TEXTURE_2D),
                std::make_pair(".gif",          LoaderFormat::TEXTURE_2D),
                std::make_pair(".hdr",          LoaderFormat::TEXTURE_2D),
                std::make_pair(".tif",          LoaderFormat::TEXTURE_2D),
                std::make_pair(".wav",          LoaderFormat::WAV_AUDIO),
                std::make_pair(".hypscript",    LoaderFormat::SCRIPT_HYPSCRIPT)
            };

            std::string path_lower(filepath);
            std::transform(path_lower.begin(), path_lower.end(), path_lower.begin(), [](char ch){ return std::tolower(ch); });

            for (const auto &it : extensions.pairs) {
                if (StringUtil::EndsWith(path_lower, it.first)) {
                    return it.second;
                }
            }

            return LoaderFormat::NONE;
        }

        template <class T, LoaderFormat Format>
        auto GetLoader() const -> typename LoaderObject<T, Format>::Loader
            { return typename LoaderObject<T, Format>::Loader(); }
        
        template <class Loader>
        auto LoadResource(Engine *engine, const Loader &loader) -> ConstructedAsset<typename Loader::FinalType>
        {
            auto [result, object] = loader.Instance().Load(LoaderState{
                .filepath = filepath,
                .stream = {filepath},
                .engine = engine
            });

            if (result) {
                /* TODO: Cache here */

                DebugLog(LogType::Info, "Constructing loaded asset %s...\n", filepath.c_str());

                return {
                    .object = loader.Build(engine, object),
                    .result = result
                };
            }

            return {
                .object = nullptr,
                .result = result
            };
        }
    };

    template <class T>
    struct HandleAssetFunctor : HandleAssetFunctorBase {
        std::unique_ptr<T> operator()(Engine *engine)
        {
            static_assert(resolution_failure<T>, "No handler defined for the given type");

            return nullptr;
        }
    };

    template <>
    struct HandleAssetFunctor<Node> : HandleAssetFunctorBase {
        ConstructedAsset<Node> operator()(Engine *engine)
        {
            switch (GetResourceFormat()) {
            case LoaderFormat::OBJ_MODEL:
                return LoadResource(engine, GetLoader<Node, LoaderFormat::OBJ_MODEL>());
            case LoaderFormat::OGRE_XML_MODEL:
                return LoadResource(engine, GetLoader<Node, LoaderFormat::OGRE_XML_MODEL>());
            default:
                return {.result = {LoaderResult::Status::ERR, "Unexpected file format"}};
            }
        }
    };

    template <>
    struct HandleAssetFunctor<MaterialGroup> : HandleAssetFunctorBase {
        ConstructedAsset<MaterialGroup> operator()(Engine *engine)
        {
            return LoadResource(engine, GetLoader<MaterialGroup, LoaderFormat::MTL_MATERIAL_LIBRARY>());
        }
    };

    template <>
    struct HandleAssetFunctor<Skeleton> : HandleAssetFunctorBase {
        ConstructedAsset<Skeleton> operator()(Engine *engine)
        {
            switch (GetResourceFormat()) {
            case LoaderFormat::OGRE_XML_SKELETON:
                return LoadResource(engine, GetLoader<Skeleton, LoaderFormat::OGRE_XML_SKELETON>());
            default:
                return {.result = {LoaderResult::Status::ERR, "Unexpected file format"}};
            }
        }
    };

    template <>
    struct HandleAssetFunctor<Texture> : HandleAssetFunctorBase {
        ConstructedAsset<Texture> operator()(Engine *engine)
        {
            return LoadResource(engine, GetLoader<Texture, LoaderFormat::TEXTURE_2D>());
        }
    };

    template <>
    struct HandleAssetFunctor<AudioSource> : HandleAssetFunctorBase {
        ConstructedAsset<AudioSource> operator()(Engine *engine)
        {
            switch (GetResourceFormat()) {
            case LoaderFormat::WAV_AUDIO:
                return LoadResource(engine, GetLoader<AudioSource, LoaderFormat::WAV_AUDIO>());
            default:
                return {.result = {LoaderResult::Status::ERR, "Unexpected file format"}};
            }
        }
    };

    template <>
    struct HandleAssetFunctor<Script> : HandleAssetFunctorBase {
        ConstructedAsset<Script> operator()(Engine *engine)
        {
            switch (GetResourceFormat()) {
            case LoaderFormat::SCRIPT_HYPSCRIPT:
                return LoadResource(engine, GetLoader<Script, LoaderFormat::SCRIPT_HYPSCRIPT>());
            default:
                return {.result = {LoaderResult::Status::ERR, "Unexpected file format"}};
            }
        }
    };

    static inline auto GetTryFilepaths(const std::string &filepath, const std::string &original_filepath)
    {
        const auto current_path = FileSystem::CurrentPath();

        std::array paths{
            filepath,
            FileSystem::RelativePath(filepath, current_path),
            FileSystem::RelativePath(original_filepath, current_path)
        };

        return paths;
    }

    auto GetRebasedFilepath(std::string filepath)
    {
        filepath = FileSystem::RelativePath(filepath, FileSystem::CurrentPath());

        if (!m_base_path.empty()) {
            return FileSystem::Join(
                m_base_path,
                filepath
            );
        }

        return filepath;
    }

    static inline void DisplayAssetLoadError(const std::string &filepath, const std::string &error_message)
    {
        DebugLog(
            LogType::Warn,
            "[%s]: The asset could not be loaded and will be returned as null.\n\t"
            "Any usages or indirection may result in the application crashing!\n"
            "The message was: %s\n",
            filepath.c_str(),
            error_message.c_str()
        );
        
//#if HYP_DEBUG_MODE
        // AssertThrowMsg(
        //     false,
        //    "[%s]: The asset could not be loaded and will be returned as null.\n\t"
        //    "Any usages or indirection may result in the application crashing!\n"
        //    "The message was: %s\n",
        //    filepath.c_str(),
        //    error_message.c_str()
        // );
//#endif
    }

    template <class In, class Out>
    void ExtractConstructedAsset(In &&constructed_asset, Out &out)
    {
        out = std::move(constructed_asset.object);
    }

    template <class InContainer, class OutContainer>
    void ExtractConstructedAssets(InContainer &&constructed_assets, OutContainer &out)
    {
        for (size_t i = 0; i < constructed_assets.size(); i++) {
            ExtractConstructedAsset(std::move(constructed_assets[i]), out[i]);
        }
    }

    template <class Type, class ...Args>
    std::unique_ptr<Type> LoadSync(const std::string &path)
    {
        const std::string original_filepath = path;
        const std::string filepath          = GetRebasedFilepath(path);
        const auto try_filepaths            = GetTryFilepaths(filepath, original_filepath);

        ConstructedAsset<Type> result{};
        
        for (auto &try_filepath : try_filepaths) {
            DebugLog(LogType::Info, "Loading asset %s...\n", try_filepath.c_str());

            auto functor = HandleAssetFunctor<Type>{};
            functor.filepath = try_filepath;
            result = functor(m_engine);

            if (result.result) {
                AssertThrowMsg(
                    result.object != nullptr,
                    "Loader failure -- the loader returned OK status but the constructed object was nullptr!"
                );

                break;
            }

            if (result.result.status != LoaderResult::Status::ERR_NOT_FOUND) {
                // error result returned
                break;
            }

            // status == ERR_NOT_FOUND; continue trying
        }

        if (!result.result) {
            DisplayAssetLoadError(filepath, result.result.message);
        }
        
        std::unique_ptr<Type> extracted_object;
        ExtractConstructedAsset(std::move(result), extracted_object);

        return extracted_object;
    }

    template <class Type, class ...Args>
    auto LoadAsync(Args &&... paths) -> std::array<std::unique_ptr<Type>, sizeof...(paths)>
    {
        const std::array<std::string,      sizeof...(paths)> original_filepaths{paths...};
        std::array<std::string,            sizeof...(paths)> filepaths{paths...};
        std::array<ConstructedAsset<Type>, sizeof...(paths)> results{};
        
        for (auto &path : filepaths) {
            path = GetRebasedFilepath(path);
        }
        
        std::vector<std::thread> threads;
        threads.reserve(results.size());
        
        for (size_t i = 0; i < filepaths.size(); i++) {
            threads.emplace_back([index = i, engine = m_engine, &filepaths, &original_filepaths, &results] {
                const auto try_filepaths = GetTryFilepaths(filepaths[index], original_filepaths[index]);

                for (auto &try_filepath : try_filepaths) {
                    DebugLog(LogType::Info, "Loading asset %s...\n", try_filepath.c_str());

                    auto functor = HandleAssetFunctor<Type>{};
                    functor.filepath = try_filepath;
                    results[index] = functor(engine);

                    if (results[index].result) {
                        AssertThrowMsg(
                            results[index].object != nullptr,
                            "Loader failure -- the loader returned OK status but the constructed object was nullptr!"
                        );

                        break;
                    }

                    if (results[index].result.status != LoaderResult::Status::ERR_NOT_FOUND) {
                        // error result returned
                        break;
                    }

                    // status == ERR_NOT_FOUND; continue trying
                }

                if (!results[index].result) {
                    DisplayAssetLoadError(filepaths[index], results[index].result.message);
                }
            });
        }

        for (auto &thread : threads) {
            thread.join();
        }
        
        std::array<std::unique_ptr<Type>, sizeof...(paths)> extracted_objects;
        ExtractConstructedAssets(std::move(results), extracted_objects);

        return extracted_objects;
    }

    template <class Type>
    auto LoadAsyncVector(std::vector<std::string> filepaths) -> std::vector<std::unique_ptr<Type>>
    {
        const std::vector<std::string> original_filepaths(filepaths);
        
        for (auto &path : filepaths) {
            path = GetRebasedFilepath(path);
        }

        std::vector<ConstructedAsset<Type>> results(filepaths.size());

        std::vector<std::thread> threads;
        threads.reserve(filepaths.size());
        
        for (size_t i = 0; i < filepaths.size(); i++) {
            threads.emplace_back([index = i, engine = m_engine, &filepaths, &original_filepaths, &results] {
                DebugLog(LogType::Info, "Loading asset %s...\n", filepaths[index].c_str());
                
                for (auto &try_filepath : GetTryFilepaths(filepaths[index], original_filepaths[index])) {
                    auto functor = HandleAssetFunctor<Type>{};
                    functor.filepath = try_filepath;
                    results[index] = functor(engine);

                    if (results[index].result) {
                        break;
                    }
                }

                if (!results[index].result) {
                    DisplayAssetLoadError(filepaths[index], results[index].result.message);
                }
            });
        }

        for (auto &thread : threads) {
            thread.join();
        }
        
        std::vector<std::unique_ptr<Type>> extracted_objects(results.size());
        ExtractConstructedAssets(std::move(results), extracted_objects);

        return std::move(extracted_objects);
    }

public:
    Assets(Engine *engine);
    Assets(const Assets &other) = delete;
    Assets &operator=(const Assets &other) = delete;
    ~Assets();

    /*! \brief Set the path to prefix all assets with.
     * @param base_path A string equal to the base path
     */
    void SetBasePath(const std::string &base_path) { m_base_path = base_path; }
    const std::string &GetBasePath() const         { return m_base_path; }

    /*! \brief Load a single asset from the given path. If no asset could be loaded, nullptr is returned.
     * @param filepath The path of the asset
     * @returns A unique pointer to Type
     */
    template <class Type>
    auto Load(const std::string &filepath) -> std::unique_ptr<Type>
    {
        return std::move(LoadSync<Type>(filepath));
    }

    /*! \brief Loads a collection of assets asynchronously from one another (the function returns when all have
     *   been loaded). For any assets that could not be loaded, nullptr will be in the array in place of the
     *   asset.
     * @param filepath The path of the first asset to be loaded
     * @param other_paths Argument pack of assets 1..n to be loaded
     * @returns An array of unique pointers to Type
     */
    template <class Type, class ...Args>
    auto Load(const std::string &filepath, Args &&... other_paths)
    {
        return std::move(LoadAsync<Type>(filepath, std::move(other_paths)...));
    }

    /*! \brief Loads a collection of assets asynchronously from one another (the function returns when all have
     *   been loaded). For any assets that could not be loaded, nullptr will be in the vector in place of the
     *   asset.
     * @param filepaths A vector of filepaths of files to load
     * @returns A vector of unique pointers to Type
     */
    template <class Type>
    auto Load(const std::vector<std::string> &filepaths)
    {
        return LoadAsyncVector<Type>(filepaths);
    }

private:
    Engine     *m_engine;
    std::string m_base_path;
};

} // namespace hyperion::v2

#endif
