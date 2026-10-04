/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainCellTextures.hpp>
#include <Scene/WorldGrid/Terrain/TerrainCellData.hpp>
#include <Scene/WorldGrid/Terrain/Generation/TerrainGenerator.hpp>
#include <Scene/WorldGrid/Terrain/Generation/TerrainErosion.hpp>

#include <Streaming/StreamingCell.hpp>

#include <Rendering/Texture.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Memory/Memory.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(WorldGrid);

namespace TerrainCellTextures {

Handle<Texture> CreateCellTexture(Name name, uint32 cellSize, const Array<ubyte>& uploadBytes)
{
    Handle<Texture> texture = MakeHandle<Texture>();
    texture->SetName(name);

    TextureDesc textureDesc;
    textureDesc.type = TextureType::Texture2D;
    textureDesc.format = TextureFormat::RGBA8;
    textureDesc.extent = Vec3u(cellSize, cellSize, 1);
    textureDesc.filterModeMin = TextureFilterMode::Linear;
    textureDesc.filterModeMag = TextureFilterMode::Linear;

    texture->SetTextureDesc(textureDesc);
    texture->SetImageData(ConstByteView(uploadBytes.Data(), uploadBytes.Size()));
    texture->SetIsTransient(true);

    InitObject(texture);

    return texture;
}

Handle<Texture> CreateSplatTexture(const Vec2i& coord, uint32 cellSize, const Array<ubyte>& uploadBytes)
{
    return CreateCellTexture(NAME_FMT("TerrainCellSplatMap_{}", coord), cellSize, uploadBytes);
}

static void FlipSplatRowsForUpload(uint32 cellSize, const Array<ubyte>& splatBytes, Array<ubyte>& outUploadBytes)
{
    const size_t requiredSize = size_t(cellSize) * size_t(cellSize) * 4;

    outUploadBytes.Resize(requiredSize);

    const size_t rowSize = size_t(cellSize) * 4;

    for (uint32 z = 0; z < cellSize; z++)
    {
        const size_t srcRow = size_t(cellSize - 1 - z) * rowSize;
        const size_t dstRow = size_t(z) * rowSize;

        Memory::Copy(outUploadBytes.Data() + dstRow, splatBytes.Data() + srcRow, rowSize);
    }
}

static Handle<Texture> BuildSplatTextureFromWeights(const Vec2i& coord, uint32 cellSize, const Array<ubyte>& splatBytes)
{
    Array<ubyte> uploadBytes;
    FlipSplatRowsForUpload(cellSize, splatBytes, uploadBytes);

    return CreateSplatTexture(coord, cellSize, uploadBytes);
}

void PrepareNormalMapBytes(
    Span<const float> paddedHeights,
    Span<const ubyte> erosionMasks,
    uint32 cellSize,
    const Vec3f& scale,
    Array<ubyte>& outUploadBytes)
{
    const size_t texelCount = size_t(cellSize) * size_t(cellSize);
    const uint32 paddedSize = cellSize + TerrainGenerator::CellPadding * 2u;

    const bool hasErosionMasks = erosionMasks.Size() == texelCount * TerrainErosionMasks::NumChannels;

    Array<float> heights;
    Array<Vec3f> localNormals;
    TerrainGenerator::ExtractCellHeightsAndNormals(paddedHeights, cellSize, heights, localNormals);

    const auto paddedHeightAt = [&](int32 x, int32 z) -> float
    {
        return paddedHeights[size_t(z + int32(TerrainGenerator::CellPadding)) * paddedSize + size_t(x + int32(TerrainGenerator::CellPadding))];
    };

    Array<ubyte> normalBytes;
    normalBytes.Resize(texelCount * 4);

    const auto encodeUnorm = [](float value) -> ubyte
    {
        return ubyte(MathUtil::Clamp(value * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f + 0.5f);
    };

    const float sampleSpacing = MathUtil::Max((scale.x + scale.z) * 0.5f, 0.0001f);

    for (uint32 z = 0; z < cellSize; z++)
    {
        for (uint32 x = 0; x < cellSize; x++)
        {
            const size_t texelIndex = size_t(z) * cellSize + x;

            // grid normals are in the cell's local (unscaled) space
            const Vec3f localNormal = localNormals[texelIndex];
            const Vec3f worldNormal = Vec3f(localNormal.x / scale.x, localNormal.y / scale.y, localNormal.z / scale.z).Normalized();

            // sculpted detail the erosion masks never saw still reads as hollows and bumps
            const float neighborMean = (paddedHeightAt(int32(x) - 1, int32(z))
                + paddedHeightAt(int32(x) + 1, int32(z))
                + paddedHeightAt(int32(x), int32(z) - 1)
                + paddedHeightAt(int32(x), int32(z) + 1))
                * 0.25f;

            float concavity = (neighborMean - heights[texelIndex]) / sampleSpacing;

            if (hasErosionMasks)
            {
                concavity += TerrainErosionMasks::DecodeConcavity(erosionMasks[texelIndex * TerrainErosionMasks::NumChannels + TerrainErosionMasks::ConcavityChannel]);
            }

            normalBytes[texelIndex * 4] = encodeUnorm(worldNormal.x);
            normalBytes[texelIndex * 4 + 1] = encodeUnorm(worldNormal.y);
            normalBytes[texelIndex * 4 + 2] = encodeUnorm(worldNormal.z);
            normalBytes[texelIndex * 4 + 3] = ubyte(TerrainErosionMasks::EncodeConcavity(concavity) * 255.0f + 0.5f);
        }
    }

    FlipSplatRowsForUpload(cellSize, normalBytes, outUploadBytes);
}

///copies a painted splat map out of cell data and prepares it for upload
///call from streaming thread!
bool PreparePaintedSplatBytes(const Handle<TerrainCellData>& cellData, const Vec2i& coord, uint32 cellSize, Array<ubyte>& outUploadBytes)
{
    const size_t requiredSize = size_t(cellSize) * size_t(cellSize) * 4;

    Array<ubyte> splatBytes;

    {
        auto readScope = cellData->GetReadScope();

        ConstByteView splatData = static_cast<const TerrainCellData&>(*cellData).GetSplatMap();

        if (splatData.Size() < requiredSize)
        {
            if (splatData.Size() != 0)
            {
                HYP_LOG(WorldGrid, Warning,
                    "Saved splat map for cell {} is {} bytes but the layer expects {} !",
                    coord,
                    splatData.Size(),
                    requiredSize);
            }

            return false;
        }

        splatBytes.Resize(requiredSize);
        Memory::Copy(splatBytes.Data(), splatData.Data(), requiredSize);
    }

    FlipSplatRowsForUpload(cellSize, splatBytes, outUploadBytes);

    return true;
}

///synthesizes auto splat weights and prepares them for upload.
///always sampled at full resolution, since the splat texture is a fixed cellSize x cellSize regardless of the mesh LOD in use
bool PrepareAutoSplatBytes(
    const TerrainGenerator& generator,
    const StreamingCellInfo& cellInfo,
    Span<const float> paddedHeights,
    Span<const ubyte> erosionMasks,
    Array<ubyte>& outUploadBytes)
{
    const uint32 cellSize = cellInfo.extent.x;
    const uint32 paddedSize = cellSize + TerrainGenerator::CellPadding * 2u;

    if (paddedHeights.Size() != size_t(paddedSize) * size_t(paddedSize))
    {
        return false;
    }

    Array<ubyte> splatWeights;
    splatWeights.Resize(size_t(cellSize) * size_t(cellSize) * 4);

    generator.SynthesizeSplatWeights(
        paddedHeights,
        erosionMasks,
        Vec2f(cellInfo.bounds.min.x, cellInfo.bounds.min.z),
        Vec2f(cellInfo.scale.x, cellInfo.scale.z),
        cellSize,
        splatWeights);

    FlipSplatRowsForUpload(cellSize, splatWeights, outUploadBytes);

    return true;
}

Handle<Texture> BuildPaintedSplatTexture(const Handle<TerrainCellData>& cellData, const Vec2i& coord, uint32 cellSize)
{
    const size_t requiredSize = size_t(cellSize) * size_t(cellSize) * 4;

    Array<ubyte> splatBytes;

    {
        auto readScope = cellData->GetReadScope();

        ConstByteView splatData = static_cast<const TerrainCellData&>(*cellData).GetSplatMap();

        if (splatData.Size() < requiredSize)
        {
            if (splatData.Size() != 0)
            {
                HYP_LOG(WorldGrid, Warning,
                    "Saved splat map for cell {} is {} bytes but the layer expects {} !",
                    coord,
                    splatData.Size(),
                    requiredSize);
            }

            return Handle<Texture>::Null();
        }

        splatBytes.Resize(requiredSize);
        Memory::Copy(splatBytes.Data(), splatData.Data(), requiredSize);
    }

    return BuildSplatTextureFromWeights(coord, cellSize, splatBytes);
}

} // namespace TerrainCellTextures

} // namespace Hyperion
