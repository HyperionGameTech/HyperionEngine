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
#include <Core/HashCode.hpp>
#include <Core/Memory/Memory.hpp>
#include <Core/Threading/TaskSystem.hpp>

#include <Framework/EngineGlobals.hpp>

#ifdef HYP_EDITOR
#include <Editor/EditorTask.hpp>
#include <Editor/EditorState.hpp>
#endif

#include <random>

#include <TerrainWorldGridLayer.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(WorldGrid);

static const Name s_terrainWorldGridLayerName = NAME("TerrainWorldGridLayer");
static const Name s_terrainSceneName = NAME("TerrainScene");

static const WorldGridLayerInfo s_defaultTerrainWorldGridLayerInfo = {
    .scale = Vec3f { 1.0f, 1.0f, 1.0f },
    .cellSize = 257,
    .maxDistance = 8.0f,
    .seed = 1951233096
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
      m_generator(MakeShared<TerrainGenerator>())
{
    m_layerInfo.seed = std::random_device()();
}

TerrainWorldGridLayer::TerrainWorldGridLayer(Name name, const WorldGridLayerInfo& layerInfo)
    : WorldGridLayer(name, layerInfo),
      m_scene(MakeTerrainScene()),
      m_generator(MakeShared<TerrainGenerator>())
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

    Mutex::Guard guard(m_generationStateMutex);

    m_layerInfo.seed = seed;
}

void TerrainWorldGridLayer::SetLayerInfo(const WorldGridLayerInfo& layerInfo)
{
    WorldGridLayerInfo adjustedLayerInfo = layerInfo;
    adjustedLayerInfo.scale.y = 1.0f;

    const bool cellGeometryChanged = adjustedLayerInfo.cellSize != m_layerInfo.cellSize
        || adjustedLayerInfo.scale != m_layerInfo.scale
        || adjustedLayerInfo.offset != m_layerInfo.offset;

    const uint64 cellFingerprint = m_generator ? ComputeCellFingerprint(*m_generator, adjustedLayerInfo) : uint64(0);

    {
        Mutex::Guard guard(m_generationStateMutex);

        WorldGridLayer::SetLayerInfo(adjustedLayerInfo);

        m_cellFingerprint = cellFingerprint;

        // cells still streaming in were built for the old geometry - they must never be spawned or persisted
        if (cellGeometryChanged)
        {
            m_generationEpoch.Increment(1, MemoryOrder::ACQUIRE_RELEASE);
        }
    }

    if (cellGeometryChanged)
    {
        ClearHeightCaches();
    }
}

uint8 TerrainWorldGridLayer::GetEffectiveLodCount() const
{
    const uint8 requested = MathUtil::Clamp<uint8>(m_lodCount, 1, TerrainMeshHelpers::MaxTerrainLods);

    return MathUtil::Min<uint8>(requested, TerrainMeshHelpers::CalculateMaxLodIndex(m_layerInfo.cellSize, GetEffectiveLodStrideMultiplier()) + 1);
}

uint32 TerrainWorldGridLayer::GetEffectiveLodStrideMultiplier() const
{
    return MathUtil::Clamp<uint32>(m_lodStrideMultiplier, 2, 8);
}

float TerrainWorldGridLayer::GetEffectiveLodRangeMultiplier() const
{
    return MathUtil::Max(m_lodRangeMultiplier, 1.0f);
}

float TerrainWorldGridLayer::GetLodRange(uint8 lodIndex) const
{
    return m_lodBaseRange * MathUtil::Pow(GetEffectiveLodRangeMultiplier(), float(lodIndex));
}

float TerrainWorldGridLayer::GetLodMorphStart(uint8 lodIndex) const
{
    // LOD 0's range also starts at range / multiplier (not 0) so every morph band is the previous one scaled by the
    // multiplier - Terrain morph shaders rely on that to derive the next LOD's band
    const float rangeEnd = GetLodRange(lodIndex);
    const float rangeStart = rangeEnd / GetEffectiveLodRangeMultiplier();

    const float morphStartRatio = MathUtil::Clamp(m_lodMorphStartRatio, 0.0f, 0.95f);

    return rangeStart + (rangeEnd - rangeStart) * morphStartRatio;
}

void TerrainWorldGridLayer::Regenerate()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    DiscardAllCellData();

    ReplaceGenerator();

    DetachLoadedCells();

    if (Handle<TerrainWorldGridLayer> strongThis = HandleFromThis(); strongThis.IsValid())
    {
        g_streamingManager->RequestLayerRefresh(strongThis.Get());
    }
}

TerrainGenerationParams TerrainWorldGridLayer::MakeGenerationParams() const
{
    TerrainGenerationParams params;
    params.seed = m_layerInfo.seed;

    return params;
}

TerrainGenerationState TerrainWorldGridLayer::GetGenerationState() const
{
    Mutex::Guard guard(m_generationStateMutex);

    return TerrainGenerationState {
        .generator = m_generator,
        .cellFingerprint = m_cellFingerprint,
        .epoch = m_generationEpoch.Get(MemoryOrder::ACQUIRE),
        .layerInfo = m_layerInfo
    };
}

void TerrainWorldGridLayer::ReplaceGenerator()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    SharedPtr<TerrainGenerator> generator = MakeShared<TerrainGenerator>();
    generator->Configure(MakeGenerationParams());

    const uint64 cellFingerprint = ComputeCellFingerprint(*generator, m_layerInfo);

    SharedPtr<TerrainGenerator> previousGenerator;

    {
        Mutex::Guard guard(m_generationStateMutex);

        previousGenerator = std::move(m_generator);

        m_generator = std::move(generator);
        m_cellFingerprint = cellFingerprint;

        m_generationEpoch.Increment(1, MemoryOrder::ACQUIRE_RELEASE);
    }

    // in-flight work holds its own reference to the old generator; stop its queued region builds from starting
    if (previousGenerator)
    {
        previousGenerator->Cancel();
    }

    ClearHeightCaches();
}

void TerrainWorldGridLayer::ClearHeightCaches()
{
    HYP_SCOPE;

    {
        // cleared after the epoch bump, so a warm task from the old epoch can't insert after this
        Mutex::Guard guard(m_heightCacheMutex);

        m_cellHeightsCache.Clear();
    }

    m_heightsSampleCache.Invalidate();
}

void TerrainWorldGridLayer::DetachLoadedCells()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    for (const KeyValuePair<Vec2i, WeakHandle<TerrainStreamingCell>>& pair : m_loadedCells)
    {
        if (Handle<TerrainStreamingCell> loadedCell = pair.second.Lock(); loadedCell)
        {
            loadedCell->DetachFromScene();
        }
    }

    m_loadedCells.Clear();
    m_cellsModifiedSinceStrokeEnd.Clear();
}

void TerrainWorldGridLayer::DiscardAllCellData()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (m_objectsByCoord.Empty())
    {
        return;
    }

    m_heightsSampleCache.Invalidate();

    for (const KeyValuePair<Vec2i, Array<AssetReference, StreamingAllocator>>& pair : m_objectsByCoord)
    {
        for (const AssetReference& assetReference : pair.second)
        {
            DeletePersistedCellData(assetReference.GetAssetPath());
        }
    }

    m_objectsByCoord.Clear();
}

void TerrainWorldGridLayer::DeletePersistedCellData(const AssetPath& assetPath)
{
    HYP_SCOPE;

    if (!assetPath.IsValid())
    {
        return;
    }

    Handle<AssetRegistry> registry = GetCurrentAssetRegistry();

    if (!registry.IsValid())
    {
        return;
    }

    const FilePath bucketDirectory = registry->GetRootPath() / assetPath.GetBucket().GetName();
    const String assetName = assetPath.assetName.ToString();

    // delete by path, without resolving: cells that were never streamed in don't need loading just to be deleted
    const FilePath persistedFiles[] = {
        registry->GetManifestPath(assetPath),
        bucketDirectory / (assetName + "." + TerrainCellData::HeightsBlobMagic + ".raw.blob"),
        bucketDirectory / (assetName + "." + TerrainCellData::SplatMapBlobMagic + ".raw.blob")
    };

    for (const FilePath& persistedFile : persistedFiles)
    {
        if (persistedFile.Exists() && !persistedFile.Remove())
        {
            HYP_LOG(WorldGrid, Error, "Failed to delete stale terrain cell data file '{}'", persistedFile);
        }
    }

    // unregisters a live instance too, so it can no longer page blob data from files a new cell will write
    registry->RemoveAsset(assetPath.GetBucket(), assetPath.assetName);
}

void TerrainWorldGridLayer::GenerateAll()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!m_generator)
    {
        return;
    }

    const WorldGridLayerInfo& layerInfo = m_layerInfo;

    if (layerInfo.infinite)
    {
        HYP_LOG(WorldGrid, Warning, "Cannot GenerateAll on infinite terrain layer '{}' - set a finite range first!", GetName());

        return;
    }

    if (m_generator->GetParams() != MakeGenerationParams())
    {
        HYP_LOG(WorldGrid, Warning, "Terrain layer '{}' generation settings changed - Regenerate before using GenerateAll!", GetName());

        return;
    }

    const int64 numCellsX = int64(layerInfo.range.y) - int64(layerInfo.range.x) + 1;
    const int64 numCellsY = int64(layerInfo.range.y) - int64(layerInfo.range.x) + 1;

    if (numCellsX <= 0 || numCellsY <= 0)
    {
        HYP_LOG(WorldGrid, Error, "Terrain layer '{}' has an invalid range [{}, {}] - nothing to generate!",
                GetName(), layerInfo.range.x, layerInfo.range.y);

        return;
    }

    const int64 numCells = numCellsX * numCellsY;

    static constexpr int64 maxCells = 100 * 100; // sanity limit - cells hold padded height data assets

    if (numCells > maxCells)
    {
        HYP_LOG(WorldGrid, Error, "Terrain layer '{}' range [{}, {}] covers {} cells (max: {}) - refusing to GenerateAll!",
                GetName(), layerInfo.range.x, layerInfo.range.y, numCells, maxCells);

        return;
    }

    HYP_LOG(WorldGrid, Info, "Generating all {} cells for terrain layer '{}'...", numCells, GetName());

#ifdef HYP_EDITOR
    int64 numCellsToGenerate = 0;

    for (int y = layerInfo.range.x; y <= layerInfo.range.y; y++)
    {
        for (int x = layerInfo.range.x; x <= layerInfo.range.y; x++)
        {
            if (Handle<TerrainCellData> existingCellData = FindCellData({ x, y }); !existingCellData.IsValid() || !AreCellHeightsCurrent(*existingCellData))
            {
                numCellsToGenerate++;
            }
        }
    }

    // the sim thread is blocked for the duration, so live updates reach the UI via the description's change delegate;
    // progress values only propagate once the task manager ticks again after this returns
    Handle<TickableEditorTask> editorTask;

    if (numCellsToGenerate != 0 && g_editorState.IsValid() && EngineGlobals::IsEditor())
    {
        editorTask = MakeHandle<TickableEditorTask>(
            []()
            { /* no tick function */ },
            "Generating terrain",
            HYP_FORMAT("{} cells", numCellsToGenerate));

        InitObject(editorTask);

        editorTask->SetIsForegroundTask(true);

        g_editorState->AddTask(editorTask);
    }

    int64 numCellsGenerated = 0;
#endif

    Array<float> paddedHeights;

    bool wasCancelled = false;

    for (int y = layerInfo.range.x; y <= layerInfo.range.y; y++)
    {
        for (int x = layerInfo.range.x; x <= layerInfo.range.y; x++)
        {
            const Vec2i coord = { x, y };

            if (Handle<TerrainCellData> existingCellData = FindCellData(coord); existingCellData.IsValid() && AreCellHeightsCurrent(*existingCellData))
            {
                // already generated & persisted
                continue;
            }

#ifdef HYP_EDITOR
            if (editorTask.IsValid() && editorTask->IsCancellationRequested())
            {
                wasCancelled = true;

                break;
            }
#endif

            GenerateCellPaddedHeights(coord, paddedHeights);
            StoreGeneratedCellHeights(coord, paddedHeights);

#ifdef HYP_EDITOR
            if (editorTask.IsValid())
            {
                numCellsGenerated++;

                editorTask->SetProgress(float(double(numCellsGenerated) / double(numCellsToGenerate)));
                editorTask->SetDescription(HYP_FORMAT("{} / {} cells", numCellsGenerated, numCellsToGenerate));
            }
#endif
        }

        if (wasCancelled)
        {
            break;
        }
    }

#ifdef HYP_EDITOR
    if (editorTask.IsValid())
    {
        editorTask->Cancel();
    }
#endif

    if (wasCancelled)
    {
        HYP_LOG(WorldGrid, Info, "GenerateAll cancelled for terrain layer '{}'", GetName());
    }
    else
    {
        HYP_LOG(WorldGrid, Info, "GenerateAll finished for terrain layer '{}'", GetName());
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

    // not Regenerate() - that would discard the cell data just loaded with the layer
    ReplaceGenerator();

    world->AddScene(m_scene);

    if (Handle<TerrainWorldGridLayer> strongThis = HandleFromThis(); strongThis.IsValid())
    {
        g_streamingManager->RequestLayerRefresh(strongThis.Get());
    }
}

void TerrainWorldGridLayer::OnRemoved(WorldGrid* worldGrid)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    AssertDebug(worldGrid != nullptr);
    AssertDebug(m_scene.IsValid());

    {
        // cells still streaming in must not spawn into the removed scene or persist their heights
        Mutex::Guard guard(m_generationStateMutex);

        m_generationEpoch.Increment(1, MemoryOrder::ACQUIRE_RELEASE);
    }

    if (m_generator)
    {
        m_generator->Cancel();
    }

    DetachLoadedCells();

    worldGrid->GetWorld()->RemoveScene(m_scene);
}

static Vec3f ComputeCellBoundsMin(const WorldGridLayerInfo& layerInfo, const Vec2i& coord)
{
    return Vec3f {
        layerInfo.offset.x + (float(coord.x) - 0.5f) * (float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.x,
        layerInfo.offset.y,
        layerInfo.offset.z + (float(coord.y) - 0.5f) * (float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.z
    };
}

///inverse of ComputeCellBoundsMin - caller must ensure the cell's world size is non-zero
static Vec2i ComputeCellCoord(const WorldGridLayerInfo& layerInfo, const Vec2f& worldXZ)
{
    const float cellWorldSizeX = (float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.x;
    const float cellWorldSizeZ = (float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.z;

    return Vec2i(
        int32(MathUtil::Floor((worldXZ.x - layerInfo.offset.x) / cellWorldSizeX + 0.5f)),
        int32(MathUtil::Floor((worldXZ.y - layerInfo.offset.z) / cellWorldSizeZ + 0.5f)));
}

Handle<StreamingCell> TerrainWorldGridLayer::CreateStreamingCell(const StreamingCellInfo& cellInfo)
{
    if (!m_scene)
    {
        return Handle<StreamingCell>::Null();
    }

    // snapshot before reading the cell data map - see Regenerate()
    TerrainGenerationState generationState = GetGenerationState();

    // the streaming manager read the layer info outside the generation lock; rebuild the cell's geometry from the
    // snapshot so its size always matches the epoch it was created with
    StreamingCellInfo snapshotCellInfo = cellInfo;
    snapshotCellInfo.extent = Vec3u(generationState.layerInfo.cellSize);
    snapshotCellInfo.scale = generationState.layerInfo.scale;
    snapshotCellInfo.bounds.min = ComputeCellBoundsMin(generationState.layerInfo, cellInfo.coord);
    snapshotCellInfo.bounds.max = snapshotCellInfo.bounds.min + Vec3f(snapshotCellInfo.extent) * snapshotCellInfo.scale;

    Handle<TerrainCellData> cellData;

    auto objectsByCoordIt = m_objectsByCoord.Find(cellInfo.coord);

    if (objectsByCoordIt != m_objectsByCoord.End() && objectsByCoordIt->second.Any())
    {
        cellData = DynamicCast<TerrainCellData>(objectsByCoordIt->second[0].Resolve());
    }

    return MakeHandle<TerrainStreamingCell>(snapshotCellInfo, m_scene, m_material, HandleFromThis(), cellData, std::move(generationState));
}

void TerrainWorldGridLayer::RegisterLoadedCell(const Vec2i& coord, const WeakHandle<TerrainStreamingCell>& cell)
{
    m_loadedCells[coord] = cell;
}

void TerrainWorldGridLayer::UnregisterLoadedCell(const Vec2i& coord, const TerrainStreamingCell* cell)
{
    auto loadedCellIt = m_loadedCells.Find(coord);

    if (loadedCellIt == m_loadedCells.End() || loadedCellIt->second.GetUnsafe() != cell)
    {
        return;
    }

    m_loadedCells.Erase(loadedCellIt);
}

SharedPtr<const Array<float>> TerrainWorldGridLayer::GetOrGenerateCellHeights(const TerrainGenerator& generator, uint32 generationEpoch, const Vec2i& coord) const
{
    HYP_SCOPE;

    // runs on background threads; if the snapshot is newer than generationEpoch the result just isn't cached
    const WorldGridLayerInfo layerInfo = GetGenerationState().layerInfo;

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

    generator.GenerateCellHeights(
        Vec2f(cellBoundsMin.x, cellBoundsMin.z),
        Vec2f(layerInfo.scale.x, layerInfo.scale.z),
        layerInfo.cellSize,
        *heights);

    {
        Mutex::Guard guard(m_heightCacheMutex);

        if (!IsGenerationCurrent(generationEpoch))
        {
            // the generator was replaced while we were generating - don't poison the new cache
            return heights;
        }

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

SharedPtr<const Array<float>> TerrainWorldGridLayer::TryGetCachedCellHeights(const Vec2i& coord) const
{
    HYP_SCOPE;

    Mutex::Guard guard(m_heightCacheMutex);

    auto cacheIt = m_cellHeightsCache.Find(coord);

    if (cacheIt != m_cellHeightsCache.End())
    {
        return cacheIt->second;
    }

    return nullptr;
}

void TerrainWorldGridLayer::WarmHeightsCache(const Vec2i& coord) const
{
    HYP_SCOPE;

    {
        Mutex::Guard guard(m_pendingWarmsMutex);

        if (m_pendingHeightWarms.Find(coord) != m_pendingHeightWarms.End())
        {
            return;
        }

        m_pendingHeightWarms.Set(coord, true);
    }

    Handle<TerrainWorldGridLayer> strongThis = HandleFromThis();

    if (!strongThis.IsValid())
    {
        Mutex::Guard guard(m_pendingWarmsMutex);
        m_pendingHeightWarms.Erase(coord);

        return;
    }

    TaskSystem::GetInstance().Enqueue(
        [strongThis, generator = m_generator, generationEpoch = m_generationEpoch.Get(MemoryOrder::ACQUIRE), coord]()
        {
            strongThis->GetOrGenerateCellHeights(*generator, generationEpoch, coord);

            Mutex::Guard guard(strongThis->m_pendingWarmsMutex);
            strongThis->m_pendingHeightWarms.Erase(coord);
        },
        TaskThreadPoolName::THREAD_POOL_BACKGROUND,
        TaskEnqueueFlags::FIRE_AND_FORGET);
}

void TerrainWorldGridLayer::StreamPrefetch(Span<const Vec2i> cellCoords)
{
    HYP_SCOPE;

    // runs on the streaming manager thread
    const TerrainGenerationState generationState = GetGenerationState();

    if (!generationState.generator || !cellCoords)
    {
        return;
    }

    const WorldGridLayerInfo& layerInfo = generationState.layerInfo;

    const float cellWorldSizeX = (float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.x;
    const float cellWorldSizeZ = (float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.z;

    if (cellWorldSizeX <= 0.0f || cellWorldSizeZ <= 0.0f)
    {
        return;
    }

    const SharedPtr<TerrainGenerator>& generator = generationState.generator;

    // only the regions the cells actually sample, queued in the cells' (nearest first) order
    Array<Vec2i, StreamingTempAllocator> regionCoords;

    for (const Vec2i& coord : cellCoords)
    {
        const Vec3f cellBoundsMin = ComputeCellBoundsMin(layerInfo, coord);

        generator->CollectRegionsForCell(
            Vec2f(cellBoundsMin.x, cellBoundsMin.z),
            Vec2f(layerInfo.scale.x, layerInfo.scale.z),
            layerInfo.cellSize,
            regionCoords);
    }

    for (const Vec2i& regionCoord : regionCoords)
    {
        // also skips regions already queued by an earlier cell
        if (!generator->TryBeginRegionBuild(regionCoord))
        {
            continue;
        }

        // builds run on the background pool as to not stall a streaming worker
        TaskSystem::GetInstance().Enqueue(
            [generator, regionCoord]()
            {
                generator->BuildQueuedRegion(regionCoord);
            },
            TaskThreadPoolName::THREAD_POOL_BACKGROUND,
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

uint64 TerrainWorldGridLayer::ComputeCellFingerprint(const TerrainGenerator& generator, const WorldGridLayerInfo& layerInfo)
{
    HashCode hashCode;
    hashCode.Add(generator.ComputeFingerprint(layerInfo.cellSize, Vec2f(layerInfo.scale.x, layerInfo.scale.z)));
    hashCode.Add(layerInfo.offset.x);
    hashCode.Add(layerInfo.offset.z);

    return uint64(hashCode.Value());
}

Handle<TerrainCellData> TerrainWorldGridLayer::FindCellData(const Vec2i& coord) const
{
    auto objectsByCoordIt = m_objectsByCoord.Find(coord);

    if (objectsByCoordIt == m_objectsByCoord.End() || !objectsByCoordIt->second.Any())
    {
        return Handle<TerrainCellData>();
    }

    return DynamicCast<TerrainCellData>(objectsByCoordIt->second[0].Resolve());
}

bool TerrainWorldGridLayer::AreCellHeightsCurrent(const TerrainCellData& cellData) const
{
    return AreCellHeightsCurrent(cellData, m_layerInfo.cellSize, m_cellFingerprint);
}

bool TerrainWorldGridLayer::AreCellHeightsCurrent(const TerrainCellData& cellData, uint32 cellSize, uint64 cellFingerprint)
{
    return cellData.HasHeights()
        && cellData.extent.x == cellSize
        && (cellData.isSculpted || cellData.generatorFingerprint == cellFingerprint);
}

void TerrainWorldGridLayer::GenerateCellPaddedHeights(const Vec2i& coord, Array<float>& outPaddedHeights) const
{
    AssertDebug(m_generator != nullptr);

    GenerateCellPaddedHeights(*m_generator, m_layerInfo, coord, outPaddedHeights);
}

void TerrainWorldGridLayer::GenerateCellPaddedHeights(const TerrainGenerator& generator, const WorldGridLayerInfo& layerInfo, const Vec2i& coord, Array<float>& outPaddedHeights)
{
    HYP_SCOPE;

    const Vec3f cellBoundsMin = ComputeCellBoundsMin(layerInfo, coord);

    generator.GeneratePaddedCellHeights(
        Vec2f(cellBoundsMin.x, cellBoundsMin.z),
        Vec2f(layerInfo.scale.x, layerInfo.scale.z),
        layerInfo.cellSize,
        outPaddedHeights);
}

Handle<TerrainCellData> TerrainWorldGridLayer::StoreGeneratedCellHeights(const Vec2i& coord, Span<const float> paddedHeights)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    const uint32 cellSize = m_layerInfo.cellSize;
    const uint32 paddedSize = cellSize + TerrainGenerator::CellPadding * 2u;

    if (paddedHeights.Size() != size_t(paddedSize) * size_t(paddedSize))
    {
        HYP_LOG(WorldGrid, Warning, "Not storing {} generated heights for cell {} - layer cell size {} expects {}",
            paddedHeights.Size(), coord, cellSize, size_t(paddedSize) * size_t(paddedSize));

        return FindCellData(coord);
    }

    Handle<TerrainCellData> cellData = FindCellData(coord);

    const bool isNewCellData = !cellData.IsValid();

    if (!isNewCellData && AreCellHeightsCurrent(*cellData))
    {
        return cellData;
    }

    if (isNewCellData)
    {
        cellData = MakeHandle<TerrainCellData>(NAME_FMT("TerrainCellData_{}_{}", coord.x, coord.y), coord, Vec3u(cellSize));
    }

    m_heightsSampleCache.Invalidate();

    {
        auto cellDataWriteScope = cellData->GetWriteScope();

        cellData->extent = Vec3u(cellSize);
        cellData->generatorFingerprint = m_cellFingerprint;
        cellData->isSculpted = false;
        cellData->SetHeights(paddedHeights);
    }

    if (isNewCellData)
    {
        GetCurrentAssetRegistry()->PutAssetUnique(cellData);

        AddStreamingObject(cellData.Get(), coord);
    }

    return cellData;
}

void TerrainWorldGridLayer::ApplyBrush(const Vec3f& worldPos, float radius, float strength, bool raise)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (radius <= 0.0f || strength == 0.0f)
    {
        return;
    }

    const WorldGridLayerInfo& layerInfo = m_layerInfo;
    const uint32 cellSize = layerInfo.cellSize;
    const uint32 padding = TerrainGenerator::CellPadding;
    const uint32 paddedSize = cellSize + padding * 2u;

    const Vec2f scaleXZ(layerInfo.scale.x, layerInfo.scale.z);

    const float cellWorldSizeX = (float(cellSize) - 1.0f) * layerInfo.scale.x;
    const float cellWorldSizeZ = (float(cellSize) - 1.0f) * layerInfo.scale.z;

    if (cellWorldSizeX <= 0.0f || cellWorldSizeZ <= 0.0f || !m_generator)
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

            const Vec3f cellBoundsMin = ComputeCellBoundsMin(layerInfo, coord);
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

            Handle<TerrainCellData> cellData = FindCellData(coord);

            const bool isNewCellData = !cellData.IsValid();

            m_heightsSampleCache.Invalidate();

            const bool hasCurrentHeights = !isNewCellData
                && AreCellHeightsCurrent(*cellData)
                && cellData->EnsureWritableHeights();

            Array<float> generatedHeights;

            if (!hasCurrentHeights)
            {
                GenerateCellPaddedHeights(coord, generatedHeights);
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
                cellData->generatorFingerprint = m_cellFingerprint;
                cellData->isSculpted = true;
                cellData->SetHeights(generatedHeights);
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
                    loadedCell->RebuildMesh(cellData, Vec2i(minVertexX, minVertexZ), Vec2i(maxVertexX, maxVertexZ));
                }
            }
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

            m_heightsSampleCache.Invalidate();

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
                    if (!hadSplatMap
                        && m_generator
                        && m_generator->GetParams().autoPaintSplats
                        && splatMap.Size() == size_t(cellSize) * size_t(cellSize) * TerrainCellData::NumSplatLayers)
                    {
                        // seed fresh splat maps from what the cell currently looks like
                        const uint32 paddedSize = cellSize + TerrainGenerator::CellPadding * 2u;

                        Array<float> generatedHeights;
                        Span<const float> paddedHeights;

                        if (!isNewCellData && AreCellHeightsCurrent(*cellData))
                        {
                            paddedHeights = static_cast<const TerrainCellData&>(*cellData).GetHeights();
                        }

                        if (paddedHeights.Size() != size_t(paddedSize) * size_t(paddedSize))
                        {
                            GenerateCellPaddedHeights(coord, generatedHeights);
                            paddedHeights = generatedHeights.ToSpan();
                        }

                        Array<float> heights;
                        Array<Vec3f> normals;

                        TerrainGenerator::ExtractCellHeightsAndNormals(paddedHeights, cellSize, heights, normals);

                        m_generator->SynthesizeSplatWeights(
                            heights,
                            normals,
                            cellWorldMinXZ,
                            Vec2f(layerInfo.scale.x, layerInfo.scale.z),
                            cellSize,
                            Span<ubyte>(reinterpret_cast<ubyte*>(splatMap.Data()), splatMap.Size()));
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

void TerrainWorldGridLayer::HeightsSampleCache::Invalidate()
{
    HYP_SCOPE;

    heights = Span<const float>();

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

    const Vec2i coord = ComputeCellCoord(layerInfo, worldXZ);

    const Vec3f cellBoundsMin = ComputeCellBoundsMin(layerInfo, coord);

    const int32 lx = MathUtil::Clamp(int32(MathUtil::Floor((worldXZ.x - cellBoundsMin.x) / layerInfo.scale.x + 0.5f)), 0, int32(cellSize) - 1);
    const int32 lz = MathUtil::Clamp(int32(MathUtil::Floor((worldXZ.y - cellBoundsMin.z) / layerInfo.scale.z + 0.5f)), 0, int32(cellSize) - 1);

    if (Handle<TerrainCellData> cellData = FindCellData(coord); cellData.IsValid() && AreCellHeightsCurrent(*cellData))
    {
        if (m_heightsSampleCache.cell != cellData)
        {
            m_heightsSampleCache.Invalidate();

            m_heightsSampleCache.scope.Reset(*cellData);
            m_heightsSampleCache.cell = cellData;
            m_heightsSampleCache.heights = static_cast<const TerrainCellData&>(*cellData).GetHeights();
        }

        const uint32 paddedSize = cellSize + TerrainGenerator::CellPadding * 2u;
        const Span<const float> heights = m_heightsSampleCache.heights;

        if (heights.Size() == size_t(paddedSize) * size_t(paddedSize))
        {
            return heights[size_t(lz + int32(TerrainGenerator::CellPadding)) * paddedSize + size_t(lx + int32(TerrainGenerator::CellPadding))];
        }
    }

    if (SharedPtr<const Array<float>> heights = TryGetCachedCellHeights(coord); heights.IsValid()
        && heights->Size() == size_t(cellSize) * size_t(cellSize))
    {
        return (*heights)[size_t(lz) * size_t(cellSize) + size_t(lx)];
    }

    ///heat it up
    WarmHeightsCache(coord);

    return m_generator->SampleBaseHeight(worldXZ);
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

bool TerrainWorldGridLayer::IsCollisionPendingAt(const Vec3f& worldPosition) const
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    const WorldGridLayerInfo& layerInfo = m_layerInfo;

    const float cellWorldSizeX = (float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.x;
    const float cellWorldSizeZ = (float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.z;

    if (cellWorldSizeX <= 0.0f || cellWorldSizeZ <= 0.0f || !m_scene.IsValid())
    {
        return false;
    }

    const Vec2i coord = ComputeCellCoord(layerInfo, Vec2f(worldPosition.x, worldPosition.z));

    const bool isCoordInRange = layerInfo.infinite
        || (coord.x >= layerInfo.range.x && coord.x <= layerInfo.range.y
            && coord.y >= layerInfo.range.x && coord.y <= layerInfo.range.y);

    if (!isCoordInRange)
    {
        return false;
    }

    auto loadedCellIt = m_loadedCells.Find(coord);

    if (loadedCellIt == m_loadedCells.End())
    {
        return true;
    }

    Handle<TerrainStreamingCell> loadedCell = loadedCellIt->second.Lock();

    return !loadedCell.IsValid() || !loadedCell->HasCollider();
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
