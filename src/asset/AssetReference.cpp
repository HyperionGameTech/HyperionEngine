/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <asset/AssetReference.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

HYP_API extern Handle<AssetManager> g_assetManager;

AssetReference::AssetReference(const Handle<AssetObject>& assetObject)
    : assetPath(assetObject.IsValid() ? assetObject->GetPath() : AssetPath()),
      assetObject(assetObject)
{
}

const Handle<AssetObject>& AssetReference::Resolve() const
{
    if (IsLoaded())
    {
        return assetObject;
    }

    if (assetPath.IsValid())
    {
        assetObject = g_assetManager->GetAssetRegistry()->GetAssetFromPath(assetPath.ToString());

        if (!assetObject)
        {
            HYP_LOG(Assets, Error, "Failed to resolve asset reference for path '{}'", assetPath);
        }
    }

    return assetObject;
}

AssetObject* AssetReference::operator->() const
{
    if (!IsLoaded())
    {
        Resolve();
    }

    if (!assetObject)
    {
        HYP_FAIL("Failed to resolve asset reference!");
    }

    return assetObject.Get();
}

AssetObject& AssetReference::operator*() const
{
    if (!IsLoaded())
    {
        Resolve();
    }

    if (!assetObject)
    {
        HYP_FAIL("Failed to resolve asset reference!");
    }

    return *assetObject;
}

} // namespace hyperion
