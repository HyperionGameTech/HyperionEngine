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

namespace {

TerrainGroundCoverPaintState ReadGroundCoverPaintState(
    const Handle<TerrainCellData>& cellData)
{
    TerrainGroundCoverPaintState state;

    if (!cellData.IsValid() || !cellData->HasGroundCoverPaint())
    {
        return state;
    }

    auto readScope = cellData->GetReadScope();

    state.layers = Array<Name, TerrainAllocator>(cellData->GetGroundCoverPaintLayers());

    const ConstByteView paint = cellData->GetGroundCoverPaint();

    state.paint.Resize(paint.Size());
    Memory::Copy(state.paint.Data(), paint.Data(), paint.Size());

    return state;
}

bool IsSameGroundCoverPaintState(
    const TerrainGroundCoverPaintState& first,
    const TerrainGroundCoverPaintState& second)
{
    if (first.layers.Size() != second.layers.Size()
        || first.paint.Size() != second.paint.Size())
    {
        return false;
    }

    for (size_t layerIndex = 0; layerIndex < first.layers.Size(); layerIndex++)
    {
        if (first.layers[layerIndex] != second.layers[layerIndex])
        {
            return false;
        }
    }

    for (size_t byteIndex = 0; byteIndex < first.paint.Size(); byteIndex++)
    {
        if (first.paint[byteIndex] != second.paint[byteIndex])
        {
            return false;
        }
    }

    return true;
}

} // namespace

#pragma region TerrainBrush

TerrainBrush::TerrainBrush(TerrainWorldGridLayer& layer)
    : m_layer(layer)
{
}

void TerrainBrush::ResetStroke()
{
    m_modifiedCells.Clear();
    m_groundCoverStrokeBefore.Clear();
    m_groundCoverPaintEdits.Clear();
}

Array<TerrainGroundCoverPaintEdit, TerrainAllocator> TerrainBrush::TakeGroundCoverPaintEdits()
{
    AssertOnThread(g_simThread);

    return std::move(m_groundCoverPaintEdits);
}

namespace {

// 1 up to \p inner, easing to 0 at \p outer
float FalloffWeight(float distance, float inner, float outer)
{
    if (distance <= inner)
    {
        return 1.0f;
    }

    if (distance >= outer)
    {
        return 0.0f;
    }

    const float t = 1.0f - (distance - inner) / (outer - inner);

    return t * t * (3.0f - 2.0f * t);
}

} // namespace

template <bool NeedsSource, class EditFn>
void TerrainBrush::EditHeights(const Vec4f& bounds, EditFn&& edit)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (bounds.z < bounds.x || bounds.w < bounds.y)
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
        return {
            (worldXZ.x - layerInfo.offset.x) / cellWorldSizeX + 0.5f,
            (worldXZ.y - layerInfo.offset.z) / cellWorldSizeZ + 0.5f
        };
    };

    const Vec2f boundsMin(bounds.x, bounds.y);
    const Vec2f boundsMax(bounds.z, bounds.w);

    const Vec2f minCoordF = coordAt(boundsMin);
    const Vec2f maxCoordF = coordAt(boundsMax);

    const int32 minCoordX = int32(MathUtil::Floor(minCoordF.x)) - 1;
    const int32 minCoordZ = int32(MathUtil::Floor(minCoordF.y)) - 1;
    const int32 maxCoordX = int32(MathUtil::Ceil(maxCoordF.x)) + 1;
    const int32 maxCoordZ = int32(MathUtil::Ceil(maxCoordF.y)) + 1;

    const Vec2f paddingWorldSize = scaleXZ * float(padding);

    Array<float, TerrainAllocator> source;

    for (int32 cz = minCoordZ; cz <= maxCoordZ; cz++)
    {
        for (int32 cx = minCoordX; cx <= maxCoordX; cx++)
        {
            const Vec2i coord(cx, cz);

            const Vec3f cellBoundsMin = TerrainWorldGridLayer::ComputeCellBoundsMin(layerInfo, coord);
            const Vec2f cellWorldMinXZ(cellBoundsMin.x, cellBoundsMin.z);
            const Vec2f cellWorldMaxXZ = cellWorldMinXZ + Vec2f(cellWorldSizeX, cellWorldSizeZ);

            // the padding ring duplicates the neighbor's heights next to the border, so it's edited along with them
            if (cellWorldMaxXZ.x + paddingWorldSize.x < boundsMin.x || cellWorldMinXZ.x - paddingWorldSize.x > boundsMax.x
                || cellWorldMaxXZ.y + paddingWorldSize.y < boundsMin.y || cellWorldMinXZ.y - paddingWorldSize.y > boundsMax.y)
            {
                continue;
            }

            Handle<TerrainCellData> cellData = m_layer.FindCellData(coord);

            const bool isNewCellData = !cellData.IsValid();

            m_layer.m_heightsCache.GetSampleCache().Invalidate();

            const bool hasCurrentHeights = !isNewCellData
                && m_layer.AreCellHeightsCurrent(*cellData)
                && cellData->EnsureWritableHeights();

            Array<float, TerrainAllocator> generatedHeights;
            Array<ubyte, TerrainAllocator> generatedErosionMasks;

            if (!hasCurrentHeights)
            {
                m_layer.GenerateCellPaddedHeights(coord, generatedHeights, &generatedErosionMasks);
            }

            bool anyModified = false;

            int32 minVertexX = int32(cellSize);
            int32 minVertexZ = int32(cellSize);
            int32 maxVertexX = -1;
            int32 maxVertexZ = -1;

            const auto applyEdit = [&](Span<float> paddedHeights)
            {
                if (paddedHeights.Size() != size_t(paddedSize) * size_t(paddedSize))
                {
                    return;
                }

                if constexpr (NeedsSource)
                {
                    source.Resize(paddedHeights.Size());
                    Memory::Copy(source.Data(), paddedHeights.Data(), paddedHeights.Size() * sizeof(float));
                }

                for (uint32 pz = 0; pz < paddedSize; pz++)
                {
                    for (uint32 px = 0; px < paddedSize; px++)
                    {
                        const int32 localX = int32(px) - int32(padding);
                        const int32 localZ = int32(pz) - int32(padding);

                        const Vec2f sampleWorldXZ = cellWorldMinXZ + Vec2f(float(localX), float(localZ)) * scaleXZ;

                        if (sampleWorldXZ.x < boundsMin.x || sampleWorldXZ.x > boundsMax.x
                            || sampleWorldXZ.y < boundsMin.y || sampleWorldXZ.y > boundsMax.y)
                        {
                            continue;
                        }

                        float& height = paddedHeights[size_t(pz) * paddedSize + px];
                        const float edited = edit(sampleWorldXZ, height, source.Data(), px, pz, paddedSize);

                        if (MathUtil::Abs(edited - height) < 1e-5f)
                        {
                            continue;
                        }

                        height = edited;
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

                applyEdit(cellData->GetMutableHeights());

                if (anyModified)
                {
                    cellData->isSculpted = true;
                    cellData->MarkDirty();
                }
            }
            else
            {
                applyEdit(generatedHeights.ToSpan());
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
                cellData->generatorFingerprint = m_layer.GetCellFingerprint(coord);
                cellData->isSculpted = true;
                cellData->SetHeights(generatedHeights);
                cellData->SetErosionMasks(ConstByteView(generatedErosionMasks.Data(), generatedErosionMasks.Size()));
            }

            if (isNewCellData)
            {
                m_layer.AddCellData(coord, cellData);
            }

            m_modifiedCells[coord] = true;

            if (Handle<TerrainStreamingCell> loadedCell = m_layer.m_loadedCells.Find(coord); loadedCell.IsValid())
            {
                loadedCell->RebuildMesh(cellData, Vec2i(minVertexX, minVertexZ), Vec2i(maxVertexX, maxVertexZ));
            }
        }
    }
}

void TerrainBrush::Sculpt(const Vec3f& worldPos, float radius, float strength, bool raise)
{
    if (radius <= 0.0f || strength == 0.0f)
    {
        return;
    }

    const Vec2f centerXZ(worldPos.x, worldPos.z);
    const float heightChange = (raise ? 1.0f : -1.0f) * strength;

    EditHeights<false>(
        Vec4f(centerXZ.x - radius, centerXZ.y - radius, centerXZ.x + radius, centerXZ.y + radius),
        [&](const Vec2f& sampleXZ, float height, const float*, uint32, uint32, uint32)
        {
            return height + heightChange * FalloffWeight((sampleXZ - centerXZ).Length(), 0.0f, radius);
        });
}

void TerrainBrush::FlattenCircle(const Vec2f& centerXZ, float radius, float falloff, float height)
{
    radius = MathUtil::Max(radius, 0.0f);
    falloff = MathUtil::Max(falloff, 0.0f);

    const float reach = radius + falloff;
    const float target = height - m_layer.GetLayerInfo().offset.y;

    EditHeights<false>(
        Vec4f(centerXZ.x - reach, centerXZ.y - reach, centerXZ.x + reach, centerXZ.y + reach),
        [&](const Vec2f& sampleXZ, float current, const float*, uint32, uint32, uint32)
        {
            return MathUtil::Lerp(current, target, FalloffWeight((sampleXZ - centerXZ).Length(), radius, reach));
        });
}

void TerrainBrush::FlattenRect(const Vec2f& minXZ, const Vec2f& maxXZ, float falloff, float height)
{
    falloff = MathUtil::Max(falloff, 0.0f);

    const Vec2f lo(MathUtil::Min(minXZ.x, maxXZ.x), MathUtil::Min(minXZ.y, maxXZ.y));
    const Vec2f hi(MathUtil::Max(minXZ.x, maxXZ.x), MathUtil::Max(minXZ.y, maxXZ.y));
    const float target = height - m_layer.GetLayerInfo().offset.y;

    EditHeights<false>(
        Vec4f(lo.x - falloff, lo.y - falloff, hi.x + falloff, hi.y + falloff),
        [&](const Vec2f& sampleXZ, float current, const float*, uint32, uint32, uint32)
        {
            const float dx = MathUtil::Max(MathUtil::Max(lo.x - sampleXZ.x, sampleXZ.x - hi.x), 0.0f);
            const float dz = MathUtil::Max(MathUtil::Max(lo.y - sampleXZ.y, sampleXZ.y - hi.y), 0.0f);

            return MathUtil::Lerp(current, target, FalloffWeight(Vec2f(dx, dz).Length(), 0.0f, falloff));
        });
}

void TerrainBrush::Ramp(const Vec3f& from, const Vec3f& to, float width, float falloff)
{
    const float halfWidth = MathUtil::Max(width, 0.0f) * 0.5f;
    falloff = MathUtil::Max(falloff, 0.0f);

    const float reach = halfWidth + falloff;
    const float offsetY = m_layer.GetLayerInfo().offset.y;

    const Vec2f a(from.x, from.z);
    const Vec2f b(to.x, to.z);
    const Vec2f ab = b - a;
    const float lengthSquared = ab.Dot(ab);

    EditHeights<false>(
        Vec4f(MathUtil::Min(a.x, b.x) - reach, MathUtil::Min(a.y, b.y) - reach, MathUtil::Max(a.x, b.x) + reach, MathUtil::Max(a.y, b.y) + reach),
        [&](const Vec2f& sampleXZ, float current, const float*, uint32, uint32, uint32)
        {
            const float t = lengthSquared > 1e-8f ? MathUtil::Clamp((sampleXZ - a).Dot(ab) / lengthSquared, 0.0f, 1.0f) : 0.0f;
            const float distance = (sampleXZ - (a + ab * t)).Length();
            const float target = MathUtil::Lerp(from.y, to.y, t) - offsetY;

            return MathUtil::Lerp(current, target, FalloffWeight(distance, halfWidth, reach));
        });
}

void TerrainBrush::Smooth(const Vec2f& centerXZ, float radius, float strength)
{
    if (radius <= 0.0f || strength <= 0.0f)
    {
        return;
    }

    strength = MathUtil::Min(strength, 1.0f);

    EditHeights<true>(
        Vec4f(centerXZ.x - radius, centerXZ.y - radius, centerXZ.x + radius, centerXZ.y + radius),
        [&](const Vec2f& sampleXZ, float current, const float* source, uint32 px, uint32 pz, uint32 paddedSize)
        {
            float sum = 0.0f;
            uint32 count = 0;

            for (int32 dz = -1; dz <= 1; dz++)
            {
                for (int32 dx = -1; dx <= 1; dx++)
                {
                    const int32 nx = int32(px) + dx;
                    const int32 nz = int32(pz) + dz;

                    if (nx < 0 || nz < 0 || nx >= int32(paddedSize) || nz >= int32(paddedSize))
                    {
                        continue;
                    }

                    sum += source[size_t(nz) * paddedSize + size_t(nx)];
                    count++;
                }
            }

            const float weight = strength * FalloffWeight((sampleXZ - centerXZ).Length(), 0.0f, radius);

            return MathUtil::Lerp(current, sum / float(count), weight);
        });
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
            const Vec2i coord = { cx, cz };

            const Vec3f cellBoundsMin = TerrainWorldGridLayer::ComputeCellBoundsMin(layerInfo, coord);
            const Vec2f cellWorldMinXZ = { cellBoundsMin.x, cellBoundsMin.z };
            const Vec2f cellWorldMaxXZ = cellWorldMinXZ + Vec2f { cellWorldSizeX, cellWorldSizeZ };

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

                ByteView splatMap = cellData->GetMutableSplatMap();

                if (splatMap.Size() != 0)
                {
                    if (!hadSplatMap
                        && m_layer.m_generatorState.GetGenerator()
                        && m_layer.m_generatorState.GetGenerator()->GetParams().autoPaintSplats
                        && splatMap.Size() == size_t(cellSize) * size_t(cellSize) * TerrainCellData::NumSplatLayers)
                    {
                        // seed fresh splat maps from what the cell currently looks like
                        const uint32 paddedSize = cellSize + TerrainGenerator::CellPadding * 2u;

                        Array<float, TerrainAllocator> generatedHeights;
                        Array<ubyte, TerrainAllocator> generatedErosionMasks;

                        Span<const float> paddedHeights;
                        Span<const ubyte> erosionMasks;

                        if (!isNewCellData && m_layer.AreCellHeightsCurrent(*cellData))
                        {
                            paddedHeights = cellData->GetHeights();

                            const ConstByteView savedErosionMasks = cellData->GetErosionMasks();
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

void TerrainBrush::PaintGroundCover(
    const Vec3f& worldPos,
    float radius,
    float strength,
    Name groundCoverLayer,
    bool erase)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (radius <= 0.0f || strength == 0.0f || !groundCoverLayer.IsValid())
    {
        return;
    }

    const WorldGridLayerInfo& layerInfo = m_layer.GetLayerInfo();
    const uint32 cellSize = layerInfo.cellSize;
    const size_t planeSize = size_t(cellSize) * cellSize;
    const float cellWorldSizeX = (float(cellSize) - 1.0f) * layerInfo.scale.x;
    const float cellWorldSizeZ = (float(cellSize) - 1.0f) * layerInfo.scale.z;

    if (cellWorldSizeX <= 0.0f || cellWorldSizeZ <= 0.0f)
    {
        return;
    }

    const Vec2f worldPosXZ(worldPos.x, worldPos.z);

    const Vec2i minCoord = TerrainWorldGridLayer::ComputeCellCoord(layerInfo, worldPosXZ - Vec2f(radius, radius)) - Vec2i(1, 1);
    const Vec2i maxCoord = TerrainWorldGridLayer::ComputeCellCoord(layerInfo, worldPosXZ + Vec2f(radius, radius)) + Vec2i(1, 1);

    for (int32 cz = minCoord.y; cz <= maxCoord.y; cz++)
    {
        for (int32 cx = minCoord.x; cx <= maxCoord.x; cx++)
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
                if (erase)
                {
                    continue;
                }

                cellData = MakeHandle<TerrainCellData>(NAME_FMT("TerrainCellData_{}_{}", coord.x, coord.y), coord, Vec3u(cellSize));
            }

            if (m_groundCoverStrokeBefore.Find(coord) == m_groundCoverStrokeBefore.End())
            {
                if (isNewCellData)
                {
                    m_groundCoverStrokeBefore.Emplace(coord);
                }
                else
                {
                    m_groundCoverStrokeBefore[coord] = ReadGroundCoverPaintState(cellData);
                }
            }

            const int32 paintPlane = erase
                ? cellData->FindGroundCoverPaintLayer(groundCoverLayer)
                : cellData->EnsureGroundCoverPaintLayer(groundCoverLayer, cellSize * cellSize);

            if (paintPlane < 0 || (erase && !cellData->EnsureWritableGroundCoverPaint()))
            {
                continue;
            }

            const uint32 numPlanes = uint32(cellData->GetGroundCoverPaintLayers().Size());

            bool anyModified = false;

            int32 minVertexX = int32(cellSize);
            int32 minVertexZ = int32(cellSize);
            int32 maxVertexX = -1;
            int32 maxVertexZ = -1;

            {
                auto cellDataWriteScope = cellData->GetWriteScope();

                ByteView paint = cellData->GetMutableGroundCoverPaint();

                if (paint.Size() != planeSize * numPlanes)
                {
                    continue;
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

                        const size_t vertexIndex = size_t(z) * cellSize + x;

                        ubyte& value = paint[planeSize * size_t(paintPlane) + vertexIndex];

                        const int32 oldValue = int32(value);
                        const int32 newValue = erase
                            ? MathUtil::Max(oldValue - paintDelta, 0)
                            : MathUtil::Min(oldValue + paintDelta, 255);

                        if (newValue == oldValue)
                        {
                            continue;
                        }

                        value = ubyte(newValue);

                        if (!erase)
                        {
                            int32 othersTotal = 0;

                            for (uint32 plane = 0; plane < numPlanes; plane++)
                            {
                                if (plane != uint32(paintPlane))
                                {
                                    othersTotal += int32(paint[planeSize * plane + vertexIndex]);
                                }
                            }

                            const int32 targetOthers = 255 - newValue;

                            if (othersTotal > targetOthers)
                            {
                                for (uint32 plane = 0; plane < numPlanes; plane++)
                                {
                                    if (plane == uint32(paintPlane))
                                    {
                                        continue;
                                    }

                                    ubyte& otherValue = paint[planeSize * plane + vertexIndex];

                                    otherValue = ubyte(int32(otherValue) * targetOthers / othersTotal);
                                }
                            }
                        }

                        anyModified = true;

                        minVertexX = MathUtil::Min(minVertexX, int32(x));
                        minVertexZ = MathUtil::Min(minVertexZ, int32(z));
                        maxVertexX = MathUtil::Max(maxVertexX, int32(x));
                        maxVertexZ = MathUtil::Max(maxVertexZ, int32(z));
                    }
                }

                if (anyModified)
                {
                    cellData->MarkDirty();
                }
            }

            if (!anyModified)
            {
                continue;
            }

            if (isNewCellData)
            {
                m_layer.AddCellData(coord, cellData);
            }

            if (Handle<TerrainStreamingCell> loadedCell = m_layer.m_loadedCells.Find(coord); loadedCell)
            {
                loadedCell->UpdateGroundCoverPaint(cellData, Vec2i(minVertexX, minVertexZ), Vec2i(maxVertexX, maxVertexZ));
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
            // the heights were edited but no tile was there to rebuild
            HYP_LOG(WorldGrid, Warning, "Sculpted cell {} of terrain layer '{}' has no loaded tile - the edit won't show until it streams in",
                pair.first, m_layer.GetName());

            continue;
        }

        loadedCell->RefreshAutoSplat();
    }

    m_modifiedCells.Clear();

    for (KeyValuePair<Vec2i, TerrainGroundCoverPaintState>& pair : m_groundCoverStrokeBefore)
    {
        TerrainGroundCoverPaintState after = ReadGroundCoverPaintState(m_layer.FindCellData(pair.first));

        if (IsSameGroundCoverPaintState(pair.second, after))
        {
            continue;
        }

        m_groundCoverPaintEdits.PushBack(TerrainGroundCoverPaintEdit { pair.first, std::move(pair.second), std::move(after) });
    }

    m_groundCoverStrokeBefore.Clear();
}

#pragma endregion TerrainBrush

} // namespace Hyperion
