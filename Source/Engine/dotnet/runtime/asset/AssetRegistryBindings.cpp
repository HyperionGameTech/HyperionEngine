/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <asset/AssetRegistry.hpp>

using namespace Hyperion;

extern "C"
{
    HYP_EXPORT uint32 AssetRegistry_GetPackages(AssetRegistry* pRegistry, Handle<AssetPackage>* pOutPackageHandles)
    {
        Assert(pRegistry != nullptr);

        if (!pOutPackageHandles)
        {
            return uint32(pRegistry->GetPackages().Size());
        }

        Array<Handle<AssetPackage>> packages = pRegistry->GetPackages().ToArray();

        for (uint32 i = 0; i < uint32(packages.Size()); i++)
        {
            new (&pOutPackageHandles[i]) Handle<AssetPackage>(std::move(packages[i]));
        }

        return uint32(packages.Size());
    }
} // extern "C"
