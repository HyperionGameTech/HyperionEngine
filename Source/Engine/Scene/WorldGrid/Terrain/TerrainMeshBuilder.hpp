/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Utilities/Span.hpp>

#include <Rendering/Vertex.hpp>

namespace Hyperion {

class TerrainMeshBuilder
{
public:
    struct CellMeshData
    {
        Array<SimpleVertex> vertices;
        Array<uint32> indices;
    };

    explicit TerrainMeshBuilder(uint32 cellSize);

    TerrainMeshBuilder(const TerrainMeshBuilder& other) = delete;
    TerrainMeshBuilder(TerrainMeshBuilder&& other) noexcept = delete;

    ~TerrainMeshBuilder();

    ///paddedHeights is (cellSize + 2 * TerrainGenerator::CellPadding)^2
    CellMeshData BuildCellVertexData(Span<const float> paddedHeights) const;

    ///the heightfield grid - skirt vertices are appended after these
    static constexpr uint32 CalculateGridVertexCount(uint32 cellSize)
    {
        return cellSize * cellSize;
    }

    ///4 strips of cellSize vertices hanging off the grid border
    static constexpr uint32 CalculateSkirtVertexCount(uint32 cellSize)
    {
        return 4u * cellSize;
    }

    static constexpr uint32 CalculateTotalVertexCount(uint32 cellSize)
    {
        return CalculateGridVertexCount(cellSize) + CalculateSkirtVertexCount(cellSize);
    }

    ///how far skirt vertices hang below the grid border, in local cell units
    static constexpr float CalculateSkirtDepth(uint32 cellSize)
    {
        return float(cellSize - 1) * (1.0f / 16.0f);
    }

    ///rebuilds the 4 skirt strips from grid vertices; outSkirtVertices must be CalculateSkirtVertexCount(cellSize) in size
    static void BuildSkirtVertices(
        uint32 cellSize,
        Span<const SimpleVertex> gridVertices,
        Span<SimpleVertex> outSkirtVertices);

private:
    uint32 m_cellSize;
};

} // namespace Hyperion
