/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/FixedArray.hpp>
#include <Core/Utilities/Span.hpp>

#include <Core/Math/Vector2.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

/// CDLOD quadtree over one terrain tile
class TerrainQuadtreeLayout
{
public:
    static constexpr uint8 MaxLevels = 16;

    struct NodeKey
    {
        uint8 level = 0;
        uint32 x = 0;
        uint32 z = 0;
    };

    struct PatchKey
    {
        NodeKey node;
        uint32 quadrant = 0;
    };

    /// the tile grid samples covered by one patch mesh
    struct PatchRegion
    {
        Vec2u origin;
        uint32 gridQuads = 0;
        uint32 stride = 1;
    };

    TerrainQuadtreeLayout() = default;

    /// \p cellSize - 1 must be a power of two. \p patchQuads (full resolution quads per side of a leaf) is rounded down to a
    /// power of two and clamped to the tile
    TerrainQuadtreeLayout(uint32 cellSize, uint32 patchQuads, uint8 maxLevels);

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_numLevels != 0;
    }

    HYP_FORCE_INLINE uint32 GetTileQuads() const
    {
        return m_tileQuads;
    }

    HYP_FORCE_INLINE uint32 GetPatchQuads() const
    {
        return m_patchQuads;
    }

    HYP_FORCE_INLINE uint8 GetNumLevels() const
    {
        return m_numLevels;
    }

    HYP_FORCE_INLINE uint8 GetTopLevel() const
    {
        return m_numLevels - 1;
    }

    HYP_FORCE_INLINE uint32 GetNumNodes() const
    {
        return m_numNodes;
    }

    HYP_FORCE_INLINE uint32 GetNumPatches() const
    {
        return m_numPatches;
    }

    HYP_FORCE_INLINE static uint32 GetStride(uint8 level)
    {
        return 1u << level;
    }

    uint32 GetNodeGridQuads(uint8 level) const;
    uint32 GetNodesPerSide(uint8 level) const;

    /// true when each quadrant of a node has its own child, so quadrants are drawn as separate patches
    bool HasQuadrantChildren(uint8 level) const;

    uint32 GetPatchesPerNode(uint8 level) const;

    uint32 GetNodeIndex(const NodeKey& key) const;
    NodeKey GetNodeKey(uint32 nodeIndex) const;

    uint32 GetPatchIndex(const PatchKey& key) const;
    PatchKey GetPatchKey(uint32 patchIndex) const;

    /// the level - 1 node covering \p quadrant of \p node
    NodeKey GetChildNode(const NodeKey& node, uint32 quadrant) const;

    Vec2u GetNodeOrigin(const NodeKey& node) const;
    PatchRegion GetPatchRegion(const PatchKey& key) const;

private:
    uint32 m_tileQuads = 0;
    uint32 m_patchQuads = 0;
    uint8 m_numLevels = 0;
    uint32 m_numNodes = 0;
    uint32 m_numPatches = 0;

    FixedArray<uint32, MaxLevels> m_levelNodeOffsets {};
    FixedArray<uint32, MaxLevels> m_levelPatchOffsets {};
};

/// full resolution height min/max inside each node's footprint (edges included), indexed like the layout's nodes
void ComputeTerrainQuadtreeNodeHeightBounds(
    const TerrainQuadtreeLayout& layout,
    Span<const float> paddedHeights,
    uint32 cellSize,
    Array<float>& outMinHeights,
    Array<float>& outMaxHeights);

struct TerrainQuadtreeSelectionInput
{
    const TerrainQuadtreeLayout& layout;

    /// nearest LOD viewpoint distance to each node's bounds
    Span<const float> nodeDistances;

    /// CDLOD range of each level
    Span<const float> levelRanges;

    /// nodes whose patches are all built
    Span<const uint8> nodeResident;

    /// outNodeInRange from the previous selection, for hysteresis
    Span<const uint8> previousNodeInRange;
};

void SelectTerrainQuadtreePatches(const TerrainQuadtreeSelectionInput& input, Span<uint8> outNodeInRange, Span<uint8> outPatchDrawn);

} // namespace Hyperion
