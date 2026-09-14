/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/FixedArray.hpp>
#include <Core/Utilities/Span.hpp>

#include <Rendering/Vertex.hpp>
#include <Rendering/Mesh.hpp>

namespace Hyperion {

///UV1: U = height on the next coarser LOD's surface, V = height two LODs coarser (both clamped to the coarsest built LOD)
using TerrainVertex = TVertex<VT_Simple | VT_UV1>;

namespace TerrainMeshHelpers {

static constexpr uint8 MaxTerrainLods = 3;

static constexpr uint32 CalculateLodStride(uint8 lodIndex, uint32 strideMultiplier = 2)
{
    uint32 stride = 1;

    for (uint8 i = 0; i < lodIndex; i++)
    {
        stride *= strideMultiplier;
    }

    return stride;
}

static constexpr uint32 CalculateLodGridDimension(uint32 cellSize, uint8 lodIndex, uint32 strideMultiplier = 2)
{
    const uint32 stride = CalculateLodStride(lodIndex, strideMultiplier);

    return ((cellSize - 1) + (stride - 1)) / stride + 1;
}

static constexpr uint8 CalculateMaxLodIndex(uint32 cellSize, uint32 strideMultiplier = 2)
{
    uint8 lodIndex = 0;

    while (lodIndex + 1 < MaxTerrainLods
        && CalculateLodStride(lodIndex + 1, strideMultiplier) < cellSize - 1)
    {
        lodIndex++;
    }

    return lodIndex;
}

static constexpr uint32 CalculateGridVertexCount(uint32 gridDimension)
{
    return gridDimension * gridDimension;
}

static constexpr uint32 CalculateSkirtVertexCount(uint32 gridDimension)
{
    return 4u * gridDimension;
}

static constexpr uint32 CalculateTotalVertexCount(uint32 gridDimension)
{
    return CalculateGridVertexCount(gridDimension) + CalculateSkirtVertexCount(gridDimension);
}

static constexpr float CalculateSkirtDepth(uint32 gridDimension)
{
    return float(gridDimension - 1) * (1.0f / 16.0f);
}

void BuildSkirtVertices(
    uint32 gridDimension,
    Span<const TerrainVertex> gridVertices,
    Span<TerrainVertex> outSkirtVertices,
    float skirtDepth = -1.0f);

} // namespace TerrainMeshHelpers

class TerrainMeshBuilder
{
public:
    struct LodMeshData
    {
        ///grid vertices followed by skirt vertices (CalculateGridVertexCount(gridDimension) then CalculateSkirtVertexCount(gridDimension))
        Array<TerrainVertex> vertices;
        Array<uint32> indices;

        ///vertices per side of this LOD's grid (== cellSize for LOD 0)
        uint32 gridDimension = 0;

        ///max |sourceHeight - morphTargetHeight| across this LOD's grid vertices and both morph targets; 0 for the coarsest built LOD
        float geometricError = 0.0f;
    };

    struct CellMeshData
    {
        FixedArray<LodMeshData, MaxMeshLods> lods;
        uint8 numLods = 0;

        ///the LOD built into lods[0] - finer LODs were left out
        uint8 firstLodIndex = 0;
    };

    TerrainMeshBuilder(uint32 cellSize, uint8 numLods = 1, uint32 strideMultiplier = 2);

    TerrainMeshBuilder(const TerrainMeshBuilder& other) = delete;
    TerrainMeshBuilder(TerrainMeshBuilder&& other) noexcept = delete;

    ~TerrainMeshBuilder();

    ///paddedHeights is (cellSize + 2 * TerrainGenerator::CellPadding)^2; builds the LODs requested in the constructor from \p firstLodIndex on.
    ///morph targets are the same whichever LODs are left out
    CellMeshData BuildCellMeshData(Span<const float> paddedHeights, uint8 firstLodIndex = 0) const;

    HYP_FORCE_INLINE uint8 GetNumLods() const
    {
        return m_numLods;
    }

private:
    uint32 m_cellSize;
    uint32 m_strideMultiplier;
    uint8 m_numLods;
};

} // namespace Hyperion
