#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include "asset_loader.h"

#include <memory>
#include <string>
#include <map>
#include <algorithm>

namespace hyperion {
class AssetManager {
public:
    static AssetManager *GetInstance();

    AssetManager();

    void SetRootDir(const std::string &path);
    std::string GetRootDir();
    std::shared_ptr<Loadable> LoadFromFile(const std::string &path, bool use_caching = true);
    const std::unique_ptr<AssetLoader> &GetLoader(const std::string &path);

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

    std::string root_path = "./";

    std::map<std::string, std::unique_ptr<AssetLoader>> loaders;
    std::map<std::string, std::shared_ptr<Loadable>> loaded_assets;
};
}

#endif
