/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <asset/AssetRegistry.hpp>

using namespace Hyperion;

extern "C"
{
    HYP_EXPORT uint32 AssetPackage_GetAssets(AssetPackage* pPackage, Handle<AssetObject>* pOutAssetHandles)
    {
        Assert(pPackage != nullptr);

        if (!pOutAssetHandles)
        {
            return uint32(pPackage->GetAssets().Size());
        }

        Array<Handle<AssetObject>> assets = pPackage->GetAssets().ToArray();

        for (uint32 i = 0; i < uint32(assets.Size()); i++)
        {
            new (&pOutAssetHandles[i]) Handle<AssetObject>(std::move(assets[i]));
        }

        return uint32(assets.Size());
    }

    HYP_EXPORT uint32 AssetPackage_GetSubpackages(AssetPackage* pPackage, Handle<AssetPackage>* pOutPackageHandles)
    {
        Assert(pPackage != nullptr);

        if (!pOutPackageHandles)
        {
            return uint32(pPackage->GetSubpackages().Size());
        }

        Array<Handle<AssetPackage>> packages = pPackage->GetSubpackages().ToArray();

        for (uint32 i = 0; i < uint32(packages.Size()); i++)
        {
            new (&pOutPackageHandles[i]) Handle<AssetPackage>(std::move(packages[i]));
        }

        return uint32(packages.Size());
    }
} // extern "C"
