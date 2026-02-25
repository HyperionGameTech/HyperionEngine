/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <asset/Assets.hpp>

using namespace Hyperion;

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

    HYP_EXPORT void Asset_GetBoxed(LoadedAsset* pLoadedAsset, BoxedValue* pOutData)
    {
        if (!pLoadedAsset || !pOutData)
        {
            return;
        }

        *pOutData = std::move(pLoadedAsset->value);
        pLoadedAsset->value.Reset();
    }

} // extern "C"
