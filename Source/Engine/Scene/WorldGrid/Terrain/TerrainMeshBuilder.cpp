/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainMeshBuilder.hpp>
#include <Scene/WorldGrid/Terrain/Generation/TerrainGenerator.hpp>

#include <Core/Math/MathUtil.hpp>

namespace Hyperion {

#pragma region Helpers

namespace TerrainMeshHelpers {

///grid-space coordinates of the LOD grid line at or before \p value and the one after it (equal when \p value lies on a grid line)
static Vec2u BracketLodGridCoordinate(uint32 cellSize, uint32 stride, uint32 value)
{
    if (value % stride == 0 || value == cellSize - 1)
    {
        return Vec2u { value, value };
    }

    const uint32 lower = (value / stride) * stride;
    const uint32 upper = MathUtil::Min(lower + stride, cellSize - 1);

    return Vec2u { lower, upper };
}

static float BuildLodGridVertices(
    uint32 cellSize,
    uint8 lodIndex,
    uint8 numLods,
    uint32 strideMultiplier,
    Span<const float> paddedHeights,
    Array<TerrainVertex>& outVertices)
{
    const uint32 padding = TerrainGenerator::CellPadding;
    const uint32 paddedPitch = cellSize + padding * 2u;

    Assert(paddedHeights.Size() == size_t(paddedPitch) * size_t(paddedPitch), "Padded heights have unexpected size");

    if (paddedHeights.Size() != size_t(paddedPitch) * size_t(paddedPitch))
    {
        return 0.0f;
    }

    const auto heightAt = [&](int32 x, int32 z) -> float
    {
        return paddedHeights[size_t(z + int32(padding)) * paddedPitch + size_t(x + int32(padding))];
    };

    // height of the surface a coarser LOD with the given stride renders at (x, z) - triangle diagonals must match BuildLodIndices()
    const auto lodSurfaceHeightAt = [&](uint32 lodStride, uint32 x, uint32 z) -> float
    {
        const Vec2u xBracket = BracketLodGridCoordinate(cellSize, lodStride, x);
        const Vec2u zBracket = BracketLodGridCoordinate(cellSize, lodStride, z);

        const float u = xBracket.y > xBracket.x ? float(x - xBracket.x) / float(xBracket.y - xBracket.x) : 0.0f;
        const float v = zBracket.y > zBracket.x ? float(z - zBracket.x) / float(zBracket.y - zBracket.x) : 0.0f;

        const float heightTopLeft = heightAt(int32(xBracket.x), int32(zBracket.x));
        const float heightTopRight = heightAt(int32(xBracket.y), int32(zBracket.x));
        const float heightBottomLeft = heightAt(int32(xBracket.x), int32(zBracket.y));
        const float heightBottomRight = heightAt(int32(xBracket.y), int32(zBracket.y));

        return u >= v
            ? heightTopLeft + u * (heightTopRight - heightTopLeft) + v * (heightBottomRight - heightTopRight)
            : heightTopLeft + v * (heightBottomLeft - heightTopLeft) + u * (heightBottomRight - heightBottomLeft);
    };

    const uint8 coarsestLodIndex = numLods - 1;

    const uint32 stride = CalculateLodStride(lodIndex, strideMultiplier);
    const uint32 nextLodStride = CalculateLodStride(MathUtil::Min<uint8>(lodIndex + 1, coarsestLodIndex), strideMultiplier);
    const uint32 secondLodStride = CalculateLodStride(MathUtil::Min<uint8>(lodIndex + 2, coarsestLodIndex), strideMultiplier);

    const uint32 dimension = CalculateLodGridDimension(cellSize, lodIndex, strideMultiplier);

    outVertices.Resize(CalculateGridVertexCount(dimension));

    float geometricError = 0.0f;

    for (uint32 k = 0; k < dimension; k++)
    {
        const uint32 wz = MathUtil::Min(k * stride, cellSize - 1);

        for (uint32 i = 0; i < dimension; i++)
        {
            const uint32 wx = MathUtil::Min(i * stride, cellSize - 1);

            const float height = heightAt(int32(wx), int32(wz));

            const Vec2f texcoord((float(wx) + 0.5f) / float(cellSize), (float(wz) + 0.5f) / float(cellSize));

            const Vec3f normal = TerrainGenerator::ComputeGridNormal(
                heightAt(int32(wx) - 1, int32(wz)),
                heightAt(int32(wx) + 1, int32(wz)),
                heightAt(int32(wx), int32(wz) - 1),
                heightAt(int32(wx), int32(wz) + 1));

            const float nextLodHeight = lodSurfaceHeightAt(nextLodStride, wx, wz);
            const float secondLodHeight = lodSurfaceHeightAt(secondLodStride, wx, wz);

            geometricError = MathUtil::Max(geometricError, MathUtil::Abs(height - nextLodHeight), MathUtil::Abs(height - secondLodHeight));

            TerrainVertex vertex;
            vertex.SetPosition(Vec3f { float(wx), height, float(wz) });
            vertex.SetNormal(normal);
            vertex.SetUV0(texcoord);
            vertex.SetUV1(Vec2f(nextLodHeight, secondLodHeight));

            outVertices[k * dimension + i] = vertex;
        }
    }

    return geometricError;
}

static Array<uint32> BuildLodIndices(uint32 gridDimension)
{
    const size_t gridIndexCount = size_t(6) * size_t(gridDimension - 1) * size_t(gridDimension - 1);
    const size_t skirtIndexCount = size_t(4) * size_t(6) * size_t(gridDimension - 1);

    Array<uint32> indices;
    indices.Resize(gridIndexCount + skirtIndexCount);

    uint32 pitch = gridDimension;
    uint32 row = 0;

    uint32 i0 = row;
    uint32 i1 = row + 1;
    uint32 i2 = pitch + i1;
    uint32 i3 = pitch + row;

    uint32 i = 0;

    for (uint32 z = 0; z < gridDimension - 1; z++)
    {
        for (uint32 x = 0; x < gridDimension - 1; x++)
        {
            // diagonal is always top-left(i0) to bottom-right(i2) - BuildLodGridVertices() relies on this
            // being consistent across every LOD to compute exact CDLOD morph targets
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
    const uint32 last = gridDimension - 1;

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

void BuildSkirtVertices(
    uint32 gridDimension,
    Span<const TerrainVertex> gridVertices,
    Span<TerrainVertex> outSkirtVertices,
    float skirtDepth)
{
    Assert(gridVertices.Size() >= CalculateGridVertexCount(gridDimension), "Grid vertex buffer too small");
    Assert(outSkirtVertices.Size() == CalculateSkirtVertexCount(gridDimension), "Skirt vertex buffer has unexpected size");

    if (skirtDepth < 0.0f)
    {
        skirtDepth = CalculateSkirtDepth(gridDimension);
    }

    const uint32 last = gridDimension - 1;

    for (uint32 k = 0; k < gridDimension; k++)
    {
        const TerrainVertex& northGrid = gridVertices[k];
        const TerrainVertex& southGrid = gridVertices[size_t(last) * gridDimension + k];
        const TerrainVertex& westGrid = gridVertices[size_t(k) * gridDimension];
        const TerrainVertex& eastGrid = gridVertices[size_t(k) * gridDimension + last];

        // skirts reuse the border vertex normal so any sliver visible through a crack shades like the adjacent
        // terrain, and offset their morph target by the same depth so they stay glued to their grid vertex at
        // every point in the morph, never opening their own crack

        const auto skirtOf = [skirtDepth](const TerrainVertex& gridVertex) -> TerrainVertex
        {
            const Vec3f position = gridVertex.GetPosition();
            const Vec2f uv1 = gridVertex.GetUV1();

            TerrainVertex vertex;
            vertex.SetPosition(Vec3f { position.x, position.y - skirtDepth, position.z });
            vertex.SetNormal(gridVertex.GetNormal());
            vertex.SetUV0(gridVertex.GetUV0());
            vertex.SetUV1(Vec2f(uv1.x - skirtDepth, uv1.y - skirtDepth));

            return vertex;
        };

        outSkirtVertices[k] = skirtOf(northGrid);
        outSkirtVertices[gridDimension + k] = skirtOf(southGrid);
        outSkirtVertices[gridDimension * 2u + k] = skirtOf(westGrid);
        outSkirtVertices[gridDimension * 3u + k] = skirtOf(eastGrid);
    }
}

} // namespace TerrainMeshHelpers

#pragma endregion Helpers

#pragma region TerrainMeshBuilder

TerrainMeshBuilder::TerrainMeshBuilder(uint32 cellSize, uint8 numLods, uint32 strideMultiplier)
    : m_cellSize(cellSize),
      m_strideMultiplier(MathUtil::Max<uint32>(strideMultiplier, 2)),
      m_numLods(MathUtil::Clamp<uint8>(numLods, 1, TerrainMeshHelpers::CalculateMaxLodIndex(cellSize, m_strideMultiplier) + 1))
{
}

TerrainMeshBuilder::~TerrainMeshBuilder() = default;

TerrainMeshBuilder::CellMeshData TerrainMeshBuilder::BuildCellMeshData(Span<const float> paddedHeights) const
{
    CellMeshData result;
    result.numLods = m_numLods;

    for (uint8 lodIndex = 0; lodIndex < m_numLods; lodIndex++)
    {
        LodMeshData& lodMeshData = result.lods[lodIndex];

        const uint32 dimension = TerrainMeshHelpers::CalculateLodGridDimension(m_cellSize, lodIndex, m_strideMultiplier);

        lodMeshData.gridDimension = dimension;

        Array<TerrainVertex> gridVertices;
        lodMeshData.geometricError = TerrainMeshHelpers::BuildLodGridVertices(m_cellSize, lodIndex, m_numLods, m_strideMultiplier, paddedHeights, gridVertices);

        const uint32 gridVertexCount = TerrainMeshHelpers::CalculateGridVertexCount(dimension);
        const uint32 skirtVertexCount = TerrainMeshHelpers::CalculateSkirtVertexCount(dimension);

        lodMeshData.vertices.Resize(gridVertexCount + skirtVertexCount);
        Memory::Copy(lodMeshData.vertices.Data(), gridVertices.Data(), gridVertexCount * sizeof(TerrainVertex));

        // widen the skirt to also cover this LOD's own geometric error
        const float skirtDepth = MathUtil::Max(TerrainMeshHelpers::CalculateSkirtDepth(m_cellSize), 2.0f * lodMeshData.geometricError);

        TerrainMeshHelpers::BuildSkirtVertices(
            dimension,
            Span<const TerrainVertex>(lodMeshData.vertices.Data(), gridVertexCount),
            Span<TerrainVertex>(lodMeshData.vertices.Data() + gridVertexCount, skirtVertexCount),
            skirtDepth);

        lodMeshData.indices = TerrainMeshHelpers::BuildLodIndices(dimension);
    }

    return result;
}

#pragma endregion TerrainMeshBuilder

} // namespace Hyperion
