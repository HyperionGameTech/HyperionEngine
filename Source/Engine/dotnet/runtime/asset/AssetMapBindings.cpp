/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <dotnet/runtime/asset/AssetMapBindings.hpp>

#include <scene/Node.hpp>
#include <scene/animation/Skeleton.hpp>

#include <rendering/Material.hpp>

#include <audio/AudioSource.hpp>

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
