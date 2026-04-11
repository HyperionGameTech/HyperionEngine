/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <asset/Assets.hpp>

using namespace Hyperion;

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

        if (AssetLoadResult result = loader->Load(*assetManager, path); result.HasValue())
        {
            return new LoadedAsset(std::move(result.GetValue()));
        }
        // else if (result.HasError())
        // {
        //     return new LoadedAsset(result.GetError());
        // }

        return nullptr;
    }

    HYP_EXPORT void AssetManager_LoadAsync(AssetManager* assetManager, AssetLoaderDefinition* loaderDefinition, const char* path, void (*callback)(void*))
    {
        HYP_NOT_IMPLEMENTED();
    }

} // extern "C"
