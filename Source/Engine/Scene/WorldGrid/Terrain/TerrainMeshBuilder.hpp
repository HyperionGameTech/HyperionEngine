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

class TerrainGenerator;
struct StreamingCellInfo;

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

    ///builds vertex/index data for one terrain cell from the generator's full pipeline (incl. erosion), plus an optional sculpt delta view
    CellMeshData BuildCellVertexData(
        const StreamingCellInfo& cellInfo,
        const TerrainGenerator& generator,
        Span<const float> sculptDelta) const;

private:
    uint32 m_cellSize;
};

} // namespace Hyperion
