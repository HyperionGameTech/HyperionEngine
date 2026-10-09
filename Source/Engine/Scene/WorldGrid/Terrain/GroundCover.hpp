/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Asset/AssetObject.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Name/Name.hpp>

#include <Core/HashCode.hpp>

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

    HYP_FIELD(Property = "ColorVariation", Serialize, Editor)
    float colorVariation = 0.6f;

    HYP_FIELD(Property = "GroundNormalBlend", Serialize, Editor)
    float groundNormalBlend = 0.7f;

    HYP_FIELD(Property = "BaseOcclusion", Serialize, Editor)
    float baseOcclusion = 0.6f;
};

HYP_ENUM()
enum class GroundCoverSource : uint8
{
    SplatLayer = 0,
    Painted
};

HYP_STRUCT()
struct GroundCoverLayer
{
    HYP_STRUCT_BODY(GroundCoverLayer);

    /// painted layers are stored under this in the terrain's cells, and listed by name in the paint tool
    HYP_FIELD(Property = "Name", Serialize, Editor)
    Name name;

    HYP_FIELD(Property = "Source", Serialize, Editor)
    GroundCoverSource source = GroundCoverSource::SplatLayer;

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

HYP_CLASS(AssetBucket = "Terrain")
class ENGINE_API GroundCover final : public AssetObject
{
    HYP_OBJECT_BODY(GroundCover);

public:
    GroundCover();
    explicit GroundCover(Name name);

    virtual ~GroundCover() override = default;

    /// the painted layer that plants just \p prefab, or an invalid name
    HYP_METHOD()
    Name FindPaintedLayer(const Handle<Prefab>& prefab) const;

    /// the painted layer that plants just \p prefab, added and named after it if there is none
    HYP_METHOD()
    Name EnsurePaintedLayer(const Handle<Prefab>& prefab);

    /// the prefab \p layerName plants, if it plants just one
    HYP_METHOD()
    Handle<Prefab> GetPaintedLayerPrefab(Name layerName) const;

    /// changes whenever what the layers plant does, so terrains know to replant
    HashCode GetContentHashCode() const;

    HYP_FIELD(Property = "Layers", Serialize, Editor)
    Array<GroundCoverLayer> layers;
};

} // namespace Hyperion
