/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Asset/AssetObject.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class Prefab;

HYP_STRUCT()
struct GroundCoverType
{
    HYP_STRUCT_BODY(GroundCoverType);

    HYP_FIELD(Property = "Prefab", Serialize, Editor)
    Handle<Prefab> prefab;

    /// share of its layer's patches, relative to the layer's other types
    HYP_FIELD(Property = "Weight", Serialize, Editor)
    float weight = 1.0f;
};

HYP_STRUCT()
struct GroundCoverLayer
{
    HYP_STRUCT_BODY(GroundCoverLayer);

    /// the terrain splat layer whose weight decides where this grows - 0 grass, 1 rock, 2 dirt, 3 snow
    HYP_FIELD(Property = "SplatLayer", Serialize, Editor)
    uint32 splatLayer = 0;

    /// patches per footprint where the splat weight is full
    HYP_FIELD(Property = "Density", Serialize, Editor)
    float density = 1.0f;

    /// world units across the drifts that one type grows in
    HYP_FIELD(Property = "ClumpSize", Serialize, Editor)
    float clumpSize = 12.0f;

    HYP_FIELD(Property = "Types", Serialize, Editor)
    Array<GroundCoverType> types;
};

/// what a terrain plants over its splat layers - prefabs exported from Arbor's ground cover
HYP_CLASS(AssetBucket = "Terrain")
class ENGINE_API GroundCover final : public AssetObject
{
    HYP_OBJECT_BODY(GroundCover);

public:
    GroundCover();
    explicit GroundCover(Name name);

    virtual ~GroundCover() override = default;

    HYP_FIELD(Property = "Layers", Serialize, Editor)
    Array<GroundCoverLayer> layers;
};

} // namespace Hyperion
