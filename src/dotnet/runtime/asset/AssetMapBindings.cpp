#include <dotnet/runtime/asset/AssetMapBindings.hpp>

#include <rendering/Texture.hpp>
#include <rendering/Material.hpp>
#include <scene/Node.hpp>
#include <scene/animation/Skeleton.hpp>
#include <audio/AudioSource.hpp>

#include <core/lib/TypeMap.hpp>

extern "C" {
    // enum AssetType : uint32
    // {
    //     ASSET_TYPE_INVALID = 0,
    //     ASSET_TYPE_TEXTURE = 1,
    //     ASSET_TYPE_NODE = 2,
    //     ASSET_TYPE_AUDIO_SOURCE = 3,
    //     ASSET_TYPE_SKELETON = 4,
    //     ASSET_TYPE_MATERIAL_GROUP = 5
    // };

    void AssetMap_Destroy(ManagedAssetMap managed_map)
    {
        AssertThrowMsg(managed_map.map != nullptr, "ManagedAssetMap map is null");

        delete managed_map.map;
    }

    EnqueuedAsset *AssetMap_GetAsset(ManagedAssetMap managed_map, const char *key)
    {
        AssertThrowMsg(managed_map.map != nullptr, "ManagedAssetMap map is null");

        auto it = managed_map.map->Find(key);

        if (it != managed_map.map->End()) {
            return &it->second;
        }

        return nullptr;
    }
}