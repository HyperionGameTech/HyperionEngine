/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainQuadtree.hpp>
#include <Scene/WorldGrid/Terrain/Generation/TerrainGenerator.hpp>

#include <Core/Math/MathUtil.hpp>

namespace Hyperion {

#pragma region TerrainQuadtreeLayout

static uint32 RoundDownToPowerOfTwo(uint32 value)
{
    uint32 result = 1;

    while (result <= value / 2)
    {
        result *= 2;
    }

    return result;
}

TerrainQuadtreeLayout::TerrainQuadtreeLayout(uint32 cellSize, uint32 patchQuads, uint8 maxLevels)
{
    const uint32 tileQuads = cellSize > 1 ? cellSize - 1 : 0;

    if (tileQuads < 2 || !MathUtil::IsPowerOfTwo(tileQuads))
    {
        return;
    }

    m_tileQuads = tileQuads;
    m_patchQuads = MathUtil::Min(RoundDownToPowerOfTwo(MathUtil::Max(patchQuads, 2u)), tileQuads);

    const uint8 levelLimit = MathUtil::Clamp<uint8>(maxLevels, 1, MaxLevels);

    // the coarsest level still has at least two vertex quads across the tile
    uint8 numLevels = 1;

    while (numLevels < levelLimit && (tileQuads >> numLevels) >= 2)
    {
        numLevels++;
    }

    m_numLevels = numLevels;

    uint32 nodeOffset = 0;
    uint32 patchOffset = 0;

    for (uint8 level = 0; level < m_numLevels; level++)
    {
        const uint32 nodesPerSide = GetNodesPerSide(level);
        const uint32 levelNodeCount = nodesPerSide * nodesPerSide;

        m_levelNodeOffsets[level] = nodeOffset;
        m_levelPatchOffsets[level] = patchOffset;

        nodeOffset += levelNodeCount;
        patchOffset += levelNodeCount * GetPatchesPerNode(level);
    }

    m_numNodes = nodeOffset;
    m_numPatches = patchOffset;
}

uint32 TerrainQuadtreeLayout::GetNodeGridQuads(uint8 level) const
{
    if (level >= 32)
    {
        return m_tileQuads;
    }

    return uint32(MathUtil::Min(uint64(m_patchQuads) << level, uint64(m_tileQuads)));
}

uint32 TerrainQuadtreeLayout::GetNodesPerSide(uint8 level) const
{
    return m_tileQuads / GetNodeGridQuads(level);
}

bool TerrainQuadtreeLayout::HasQuadrantChildren(uint8 level) const
{
    return level > 0 && GetNodeGridQuads(level - 1) < GetNodeGridQuads(level);
}

uint32 TerrainQuadtreeLayout::GetPatchesPerNode(uint8 level) const
{
    return HasQuadrantChildren(level) ? 4 : 1;
}

uint32 TerrainQuadtreeLayout::GetNodeIndex(const NodeKey& key) const
{
    AssertDebug(key.level < m_numLevels);

    return m_levelNodeOffsets[key.level] + key.z * GetNodesPerSide(key.level) + key.x;
}

TerrainQuadtreeLayout::NodeKey TerrainQuadtreeLayout::GetNodeKey(uint32 nodeIndex) const
{
    AssertDebug(nodeIndex < m_numNodes);

    uint8 level = 0;

    while (level + 1 < m_numLevels && m_levelNodeOffsets[level + 1] <= nodeIndex)
    {
        level++;
    }

    const uint32 nodesPerSide = GetNodesPerSide(level);
    const uint32 levelNodeIndex = nodeIndex - m_levelNodeOffsets[level];

    return NodeKey { level, levelNodeIndex % nodesPerSide, levelNodeIndex / nodesPerSide };
}

uint32 TerrainQuadtreeLayout::GetPatchIndex(const PatchKey& key) const
{
    AssertDebug(key.node.level < m_numLevels);
    AssertDebug(key.quadrant < GetPatchesPerNode(key.node.level));

    const uint32 levelNodeIndex = key.node.z * GetNodesPerSide(key.node.level) + key.node.x;

    return m_levelPatchOffsets[key.node.level] + levelNodeIndex * GetPatchesPerNode(key.node.level) + key.quadrant;
}

TerrainQuadtreeLayout::PatchKey TerrainQuadtreeLayout::GetPatchKey(uint32 patchIndex) const
{
    AssertDebug(patchIndex < m_numPatches);

    uint8 level = 0;

    while (level + 1 < m_numLevels && m_levelPatchOffsets[level + 1] <= patchIndex)
    {
        level++;
    }

    const uint32 patchesPerNode = GetPatchesPerNode(level);
    const uint32 nodesPerSide = GetNodesPerSide(level);
    const uint32 levelPatchIndex = patchIndex - m_levelPatchOffsets[level];
    const uint32 levelNodeIndex = levelPatchIndex / patchesPerNode;

    return PatchKey {
        NodeKey { level, levelNodeIndex % nodesPerSide, levelNodeIndex / nodesPerSide },
        levelPatchIndex % patchesPerNode
    };
}

TerrainQuadtreeLayout::NodeKey TerrainQuadtreeLayout::GetChildNode(const NodeKey& node, uint32 quadrant) const
{
    AssertDebug(node.level > 0);

    if (!HasQuadrantChildren(node.level))
    {
        return NodeKey { uint8(node.level - 1), node.x, node.z };
    }

    return NodeKey { uint8(node.level - 1), node.x * 2 + quadrant % 2, node.z * 2 + quadrant / 2 };
}

Vec2u TerrainQuadtreeLayout::GetNodeOrigin(const NodeKey& node) const
{
    const uint32 nodeGridQuads = GetNodeGridQuads(node.level);

    return Vec2u { node.x * nodeGridQuads, node.z * nodeGridQuads };
}

TerrainQuadtreeLayout::PatchRegion TerrainQuadtreeLayout::GetPatchRegion(const PatchKey& key) const
{
    const uint32 nodeGridQuads = GetNodeGridQuads(key.node.level);

    PatchRegion region;
    region.origin = GetNodeOrigin(key.node);
    region.gridQuads = nodeGridQuads;
    region.stride = GetStride(key.node.level);

    if (HasQuadrantChildren(key.node.level))
    {
        region.gridQuads = nodeGridQuads / 2;
        region.origin += Vec2u { key.quadrant % 2, key.quadrant / 2 } * region.gridQuads;
    }

    return region;
}

#pragma endregion TerrainQuadtreeLayout

void ComputeTerrainQuadtreeNodeHeightBounds(
    const TerrainQuadtreeLayout& layout,
    Span<const float> paddedHeights,
    uint32 cellSize,
    Array<float>& outMinHeights,
    Array<float>& outMaxHeights)
{
    HYP_SCOPE;

    outMinHeights.Resize(layout.GetNumNodes());
    outMaxHeights.Resize(layout.GetNumNodes());

    const uint32 padding = TerrainGenerator::CellPadding;
    const uint32 paddedSize = cellSize + padding * 2u;

    if (!layout.IsValid() || paddedHeights.Size() != size_t(paddedSize) * size_t(paddedSize))
    {
        for (uint32 nodeIndex = 0; nodeIndex < layout.GetNumNodes(); nodeIndex++)
        {
            outMinHeights[nodeIndex] = 0.0f;
            outMaxHeights[nodeIndex] = 0.0f;
        }

        return;
    }

    const uint32 leafNodesPerSide = layout.GetNodesPerSide(0);
    const uint32 leafGridQuads = layout.GetNodeGridQuads(0);

    for (uint32 nodeZ = 0; nodeZ < leafNodesPerSide; nodeZ++)
    {
        for (uint32 nodeX = 0; nodeX < leafNodesPerSide; nodeX++)
        {
            const TerrainQuadtreeLayout::NodeKey node { 0, nodeX, nodeZ };
            const Vec2u origin = layout.GetNodeOrigin(node);

            float minHeight = MathUtil::Infinity<float>();
            float maxHeight = -MathUtil::Infinity<float>();

            for (uint32 gridZ = origin.y; gridZ <= origin.y + leafGridQuads; gridZ++)
            {
                for (uint32 gridX = origin.x; gridX <= origin.x + leafGridQuads; gridX++)
                {
                    const float height = paddedHeights[size_t(gridZ + padding) * paddedSize + size_t(gridX + padding)];

                    minHeight = MathUtil::Min(minHeight, height);
                    maxHeight = MathUtil::Max(maxHeight, height);
                }
            }

            const uint32 nodeIndex = layout.GetNodeIndex(node);

            outMinHeights[nodeIndex] = minHeight;
            outMaxHeights[nodeIndex] = maxHeight;
        }
    }

    for (uint8 level = 1; level < layout.GetNumLevels(); level++)
    {
        const uint32 nodesPerSide = layout.GetNodesPerSide(level);

        for (uint32 nodeZ = 0; nodeZ < nodesPerSide; nodeZ++)
        {
            for (uint32 nodeX = 0; nodeX < nodesPerSide; nodeX++)
            {
                const TerrainQuadtreeLayout::NodeKey node { level, nodeX, nodeZ };
                const uint32 nodeIndex = layout.GetNodeIndex(node);

                float minHeight = MathUtil::Infinity<float>();
                float maxHeight = -MathUtil::Infinity<float>();

                for (uint32 quadrant = 0; quadrant < layout.GetPatchesPerNode(level); quadrant++)
                {
                    const uint32 childIndex = layout.GetNodeIndex(layout.GetChildNode(node, quadrant));

                    minHeight = MathUtil::Min(minHeight, outMinHeights[childIndex]);
                    maxHeight = MathUtil::Max(maxHeight, outMaxHeights[childIndex]);
                }

                outMinHeights[nodeIndex] = minHeight;
                outMaxHeights[nodeIndex] = maxHeight;
            }
        }
    }
}

#pragma region Selection

// refining early and coarsening late only ever draws a patch finer than it has to be, which its morph targets cover
static constexpr float RefineRangeScale = 1.05f;
static constexpr float CoarsenRangeScale = 1.1f;

static bool SelectTerrainQuadtreeNode(
    const TerrainQuadtreeSelectionInput& input,
    const TerrainQuadtreeLayout::NodeKey& node,
    Span<uint8> outNodeInRange,
    Span<uint8> outPatchDrawn)
{
    const TerrainQuadtreeLayout& layout = input.layout;
    const uint32 nodeIndex = layout.GetNodeIndex(node);

    const float rangeScale = input.previousNodeInRange[nodeIndex] ? CoarsenRangeScale : RefineRangeScale;

    if (input.nodeDistances[nodeIndex] >= input.levelRanges[node.level] * rangeScale)
    {
        return false;
    }

    outNodeInRange[nodeIndex] = 1;

    // the parent draws this area at its own level until the patches exist
    if (!input.nodeResident[nodeIndex])
    {
        return false;
    }

    for (uint32 quadrant = 0; quadrant < layout.GetPatchesPerNode(node.level); quadrant++)
    {
        const bool childDrawn = node.level > 0
            && SelectTerrainQuadtreeNode(input, layout.GetChildNode(node, quadrant), outNodeInRange, outPatchDrawn);

        if (!childDrawn)
        {
            outPatchDrawn[layout.GetPatchIndex({ node, quadrant })] = 1;
        }
    }

    return true;
}

void SelectTerrainQuadtreePatches(const TerrainQuadtreeSelectionInput& input, Span<uint8> outNodeInRange, Span<uint8> outPatchDrawn)
{
    HYP_SCOPE;

    const TerrainQuadtreeLayout& layout = input.layout;

    Assert(input.nodeDistances.Size() == layout.GetNumNodes()
        && input.nodeResident.Size() == layout.GetNumNodes()
        && input.previousNodeInRange.Size() == layout.GetNumNodes()
        && input.levelRanges.Size() >= layout.GetNumLevels()
        && outNodeInRange.Size() == layout.GetNumNodes()
        && outPatchDrawn.Size() == layout.GetNumPatches());

    for (uint8& nodeInRange : outNodeInRange)
    {
        nodeInRange = 0;
    }

    for (uint8& patchDrawn : outPatchDrawn)
    {
        patchDrawn = 0;
    }

    if (!layout.IsValid())
    {
        return;
    }

    const TerrainQuadtreeLayout::NodeKey topNode { layout.GetTopLevel(), 0, 0 };

    if (SelectTerrainQuadtreeNode(input, topNode, outNodeInRange, outPatchDrawn))
    {
        return;
    }

    // out of every range - the whole tile is drawn at the coarsest level
    for (uint32 quadrant = 0; quadrant < layout.GetPatchesPerNode(topNode.level); quadrant++)
    {
        outPatchDrawn[layout.GetPatchIndex({ topNode, quadrant })] = 1;
    }
}

#pragma endregion Selection

} // namespace Hyperion
