/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

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

        if (pLoadedAsset->valueOrError.Is<BoxedValue>())
            *pOutData = std::move(pLoadedAsset->valueOrError.GetUnchecked<BoxedValue>());

        pLoadedAsset->valueOrError.Reset();
    }

    HYP_EXPORT int8 AssetPackage_GetAssetObjectBoxed(AssetPackage* pPackage, const Name* pName, BoxedValue* pOutBoxed)
    {
        if (!pPackage || !pName || !pOutBoxed)
        {
            return 0;
        }

        Handle<AssetObject> assetObject = pPackage->GetAssetObject(*pName);

        if (!assetObject.IsValid())
        {
            return 0;
        }

        *pOutBoxed = BoxedValue(assetObject);

        return 1;
    }

} // extern "C"
