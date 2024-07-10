/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/AssetBatch.hpp>
#include <dotnet/runtime/asset/AssetMapBindings.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;

extern "C" {
HYP_EXPORT AssetBatch *AssetBatch_Create()
{
    return new AssetBatch(g_asset_manager);
}

HYP_EXPORT void AssetBatch_Destroy(AssetBatch *batch)
{
    delete batch;
}

HYP_EXPORT void AssetBatch_LoadAsync(AssetBatch *batch, void(*callback)(void *), void *user_data)
{
    batch->LoadAsync();

    // Note: Will be deleted when AssetMap_Destroy is called from C#.
    AssetMap *asset_map = new AssetMap(batch->AwaitResults());
    callback(asset_map);
}

HYP_EXPORT AssetMap *AssetBatch_AwaitResults(AssetBatch *batch)
{
    return new AssetMap(batch->AwaitResults());
}

HYP_EXPORT void AssetBatch_AddToBatch(AssetBatch *batch, const char *key, const char *path)
{
    batch->Add(key, path);
}
} // extern "C"