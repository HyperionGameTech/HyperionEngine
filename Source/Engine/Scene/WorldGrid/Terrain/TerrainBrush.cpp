/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainBrush.hpp>
#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>
#include <Scene/WorldGrid/Terrain/TerrainStreamingCell.hpp>
#include <Scene/WorldGrid/Terrain/TerrainCellData.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Memory/Memory.hpp>

#include <Core/Threading/Threads.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(WorldGrid);

TerrainBrush::TerrainBrush(TerrainWorldGridLayer& layer)
    : m_layer(layer)
{
}

void TerrainBrush::ResetStroke()
{
    m_modifiedCells.Clear();
}

void TerrainBrush::Sculpt(const Vec3f& worldPos, float radius, float strength, bool raise)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (radius <= 0.0f || strength == 0.0f)
    {
        return;
    }

    const WorldGridLayerInfo& layerInfo = m_layer.GetLayerInfo();
    const uint32 cellSize = layerInfo.cellSize;
    const uint32 padding = TerrainGenerator::CellPadding;
    const uint32 paddedSize = cellSize + padding * 2u;

    const Vec2f scaleXZ(layerInfo.scale.x, layerInfo.scale.z);

    const float cellWorldSizeX = (float(cellSize) - 1.0f) * layerInfo.scale.x;
    const float cellWorldSizeZ = (float(cellSize) - 1.0f) * layerInfo.scale.z;

    if (cellWorldSizeX <= 0.0f || cellWorldSizeZ <= 0.0f || !m_layer.m_generatorState.GetGenerator())
    {
        return;
    }

    auto coordAt = [&](const Vec2f& worldXZ) -> Vec2f
    {
        return Vec2f(
            (worldXZ.x - layerInfo.offset.x) / cellWorldSizeX + 0.5f,
            (worldXZ.y - layerInfo.offset.z) / cellWorldSizeZ + 0.5f);
    };

    const Vec2f worldPosXZ(worldPos.x, worldPos.z);
    const Vec2f minCoordF = coordAt(worldPosXZ - Vec2f(radius, radius));
    const Vec2f maxCoordF = coordAt(worldPosXZ + Vec2f(radius, radius));

    const int32 minCoordX = int32(MathUtil::Floor(minCoordF.x)) - 1;
    const int32 minCoordZ = int32(MathUtil::Floor(minCoordF.y)) - 1;
    const int32 maxCoordX = int32(MathUtil::Ceil(maxCoordF.x)) + 1;
    const int32 maxCoordZ = int32(MathUtil::Ceil(maxCoordF.y)) + 1;

    const float heightChange = (raise ? 1.0f : -1.0f) * strength;
    const Vec2f paddingWorldSize = scaleXZ * float(padding);

    for (int32 cz = minCoordZ; cz <= maxCoordZ; cz++)
    {
        for (int32 cx = minCoordX; cx <= maxCoordX; cx++)
        {
            const Vec2i coord(cx, cz);

            const Vec3f cellBoundsMin = TerrainWorldGridLayer::ComputeCellBoundsMin(layerInfo, coord);
            const Vec2f cellWorldMinXZ(cellBoundsMin.x, cellBoundsMin.z);
            const Vec2f cellWorldMaxXZ = cellWorldMinXZ + Vec2f(cellWorldSizeX, cellWorldSizeZ);

            // the padding ring duplicates the neighbor's heights next to the border, so it's edited along with them
            const Vec2f closestPoint(
                MathUtil::Clamp(worldPosXZ.x, cellWorldMinXZ.x - paddingWorldSize.x, cellWorldMaxXZ.x + paddingWorldSize.x),
                MathUtil::Clamp(worldPosXZ.y, cellWorldMinXZ.y - paddingWorldSize.y, cellWorldMaxXZ.y + paddingWorldSize.y));

            if ((closestPoint - worldPosXZ).Length() > radius)
            {
                continue;
            }

            Handle<TerrainCellData> cellData = m_layer.FindCellData(coord);

            const bool isNewCellData = !cellData.IsValid();

            m_layer.m_heightsCache.GetSampleCache().Invalidate();

            const bool hasCurrentHeights = !isNewCellData
                && m_layer.AreCellHeightsCurrent(*cellData)
                && cellData->EnsureWritableHeights();

            Array<float> generatedHeights;
            Array<ubyte> generatedErosionMasks;

            if (!hasCurrentHeights)
            {
                m_layer.GenerateCellPaddedHeights(coord, generatedHeights, &generatedErosionMasks);
            }

            bool anyModified = false;

            int32 minVertexX = int32(cellSize);
            int32 minVertexZ = int32(cellSize);
            int32 maxVertexX = -1;
            int32 maxVertexZ = -1;

            const auto applyBrush = [&](Span<float> paddedHeights)
            {
                if (paddedHeights.Size() != size_t(paddedSize) * size_t(paddedSize))
                {
                    return;
                }

                for (uint32 pz = 0; pz < paddedSize; pz++)
                {
                    for (uint32 px = 0; px < paddedSize; px++)
                    {
                        const int32 localX = int32(px) - int32(padding);
                        const int32 localZ = int32(pz) - int32(padding);

                        const Vec2f sampleWorldXZ = cellWorldMinXZ + Vec2f(float(localX), float(localZ)) * scaleXZ;

                        const float dist = (sampleWorldXZ - worldPosXZ).Length();

                        if (dist > radius)
                        {
                            continue;
                        }

                        const float falloff = 1.0f - (dist / radius);
                        const float weight = falloff * falloff * (3.0f - 2.0f * falloff); // smoothstep

                        paddedHeights[size_t(pz) * paddedSize + px] += heightChange * weight;
                        anyModified = true;

                        // a moved padding sample changes the normal of the border vertex next to it
                        const int32 vertexX = MathUtil::Clamp(localX, 0, int32(cellSize) - 1);
                        const int32 vertexZ = MathUtil::Clamp(localZ, 0, int32(cellSize) - 1);

                        minVertexX = MathUtil::Min(minVertexX, vertexX);
                        minVertexZ = MathUtil::Min(minVertexZ, vertexZ);
                        maxVertexX = MathUtil::Max(maxVertexX, vertexX);
                        maxVertexZ = MathUtil::Max(maxVertexZ, vertexZ);
                    }
                }
            };

            if (hasCurrentHeights)
            {
                auto cellDataWriteScope = cellData->GetWriteScope();

                applyBrush(cellData->GetHeights());

                if (anyModified)
                {
                    cellData->isSculpted = true;
                    cellData->MarkDirty();
                }
            }
            else
            {
                applyBrush(generatedHeights.ToSpan());
            }

            if (!anyModified)
            {
                continue;
            }

            if (!hasCurrentHeights)
            {
                if (isNewCellData)
                {
                    cellData = MakeHandle<TerrainCellData>(NAME_FMT("TerrainCellData_{}_{}", coord.x, coord.y), coord, Vec3u(cellSize));
                }
                else if (cellData->isSculpted && cellData->HasHeights())
                {
                    HYP_LOG(WorldGrid, Warning, "Replacing unusable sculpted heights for cell {} with regenerated terrain", coord);
                }

                auto cellDataWriteScope = cellData->GetWriteScope();

                cellData->extent = Vec3u(cellSize);
                cellData->generatorFingerprint = m_layer.m_generatorState.GetCellFingerprint();
                cellData->isSculpted = true;
                cellData->SetHeights(generatedHeights);
                cellData->SetErosionMasks(ConstByteView(generatedErosionMasks.Data(), generatedErosionMasks.Size()));
            }

            if (isNewCellData)
            {
                m_layer.AddCellData(coord, cellData);
            }

            m_modifiedCells[coord] = true;

            if (Handle<TerrainStreamingCell> loadedCell = m_layer.m_loadedCells.Find(coord); loadedCell)
            {
                loadedCell->RebuildMesh(cellData, Vec2i(minVertexX, minVertexZ), Vec2i(maxVertexX, maxVertexZ));
            }
        }
    }
}

void TerrainBrush::Paint(const Vec3f& worldPos, float radius, float strength, uint32 layerIndex, bool erase)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (radius <= 0.0f || strength == 0.0f)
    {
        return;
    }

    layerIndex = MathUtil::Min(layerIndex, TerrainCellData::NumSplatLayers - 1);

    const WorldGridLayerInfo& layerInfo = m_layer.GetLayerInfo();
    const uint32 cellSize = layerInfo.cellSize;
    const float cellWorldSizeX = (float(cellSize) - 1.0f) * layerInfo.scale.x;
    const float cellWorldSizeZ = (float(cellSize) - 1.0f) * layerInfo.scale.z;

    if (cellWorldSizeX <= 0.0f || cellWorldSizeZ <= 0.0f)
    {
        return;
    }

    auto coordAt = [&](const Vec2f& worldXZ) -> Vec2f
    {
        return Vec2f(
            (worldXZ.x - layerInfo.offset.x) / cellWorldSizeX + 0.5f,
            (worldXZ.y - layerInfo.offset.z) / cellWorldSizeZ + 0.5f);
    };

    const Vec2f worldPosXZ(worldPos.x, worldPos.z);
    const Vec2f minCoordF = coordAt(worldPosXZ - Vec2f(radius, radius));
    const Vec2f maxCoordF = coordAt(worldPosXZ + Vec2f(radius, radius));

    const int32 minCoordX = int32(MathUtil::Floor(minCoordF.x)) - 1;
    const int32 minCoordZ = int32(MathUtil::Floor(minCoordF.y)) - 1;
    const int32 maxCoordX = int32(MathUtil::Ceil(maxCoordF.x)) + 1;
    const int32 maxCoordZ = int32(MathUtil::Ceil(maxCoordF.y)) + 1;

    for (int32 cz = minCoordZ; cz <= maxCoordZ; cz++)
    {
        for (int32 cx = minCoordX; cx <= maxCoordX; cx++)
        {
            const Vec2i coord(cx, cz);

            const Vec3f cellBoundsMin = TerrainWorldGridLayer::ComputeCellBoundsMin(layerInfo, coord);
            const Vec2f cellWorldMinXZ(cellBoundsMin.x, cellBoundsMin.z);
            const Vec2f cellWorldMaxXZ = cellWorldMinXZ + Vec2f(cellWorldSizeX, cellWorldSizeZ);

            const Vec2f closestPoint(
                MathUtil::Clamp(worldPosXZ.x, cellWorldMinXZ.x, cellWorldMaxXZ.x),
                MathUtil::Clamp(worldPosXZ.y, cellWorldMinXZ.y, cellWorldMaxXZ.y));

            if ((closestPoint - worldPosXZ).Length() > radius)
            {
                continue;
            }

            Handle<TerrainCellData> cellData = m_layer.FindCellData(coord);

            const bool isNewCellData = !cellData.IsValid();

            if (isNewCellData)
            {
                cellData = MakeHandle<TerrainCellData>(NAME_FMT("TerrainCellData_{}_{}", coord.x, coord.y), coord, Vec3u(cellSize));

                GetCurrentAssetRegistry()->PutAssetUnique(cellData);
            }

            m_layer.m_heightsCache.GetSampleCache().Invalidate();

            const bool hadSplatMap = cellData->HasSplatMap();

            if (!cellData->EnsureSplatMapAllocated(cellSize * cellSize))
            {
                continue;
            }

            bool anyModified = false;

            int32 minVertexX = int32(cellSize);
            int32 minVertexZ = int32(cellSize);
            int32 maxVertexX = -1;
            int32 maxVertexZ = -1;

            {
                auto cellDataWriteScope = cellData->GetWriteScope();

                ByteView splatMap = cellData->GetSplatMap();

                if (splatMap.Size() != 0)
                {
                    if (!hadSplatMap
                        && m_layer.m_generatorState.GetGenerator()
                        && m_layer.m_generatorState.GetGenerator()->GetParams().autoPaintSplats
                        && splatMap.Size() == size_t(cellSize) * size_t(cellSize) * TerrainCellData::NumSplatLayers)
                    {
                        // seed fresh splat maps from what the cell currently looks like
                        const uint32 paddedSize = cellSize + TerrainGenerator::CellPadding * 2u;

                        Array<float> generatedHeights;
                        Array<ubyte> generatedErosionMasks;

                        Span<const float> paddedHeights;
                        Span<const ubyte> erosionMasks;

                        if (!isNewCellData && m_layer.AreCellHeightsCurrent(*cellData))
                        {
                            const TerrainCellData& constCellData = *cellData;

                            paddedHeights = constCellData.GetHeights();

                            const ConstByteView savedErosionMasks = constCellData.GetErosionMasks();
                            erosionMasks = Span<const ubyte>(savedErosionMasks.Data(), savedErosionMasks.Size());
                        }

                        if (paddedHeights.Size() != size_t(paddedSize) * size_t(paddedSize))
                        {
                            m_layer.GenerateCellPaddedHeights(coord, generatedHeights, &generatedErosionMasks);

                            paddedHeights = generatedHeights.ToSpan();
                            erosionMasks = generatedErosionMasks.ToSpan();
                        }

                        m_layer.m_generatorState.GetGenerator()->SynthesizeSplatWeights(
                            paddedHeights,
                            erosionMasks,
                            cellWorldMinXZ,
                            Vec2f(layerInfo.scale.x, layerInfo.scale.z),
                            cellSize,
                            Span<ubyte>(reinterpret_cast<ubyte*>(splatMap.Data()), splatMap.Size()));

                        minVertexX = 0;
                        minVertexZ = 0;
                        maxVertexX = int32(cellSize) - 1;
                        maxVertexZ = int32(cellSize) - 1;
                    }

                    for (uint32 z = 0; z < cellSize; z++)
                    {
                        for (uint32 x = 0; x < cellSize; x++)
                        {
                            const Vec2f vertexWorldXZ = cellWorldMinXZ + Vec2f(float(x), float(z)) * Vec2f(layerInfo.scale.x, layerInfo.scale.z);

                            const float dist = (vertexWorldXZ - worldPosXZ).Length();

                            if (dist > radius)
                            {
                                continue;
                            }

                            const float falloff = 1.0f - (dist / radius);
                            const float weight = falloff * falloff * (3.0f - 2.0f * falloff); // smoothstep

                            const int32 paintDelta = int32(MathUtil::Clamp(strength * weight, 0.0f, 1.0f) * 255.0f);

                            if (paintDelta == 0)
                            {
                                continue;
                            }

                            const size_t baseIndex = (size_t(z) * cellSize + x) * TerrainCellData::NumSplatLayers;

                            ubyte& channel = splatMap[baseIndex + layerIndex];

                            const int32 oldValue = int32(channel);

                            minVertexX = MathUtil::Min(minVertexX, int32(x));
                            minVertexZ = MathUtil::Min(minVertexZ, int32(z));
                            maxVertexX = MathUtil::Max(maxVertexX, int32(x));
                            maxVertexZ = MathUtil::Max(maxVertexZ, int32(z));

                            if (erase)
                            {
                                channel = ubyte(MathUtil::Max(oldValue - paintDelta, 0));

                                anyModified |= channel != oldValue;
                            }
                            else
                            {
                                const int32 newValue = MathUtil::Min(oldValue + paintDelta, 255);

                                if (newValue != oldValue)
                                {
                                    // Pull the other layers down so the total stays at 255
                                    int32 othersTotal = 0;

                                    for (uint32 layer = 0; layer < TerrainCellData::NumSplatLayers; layer++)
                                    {
                                        if (layer != layerIndex)
                                        {
                                            othersTotal += int32(splatMap[baseIndex + layer]);
                                        }
                                    }

                                    channel = ubyte(newValue);

                                    const int32 targetOthers = MathUtil::Max(255 - newValue, 0);

                                    if (othersTotal > targetOthers)
                                    {
                                        for (uint32 layer = 0; layer < TerrainCellData::NumSplatLayers; layer++)
                                        {
                                            if (layer == layerIndex)
                                            {
                                                continue;
                                            }

                                            const int32 layerValue = int32(splatMap[baseIndex + layer]);

                                            splatMap[baseIndex + layer] = othersTotal != 0
                                                ? ubyte(layerValue * targetOthers / othersTotal)
                                                : ubyte(0);
                                        }
                                    }

                                    anyModified = true;
                                }
                            }
                        }
                    }

                    if (anyModified)
                    {
                        cellData->MarkDirty();
                    }
                }
            }

            if (!anyModified)
            {
                // The stroke did not actually touch this cell. Drop the default splat map that
                // EnsureSplatMapAllocated() may have created, otherwise it would override the
                // auto-painted splats when the cell is streamed again (e.g. after loading).
                if (!hadSplatMap)
                {
                    cellData->ClearSplatMap();
                }

                // Nothing painted - the freshly created cell data simply dies without being
                // registered.
                continue;
            }

            if (isNewCellData)
            {
                m_layer.AddCellData(coord, cellData);
            }

            m_modifiedCells[coord] = true;

            if (Handle<TerrainStreamingCell> loadedCell = m_layer.m_loadedCells.Find(coord); loadedCell)
            {
                loadedCell->UpdateSplatMaterial(cellData, Vec2i(minVertexX, minVertexZ), Vec2i(maxVertexX, maxVertexZ));
            }
        }
    }
}

void TerrainBrush::EndStroke()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    for (const KeyValuePair<Vec2i, bool>& pair : m_modifiedCells)
    {
        Handle<TerrainStreamingCell> loadedCell = m_layer.FindLoadedCell(pair.first);

        if (!loadedCell.IsValid())
        {
            // the heights were edited but no tile was there to rebuild, so nothing changed on screen. Once per
            // stroke rather than per brush dab, which runs every frame
            HYP_LOG(WorldGrid, Warning, "Sculpted cell {} of terrain layer '{}' has no loaded tile - the edit won't show until it streams in",
                pair.first, m_layer.GetName());

            continue;
        }

        loadedCell->RefreshAutoSplat();
    }

    m_modifiedCells.Clear();
}

} // namespace Hyperion
