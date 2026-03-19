/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <AssetPch.hpp>

#include <asset/AssetLoader.hpp>
#include <asset/Assets.hpp>

#include <AssetLoader.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

HYP_API void OnPostLoad_Impl(const Class* cls, void* objectPtr)
{
    cls->PostLoad(objectPtr);
}

#pragma region LoadedAsset

HYP_API void LoadedAsset::OnPostLoad()
{
    if (!value.IsValid())
    {
        return;
    }

    /// \todo : Change to use T::InstanceClass() from TLoadedAsset<T>, as types might not be an exact match
    const Class* cls = GetClass(value.GetTypeId());

    if (!cls)
    {
        return;
    }

    cls->PostLoad(value.ToRef().GetPointer());
}

#pragma endregion LoadedAsset

#pragma region AssetLoaderBase

FilePath AssetLoaderBase::GetRebasedFilepath(const FilePath& basePath, const FilePath& filepath)
{
    const FilePath relativeFilepath = FilePath::Relative(filepath, FilePath::Current());

    if (basePath.Any())
    {
        return FilePath::Join(basePath, relativeFilepath);
    }

    return relativeFilepath;
}

Array<FilePath> AssetLoaderBase::GetTryFilepaths(const FilePath& originalFilepath) const
{
    const FilePath currentPath = FilePath::Current();

    Array<FilePath> paths {
        FilePath::Relative(originalFilepath, currentPath)
    };

    auto AddRebasedFilepath = [&paths, &originalFilepath, &currentPath](const FilePath& basePath)
    {
        const FilePath filepath = GetRebasedFilepath(basePath, originalFilepath);

        paths.PushBack(FilePath::Relative(filepath, currentPath));
        paths.PushBack(filepath);
    };

    const FilePath& basePath = AssetManager::GetInstance()->GetBasePath();

    if (basePath.Any())
    {
        AddRebasedFilepath(basePath);
    }

    AssetManager::GetInstance()->FindAssetCollector([&AddRebasedFilepath, &basePath](const Handle<AssetCollector>& assetCollector)
        {
            if (assetCollector->GetBasePath() == basePath)
            {
                return false;
            }

            AddRebasedFilepath(assetCollector->GetBasePath());

            return false;
        });

    return paths;
}

AssetLoadResult AssetLoaderBase::Load(
    AssetManager& assetManager,
    const String& path,
    const String& batchIdentifier,
    AssetLoadHint hint) const
{
    HYP_SCOPE;

    static const AssetLoadError s_defaultError = HYP_MAKE_ERROR(AssetLoadError, "File could not be found", AssetLoadError::ERR_NOT_FOUND);

    const FilePath originalFilepath(path);

    const Array<FilePath> filepaths = GetTryFilepaths(originalFilepath);

    uint32 numAttempts = 0;

    for (const FilePath& filepath : filepaths)
    {
        HYP_LOG(Assets, Verbose, "Trying to load asset from path: {} (attempt {}/{})", filepath, numAttempts + 1, filepaths.Size());
        ++numAttempts;

        if (!filepath.Exists())
        {
            // File does not exist, try next path
            continue;
        }

        LoaderState state { FileByteReader { filepath } };

        if (state.stream.Eof())
        {
            continue;
        }

        state.assetManager = &assetManager;
        state.filepath = filepath;
        state.batchIdentifier = batchIdentifier;
        state.hint = hint;

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

    return s_defaultError;
}

#pragma endregion AssetLoaderBase

} // namespace Hyperion
