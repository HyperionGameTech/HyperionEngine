/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/runtime/asset/AssetMapBindings.hpp>

#include <rendering/RenderTexture.hpp>
#include <rendering/RenderMaterial.hpp>
#include <scene/Node.hpp>
#include <scene/animation/Skeleton.hpp>
#include <audio/AudioSource.hpp>

#include <core/containers/TypeMap.hpp>

extern "C" {
HYP_EXPORT void AssetMap_Destroy(ManagedAssetMap managed_map)
{
    AssertThrowMsg(managed_map.map != nullptr, "ManagedAssetMap map is null");

    delete managed_map.map;
}

HYP_EXPORT LoadedAsset *AssetMap_GetAsset(ManagedAssetMap managed_map, const char *key)
{
    AssertThrowMsg(managed_map.map != nullptr, "ManagedAssetMap map is null");

    auto it = managed_map.map->Find(key);

    if (it != managed_map.map->End()) {
        return &it->second;
    }

    return nullptr;
}
} // extern "C"