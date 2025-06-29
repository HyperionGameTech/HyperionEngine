/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/AssetLoader.hpp>
#include <asset/Assets.hpp>

#include <core/object/HypClass.hpp>

#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

HYP_API void OnPostLoad_Impl(const HypClass* hyp_class, void* object_ptr)
{
    hyp_class->PostLoad(object_ptr);
}

#pragma region LoadedAsset

HYP_API void LoadedAsset::OnPostLoad()
{
    if (!value.IsValid())
    {
        return;
    }

    // @TODO: Change to use T::InstanceClass() from Asset<T>, as types might not be an exact match
    // @TODO: Walk up class heirarchy, call PostLoad() for each class
    const HypClass* hyp_class = GetClass(value.GetTypeId());

    if (!hyp_class)
    {
        return;
    }

    hyp_class->PostLoad(value.ToRef().GetPointer());
}

#pragma endregion LoadedAsset

#pragma region AssetLoaderBase

FilePath AssetLoaderBase::GetRebasedFilepath(const FilePath& base_path, const FilePath& filepath)
{
    const FilePath relative_filepath = FilePath::Relative(filepath, FilePath::Current());

    if (base_path.Any())
    {
        return FilePath::Join(
            base_path.Data(),
            relative_filepath.Data());
    }

    return relative_filepath;
}

Array<FilePath> AssetLoaderBase::GetTryFilepaths(const FilePath& original_filepath) const
{
    const FilePath current_path = FilePath::Current();

    Array<FilePath> paths {
        FilePath::Relative(original_filepath, current_path)
    };

    auto add_rebased_filepath = [&paths, &original_filepath, &current_path](const FilePath& base_path)
    {
        const FilePath filepath = GetRebasedFilepath(base_path, original_filepath);

        paths.PushBack(FilePath::Relative(filepath, current_path));
        paths.PushBack(filepath);
    };

    const FilePath& base_path = AssetManager::GetInstance()->GetBasePath();

    if (base_path.Any())
    {
        add_rebased_filepath(base_path);
    }

    AssetManager::GetInstance()->FindAssetCollector([&add_rebased_filepath, &base_path](const Handle<AssetCollector>& asset_collector)
        {
            if (asset_collector->GetBasePath() == base_path)
            {
                return false;
            }

            add_rebased_filepath(asset_collector->GetBasePath());

            return false;
        });

    return paths;
}

AssetLoadResult AssetLoaderBase::Load(AssetManager& asset_manager, const String& path) const
{
    HYP_SCOPE;

    static const AssetLoadError default_error = HYP_MAKE_ERROR(AssetLoadError, "File could not be found", AssetLoadError::ERR_NOT_FOUND);

    const FilePath original_filepath(path);

    const Array<FilePath> paths = GetTryFilepaths(original_filepath);

    for (const FilePath& path : paths)
    {
        if (!path.Exists())
        {
            // File does not exist, try next path
            continue;
        }

        FileBufferedReaderSource source { path };
        BufferedReader reader { &source };

        if (!reader.IsOpen())
        {
            HYP_LOG(Assets, Warning, "Could not open file at path: {}, trying next path...", path);

            continue;
        }

        LoaderState state {
            &asset_manager,
            path.Data(),
            std::move(reader)
        };

        auto result = LoadAsset(state);

        state.stream.Close();

        if (result.HasError())
        {
            if (result.GetError().GetErrorCode() == AssetLoadError::ERR_NOT_FOUND)
            {
                // Keep trying
                continue;
            }

            return result;
        }
        else if (result.HasValue())
        {
            return result;
        }
    }

    return default_error;
}

#pragma endregion AssetLoaderBase

} // namespace hyperion