/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <asset/Assets.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void Asset_Destroy(LoadedAsset* pLoadedAsset)
    {
        if (!pLoadedAsset)
        {
            return;
        }

        delete pLoadedAsset;
    }

    HYP_EXPORT void Asset_GetHypData(LoadedAsset* pLoadedAsset, BoxedValue* pOutData)
    {
        if (!pLoadedAsset || !pOutData)
        {
            return;
        }

        *pOutData = std::move(pLoadedAsset->value);
        pLoadedAsset->value.Reset();
    }

} // extern "C"
