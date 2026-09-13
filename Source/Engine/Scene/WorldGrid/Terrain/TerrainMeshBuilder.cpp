/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainMeshBuilder.hpp>
#include <Scene/WorldGrid/Terrain/Generation/TerrainGenerator.hpp>

#include <Streaming/StreamingCell.hpp>

namespace Hyperion {

#pragma region Helpers

static Array<SimpleVertex> BuildVertices(
    uint32 cellSize,
    const StreamingCellInfo& cellInfo,
    const TerrainGenerator& generator,
    Span<const float> sculptDelta)
{
    const Vec2f cellWorldMinXZ(cellInfo.bounds.min.x, cellInfo.bounds.min.z);
    const Vec2f scaleXZ(cellInfo.scale.x, cellInfo.scale.z);

    Array<float> paddedHeights;
    generator.GeneratePaddedCellHeights(cellWorldMinXZ, scaleXZ, cellSize, paddedHeights);

    const uint32 margin = generator.GetErosionMargin();
    const uint32 paddedPitch = cellSize + margin * 2u;

    const bool hasSculptDelta = sculptDelta.Size() > 0;

    if (hasSculptDelta)
    {
        Assert(sculptDelta.Size() == size_t(cellSize) * size_t(cellSize),
            "Bad sculpt deltas !!! BAD!!!!");

        // for debugging so we don't kill the whole thing
        if (sculptDelta.Size() != size_t(cellSize) * size_t(cellSize))
        {
            return {};
        }
    }

    const auto paddedHeightAt = [&](int32 x, int32 z) -> float
    {
        return paddedHeights[size_t(z + int32(margin)) * paddedPitch + size_t(x + int32(margin))];
    };

    Array<SimpleVertex> vertices;
    vertices.Resize(cellSize * cellSize);

    for (uint32 z = 0; z < cellSize; z++)
    {
        for (uint32 x = 0; x < cellSize; x++)
        {
            const uint32 i = z * cellSize + x;

            float h = paddedHeightAt(int32(x), int32(z));

            if (hasSculptDelta)
            {
                h += sculptDelta[i];
            }

            const float hL = paddedHeightAt(int32(x) - 1, int32(z));
            const float hR = paddedHeightAt(int32(x) + 1, int32(z));
            const float hD = paddedHeightAt(int32(x), int32(z) - 1);
            const float hU = paddedHeightAt(int32(x), int32(z) + 1);

            const Vec3f position = Vec3f { float(x), h, float(z) };
            const Vec2f texcoord(float(x) / float(cellSize), float(z) / float(cellSize));

            const Vec3f tangentX(2.0f, hR - hL, 0.0f);
            const Vec3f tangentZ(0.0f, hU - hD, 2.0f);
            const Vec3f normal = tangentZ.Cross(tangentX).Normalized();

            vertices[i] = SimpleVertex { position, normal, texcoord };
        }
    }

    return vertices;
}

static Array<uint32> BuildIndices(uint32 cellSize)
{
    Array<uint32> indices;
    indices.Resize(size_t(6 * (cellSize - 1) * (cellSize - 1)));

    uint32 pitch = uint32(cellSize);
    uint32 row = 0;

    uint32 i0 = row;
    uint32 i1 = row + 1;
    uint32 i2 = pitch + i1;
    uint32 i3 = pitch + row;

    uint32 i = 0;

    for (uint32 z = 0; z < cellSize - 1; z++)
    {
        for (uint32 x = 0; x < cellSize - 1; x++)
        {
            indices[i++] = i0;
            indices[i++] = i2;
            indices[i++] = i3;
            indices[i++] = i0;
            indices[i++] = i1;
            indices[i++] = i2;

            i0++;
            i1++;
            i2++;
            i3++;
        }

        row += pitch;

        i0 = row;
        i1 = row + 1;
        i2 = pitch + i1;
        i3 = pitch + row;
    }

    return indices;
}

#pragma endregion Helpers

#pragma region TerrainMeshBuilder

TerrainMeshBuilder::TerrainMeshBuilder(uint32 cellSize)
    : m_cellSize(cellSize)
{
}

TerrainMeshBuilder::~TerrainMeshBuilder() = default;

TerrainMeshBuilder::CellMeshData TerrainMeshBuilder::BuildCellVertexData(
    const StreamingCellInfo& cellInfo,
    const TerrainGenerator& generator,
    Span<const float> sculptDelta) const
{
    CellMeshData result;
    result.vertices = BuildVertices(m_cellSize, cellInfo, generator, sculptDelta);
    result.indices = BuildIndices(m_cellSize);

    return result;
}

#pragma endregion TerrainMeshBuilder

} // namespace Hyperion
