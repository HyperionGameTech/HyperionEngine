/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/AssetLoader.hpp>
#include <asset/Assets.hpp>

#include <core/object/HypClass.hpp>

#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

HYP_API void OnPostLoad_Impl(const HypClass* hypClass, void* objectPtr)
{
    hypClass->PostLoad(objectPtr);
}

#pragma region LoadedAsset

HYP_API void LoadedAsset::OnPostLoad()
{
    if (!value.IsValid())
    {
        return;
    }

    // @TODO: Change to use T::InstanceClass() from TLoadedAsset<T>, as types might not be an exact match
    // @TODO: Walk up class heirarchy, call PostLoad() for each class
    const HypClass* hypClass = GetClass(value.GetTypeId());

    if (!hypClass)
    {
        return;
    }

    hypClass->PostLoad(value.ToRef().GetPointer());
}

#pragma endregion LoadedAsset

#pragma region AssetLoaderBase

FilePath AssetLoaderBase::GetRebasedFilepath(const FilePath& basePath, const FilePath& filepath)
{
    const FilePath relativeFilepath = FilePath::Relative(filepath, FilePath::Current());

    if (basePath.Any())
    {
        return FilePath::Join(
            basePath.Data(),
            relativeFilepath.Data());
    }

    return relativeFilepath;
}

Array<FilePath> AssetLoaderBase::GetTryFilepaths(const FilePath& originalFilepath) const
{
    const FilePath currentPath = FilePath::Current();

    Array<FilePath> paths {
        FilePath::Relative(originalFilepath, currentPath)
    };

    auto addRebasedFilepath = [&paths, &originalFilepath, &currentPath](const FilePath& basePath)
    {
        const FilePath filepath = GetRebasedFilepath(basePath, originalFilepath);

        paths.PushBack(FilePath::Relative(filepath, currentPath));
        paths.PushBack(filepath);
    };

    const FilePath& basePath = AssetManager::GetInstance()->GetBasePath();

    if (basePath.Any())
    {
        addRebasedFilepath(basePath);
    }

    AssetManager::GetInstance()->FindAssetCollector([&addRebasedFilepath, &basePath](const Handle<AssetCollector>& assetCollector)
        {
            if (assetCollector->GetBasePath() == basePath)
            {
                return false;
            }

            addRebasedFilepath(assetCollector->GetBasePath());

            return false;
        });

    return paths;
}

AssetLoadResult AssetLoaderBase::Load(AssetManager& assetManager, const String& path, const String& batchIdentifier) const
{
    HYP_SCOPE;

    static const AssetLoadError defaultError = HYP_MAKE_ERROR(AssetLoadError, "File could not be found", AssetLoadError::ERR_NOT_FOUND);

    const FilePath originalFilepath(path);

    const Array<FilePath> filepaths = GetTryFilepaths(originalFilepath);

    for (const FilePath& filepath : filepaths)
    {
        if (!filepath.Exists())
        {
            // File does not exist, try next path
            continue;
        }

        FileBufferedReaderSource source { filepath };
        BufferedReader reader { &source };

        if (!reader.IsOpen())
        {
            HYP_LOG(Assets, Warning, "Could not open file at path: {}, trying next path...", filepath);

            continue;
        }

        LoaderState state {};
        state.assetManager = &assetManager;
        state.filepath = filepath;
        state.batchIdentifier = batchIdentifier;
        state.stream = std::move(reader);
        
        if (state.batchIdentifier.Empty())
        {
            state.batchIdentifier = filepath.Basename();
        }

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

    return defaultError;
}

#pragma endregion AssetLoaderBase

} // namespace hyperion
