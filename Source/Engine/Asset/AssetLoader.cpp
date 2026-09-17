/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <AssetPch.hpp>

#include <Asset/AssetLoader.hpp>
#include <Asset/Assets.hpp>

#include <AssetLoader.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Assets);

namespace CoreApi {
CORE_API extern const FilePath& GetExecutablePath();
} // namespace CoreApi

ENGINE_API void OnPostLoad_Impl(const Class* cls, void* objectPtr)
{
    cls->PostLoad(objectPtr);
}

#pragma region LoadedAsset

ENGINE_API void LoadedAsset::OnPostLoad()
{
    if (!IsValid())
    {
        return;
    }

    BoxedValue& bv = GetValue();

    /// \todo : Change to use T::InstanceClass() from TLoadedAsset<T>, as types might not be an exact match
    const Class* cls = GetClass(bv.GetTypeId());

    if (!cls)
    {
        return;
    }

    cls->PostLoad(bv.ToRef().GetPointer());
}

#pragma endregion LoadedAsset

#pragma region AssetLoaderBase

static FilePath Normalized(const FilePath& filepath)
{
    return filepath.IsAbsolute() ? FilePath::Canonical(filepath) : filepath;
}

FilePath AssetLoaderBase::ResolveReferencedFilepath(const FilePath& referencingFilepath, const FilePath& referencedPath)
{
    if (referencedPath.Empty() || referencedPath.IsAbsolute())
    {
        return referencedPath;
    }

    const FilePath baseDirectory = referencingFilepath.BasePath();

    if (baseDirectory.Any())
    {
        return Normalized(FilePath::Join(baseDirectory, referencedPath));
    }

    // Nothing to anchor on, so fall back to the working directory the way a plain fopen() would.
    return Normalized(FilePath::Join(FilePath::Current(), referencedPath));
}

Array<FilePath> AssetLoaderBase::GetTryFilepaths(const FilePath& originalFilepath) const
{
    Array<FilePath> paths;

    auto AddCandidate = [&paths](const FilePath& basePath, const FilePath& filepath)
    {
        if (basePath.Empty())
        {
            return;
        }

        // Already rooted at this base (an asset path carrying its own prefix), so don't prefix it twice.
        const FilePath candidate = filepath.StartsWith(basePath)
            ? Normalized(filepath)
            : Normalized(FilePath::Join(basePath, filepath));

        if (candidate.Any() && !paths.Contains(candidate))
        {
            paths.PushBack(candidate);
        }
    };

    if (originalFilepath.IsAbsolute())
    {
        paths.PushBack(FilePath::Canonical(originalFilepath));

        return paths;
    }

    // Relative paths are anchored on the roots we know about rather than the process working directory,
    // which anything in the host application is free to move out from under us.
    AddCandidate(CoreApi::GetExecutablePath(), originalFilepath);

    AssetManager* assetManager = AssetManager::GetInstance();

    AddCandidate(assetManager->GetBasePath(), originalFilepath);

    auto FindAssetCollectorFunctor = [&AddCandidate, &originalFilepath](const Handle<AssetCollector>& assetCollector)
    {
        AddCandidate(assetCollector->GetBasePath(), originalFilepath);

        return false;
    };

    assetManager->FindAssetCollector(FindAssetCollectorFunctor);

    AddCandidate(FilePath::Current(), originalFilepath);

    return paths;
}

AssetLoadResult AssetLoaderBase::Load(
    AssetManager& assetManager,
    const String& path,
    const String& batchIdentifier,
    EnumFlags<AssetLoadHint> hint) const
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

    HYP_LOG(Assets, Error, "Failed to load asset {} after {} attempts", originalFilepath, numAttempts);

    return s_defaultError;
}

#pragma endregion AssetLoaderBase

} // namespace Hyperion
