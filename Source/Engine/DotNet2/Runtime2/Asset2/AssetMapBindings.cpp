/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <DotNET/runtime/asset/AssetMapBindings.hpp>

#include <Scene/Node.hpp>
#include <Scene/animation/Skeleton.hpp>


#include <Audio/AudioSource.hpp>

#include <Core/containers/TypeMap.hpp>

extern "C"
{
    HYP_EXPORT void AssetMap_Destroy(ManagedAssetMap managedMap)
    {
        Assert(managedMap.map != nullptr, "ManagedAssetMap map is null");

        delete managedMap.map;
    }

    HYP_EXPORT LoadedAsset* AssetMap_GetAsset(ManagedAssetMap managedMap, const char* key)
    {
        Assert(managedMap.map != nullptr, "ManagedAssetMap map is null");

        auto it = managedMap.map->Find(key);

        if (it != managedMap.map->End())
        {
            return &it->second;
        }

        return nullptr;
    }
} // extern "C"
