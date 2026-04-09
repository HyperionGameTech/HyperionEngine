/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <asset/AssetRegistry.hpp>

using namespace Hyperion;

extern "C"
{
    HYP_EXPORT uint32 AssetPackage_GetAssetDescs(AssetPackage* pPackage, AssetDesc* pOutAssetDescs, uint32 maxCount)
    {
        Assert(pPackage != nullptr);

        Array<AssetDesc> assetDescs;
        pPackage->GetAssetDescs(assetDescs);

        if (!pOutAssetDescs)
        {
           return uint32(assetDescs.Size());
        }

        if (maxCount > uint32(assetDescs.Size()))
        {
            maxCount = uint32(assetDescs.Size());
        }

        for (uint32 i = 0; i < maxCount; i++)
        {
           new (&pOutAssetDescs[i]) AssetDesc(std::move(assetDescs[i]));
        }

        return maxCount;
    }

    HYP_EXPORT uint32 AssetPackage_GetSubpackages(AssetPackage* pPackage, Handle<AssetPackage>* pOutPackageHandles, uint32 maxCount)
    {
        Assert(pPackage != nullptr);

        Array<Handle<AssetPackage>> subpackages;
        pPackage->GetSubpackages(subpackages);

        if (!pOutPackageHandles)
        {
            return uint32(subpackages.Size());
        }

        if (maxCount > uint32(subpackages.Size()))
        {
            maxCount = uint32(subpackages.Size());
        }

        for (uint32 i = 0; i < maxCount; i++)
        {
            new (&pOutPackageHandles[i]) Handle<AssetPackage>(std::move(subpackages[i]));
        }

        return maxCount;
    }
} // extern "C"
