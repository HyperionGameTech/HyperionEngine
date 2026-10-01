/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Asset/AssetReference.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Utilities/Span.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Mat4f.hpp>
#include <Core/Math/BoundingBox.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class Mesh;
class Material;
class GroundCover;

static constexpr uint32 TerrainGrassTileQuads = 16;
static constexpr uint32 TerrainNumSplatLayers = 4;

struct TerrainCoverMember
{
    Handle<Mesh> mesh;
    Handle<Material> material;
    Handle<Material> materialNoShadows;
};

struct TerrainCoverType
{
    Array<TerrainCoverMember> members;

    /// index of this type's first member among a tile's slots
    uint32 firstSlot = 0;
};

struct TerrainCoverLayer
{
    Array<TerrainCoverType> types;
};

/// what a tile build needs of a cover type, safe to copy to other threads
struct TerrainCoverTypePlan
{
    float weight = 1.0f;

    /// each member placed in prefab root space
    Array<Mat4f> memberMatrices;
    Array<BoundingBox> memberBounds;
};

struct TerrainCoverLayerPlan
{
    Name name;

    /// grows where painted in the cell's plane for name, rather than over splatLayer
    bool isPainted = false;

    uint32 splatLayer = 0;

    /// world units between patches where the splat weight is full
    float spacing = 1.1f;

    float clumpSize = 12.0f;

    /// how much of the ground the layer hides from above where it grows fully
    float coverage = 1.0f;

    Array<TerrainCoverTypePlan> types;
};

struct TerrainGrassTileInput
{
    Span<const float> paddedHeights;

    /// cellSize^2 * TerrainNumSplatLayers weights, row z first
    Span<const ubyte> splatWeights;

    /// planes of cellSize^2 painted weights, row z first - see TerrainCellData::GetGroundCoverPaint()
    Span<const ubyte> paintWeights;

    /// the paintWeights plane of each of layers, -1 where the cell has none
    Span<const int32> layerPaintPlanes;

    uint32 cellSize = 0;
    Vec3f cellMin;
    Vec3f cellScale;

    Vec2u tileMin;
    Vec2u tileMax;

    uint32 seed = 0;

    /// patches this much further apart and wider (and sqrt of it taller), so fewer of them still cover the ground
    float stretch = 1.0f;

    Span<const TerrainCoverLayerPlan> layers;
};

struct TerrainGrassSlotInstances
{
    Array<Mat4f> transforms;
    BoundingBox bounds = BoundingBox::Empty();
};

struct TerrainGrassTileOutput
{
    /// one per member of every type of every layer, in layer then type order
    Array<TerrainGrassSlotInstances> slots;
};

/// sim thread only - what a terrain plants, resolved from its GroundCover or the engine's DefaultGroundCover without one
class TerrainGroundCoverResources
{
public:
    const Handle<GroundCover>& GetGroundCover() const;
    void SetGroundCover(const Handle<GroundCover>& groundCover);

    /// resolved on first use, so it can be set before the asset registry is ready
    void SetGroundCoverPath(const AssetPath& assetPath);

    const AssetReference& GetGroundCoverReference() const
    {
        return m_groundCover;
    }

    /// re-resolves on next use, e.g after the GroundCover asset was edited
    void Invalidate();

    const Array<TerrainCoverLayer>& GetLayers();
    const Array<TerrainCoverLayerPlan>& GetPlans();

    /// the painted layers, for the paint tool to list
    Array<Name> GetPaintedLayerNames();

    uint32 GetNumSlots();

    /// bumped whenever what's planted changes
    uint32 GetVersion();

private:
    void Resolve();

    static Handle<GroundCover> GetDefaultGroundCover();

    const Handle<Material>& GetNoShadowMaterial(const Handle<Material>& material);

    AssetReference m_groundCover;

    bool m_isResolved = false;
    uint32 m_version = 0;
    uint32 m_numSlots = 0;

    Array<TerrainCoverLayer> m_layers;
    Array<TerrainCoverLayerPlan> m_plans;

    Array<Handle<Material>> m_noShadowSources;
    Array<Handle<Material>> m_noShadowMaterials;
};

namespace TerrainGrass {

void GenerateTile(const TerrainGrassTileInput& input, TerrainGrassTileOutput& output);

} // namespace TerrainGrass

} // namespace Hyperion
