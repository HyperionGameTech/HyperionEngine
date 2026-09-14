/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Scene/WorldGrid/Terrain/TerrainQuadtree.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Utilities/Span.hpp>

#include <Rendering/Vertex.hpp>

namespace Hyperion {

///positions are in tile grid space. UV1: U = height on the next coarser level's surface, V = two levels coarser
///(both clamped to the tile's coarsest level)
using TerrainVertex = TVertex<VT_Simple | VT_UV1>;

namespace TerrainMeshHelpers {

static constexpr uint32 CalculateGridVertexCount(uint32 gridDimension)
{
    return gridDimension * gridDimension;
}

static constexpr uint32 CalculateSkirtVertexCount(uint32 gridDimension)
{
    return 4u * gridDimension;
}

void BuildSkirtVertices(
    uint32 gridDimension,
    Span<const TerrainVertex> gridVertices,
    Span<TerrainVertex> outSkirtVertices,
    float skirtDepth);

} // namespace TerrainMeshHelpers

struct TerrainPatchMeshData
{
    ///grid vertices followed by skirt vertices
    Array<TerrainVertex> vertices;
    Array<uint32> indices;
};

class TerrainMeshBuilder
{
public:
    TerrainMeshBuilder(uint32 cellSize, const TerrainQuadtreeLayout& layout);

    TerrainMeshBuilder(const TerrainMeshBuilder& other) = delete;
    TerrainMeshBuilder(TerrainMeshBuilder&& other) noexcept = delete;

    ~TerrainMeshBuilder();

    ///paddedHeights is (cellSize + 2 * TerrainGenerator::CellPadding)^2
    TerrainPatchMeshData BuildPatchMeshData(Span<const float> paddedHeights, uint32 patchIndex) const;

private:
    uint32 m_cellSize;
    TerrainQuadtreeLayout m_layout;
};

} // namespace Hyperion
