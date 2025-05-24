/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/Assets.hpp>

#include <core/object/HypData.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT void Asset_Destroy(LoadedAsset *loaded_asset)
{
    if (!loaded_asset) {
        return;
    }

    delete loaded_asset;
}

HYP_EXPORT void Asset_GetHypData(LoadedAsset *loaded_asset, HypData *out_hyp_data)
{
    if (!loaded_asset || !out_hyp_data) {
        return;
    }

    *out_hyp_data = std::move(loaded_asset->value);
    loaded_asset->value.Reset();
}

} // extern "C"