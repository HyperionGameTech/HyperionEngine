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
}