/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetBucket.hpp>

using namespace Hyperion;

extern "C"
{

    HYP_EXPORT void LoadedAsset_Destroy(LoadedAsset* pLoadedAsset)
    {
        if (!pLoadedAsset)
            return;

        delete pLoadedAsset;
    }

    HYP_EXPORT void LoadedAsset_GetBoxed(LoadedAsset* pLoadedAsset, BoxedValue* pOutData)
    {
        if (!pLoadedAsset || !pOutData)
            return;

        if (pLoadedAsset->IsValid())
        {
            *pOutData = std::move(pLoadedAsset->Unwrap());
        }

        *pLoadedAsset = {};
    }

    HYP_EXPORT int8 AssetRegistry_GetAssetBoxed(AssetRegistry* pRegistry, uint32 bucketIndex, const Name* pName, BoxedValue* pOutBoxed)
    {
        if (!pRegistry || !pName || !pOutBoxed)
        {
            return 0;
        }

        if (bucketIndex >= MaxAssetBuckets)
        {
            return 0;
        }

        const AssetBucket& bucket = *AssetBuckets::AllBuckets[bucketIndex];

        Handle<AssetObject> assetObject = pRegistry->GetAsset(bucket, StringHash(*pName));

        if (!assetObject.IsValid())
        {
            return 0;
        }

        *pOutBoxed = BoxedValue(assetObject);

        return 1;
    }

} // extern "C"
