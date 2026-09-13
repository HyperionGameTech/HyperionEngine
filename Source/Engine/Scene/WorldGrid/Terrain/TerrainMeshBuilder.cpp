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

    const int32 last = int32(cellSize) - 1;

    const auto sculptDeltaAt = [&](int32 x, int32 z) -> float
    {
        return sculptDelta[size_t(z) * cellSize + size_t(x)];
    };

    const auto heightAt = [&](int32 x, int32 z) -> float
    {
        const float height = paddedHeights[size_t(z + int32(margin)) * paddedPitch + size_t(x + int32(margin))];

        if (!hasSculptDelta)
        {
            return height;
        }

        const int32 clampedX = MathUtil::Clamp(x, 0, last);
        const int32 clampedZ = MathUtil::Clamp(z, 0, last);

        float delta = sculptDeltaAt(clampedX, clampedZ);

        if (x != clampedX)
        {
            delta += delta - sculptDeltaAt(clampedX + (x < 0 ? 1 : -1), clampedZ);
        }

        if (z != clampedZ)
        {
            delta += delta - sculptDeltaAt(clampedX, clampedZ + (z < 0 ? 1 : -1));
        }

        return height + delta;
    };

    Array<SimpleVertex> vertices;
    vertices.Resize(TerrainMeshBuilder::CalculateTotalVertexCount(cellSize));

    for (uint32 z = 0; z < cellSize; z++)
    {
        for (uint32 x = 0; x < cellSize; x++)
        {
            const uint32 i = z * cellSize + x;

            const Vec3f position = Vec3f { float(x), heightAt(int32(x), int32(z)), float(z) };

            // the splat map has one texel per vertex, so each vertex must sample its texel center -
            // otherwise the bilinear blend at the border pulls in texels that the neighbor cell doesn't see
            const Vec2f texcoord((float(x) + 0.5f) / float(cellSize), (float(z) + 0.5f) / float(cellSize));

            const Vec3f normal = TerrainGenerator::ComputeGridNormal(
                heightAt(int32(x) - 1, int32(z)),
                heightAt(int32(x) + 1, int32(z)),
                heightAt(int32(x), int32(z) - 1),
                heightAt(int32(x), int32(z) + 1));

            vertices[i] = SimpleVertex { position, normal, texcoord };
        }
    }

    const uint32 gridVertexCount = TerrainMeshBuilder::CalculateGridVertexCount(cellSize);
    const uint32 skirtVertexCount = TerrainMeshBuilder::CalculateSkirtVertexCount(cellSize);

    TerrainMeshBuilder::BuildSkirtVertices(
        cellSize,
        vertices.ToSpan(),
        Span<SimpleVertex>(vertices.Data() + gridVertexCount, skirtVertexCount));

    return vertices;
}

static Array<uint32> BuildIndices(uint32 cellSize)
{
    const size_t gridIndexCount = size_t(6 * (cellSize - 1) * (cellSize - 1));
    const size_t skirtIndexCount = size_t(4) * size_t(6) * size_t(cellSize - 1);

    Array<uint32> indices;
    indices.Resize(gridIndexCount + skirtIndexCount);

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

    // skirt walls around the grid border; walk each edge such that the wall faces away from the cell
    const uint32 skirtBase = pitch * pitch;
    const uint32 last = cellSize - 1;

    const auto emitSkirtQuad = [&indices, &i](uint32 g0, uint32 g1, uint32 s0, uint32 s1)
    {
        indices[i++] = g0;
        indices[i++] = s0;
        indices[i++] = s1;
        indices[i++] = g0;
        indices[i++] = s1;
        indices[i++] = g1;
    };

    // north edge (z = 0)
    for (uint32 k = 0; k < last; k++)
    {
        emitSkirtQuad(k, k + 1, skirtBase + k, skirtBase + k + 1);
    }

    // south edge (z = last)
    for (uint32 k = 0; k < last; k++)
    {
        const uint32 n = last - k;

        emitSkirtQuad(
            last * pitch + n,
            last * pitch + n - 1,
            skirtBase + pitch + n,
            skirtBase + pitch + n - 1);
    }

    // west edge (x = 0)
    for (uint32 k = 0; k < last; k++)
    {
        const uint32 n = last - k;

        emitSkirtQuad(
            n * pitch,
            (n - 1) * pitch,
            skirtBase + pitch * 2u + n,
            skirtBase + pitch * 2u + n - 1);
    }

    // east edge (x = last)
    for (uint32 k = 0; k < last; k++)
    {
        emitSkirtQuad(
            k * pitch + last,
            (k + 1) * pitch + last,
            skirtBase + pitch * 3u + k,
            skirtBase + pitch * 3u + k + 1);
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

void TerrainMeshBuilder::BuildSkirtVertices(
    uint32 cellSize,
    Span<const SimpleVertex> gridVertices,
    Span<SimpleVertex> outSkirtVertices)
{
    Assert(gridVertices.Size() >= CalculateGridVertexCount(cellSize), "Grid vertex buffer too small");
    Assert(outSkirtVertices.Size() == CalculateSkirtVertexCount(cellSize), "Skirt vertex buffer has unexpected size");

    const float skirtDepth = CalculateSkirtDepth(cellSize);
    const uint32 last = cellSize - 1;

    for (uint32 k = 0; k < cellSize; k++)
    {
        const SimpleVertex& northGrid = gridVertices[k];
        const SimpleVertex& southGrid = gridVertices[size_t(last) * cellSize + k];
        const SimpleVertex& westGrid = gridVertices[size_t(k) * cellSize];
        const SimpleVertex& eastGrid = gridVertices[size_t(k) * cellSize + last];

        const Vec3f northPosition = northGrid.GetPosition();
        const Vec3f southPosition = southGrid.GetPosition();
        const Vec3f westPosition = westGrid.GetPosition();
        const Vec3f eastPosition = eastGrid.GetPosition();

        // skirts reuse the border vertex normal so any sliver visible through a crack shades like the adjacent terrain

        // north strip (z = 0)
        outSkirtVertices[k] = SimpleVertex {
            Vec3f { northPosition.x, northPosition.y - skirtDepth, northPosition.z },
            northGrid.GetNormal(),
            northGrid.GetUV0()
        };

        // south strip (z = last)
        outSkirtVertices[cellSize + k] = SimpleVertex {
            Vec3f { southPosition.x, southPosition.y - skirtDepth, southPosition.z },
            southGrid.GetNormal(),
            southGrid.GetUV0()
        };

        // west strip (x = 0)
        outSkirtVertices[cellSize * 2u + k] = SimpleVertex {
            Vec3f { westPosition.x, westPosition.y - skirtDepth, westPosition.z },
            westGrid.GetNormal(),
            westGrid.GetUV0()
        };

        // east strip (x = last)
        outSkirtVertices[cellSize * 3u + k] = SimpleVertex {
            Vec3f { eastPosition.x, eastPosition.y - skirtDepth, eastPosition.z },
            eastGrid.GetNormal(),
            eastGrid.GetUV0()
        };
    }
}

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
