#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include "asset_loader.h"

#include <memory>
#include <deque>
#include <string>
#include <map>
#include <algorithm>

namespace hyperion {
class AssetManager {
public:
    static AssetManager *GetInstance();

    struct ErrorList {
        static const AssetLoader::Result no_error;
        static const int max_size;

        inline int Size() const { return results.size(); }

        inline const AssetLoader::Result &Last() const
        {
            if (results.empty()) {
                return no_error;
            }

            return results.back();
        }

        inline void Add(const AssetLoader::Result &result)
        {
            if (result) {
                return;
            }

            if (results.size() == max_size) {
                results.pop_front();
            }

            results.emplace_back(result);
        }

    private:
        std::deque<AssetLoader::Result> results;
    };

    AssetManager();

    void SetRootDir(const std::string &path);
    std::string GetRootDir();
    std::shared_ptr<Loadable> LoadFromFile(const std::string &path, bool use_caching = true);
    const std::unique_ptr<AssetLoader> &GetLoader(const std::string &path);

    inline const ErrorList &GetErrors() const { return m_error_list; }

    template <typename T>
    const std::shared_ptr<T> LoadFromFile(const std::string &path, bool use_caching = true)
    {
        static_assert(std::is_base_of<Loadable, T>::value,
            "Must be a derived class of Loadable");
        return std::dynamic_pointer_cast<T>(LoadFromFile(path, use_caching));
    }
    
    template <typename T>
    void RegisterLoader(const std::string &extension)
    {
        static_assert(std::is_base_of<AssetLoader, T>::value, 
            "Must be a derived class of AssetLoader");
        std::string ext_lower;
        ext_lower.resize(extension.length());
        std::transform(extension.begin(), extension.end(), 
            ext_lower.begin(), ::tolower);
        loaders[ext_lower] = std::make_unique<T>();
    }

private:
    static AssetManager *instance;

    ErrorList m_error_list;

    std::string root_path = "./";

    std::map<std::string, std::unique_ptr<AssetLoader>> loaders;
    std::map<std::string, std::shared_ptr<Loadable>> loaded_assets;
};
}

#endif
