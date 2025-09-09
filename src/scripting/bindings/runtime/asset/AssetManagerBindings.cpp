/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/Assets.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT const AssetLoaderDefinition* AssetManager_GetLoaderDefinition(AssetManager* assetManager, const char* path, TypeId desiredTypeId)
    {
        Assert(assetManager != nullptr);

        return assetManager->GetLoaderDefinition(path, desiredTypeId);
    }

    HYP_EXPORT LoadedAsset* AssetManager_Load(AssetManager* assetManager, AssetLoaderDefinition* loaderDefinition, const char* path)
    {
        Assert(assetManager != nullptr);
        Assert(loaderDefinition != nullptr);

        AssetLoaderBase* loader = loaderDefinition->loader.Get();

        if (!loader)
        {
            return nullptr;
        }

        if (AssetLoadResult result = loader->Load(*assetManager, path))
        {
            return new LoadedAsset(std::move(result.GetValue()));
        }

        return nullptr;
    }

    HYP_EXPORT void AssetManager_LoadAsync(AssetManager* assetManager, AssetLoaderDefinition* loaderDefinition, const char* path, void (*callback)(void*))
    {
        HYP_NOT_IMPLEMENTED();
    }

} // extern "C"