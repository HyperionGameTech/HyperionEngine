/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetObject.hpp>

using namespace Hyperion;

extern "C"
{
    HYP_EXPORT uint32 AssetRegistry_GetBucketAssetDescs(AssetRegistry* pRegistry, uint32 bucketIndex, AssetDesc* pOutAssetDescs, uint32 maxCount)
    {
        Assert(pRegistry != nullptr);

        Array<AssetDesc> descs;
        pRegistry->GetBucketAssetDescs(bucketIndex, descs);

        if (!pOutAssetDescs)
        {
            return uint32(descs.Size());
        }

        if (maxCount > uint32(descs.Size()))
        {
            maxCount = uint32(descs.Size());
        }

        for (uint32 i = 0; i < maxCount; i++)
        {
            new (&pOutAssetDescs[i]) AssetDesc(std::move(descs[i]));
        }

        return maxCount;
    }

    HYP_EXPORT const char* AssetRegistry_GetBucketName(uint32 bucketIndex)
    {
        return GetAssetBucketName(bucketIndex);
    }

    HYP_EXPORT void AssetRegistry_PutAsset(AssetRegistry* pRegistry, AssetObject* pAsset)
    {
        Assert(pRegistry != nullptr);
        Assert(pAsset != nullptr);

        pRegistry->PutAsset(MakeStrongRef(pAsset));
    }

    HYP_EXPORT void AssetRegistry_PutAssetUnique(AssetRegistry* pRegistry, AssetObject* pAsset)
    {
        Assert(pRegistry != nullptr);
        Assert(pAsset != nullptr);

        pRegistry->PutAssetUnique(MakeStrongRef(pAsset));
    }
} // extern "C"
