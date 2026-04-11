/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <asset/Assets.hpp>

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

} // extern "C"
