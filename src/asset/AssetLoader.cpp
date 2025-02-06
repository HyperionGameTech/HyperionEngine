/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/AssetLoader.hpp>
#include <asset/Assets.hpp>

#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

FilePath AssetLoader::GetRebasedFilepath(const FilePath &base_path, const FilePath &filepath)
{
    const FilePath relative_filepath = FilePath::Relative(filepath, FilePath::Current());

    if (base_path.Any()) {
        return FilePath::Join(
            base_path.Data(),
            relative_filepath.Data()
        );
    }

    return relative_filepath;
}

Array<FilePath> AssetLoader::GetTryFilepaths(const FilePath &original_filepath) const
{
    const FilePath current_path = FilePath::Current();

    Array<FilePath> paths {
        FilePath::Relative(original_filepath, current_path)
    };

    auto AddRebasedFilepath = [&paths, &original_filepath, &current_path](const FilePath &base_path)
    {
        const FilePath filepath = GetRebasedFilepath(base_path, original_filepath);

        paths.PushBack(FilePath::Relative(filepath, current_path));
        paths.PushBack(filepath);
    };

    const FilePath &base_path = AssetManager::GetInstance()->GetBasePath();

    if (base_path.Any()) {
        AddRebasedFilepath(base_path);
    }

    AssetManager::GetInstance()->FindAssetCollector([&AddRebasedFilepath, &base_path](const Handle<AssetCollector> &asset_collector)
    {
        if (asset_collector->GetBasePath() == base_path) {
            return false;
        }

        AddRebasedFilepath(asset_collector->GetBasePath());

        return false;
    });

    return paths;
}

LoadedAsset AssetLoader::Load(AssetManager &asset_manager, const String &path) const
{
    HYP_SCOPE;

    LoadedAsset asset;

    asset.result = LoaderResult {
        LoaderResult::Status::ERR_NOT_FOUND,
        "File could not be found"
    };

    const FilePath original_filepath(path);
    
    const Array<FilePath> paths = GetTryFilepaths(original_filepath);

    for (const FilePath &path : paths) {
        BufferedReader reader;

        if (!path.Open(reader)) {
            // could not open... try next path
            HYP_LOG(Assets, Warning, "Could not open file at path: {}, trying next path...", path);

            continue;
        }

        LoaderState state {
            &asset_manager,
            path.Data(),
            std::move(reader)
        };

        asset = LoadAsset(state);

        reader.Close();

        if (asset.result) {
            break; // stop searching when value result is found
        }
    }

    return asset;
}

} // namespace hyperion