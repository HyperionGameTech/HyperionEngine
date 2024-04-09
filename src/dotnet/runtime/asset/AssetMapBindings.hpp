#ifndef HYPERION_V2_RUNTIME_DOTNET_ASSET_MAP_BINDINGS_HPP
#define HYPERION_V2_RUNTIME_DOTNET_ASSET_MAP_BINDINGS_HPP

#include <asset/AssetBatch.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
struct ManagedAssetMap
{
    AssetMap *map;
};
} // extern "C"

#endif