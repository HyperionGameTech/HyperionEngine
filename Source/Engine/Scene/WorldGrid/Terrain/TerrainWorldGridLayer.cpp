/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>
#include <Scene/WorldGrid/Terrain/TerrainStreamingCell.hpp>
#include <Scene/WorldGrid/Terrain/TerrainCellData.hpp>

#include <Scene/WorldGrid/WorldGrid.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetReference.hpp>

#include <Streaming/StreamingManager.hpp>

#include <Rendering/Material.hpp>
#include <Rendering/Texture.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Memory/Memory.hpp>

#include <Framework/EngineGlobals.hpp>

#include <random>

#include <TerrainWorldGridLayer.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(WorldGrid);

static const Name s_terrainWorldGridLayerName = NAME("TerrainWorldGridLayer");
static const Name s_terrainSceneName = NAME("TerrainScene");

static const WorldGridLayerInfo s_defaultTerrainWorldGridLayerInfo = {
    .cellSize = 64,
    .maxDistance = 5.0f
};

static Handle<Texture> LoadTerrainTexture(const char* name)
{
    auto tryLoadFromRegistry = [name](AssetRegistry& registry) -> Handle<Texture>
    {
        return DynamicCast<Texture>(registry.GetAsset(AssetBuckets::Textures, StringHash(name)));
    };

    Handle<Texture> texture;

    if (Handle<AssetRegistry> registry = GetCurrentAssetRegistry(); registry.IsValid())
    {
        texture = tryLoadFromRegistry(*registry);
    }

    if (!texture.IsValid())
    {
        if (Handle<AssetRegistry> registry = GetEngineAssetRegistry(); registry.IsValid())
        {
            texture = tryLoadFromRegistry(*registry);
        }
    }

#ifdef HYP_EDITOR
    if (!texture.IsValid())
    {
        if (Handle<AssetRegistry> registry = GetEditorAssetRegistry(); registry.IsValid())
        {
            texture = tryLoadFromRegistry(*registry);
        }
    }
#endif

    if (!texture.IsValid())
    {
        HYP_LOG(WorldGrid, Warning, "Cooked terrain texture '{}' not found in any asset registry", name);
    }

    return texture;
}

static void LoadTerrainMaterialTextures(MaterialTextures& textures)
{
    struct PerLayerTextures
    {
        MaterialTextureKey albedoKey;
        MaterialTextureKey normalKey;
        const char* albedoName;
        const char* normalName;
    };

    static constexpr PerLayerTextures Layers[] = {
        { MaterialTextureKey::TerrainLayer0, MaterialTextureKey::TerrainNormal0, "Terrain_Layer0_Albedo", "Terrain_Layer0_Normal" },
        { MaterialTextureKey::TerrainLayer1, MaterialTextureKey::TerrainNormal1, "Terrain_Layer1_Albedo", "Terrain_Layer1_Normal" },
        { MaterialTextureKey::TerrainLayer2, MaterialTextureKey::TerrainNormal2, "Terrain_Layer2_Albedo", "Terrain_Layer2_Normal" },
        { MaterialTextureKey::TerrainLayer3, MaterialTextureKey::TerrainNormal3, "Terrain_Layer3_Albedo", "Terrain_Layer3_Normal" }
    };

    for (const PerLayerTextures& layer : Layers)
    {
        textures[layer.albedoKey] = LoadTerrainTexture(layer.albedoName);
        textures[layer.normalKey] = LoadTerrainTexture(layer.normalName);
    }

    // NOTE: the splat map is not bound here - each painted cell gets its own splat texture on a per-cell material
}

static Handle<Scene> MakeTerrainScene()
{
    Handle<Scene> scene = MakeHandle<Scene>(s_terrainSceneName, SceneFlags::FOREGROUND | SceneFlags::HAS_OCTREE);

    // don't save; it's generated at runtime by the terrain layer
    scene->SetIsTransient(true);

    return scene;
}

#pragma region TerrainWorldGridLayer

TerrainWorldGridLayer::TerrainWorldGridLayer()
    : WorldGridLayer(s_terrainWorldGridLayerName, s_defaultTerrainWorldGridLayerInfo),
      m_scene(MakeTerrainScene()),
      m_generator(MakeUnique<TerrainGenerator>())
{
    m_layerInfo.seed = std::random_device()();
}

TerrainWorldGridLayer::TerrainWorldGridLayer(Name name, const WorldGridLayerInfo& layerInfo)
    : WorldGridLayer(name, layerInfo),
      m_scene(MakeTerrainScene()),
      m_generator(MakeUnique<TerrainGenerator>())
{
}

TerrainWorldGridLayer::~TerrainWorldGridLayer()
{
}

void TerrainWorldGridLayer::SetSeed(uint32 seed)
{
    if (m_layerInfo.seed == seed)
    {
        return;
    }

    m_layerInfo.seed = seed;

    if (m_generator)
    {
        Regenerate();
    }
}

void TerrainWorldGridLayer::SetLayerInfo(const WorldGridLayerInfo& layerInfo)
{
    WorldGridLayerInfo adjustedLayerInfo = layerInfo;
    adjustedLayerInfo.scale.y = 1.0f;

    const bool seedChanged = adjustedLayerInfo.seed != m_layerInfo.seed;

    WorldGridLayer::SetLayerInfo(adjustedLayerInfo);

    if (seedChanged && m_generator)
    {
        Regenerate();
    }
}

void TerrainWorldGridLayer::Regenerate()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!m_generator)
    {
        return;
    }

    TerrainGenerationParams params;
    params.seed = m_layerInfo.seed;

    m_generator->Configure(params);

    {
        Mutex::Guard guard(m_heightCacheMutex);

        m_cellHeightsCache.Clear();
    }

    m_deltaSampleCache.Invalidate();

    if (Handle<TerrainWorldGridLayer> strongThis = HandleFromThis(); strongThis.IsValid())
    {
        g_streamingManager->RequestLayerRefresh(strongThis.Get());
    }
}

void TerrainWorldGridLayer::OnAdded(WorldGrid* worldGrid)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    AssertDebug(worldGrid != nullptr);
    AssertDebug(m_scene.IsValid());

    AssertDebug(m_layerInfo.scale.y == 1.0f, "TerrainWorldGridLayer requires scale.y == 1.0f");

    World* world = worldGrid->GetWorld();
    AssertDebug(world != nullptr);

    m_scene->Initialize();

    MaterialAttributes attributes;
    attributes.shaderName = NAME("Terrain");
    attributes.bucket = RenderBucket::Opaque;
    attributes.flags |= MAF_DEPTH_TEST | MAF_DEPTH_WRITE;

    MaterialParameters parameters;
    parameters.albedo = Vec4f(0.06f, 0.25f, 0.05f, 1.0f);
    parameters.roughness = 0.95f;
    parameters.metalness = 0.0f;

    MaterialTextures textures;
    LoadTerrainMaterialTextures(textures);

    m_material = MakeHandle<Material>(NAME("TerrainMaterial"), attributes, parameters, textures);
    GetCurrentAssetRegistry()->PutAsset(m_material);

    InitObject(m_material);

    Regenerate();

    world->AddScene(m_scene);
}

void TerrainWorldGridLayer::OnRemoved(WorldGrid* worldGrid)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    AssertDebug(worldGrid != nullptr);
    AssertDebug(m_scene.IsValid());

    worldGrid->GetWorld()->RemoveScene(m_scene);
}

Handle<StreamingCell> TerrainWorldGridLayer::CreateStreamingCell(const StreamingCellInfo& cellInfo)
{
    if (!m_scene)
    {
        return Handle<StreamingCell>::Null();
    }

    Handle<TerrainCellData> cellData;

    auto objectsByCoordIt = m_objectsByCoord.Find(cellInfo.coord);

    if (objectsByCoordIt != m_objectsByCoord.End() && objectsByCoordIt->second.Any())
    {
        cellData = DynamicCast<TerrainCellData>(objectsByCoordIt->second[0].Resolve());
    }

    return MakeHandle<TerrainStreamingCell>(cellInfo, m_scene, m_material, HandleFromThis(), cellData);
}

void TerrainWorldGridLayer::RegisterLoadedCell(const Vec2i& coord, const WeakHandle<TerrainStreamingCell>& cell)
{
    m_loadedCells[coord] = cell;
}

void TerrainWorldGridLayer::UnregisterLoadedCell(const Vec2i& coord)
{
    m_loadedCells.Erase(coord);
}

static Vec3f ComputeCellBoundsMin(const WorldGridLayerInfo& layerInfo, const Vec2i& coord)
{
    return Vec3f {
        layerInfo.offset.x + (float(coord.x) - 0.5f) * (float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.x,
        layerInfo.offset.y,
        layerInfo.offset.z + (float(coord.y) - 0.5f) * (float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.z
    };
}

SharedPtr<const Array<float>> TerrainWorldGridLayer::GetOrGenerateCellHeights(const Vec2i& coord) const
{
    HYP_SCOPE;

    const WorldGridLayerInfo& layerInfo = m_layerInfo;

    AssertDebug(m_generator != nullptr);

    if (!m_generator)
    {
        return nullptr;
    }

    {
        Mutex::Guard guard(m_heightCacheMutex);

        auto cacheIt = m_cellHeightsCache.Find(coord);

        if (cacheIt != m_cellHeightsCache.End())
        {
            return cacheIt->second;
        }
    }

    const Vec3f cellBoundsMin = ComputeCellBoundsMin(layerInfo, coord);

    auto heights = MakeShared<Array<float>>();

    m_generator->GenerateCellHeights(
        Vec2f(cellBoundsMin.x, cellBoundsMin.z),
        Vec2f(layerInfo.scale.x, layerInfo.scale.z),
        layerInfo.cellSize,
        *heights);

    {
        Mutex::Guard guard(m_heightCacheMutex);

        auto cacheIt = m_cellHeightsCache.Find(coord);

        if (cacheIt != m_cellHeightsCache.End())
        {
            // another thread inserted the same cell while we were generating
            return cacheIt->second;
        }

        m_cellHeightsCache.Set(coord, heights);
    }

    return heights;
}

void TerrainWorldGridLayer::ApplyBrush(const Vec3f& worldPos, float radius, float strength, bool raise)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (radius <= 0.0f || strength == 0.0f)
    {
        return;
    }

    // a vertex normal reads its direct neighbors, so vertices one spacing outside the brush need new normals too -
    // including ones across a cell border whose own sculpt delta didn't change
    const float normalInfluenceRadius = radius + MathUtil::Max(m_layerInfo.scale.x, m_layerInfo.scale.z);

    const auto rebuildLoadedCell = [this](const Vec2i& coord, const Handle<TerrainCellData>& cellData, const Vec2i& minVertex, const Vec2i& maxVertex)
    {
        auto loadedCellIt = m_loadedCells.Find(coord);

        if (loadedCellIt == m_loadedCells.End())
        {
            return;
        }

        if (Handle<TerrainStreamingCell> loadedCell = loadedCellIt->second.Lock(); loadedCell)
        {
            loadedCell->RebuildMesh(cellData, minVertex, maxVertex);
        }
    };

    const WorldGridLayerInfo& layerInfo = m_layerInfo;
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

            const Vec3f cellBoundsMin = ComputeCellBoundsMin(layerInfo, coord);
            const Vec2f cellWorldMinXZ(cellBoundsMin.x, cellBoundsMin.z);
            const Vec2f cellWorldMaxXZ = cellWorldMinXZ + Vec2f(cellWorldSizeX, cellWorldSizeZ);

            const Vec2f closestPoint(
                MathUtil::Clamp(worldPosXZ.x, cellWorldMinXZ.x, cellWorldMaxXZ.x),
                MathUtil::Clamp(worldPosXZ.y, cellWorldMinXZ.y, cellWorldMaxXZ.y));

            if ((closestPoint - worldPosXZ).Length() > normalInfluenceRadius)
            {
                continue;
            }

            Handle<TerrainCellData> cellData;

            auto objectsByCoordIt = m_objectsByCoord.Find(coord);

            if (objectsByCoordIt != m_objectsByCoord.End() && objectsByCoordIt->second.Any())
            {
                cellData = DynamicCast<TerrainCellData>(objectsByCoordIt->second[0].Resolve());
            }

            const bool isNewCellData = !cellData.IsValid();

            if (isNewCellData)
            {
                cellData = MakeHandle<TerrainCellData>(NAME_FMT("TerrainCellData_{}_{}", coord.x, coord.y), coord, Vec3u(cellSize));

                GetCurrentAssetRegistry()->PutAssetUnique(cellData);
            }

            m_deltaSampleCache.Invalidate();

            const bool hadSculptDelta = cellData->HasSculptDelta();

            if (!cellData->EnsureWritableSculptDelta(cellSize * cellSize))
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

                ByteView delta = cellData->GetSculptDelta();

                if (delta.Size() != 0)
                {
                    for (uint32 z = 0; z < cellSize; z++)
                    {
                        for (uint32 x = 0; x < cellSize; x++)
                        {
                            const Vec2f vertexWorldXZ = cellWorldMinXZ + Vec2f(float(x), float(z)) * Vec2f(layerInfo.scale.x, layerInfo.scale.z);

                            const float dist = (vertexWorldXZ - worldPosXZ).Length();

                            if (dist > normalInfluenceRadius)
                            {
                                continue;
                            }

                            minVertexX = MathUtil::Min(minVertexX, int32(x));
                            minVertexZ = MathUtil::Min(minVertexZ, int32(z));
                            maxVertexX = MathUtil::Max(maxVertexX, int32(x));
                            maxVertexZ = MathUtil::Max(maxVertexZ, int32(z));

                            if (dist > radius)
                            {
                                continue;
                            }

                            const float falloff = 1.0f - (dist / radius);
                            const float weight = falloff * falloff * (3.0f - 2.0f * falloff); // smoothstep

                            reinterpret_cast<float*>(delta.Data())[z * cellSize + x] += (raise ? 1.0f : -1.0f) * strength * weight;
                            anyModified = true;
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
                // The stroke did not actually touch this cell. Drop the empty sculpt delta that
                // EnsureWritableSculptDelta() may have created so no pointless data is persisted.
                if (!hadSculptDelta)
                {
                    cellData->ClearSculptDelta();
                }

                // border normals may still read vertices the stroke moved in the adjacent cell
                if (maxVertexX >= 0)
                {
                    rebuildLoadedCell(
                        coord,
                        isNewCellData ? Handle<TerrainCellData>() : cellData,
                        Vec2i(minVertexX, minVertexZ),
                        Vec2i(maxVertexX, maxVertexZ));
                }

                continue;
            }

            if (isNewCellData)
            {
                AddStreamingObject(cellData.Get(), coord);
            }

            m_cellsModifiedSinceStrokeEnd[coord] = true;

            rebuildLoadedCell(coord, cellData, Vec2i(minVertexX, minVertexZ), Vec2i(maxVertexX, maxVertexZ));
        }
    }
}

void TerrainWorldGridLayer::PaintSplat(const Vec3f& worldPos, float radius, float strength, uint32 layerIndex, bool erase)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (radius <= 0.0f || strength == 0.0f)
    {
        return;
    }

    layerIndex = MathUtil::Min(layerIndex, TerrainCellData::NumSplatLayers - 1);

    const WorldGridLayerInfo& layerInfo = m_layerInfo;
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

            const Vec3f cellBoundsMin = ComputeCellBoundsMin(layerInfo, coord);
            const Vec2f cellWorldMinXZ(cellBoundsMin.x, cellBoundsMin.z);
            const Vec2f cellWorldMaxXZ = cellWorldMinXZ + Vec2f(cellWorldSizeX, cellWorldSizeZ);

            const Vec2f closestPoint(
                MathUtil::Clamp(worldPosXZ.x, cellWorldMinXZ.x, cellWorldMaxXZ.x),
                MathUtil::Clamp(worldPosXZ.y, cellWorldMinXZ.y, cellWorldMaxXZ.y));

            if ((closestPoint - worldPosXZ).Length() > radius)
            {
                continue;
            }

            Handle<TerrainCellData> cellData;

            auto objectsByCoordIt = m_objectsByCoord.Find(coord);

            if (objectsByCoordIt != m_objectsByCoord.End() && objectsByCoordIt->second.Any())
            {
                cellData = DynamicCast<TerrainCellData>(objectsByCoordIt->second[0].Resolve());
            }

            const bool isNewCellData = !cellData.IsValid();

            if (isNewCellData)
            {
                cellData = MakeHandle<TerrainCellData>(NAME_FMT("TerrainCellData_{}_{}", coord.x, coord.y), coord, Vec3u(cellSize));

                GetCurrentAssetRegistry()->PutAssetUnique(cellData);
            }

            m_deltaSampleCache.Invalidate();

            const bool hadSplatMap = cellData->HasSplatMap();

            if (!cellData->EnsureSplatMapAllocated(cellSize * cellSize))
            {
                continue;
            }

            bool anyModified = false;

            {
                auto cellDataWriteScope = cellData->GetWriteScope();

                ByteView splatMap = cellData->GetSplatMap();

                if (splatMap.Size() != 0)
                {
                    if (isNewCellData && !hadSplatMap && m_generator && m_generator->GetParams().autoPaintSplats)
                    {
                        // seed fresh splat maps
                        if (splatMap.Size() == size_t(cellSize) * size_t(cellSize) * TerrainCellData::NumSplatLayers)
                        {
                            Array<float> heights;
                            Array<Vec3f> normals;

                            m_generator->GenerateCellHeightsAndNormals(
                                cellWorldMinXZ,
                                Vec2f(layerInfo.scale.x, layerInfo.scale.z),
                                cellSize,
                                heights,
                                normals);

                            m_generator->SynthesizeSplatWeights(
                                heights,
                                normals,
                                cellWorldMinXZ,
                                Vec2f(layerInfo.scale.x, layerInfo.scale.z),
                                cellSize,
                                Span<ubyte>(reinterpret_cast<ubyte*>(splatMap.Data()), splatMap.Size()));
                        }
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
                                    // Pull the other layers down so the total stays at 255 - a fully
                                    // painted layer needs to reach full weight, otherwise its normal
                                    // map blends away into the other layers.
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
                AddStreamingObject(cellData.Get(), coord);
            }

            m_cellsModifiedSinceStrokeEnd[coord] = true;

            auto loadedCellIt = m_loadedCells.Find(coord);

            if (loadedCellIt != m_loadedCells.End())
            {
                if (Handle<TerrainStreamingCell> loadedCell = loadedCellIt->second.Lock(); loadedCell)
                {
                    loadedCell->UpdateSplatMaterial(cellData);
                }
            }
        }
    }
}

void TerrainWorldGridLayer::DeltaSampleCache::Invalidate()
{
    HYP_SCOPE;
    
    blobData = ConstByteView();

    scope.Reset();
    cell.Reset();
}

float TerrainWorldGridLayer::SampleHeightAt(const Vec2f& worldXZ) const
{
    HYP_SCOPE;

    const WorldGridLayerInfo& layerInfo = m_layerInfo;
    const uint32 cellSize = layerInfo.cellSize;

    const float cellWorldSizeX = (float(cellSize) - 1.0f) * layerInfo.scale.x;
    const float cellWorldSizeZ = (float(cellSize) - 1.0f) * layerInfo.scale.z;

    if (cellWorldSizeX <= 0.0f || cellWorldSizeZ <= 0.0f || !m_generator)
    {
        return 0.0f;
    }

    const Vec2f coordF(
        (worldXZ.x - layerInfo.offset.x) / cellWorldSizeX + 0.5f,
        (worldXZ.y - layerInfo.offset.z) / cellWorldSizeZ + 0.5f);

    const Vec2i coord(int32(MathUtil::Floor(coordF.x)), int32(MathUtil::Floor(coordF.y)));

    float height = 0.0f;

    if (SharedPtr<const Array<float>> heights = GetOrGenerateCellHeights(coord); heights.IsValid()
        && heights->Size() == size_t(cellSize) * size_t(cellSize))
    {
        const Vec3f cellBoundsMin = ComputeCellBoundsMin(layerInfo, coord);

        const int32 lx = int32(MathUtil::Floor((worldXZ.x - cellBoundsMin.x) / layerInfo.scale.x + 0.5f));
        const int32 lz = int32(MathUtil::Floor((worldXZ.y - cellBoundsMin.z) / layerInfo.scale.z + 0.5f));

        if (lx >= 0 && lx < int32(cellSize) && lz >= 0 && lz < int32(cellSize))
        {
            height = (*heights)[size_t(lz) * size_t(cellSize) + size_t(lx)];
        }
    }

    auto objectsByCoordIt = m_objectsByCoord.Find(coord);

    if (objectsByCoordIt == m_objectsByCoord.End() || !objectsByCoordIt->second.Any())
    {
        return height;
    }

    Handle<TerrainCellData> cellData = DynamicCast<TerrainCellData>(objectsByCoordIt->second[0].Resolve());

    if (!cellData.IsValid())
    {
        return height;
    }

    if (m_deltaSampleCache.cell != cellData)
    {
        m_deltaSampleCache.scope.Reset(*cellData);

        m_deltaSampleCache.cell = cellData;
        m_deltaSampleCache.blobData = cellData->GetSculptDelta();
    }

    ConstByteView blobData = m_deltaSampleCache.blobData;

    if (blobData.Size() != size_t(cellSize) * size_t(cellSize) * sizeof(float))
    {
        return height;
    }

    const Vec3f cellBoundsMin = ComputeCellBoundsMin(layerInfo, coord);

    const int32 lx = int32(MathUtil::Floor((worldXZ.x - cellBoundsMin.x) / layerInfo.scale.x + 0.5f));
    const int32 lz = int32(MathUtil::Floor((worldXZ.y - cellBoundsMin.z) / layerInfo.scale.z + 0.5f));

    if (lx < 0 || lx >= int32(cellSize) || lz < 0 || lz >= int32(cellSize))
    {
        return height;
    }

    return height + reinterpret_cast<const float*>(blobData.Data())[size_t(lz) * size_t(cellSize) + size_t(lx)];
}

bool TerrainWorldGridLayer::RaycastSurface(const Ray& ray, Vec3f& outHitPoint) const
{
    HYP_SCOPE;

    const WorldGridLayerInfo& layerInfo = m_layerInfo;

    const float maxHeight = m_generator
        ? m_generator->GetMaxHeightEstimate()
        : 256.0f;

    const float slabMinY = layerInfo.offset.y - 256.0f;
    const float slabMaxY = layerInfo.offset.y + maxHeight * 2.0f + 256.0f;

    float tEnter = 0.0f;
    float tExit = 8192.0f;

    if (MathUtil::Abs(ray.direction.y) > MathUtil::epsilonF)
    {
        const float t0 = (slabMinY - ray.position.y) / ray.direction.y;
        const float t1 = (slabMaxY - ray.position.y) / ray.direction.y;

        tEnter = MathUtil::Max(tEnter, MathUtil::Min(t0, t1));
        tExit = MathUtil::Min(tExit, MathUtil::Max(t0, t1));
    }
    else if (ray.position.y < slabMinY || ray.position.y > slabMaxY)
    {
        return false;
    }

    if (tEnter >= tExit)
    {
        return false;
    }

    const float minStep = MathUtil::Max(MathUtil::Max(layerInfo.scale.x, layerInfo.scale.z) * 0.5f, 0.25f);
    const float maxStep = minStep * 16.0f;

    const auto signedDistance = [&](float t) -> float
    {
        const Vec3f p = ray.position + ray.direction * t;

        return p.y - SampleHeightAt(Vec2f(p.x, p.z));
    };

    float t0 = tEnter;
    float s0 = signedDistance(t0);

    if (s0 <= 0.0f)
    {
        outHitPoint = ray.position + ray.direction * t0;

        return true;
    }

    for (uint32 step = 0; step < 1024 && t0 < tExit; step++)
    {
        const float t1 = MathUtil::Min(t0 + MathUtil::Clamp(s0 * 0.8f, minStep, maxStep), tExit);
        const float s1 = signedDistance(t1);

        if (s1 <= 0.0f)
        {
            float a = t0;
            float b = t1;

            for (uint32 i = 0; i < 8; i++)
            {
                const float mid = (a + b) * 0.5f;

                if (signedDistance(mid) <= 0.0f)
                {
                    b = mid;
                }
                else
                {
                    a = mid;
                }
            }

            outHitPoint = ray.position + ray.direction * b;

            return true;
        }

        t0 = t1;
        s0 = s1;
    }

    return false;
}

void TerrainWorldGridLayer::EndBrushStroke()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    for (const KeyValuePair<Vec2i, bool>& pair : m_cellsModifiedSinceStrokeEnd)
    {
        auto loadedCellIt = m_loadedCells.Find(pair.first);

        if (loadedCellIt == m_loadedCells.End())
        {
            continue;
        }

        if (Handle<TerrainStreamingCell> loadedCell = loadedCellIt->second.Lock(); loadedCell)
        {
            loadedCell->RebuildPickBVH();
            loadedCell->RefreshAutoSplat();
        }
    }

    m_cellsModifiedSinceStrokeEnd.Clear();
}

#pragma endregion TerrainWorldGridLayer

} // namespace Hyperion
