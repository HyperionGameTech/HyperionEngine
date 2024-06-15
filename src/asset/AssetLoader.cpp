/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <asset/AssetLoader.hpp>
#include <asset/Assets.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

FilePath AssetLoader::GetRebasedFilepath(const AssetManager &asset_manager, const FilePath &filepath) const
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

LoadedAsset AssetLoader::Load(AssetManager &asset_manager, const String &path) const
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
        BufferedReader reader;

        if (!path.Open(reader)) {
            // could not open... try next path
            HYP_LOG(Assets, LogLevel::WARNING, "Could not open file at path: {}, trying next path...", path);

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