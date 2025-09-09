/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/Assets.hpp>

#include <core/object/HypData.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void Asset_Destroy(LoadedAsset* loadedAsset)
    {
        if (!loadedAsset)
        {
            return;
        }

        delete loadedAsset;
    }

    HYP_EXPORT void Asset_GetHypData(LoadedAsset* loadedAsset, HypData* outHypData)
    {
        if (!loadedAsset || !outHypData)
        {
            return;
        }

        *outHypData = std::move(loadedAsset->value);
        loadedAsset->value.Reset();
    }

} // extern "C"