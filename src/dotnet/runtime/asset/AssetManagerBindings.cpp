/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/Assets.hpp>

#include <Engine.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT AssetManager *AssetManager_GetInstance()
{
    return g_asset_manager;
}

HYP_EXPORT const char *AssetManager_GetBasePath(AssetManager *manager)
{
    return manager->GetBasePath().Data();
}

HYP_EXPORT void AssetManager_SetBasePath(AssetManager *manager, const char *path)
{
    manager->SetBasePath(path);
}

} // extern "C"