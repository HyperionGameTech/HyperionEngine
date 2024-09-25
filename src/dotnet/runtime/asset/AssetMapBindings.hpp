/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RUNTIME_DOTNET_ASSET_MAP_BINDINGS_HPP
#define HYPERION_RUNTIME_DOTNET_ASSET_MAP_BINDINGS_HPP

#include <asset/AssetBatch.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C" {
struct ManagedAssetMap
{
    AssetMap *map;
};
} // extern "C"

#endif