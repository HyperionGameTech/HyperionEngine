/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>
#include <Scene/WorldGrid/Terrain/TerrainStreamingCell.hpp>
#include <Scene/WorldGrid/Terrain/TerrainCellData.hpp>
#include <Scene/WorldGrid/Terrain/TerrainGrass.hpp>

#include <Scene/WorldGrid/WorldGrid.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetReference.hpp>
#include <Asset/SerializationUtils.hpp>

#include <Streaming/StreamingManager.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <Rendering/Material.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Texture.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/HashCode.hpp>
#include <Core/Reflection/Enum.hpp>
#include <Core/Memory/Memory.hpp>
#include <Core/Threading/TaskSystem.hpp>

#include <Framework/EngineDefaults.hpp>
#include <Framework/EngineGlobals.hpp>
#include <Framework/CVarManager.hpp>

#ifdef HYP_EDITOR
#include <Editor/EditorTask.hpp>
#include <Editor/EditorState.hpp>
#endif

#include <random>

#include <TerrainWorldGridLayer.generated.inl>

namespace Hyperion
{

ENGINE_API HYP_DECLARE_LOG_CHANNEL(WorldGrid);

extern uint32 GetFrameCounter();

// render frames a removed entity is kept for: the frame reading its last collection, the one dropping it, and that frame's end
static constexpr uint32 RetiredEntityFrames = 4;

// full resolution quads per side of a level 0 patch (power of two). Applies to tiles built after it changes
CVar<uint32> g_cvTerrainLodPatchQuads("Terrain.Lod.PatchQuads", 32);
// quadtree levels per tile, level 0 included - also capped by the tile size. Applies to tiles built after it changes
CVar<uint32> g_cvTerrainLodMaxLevels("Terrain.Lod.MaxLevels", 8);
// world-space distance from the LOD camera where full resolution patches finish morphing into level 1
CVar<float> g_cvTerrainLodBaseRange("Terrain.Lod.BaseRange", 96.0f);
// each level's range is the previous level's range times this
CVar<float> g_cvTerrainLodRangeMultiplier("Terrain.Lod.RangeMultiplier", 2.0f);
// fraction of each level's band at full detail before morphing toward the next - lower is a longer, smoother transition
CVar<float> g_cvTerrainLodMorphStartRatio("Terrain.Lod.MorphStartRatio", 0.5f);

namespace {

const Name g_terrainWorldGridLayerName = NAME("TerrainWorldGridLayer");
const Name g_terrainSceneName = NAME("TerrainScene");

const Name g_groundCoverDescAssetKey = NAME("GroundCover");
const Name g_heightmapTileDescAssetKey = NAME("HeightmapTile");

const WorldGridLayerInfo g_defaultTerrainWorldGridLayerInfo = {
    .scale = Vec3f { 1.0f, 1.0f, 1.0f },
    .cellSize = 257,
    .maxDistance = 8.0f,
    .seed = 1951233096
};

Handle<Texture> LoadTerrainTexture(const AssetPath& defaultPath)
{
    const Name name = defaultPath.assetName;

    auto tryLoadFromRegistry = [name](AssetRegistry& registry) -> Handle<Texture>
    {
        return TryCast<Texture>(registry.GetAsset(AssetBuckets::Textures, name));
    };

    Handle<Texture> texture;

    if (Handle<AssetRegistry> registry = GetCurrentAssetRegistry(); registry.IsValid())
    {
        texture = tryLoadFromRegistry(*registry);
    }

    if (!texture.IsValid())
    {
        texture = EngineDefaults::LoadAsset<Texture>(defaultPath);
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

void LoadTerrainMaterialTextures(MaterialTextures& textures)
{
    constexpr MaterialTextureKey LayerTextureKeys[][2] = {
        { MaterialTextureKey::TerrainLayer0, MaterialTextureKey::TerrainNormal0 },
        { MaterialTextureKey::TerrainLayer1, MaterialTextureKey::TerrainNormal1 },
        { MaterialTextureKey::TerrainLayer2, MaterialTextureKey::TerrainNormal2 },
        { MaterialTextureKey::TerrainLayer3, MaterialTextureKey::TerrainNormal3 }
    };

    const Array<EngineTerrainLayerDefaults>& layers = EngineDefaults::Get().terrain.layers;

    for (uint32 layerIndex = 0; layerIndex < uint32(GetArrayCount(LayerTextureKeys)) && layerIndex < uint32(layers.Size()); layerIndex++)
    {
        textures[LayerTextureKeys[layerIndex][0]] = LoadTerrainTexture(layers[layerIndex].albedo);
        textures[LayerTextureKeys[layerIndex][1]] = LoadTerrainTexture(layers[layerIndex].normal);
    }
}

Handle<Scene> MakeTerrainScene()
{
    Handle<Scene> scene = MakeHandle<Scene>(g_terrainSceneName, SceneFlags::FOREGROUND | SceneFlags::HAS_OCTREE | SceneFlags::NO_EDITOR_WIREFRAME);

    // don't save; it's generated at runtime by the terrain layer
    scene->SetIsTransient(true);

    return scene;
}

SharedPtr<const TerrainHeightmapTileSamples> MakeHeightmapTileSamples(const Texture& texture, const SharedPtr<const TerrainHeightmapSamples>& previousSamples)
{
    HYP_SCOPE;

    auto readScope = texture.GetReadScope();

    const TextureDesc& desc = texture.GetTextureDesc();
    const ConstByteView imageData = texture.GetImageData();

    const uint32 width = desc.extent.x;
    const uint32 depth = desc.extent.y;

    const TextureFormat format = TextureUtils::ChangeFormatSRGB(desc.format, false);

    uint32 componentSize = 0;

    switch (format)
    {
    case TextureFormat::R8:
    case TextureFormat::RG8:
    case TextureFormat::RGB8:
    case TextureFormat::RGBA8:
        componentSize = 1;
        break;
    case TextureFormat::R32F:
    case TextureFormat::RG32F:
    case TextureFormat::RGB32F:
    case TextureFormat::RGBA32F:
        componentSize = 4;
        break;
    default:
        HYP_LOG(WorldGrid, Warning, "Cannot read heightmap data for '{}'! Format: {}",
                texture.GetName(), EnumToString(desc.format));

        return nullptr;
    }

    const size_t pixelSize = size_t(TextureUtils::NumComponents(format)) * componentSize;
    const size_t baseMipSize = size_t(width) * size_t(depth) * pixelSize;

    if (desc.type != TextureType::Texture2D || width < 2 || depth < 2 || imageData.Size() < baseMipSize)
    {
        HYP_LOG(WorldGrid, Warning, "Heightmap '{}' ({}x{}, {} bytes of image data) can't be used", texture.GetName(), width, depth, imageData.Size());

        return nullptr;
    }

    HashCode hashCode;
    hashCode.Add(uint32(format));
    hashCode.Add(width);
    hashCode.Add(depth);
    hashCode.Add(FNV1::DoHashWords(imageData.Data(), baseMipSize));

    const uint64 contentHash = uint64(hashCode.Value());

    if (previousSamples)
    {
        for (const SharedPtr<const TerrainHeightmapTileSamples>& previousTile : previousSamples->tiles)
        {
            if (previousTile && previousTile->contentHash == contentHash)
            {
                return previousTile;
            }
        }
    }

    SharedPtr<TerrainHeightmapTileSamples> samples = MakeSharedWithAllocator<TerrainHeightmapTileSamples, TerrainAllocator>();
    samples->width = width;
    samples->depth = depth;
    samples->contentHash = contentHash;
    samples->samples.Resize(size_t(width) * size_t(depth));

    for (size_t index = 0; index < samples->samples.Size(); index++)
    {
        const ubyte* pixel = imageData.Data() + index * pixelSize;

        if (componentSize == 1)
        {
            samples->samples[index] = uint16(pixel[0]) * 257u;
        }
        else
        {
            float value;
            Memory::Copy(&value, pixel, sizeof(float));

            samples->samples[index] = uint16(MathUtil::Clamp(value, 0.0f, 1.0f) * 65535.0f + 0.5f);
        }
    }

    return samples;
}

SharedPtr<const TerrainHeightmapSamples> MakeHeightmapSamples(
    Span<const Vec2i> tileCoords,
    Span<const AssetReference> tileTextures,
    const SharedPtr<const TerrainHeightmapSamples>& previousSamples)
{
    constexpr int64 MaxTiles = 64 * 64;

    HYP_SCOPE;

    const size_t numTiles = MathUtil::Min(tileCoords.Size(), tileTextures.Size());

    if (numTiles == 0)
    {
        return nullptr;
    }

    Vec2i minTile = tileCoords[0];
    Vec2i maxTile = tileCoords[0];

    for (size_t index = 1; index < numTiles; index++)
    {
        minTile = Vec2i(MathUtil::Min(minTile.x, tileCoords[index].x), MathUtil::Min(minTile.y, tileCoords[index].y));
        maxTile = Vec2i(MathUtil::Max(maxTile.x, tileCoords[index].x), MathUtil::Max(maxTile.y, tileCoords[index].y));
    }

    const int64 tilesX = int64(maxTile.x) - int64(minTile.x) + 1;
    const int64 tilesZ = int64(maxTile.y) - int64(minTile.y) + 1;

    if (tilesX * tilesZ > MaxTiles)
    {
        HYP_LOG(WorldGrid, Warning, "max tiles is {}, got {} ({}x{})",
            MaxTiles, tilesX * tilesZ, tilesX, tilesZ);

        return nullptr;
    }

    SharedPtr<TerrainHeightmapSamples> samples = MakeSharedWithAllocator<TerrainHeightmapSamples, TerrainAllocator>();
    samples->tilesX = uint32(tilesX);
    samples->tilesZ = uint32(tilesZ);
    samples->tiles.Resize(size_t(tilesX * tilesZ));

    HashCode hashCode;
    hashCode.Add(samples->tilesX);
    hashCode.Add(samples->tilesZ);

    for (size_t index = 0; index < numTiles; index++)
    {
        Handle<Texture> texture = TryCast<Texture>(tileTextures[index].Resolve());

        if (!texture.IsValid())
        {
            HYP_LOG(WorldGrid, Warning, "Heightmap tile {} has no texture", tileCoords[index]);

            continue;
        }

        SharedPtr<const TerrainHeightmapTileSamples> tileSamples = MakeHeightmapTileSamples(*texture, previousSamples);

        if (!tileSamples)
        {
            continue;
        }

        if (samples->tileWidth == 0)
        {
            samples->tileWidth = tileSamples->width;
            samples->tileDepth = tileSamples->depth;
        }
        else if (tileSamples->width != samples->tileWidth || tileSamples->depth != samples->tileDepth)
        {
            HYP_LOG(WorldGrid, Warning, "Heightmap tile {} is {}x{} but other tiles are {}x{}!",
                tileCoords[index], tileSamples->width, tileSamples->depth, samples->tileWidth, samples->tileDepth);

            continue;
        }

        const Vec2i tile = tileCoords[index] - minTile;
        const size_t tileIdx = size_t(tile.y) * size_t(tilesX) + size_t(tile.x);

        samples->tiles[tileIdx] = tileSamples;

        hashCode.Add(tile);
        hashCode.Add(tileSamples->contentHash);
    }

    if (samples->tileWidth < 2 || samples->tileDepth < 2)
    {
        return nullptr;
    }

    samples->contentHash = uint64(hashCode.Value());

    if (previousSamples && previousSamples->contentHash == samples->contentHash)
    {
        return previousSamples;
    }

    uint32 numMissingTiles = 0;

    for (const SharedPtr<const TerrainHeightmapTileSamples>& tileSamples : samples->tiles)
    {
        if (!tileSamples)
        {
            numMissingTiles++;
        }
    }

    if (numMissingTiles != 0)
    {
        HYP_LOG(WorldGrid, Warning, "{} of the {}x{} heightmap tiles are missing - they read as the border's mean height", numMissingTiles, tilesX, tilesZ);
    }

    const uint32 width = samples->GetWidth();
    const uint32 depth = samples->GetDepth();

    double borderSum = 0.0;
    uint32 numBorderSamples = 0;

    const auto addBorderSample = [&](uint32 x, uint32 z)
    {
        const uint32 tileX = MathUtil::Min(x / (samples->tileWidth - 1), samples->tilesX - 1);
        const uint32 tileZ = MathUtil::Min(z / (samples->tileDepth - 1), samples->tilesZ - 1);

        if (!samples->tiles[size_t(tileZ) * samples->tilesX + tileX])
        {
            return;
        }

        borderSum += double(samples->Fetch(x, z));
        numBorderSamples++;
    };

    for (uint32 x = 0; x < width; x++)
    {
        addBorderSample(x, 0);
        addBorderSample(x, depth - 1);
    }

    for (uint32 z = 1; z + 1 < depth; z++)
    {
        addBorderSample(0, z);
        addBorderSample(width - 1, z);
    }

    if (numBorderSamples != 0)
    {
        samples->borderMean = float(borderSum / double(numBorderSamples) / 65535.0);
    }

    return samples;
}

} // namespace

#pragma region TerrainWorldGridLayer

TerrainWorldGridLayer::TerrainWorldGridLayer()
    : WorldGridLayer(g_terrainWorldGridLayerName, g_defaultTerrainWorldGridLayerInfo),
      m_scene(MakeTerrainScene())
{
    m_layerInfo.seed = std::random_device()();
}

TerrainWorldGridLayer::TerrainWorldGridLayer(Name name, const WorldGridLayerInfo& layerInfo)
    : WorldGridLayer(name, layerInfo),
      m_scene(MakeTerrainScene())
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

    Mutex::Guard guard(m_generatorState.GetMutex());

    m_layerInfo.seed = seed;
}

void TerrainWorldGridLayer::SetLayerInfo(const WorldGridLayerInfo& layerInfo)
{
    WorldGridLayerInfo adjustedLayerInfo = layerInfo;
    adjustedLayerInfo.scale.y = 1.0f;
    adjustedLayerInfo.cellSize = RoundCellSizeForQuadtree(adjustedLayerInfo.cellSize);

    const bool cellGeometryChanged = adjustedLayerInfo.cellSize != m_layerInfo.cellSize
        || adjustedLayerInfo.scale != m_layerInfo.scale
        || adjustedLayerInfo.offset != m_layerInfo.offset;

    const SharedPtr<TerrainGenerator>& generator = m_generatorState.GetGenerator();
    const uint64 cellFingerprint = generator ? ComputeCellFingerprint(*generator, adjustedLayerInfo) : uint64(0);

    {
        Mutex::Guard guard(m_generatorState.GetMutex());

        WorldGridLayer::SetLayerInfo(adjustedLayerInfo);

        m_generatorState.SetCellFingerprint_Locked(cellFingerprint);

        // cells still streaming in were built for the old geometry - they must never be spawned or persisted
        if (cellGeometryChanged)
        {
            m_generatorState.BumpEpoch_Locked();
        }
    }

    if (cellGeometryChanged)
    {
        m_heightsCache.Clear();

        if (m_settings.heightSource != TerrainHeightSource::Procedural
            && IsAddedToWorld())
        {
            ReplaceGenerator();
        }
    }
}

uint32 TerrainWorldGridLayer::RoundCellSizeForQuadtree(uint32 cellSize)
{
    uint32 tileQuads = 8;

    while (tileQuads + 1 < cellSize)
    {
        tileQuads *= 2;
    }

    return tileQuads + 1;
}

TerrainQuadtreeLayout TerrainWorldGridLayer::MakeQuadtreeLayout(uint32 cellSize)
{
    return TerrainQuadtreeLayout(
        cellSize,
        g_cvTerrainLodPatchQuads.Get(),
        uint8(MathUtil::Clamp<uint32>(g_cvTerrainLodMaxLevels.Get(), 1, TerrainQuadtreeLayout::MaxLevels)));
}

float TerrainWorldGridLayer::GetLodRangeMultiplier()
{
    return MathUtil::Max(g_cvTerrainLodRangeMultiplier.Get(), 1.0f);
}

float TerrainWorldGridLayer::CalculateLodRange(uint8 level, const TerrainQuadtreeLayout& layout, const Vec3f& scale)
{
    // a level 0 node must fit within its band or its patches get drawn past what their morph targets can reach
    const float leafNodeWorldSize = float(layout.GetPatchQuads()) * MathUtil::Max(scale.x, scale.z);
    const float baseRange = MathUtil::Max(g_cvTerrainLodBaseRange.Get(), leafNodeWorldSize * 0.75f);

    return baseRange * MathUtil::Pow(GetLodRangeMultiplier(), float(level));
}

float TerrainWorldGridLayer::CalculateLodMorphStart(uint8 level, const TerrainQuadtreeLayout& layout, const Vec3f& scale)
{
    // level 0's range also starts at range / multiplier (not 0) so every morph band is the previous one scaled by the
    // multiplier - Terrain morph shaders rely on that to derive the next level's band
    const float rangeEnd = CalculateLodRange(level, layout, scale);
    const float rangeStart = rangeEnd / GetLodRangeMultiplier();

    const float morphStartRatio = MathUtil::Clamp(g_cvTerrainLodMorphStartRatio.Get(), 0.0f, 0.95f);

    return rangeStart + (rangeEnd - rangeStart) * morphStartRatio;
}

void TerrainWorldGridLayer::UpdateLodSelection(Span<const Vec3f> viewpoints)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    ReleaseRetiredEntities();

    m_groundCoverResources.Refresh();

    m_loadedCells.ForEach(
        [&viewpoints](const Handle<TerrainStreamingCell>& loadedCell)
        {
            loadedCell->UpdateLodSelection(viewpoints);
        });
}

const Handle<GroundCover>& TerrainWorldGridLayer::GetGroundCover() const
{
    return m_groundCoverResources.GetGroundCover();
}

void TerrainWorldGridLayer::SetGroundCover(const Handle<GroundCover>& groundCover)
{
    AssertOnThread(g_simThread);

    if (groundCover == GetGroundCover())
    {
        return;
    }

    m_groundCoverResources.SetGroundCover(groundCover);

    if (World* world = m_scene.IsValid() ? m_scene->GetWorld() : nullptr)
    {
        world->MarkDirty();
    }
}

void TerrainWorldGridLayer::ReplantGroundCover()
{
    AssertOnThread(g_simThread);

    m_groundCoverResources.Invalidate();
}

void TerrainWorldGridLayer::GetDescAssets(Array<WGLayerAsset>& outAssets) const
{
    if (const AssetReference& groundCover = m_groundCoverResources.GetGroundCoverReference(); groundCover.IsValid())
    {
        outAssets.PushBack(WGLayerAsset { g_groundCoverDescAssetKey, groundCover.GetAssetPath() });
    }

    for (const AssetReference& tileTexture : m_heightmapTileTextures)
    {
        outAssets.PushBack(WGLayerAsset {
            g_heightmapTileDescAssetKey,
            tileTexture.GetAssetPath()
        });
    }
}

void TerrainWorldGridLayer::SetDescAssets(Span<const WGLayerAsset> assets)
{
    for (const WGLayerAsset& asset : assets)
    {
        if (asset.key == g_groundCoverDescAssetKey)
        {
            m_groundCoverResources.SetGroundCoverPath(asset.path);
        }
        else if (asset.key == g_heightmapTileDescAssetKey)
        {
            m_heightmapTileTextures.PushBack(AssetReference(asset.path));
        }
    }
}

BoxedValue TerrainWorldGridLayer::GetDescSettings() const
{
    if (m_settings == TerrainLayerSettings())
    {
        return BoxedValue();
    }

    TerrainLayerSettings settings = m_settings;

    HMF::UnresolvedValue unresolvedValue;

    if (Result result = BoxedToHMF(BoxedValue(std::move(settings)), unresolvedValue.source, &TypeInfo::ForType<TerrainLayerSettings>()); result.HasError())
    {
        HYP_LOG(WorldGrid, Error, "Failed to save the settings of terrain layer '{}': {}", GetName(), result.GetError().GetMessage());

        return BoxedValue();
    }

    return BoxedValue(std::move(unresolvedValue));
}

void TerrainWorldGridLayer::SetDescSettings(const BoxedValue& settings)
{
    if (!settings.IsValid())
    {
        return;
    }

    TResult<BoxedValue> settingsResult = BoxedAsType(settings, TypeInfo::ForType<TerrainLayerSettings>());

    if (settingsResult.HasError())
    {
        HYP_LOG(WorldGrid, Error, "Error in saved terrain layer settings for '{}': {}",
            GetName(), settingsResult.GetError().GetMessage());

        return;
    }

    m_settings = settingsResult.GetValue().Get<TerrainLayerSettings>();
}

bool TerrainWorldGridLayer::IsAddedToWorld() const
{
    return m_scene.IsValid() && m_scene->GetWorld() != nullptr;
}

void TerrainWorldGridLayer::MarkWorldDirty() const
{
    if (World* world = m_scene.IsValid() ? m_scene->GetWorld() : nullptr)
    {
        world->MarkDirty();
    }
}

void TerrainWorldGridLayer::SetSettings(const TerrainLayerSettings& settings)
{
    AssertOnThread(g_simThread);

    if (settings == m_settings)
    {
        return;
    }

    m_settings = settings;

    MarkWorldDirty();

    if (IsAddedToWorld())
    {
        RefreshGenerator();
    }
}

void TerrainWorldGridLayer::SetHeightSource(TerrainHeightSource heightSource)
{
    TerrainLayerSettings settings = m_settings;
    settings.heightSource = heightSource;

    SetSettings(settings);
}

void TerrainWorldGridLayer::SetFlatHeight(float flatHeight)
{
    TerrainLayerSettings settings = m_settings;
    settings.flatHeight = flatHeight;

    SetSettings(settings);
}

void TerrainWorldGridLayer::SetHeightmapCenter(Vec2f heightmapCenter)
{
    TerrainLayerSettings settings = m_settings;
    settings.heightmapCenter = heightmapCenter;

    SetSettings(settings);
}

void TerrainWorldGridLayer::SetHeightmapSize(Vec2f heightmapSize)
{
    TerrainLayerSettings settings = m_settings;
    settings.heightmapSize = heightmapSize;

    SetSettings(settings);
}

void TerrainWorldGridLayer::SetHeightmapHeightRange(Vec2f heightmapHeightRange)
{
    TerrainLayerSettings settings = m_settings;
    settings.heightmapHeightRange = heightmapHeightRange;

    SetSettings(settings);
}

void TerrainWorldGridLayer::SetProceduralEdges(bool proceduralEdges)
{
    TerrainLayerSettings settings = m_settings;
    settings.proceduralEdges = proceduralEdges;

    SetSettings(settings);
}

void TerrainWorldGridLayer::SetBlendDistance(float blendDistance)
{
    TerrainLayerSettings settings = m_settings;
    settings.blendDistance = MathUtil::Max(blendDistance, 0.0f);

    SetSettings(settings);
}

uint32 TerrainWorldGridLayer::GetNumHeightmapTiles() const
{
    return uint32(MathUtil::Min(m_settings.heightmapTiles.Size(), m_heightmapTileTextures.Size()));
}

const Handle<Texture>& TerrainWorldGridLayer::GetHeightmap() const
{
    if (m_heightmapTileTextures.Size() != 1)
    {
        return Handle<Texture>::Null();
    }

    return TryCast<Texture>(m_heightmapTileTextures[0].Resolve());
}

void TerrainWorldGridLayer::SetHeightmap(const Handle<Texture>& heightmap)
{
    AssertOnThread(g_simThread);

    Array<TerrainHeightmapTile> tiles;

    if (heightmap.IsValid())
    {
        tiles.PushBack(TerrainHeightmapTile { Vec2i(0, 0), heightmap });
    }

    SetHeightmapTiles(tiles);
}

Array<TerrainHeightmapTile> TerrainWorldGridLayer::GetHeightmapTiles() const
{
    const size_t numTiles = MathUtil::Min(m_settings.heightmapTiles.Size(), m_heightmapTileTextures.Size());

    Array<TerrainHeightmapTile> tiles;
    tiles.Reserve(numTiles);

    for (size_t index = 0; index < numTiles; index++)
    {
        tiles.PushBack(TerrainHeightmapTile {
            m_settings.heightmapTiles[index],
            TryCast<Texture>(m_heightmapTileTextures[index].Resolve())
        });
    }

    return tiles;
}

void TerrainWorldGridLayer::SetHeightmapTiles(const Array<TerrainHeightmapTile>& tiles)
{
    AssertOnThread(g_simThread);

    Array<Vec2i> tileCoords;
    Array<AssetReference, TerrainAllocator> tileTextures;

    for (const TerrainHeightmapTile& tile : tiles)
    {
        if (!tile.texture.IsValid())
        {
            continue;
        }

        if (tileCoords.Contains(tile.tile))
        {
            HYP_LOG(WorldGrid, Warning, "Terrain layer '{}' already has heightmap tile {}", GetName(), tile.tile);

            continue;
        }

        tileCoords.PushBack(tile.tile);
        tileTextures.PushBack(AssetReference(Handle<AssetObject>(tile.texture)));
    }

    bool texturesChanged = tileTextures.Size() != m_heightmapTileTextures.Size();

    for (size_t index = 0; index < tileTextures.Size() && !texturesChanged; index++)
    {
        texturesChanged = tileTextures[index].Resolve() != m_heightmapTileTextures[index].Resolve();
    }

    m_heightmapTileTextures = std::move(tileTextures);

    TerrainLayerSettings settings = m_settings;
    settings.heightmapTiles = std::move(tileCoords);

    if (settings.heightmapTiles.Any())
    {
        settings.heightSource = TerrainHeightSource::Heightmap;
    }

    if (settings != m_settings)
    {
        SetSettings(settings);

        return;
    }

    if (!texturesChanged)
    {
        return;
    }

    MarkWorldDirty();

    if (IsAddedToWorld() && m_settings.heightSource == TerrainHeightSource::Heightmap)
    {
        RefreshGenerator();
    }
}

bool TerrainWorldGridLayer::IsProceduralCell(const Vec2i& coord) const
{
    return m_settings.proceduralCells.Contains(coord);
}

void TerrainWorldGridLayer::MarkProceduralCell(const Vec3f& worldPos, bool isProcedural)
{
    AssertOnThread(g_simThread);

    const Vec2i coord = ComputeCellCoord(m_layerInfo, Vec2f(worldPos.x, worldPos.z));

    if (IsProceduralCell(coord) == isProcedural)
    {
        return;
    }

    if (!m_proceduralCellsBeforeStroke.HasValue())
    {
        m_proceduralCellsBeforeStroke = Array<Vec2i, TerrainAllocator>(m_settings.proceduralCells);
    }

    if (isProcedural)
    {
        m_settings.proceduralCells.PushBack(coord);
    }
    else
    {
        m_settings.proceduralCells.Erase(coord);
    }
}

void TerrainWorldGridLayer::SetProceduralCells(const Array<Vec2i>& proceduralCells)
{
    AssertOnThread(g_simThread);

    TerrainLayerSettings settings = m_settings;
    settings.proceduralCells = proceduralCells;

    SetSettings(settings);
}

uint32 TerrainWorldGridLayer::GetNumProceduralCells() const
{
    return uint32(m_settings.proceduralCells.Size());
}

void TerrainWorldGridLayer::ClearProceduralCells()
{
    SetProceduralCells(Array<Vec2i>());
}

bool TerrainWorldGridLayer::TakeProceduralCellsEdit(
    Array<Vec2i, TerrainAllocator>& outBefore,
    Array<Vec2i, TerrainAllocator>& outAfter)
{
    AssertOnThread(g_simThread);

    if (!m_proceduralCellsEditBefore.HasValue())
    {
        return false;
    }

    outBefore = std::move(*m_proceduralCellsEditBefore);
    outAfter = m_settings.proceduralCells;

    m_proceduralCellsEditBefore.Unset();

    return true;
}

Array<Vec3f, TerrainAllocator> TerrainWorldGridLayer::GetLodViewpoints() const
{
    return m_lodViewpoints.Get();
}

void TerrainWorldGridLayer::SetLodViewpoints(Span<const Vec3f> viewpoints)
{
    m_lodViewpoints.Set(viewpoints);
}

float TerrainWorldGridLayer::GetNearestLodViewpointDistance(const BoundingBox& worldBounds) const
{
    return m_lodViewpoints.GetNearestDistance(worldBounds);
}

MaterialAttributes TerrainWorldGridLayer::GetTerrainMaterialAttributes()
{
    MaterialAttributes attributes;
    attributes.shaderName = NAME("Terrain");
    attributes.bucket = RenderBucket::Opaque;
    attributes.flags |= MaterialAttributeFlags::DepthTest | MaterialAttributeFlags::DepthWrite;

    return attributes;
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
    return m_generatorState.Snapshot(m_layerInfo);
}

TerrainHeightSourceDesc TerrainWorldGridLayer::MakeHeightSourceDesc()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    TerrainHeightSourceDesc heightSourceDesc;
    heightSourceDesc.heightSource = m_settings.heightSource;

    if (heightSourceDesc.heightSource == TerrainHeightSource::Procedural)
    {
        m_heightmapSamples.Reset();

        return heightSourceDesc;
    }

    const WorldGridLayerInfo& layerInfo = m_layerInfo;

    heightSourceDesc.flatHeight = m_settings.flatHeight;
    heightSourceDesc.proceduralEdges = m_settings.proceduralEdges;
    heightSourceDesc.blendDistance = MathUtil::Max(m_settings.blendDistance, 0.0f);

    heightSourceDesc.cellWorldSize = Vec2f((float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.x, (float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.z);
    heightSourceDesc.cellOriginXZ = Vec2f(layerInfo.offset.x, layerInfo.offset.z) - heightSourceDesc.cellWorldSize * 0.5f;

    for (const Vec2i& coord : m_settings.proceduralCells)
    {
        heightSourceDesc.proceduralCells.Insert(coord);
    }

    if (heightSourceDesc.heightSource != TerrainHeightSource::Heightmap)
    {
        m_heightmapSamples.Reset();

        return heightSourceDesc;
    }

    if (m_heightmapTileTextures.Empty())
    {
        m_heightmapSamples.Reset();

        HYP_LOG(WorldGrid, Warning, "No height map on terrain layer '{}' but it is set to use heightmap source.", GetName());

        return heightSourceDesc;
    }

    if (m_settings.heightmapTiles.Size() != m_heightmapTileTextures.Size())
    {
        HYP_LOG(WorldGrid, Warning, "Terrain layer '{}' has {} heightmap tile textures for {} tiles - using the first {}",
            GetName(), m_heightmapTileTextures.Size(), m_settings.heightmapTiles.Size(), MathUtil::Min(m_heightmapTileTextures.Size(), m_settings.heightmapTiles.Size()));
    }

    m_heightmapSamples = MakeHeightmapSamples(m_settings.heightmapTiles.ToSpan(), m_heightmapTileTextures.ToSpan(), m_heightmapSamples);

    if (!m_heightmapSamples)
    {
        return heightSourceDesc;
    }

    Vec2f heightmapSize = m_settings.heightmapSize;

    if (heightmapSize.x <= 0.0f)
    {
        heightmapSize.x = float(m_heightmapSamples->GetWidth() - 1) * layerInfo.scale.x;
    }

    if (heightmapSize.y <= 0.0f)
    {
        heightmapSize.y = float(m_heightmapSamples->GetDepth() - 1) * layerInfo.scale.z;
    }

    heightSourceDesc.heightmap = m_heightmapSamples;
    heightSourceDesc.heightmapMinXZ = m_settings.heightmapCenter - heightmapSize * 0.5f;
    heightSourceDesc.heightmapMaxXZ = m_settings.heightmapCenter + heightmapSize * 0.5f;
    heightSourceDesc.heightmapHeightRange = m_settings.heightmapHeightRange;

    return heightSourceDesc;
}

void TerrainWorldGridLayer::RefreshGenerator()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    ReplaceGenerator();

    DetachLoadedCells();

    if (Handle<TerrainWorldGridLayer> strongThis = HandleFromThis(); strongThis.IsValid())
    {
        g_streamingManager->RequestLayerRefresh(strongThis.Get());
    }
}

void TerrainWorldGridLayer::ReplaceGenerator()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    SharedPtr<TerrainGenerator> generator = MakeSharedWithAllocator<TerrainGenerator, TerrainAllocator>();
    generator->Configure(MakeGenerationParams(), MakeHeightSourceDesc());

    if (const SharedPtr<TerrainGenerator>& currentGenerator = m_generatorState.GetGenerator())
    {
        generator->ReuseErosionRegionsFrom(*currentGenerator);
    }

    const uint64 cellFingerprint = ComputeCellFingerprint(*generator, m_layerInfo);

    SharedPtr<TerrainGenerator> previousGenerator = m_generatorState.Replace(std::move(generator), cellFingerprint);

    // in-flight work holds its own reference to the old generator; stop its queued region builds from starting
    if (previousGenerator)
    {
        previousGenerator->Cancel();
    }

    m_heightsCache.Clear();
}

void TerrainWorldGridLayer::DetachLoadedCells()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    m_loadedCells.ForEach(
        [](const Handle<TerrainStreamingCell>& loadedCell)
        {
            loadedCell->DetachFromScene();
        });

    m_loadedCells.Clear();
    m_brush.ResetStroke();
}

void TerrainWorldGridLayer::RetireEntity(Handle<Entity>&& entity)
{
    AssertOnThread(g_simThread);

    if (!entity.IsValid())
    {
        return;
    }

    // nothing renders, and the frame counter wouldn't advance
    if (EngineGlobals::IsHeadless())
    {
        entity.Reset();

        return;
    }

    m_retiredEntities.PushBack(RetiredEntity { std::move(entity), GetFrameCounter() });
}

void TerrainWorldGridLayer::ReleaseRetiredEntities()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    const uint32 frameCounter = GetFrameCounter();

    size_t numReleased = 0;

    while (numReleased < m_retiredEntities.Size() && frameCounter - m_retiredEntities[numReleased].frameCounter >= RetiredEntityFrames)
    {
        ++numReleased;
    }

    if (numReleased != 0)
    {
        m_retiredEntities.Erase(m_retiredEntities.Begin(), m_retiredEntities.Begin() + numReleased);
    }
}

void TerrainWorldGridLayer::DiscardAllCellData()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    // taken out under the lock, the file deletes below don't need to hold it
    FlatMap<Vec2i, Array<AssetReference, StreamingAllocator>, StreamingAllocator> objectsByCoord;

    {
        Mutex::Guard guard(m_objectsByCoordMutex);

        objectsByCoord = std::move(m_objectsByCoord);
        m_objectsByCoord.Clear();
    }

    if (objectsByCoord.Empty())
    {
        return;
    }

    m_heightsCache.GetSampleCache().Invalidate();

    for (const KeyValuePair<Vec2i, Array<AssetReference, StreamingAllocator>>& pair : objectsByCoord)
    {
        for (const AssetReference& assetReference : pair.second)
        {
            DeletePersistedCellData(assetReference.GetAssetPath());
        }
    }
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
        bucketDirectory / (assetName + "." + TerrainCellData::SplatMapBlobMagic + ".raw.blob"),
        bucketDirectory / (assetName + "." + TerrainCellData::ErosionMasksBlobMagic + ".raw.blob")
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

void TerrainWorldGridLayer::AddCellData(const Vec2i& coord, const Handle<TerrainCellData>& cellData)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    // left in place, they'd be retried (and fail) ahead of the new cell data on every lookup
    {
        Mutex::Guard guard(m_objectsByCoordMutex);

        if (auto objectsByCoordIt = m_objectsByCoord.Find(coord); objectsByCoordIt != m_objectsByCoord.End())
        {
            objectsByCoordIt->second.Clear();
        }
    }

    AddStreamingObject(cellData.Get(), coord);

    MarkHeightsDirty(m_layerInfo, coord);
}

bool TerrainWorldGridLayer::RemoveUnregisteredCellData()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    Handle<AssetRegistry> registry = GetCurrentAssetRegistry();

    if (!registry.IsValid())
    {
        return false;
    }

    uint32 numRemoved = 0;

    Mutex::Guard guard(m_objectsByCoordMutex);

    for (auto objectsByCoordIt = m_objectsByCoord.Begin(); objectsByCoordIt != m_objectsByCoord.End();)
    {
        Array<AssetReference, StreamingAllocator>& assetReferences = objectsByCoordIt->second;

        for (size_t index = 0; index < assetReferences.Size();)
        {
            const AssetReference& assetReference = assetReferences[index];
            const AssetPath& assetPath = assetReference.GetAssetPath();

            bool isDuplicate = false;

            for (size_t previousIndex = 0; previousIndex < index; previousIndex++)
            {
                if (assetReferences[previousIndex].GetAssetPath() == assetPath)
                {
                    isDuplicate = true;

                    break;
                }
            }

            // a loaded reference holds its cell data, so only a path can dangle. the manifest check keeps a registry whose
            // descs aren't loaded yet from dropping every reference
            const bool isUnregistered = !assetReference.IsLoaded()
                && (!assetPath.IsValid()
                    || (assetPath.registryId == registry->GetRegistryId()
                        && !registry->HasAsset(assetPath.GetBucket(), assetPath.assetName)
                        && !registry->GetManifestPath(assetPath).Exists()));

            if (isDuplicate || isUnregistered)
            {
                assetReferences.EraseAt(index);
                numRemoved++;

                continue;
            }

            index++;
        }

        if (assetReferences.Empty())
        {
            objectsByCoordIt = m_objectsByCoord.Erase(objectsByCoordIt);

            continue;
        }

        ++objectsByCoordIt;
    }

    if (numRemoved == 0)
    {
        return false;
    }

    HYP_LOG(WorldGrid, Warning, "Removed {} unregistered or duplicate cell data references from terrain layer '{}'", numRemoved, GetName());

    return true;
}

void TerrainWorldGridLayer::GenerateAll()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    const SharedPtr<TerrainGenerator>& generator = m_generatorState.GetGenerator();

    if (!generator)
    {
        return;
    }

    const WorldGridLayerInfo& layerInfo = m_layerInfo;

    if (layerInfo.infinite)
    {
        HYP_LOG(WorldGrid, Warning, "Cannot GenerateAll on infinite terrain layer '{}' - set a finite range first!", GetName());

        return;
    }

    if (generator->GetParams() != MakeGenerationParams())
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

    Array<float, TerrainAllocator> paddedHeights;
    Array<ubyte, TerrainAllocator> erosionMasks;

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

            GenerateCellPaddedHeights(coord, paddedHeights, &erosionMasks);
            StoreGeneratedCellHeights(coord, paddedHeights, erosionMasks);

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

    // layer info loaded from a world desc bypasses SetLayerInfo(); the quadtree needs 2^n + 1 samples per side
    if (const uint32 quadtreeCellSize = RoundCellSizeForQuadtree(m_layerInfo.cellSize); quadtreeCellSize != m_layerInfo.cellSize)
    {
        HYP_LOG(WorldGrid, Warning, "Terrain layer '{}' cell size {} rounded up to {} for its LOD quadtree", GetName(), m_layerInfo.cellSize, quadtreeCellSize);

        Mutex::Guard guard(m_generatorState.GetMutex());

        m_layerInfo.cellSize = quadtreeCellSize;
    }

    World* world = worldGrid->GetWorld();
    AssertDebug(world != nullptr);

    m_scene->Initialize();

    const MaterialAttributes attributes = GetTerrainMaterialAttributes();

    MaterialParameters parameters;
    parameters.albedo = Vec4f(0.06f, 0.25f, 0.05f, 1.0f);
    parameters.roughness = 0.95f;
    parameters.metalness = 0.0f;

    MaterialTextures textures;
    LoadTerrainMaterialTextures(textures);

    m_material = MakeHandle<Material>(NAME("TerrainMaterial"), attributes, parameters, textures);
    GetCurrentAssetRegistry()->PutAsset(m_material);

    InitObject(m_material);

    // eg. references saved to cell data whose manifest never was
    if (RemoveUnregisteredCellData())
    {
        world->MarkDirty();
    }

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
        Mutex::Guard guard(m_generatorState.GetMutex());

        m_generatorState.BumpEpoch_Locked();
    }

    if (const SharedPtr<TerrainGenerator>& generator = m_generatorState.GetGenerator())
    {
        generator->Cancel();
    }

    DetachLoadedCells();

    worldGrid->GetWorld()->RemoveScene(m_scene);

    // removed before World::Shutdown() reaches its scenes, so it would otherwise outlive the sim thread
    m_scene->Shutdown();
}

Vec3f TerrainWorldGridLayer::ComputeCellBoundsMin(const WorldGridLayerInfo& layerInfo, const Vec2i& coord)
{
    return {
        layerInfo.offset.x + (float(coord.x) - 0.5f) * (float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.x,
        layerInfo.offset.y,
        layerInfo.offset.z + (float(coord.y) - 0.5f) * (float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.z
    };
}

Vec2i TerrainWorldGridLayer::ComputeCellCoord(const WorldGridLayerInfo& layerInfo, const Vec2f& worldXZ)
{
    const float cellWorldSizeX = (float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.x;
    const float cellWorldSizeZ = (float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.z;

    return {
        int32(MathUtil::Floor((worldXZ.x - layerInfo.offset.x) / cellWorldSizeX + 0.5f)),
        int32(MathUtil::Floor((worldXZ.y - layerInfo.offset.z) / cellWorldSizeZ + 0.5f))
    };
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

    Handle<TerrainCellData> cellData = FindCellData(cellInfo.coord);

    return MakeHandle<TerrainStreamingCell>(snapshotCellInfo, m_scene, m_material, HandleFromThis(), cellData, std::move(generationState));
}

void TerrainWorldGridLayer::RegisterLoadedCell(const Vec2i& coord, const WeakHandle<TerrainStreamingCell>& cell)
{
    m_loadedCells.Register(coord, cell);

    MarkHeightsDirty(GetGenerationState().layerInfo, coord);
}

void TerrainWorldGridLayer::UnregisterLoadedCell(const Vec2i& coord, const TerrainStreamingCell* cell)
{
    m_loadedCells.Unregister(coord, cell);

    MarkHeightsDirty(GetGenerationState().layerInfo, coord);
}

void TerrainWorldGridLayer::MarkHeightsDirty(const WorldGridLayerInfo& layerInfo, const Vec2i& coord) const
{
    const Vec3f cellBoundsMin = ComputeCellBoundsMin(layerInfo, coord);
    const Vec2f cellWorldSize = Vec2f((float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.x, (float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.z);

    MarkHeightsDirty(Vec4f(
        cellBoundsMin.x - layerInfo.scale.x,
        cellBoundsMin.z - layerInfo.scale.z,
        cellBoundsMin.x + cellWorldSize.x + layerInfo.scale.x,
        cellBoundsMin.z + cellWorldSize.y + layerInfo.scale.z));
}

void TerrainWorldGridLayer::MarkHeightsDirty(const Vec4f& worldRect) const
{
    static constexpr size_t MaxHeightDirtyRects = 256;

    Mutex::Guard guard(m_heightDirtyMutex);

    if (m_heightDirtyRects.Size() >= MaxHeightDirtyRects)
    {
        Vec4f merged = worldRect;

        for (const Vec4f& rect : m_heightDirtyRects)
        {
            merged = Vec4f(MathUtil::Min(merged.x, rect.x), MathUtil::Min(merged.y, rect.y), MathUtil::Max(merged.z, rect.z), MathUtil::Max(merged.w, rect.w));
        }

        m_heightDirtyRects.Clear();
        m_heightDirtyRects.PushBack(merged);

        return;
    }

    m_heightDirtyRects.PushBack(worldRect);
}

Array<Vec4f, TerrainAllocator> TerrainWorldGridLayer::TakeHeightDirtyRects()
{
    Mutex::Guard guard(m_heightDirtyMutex);

    Array<Vec4f, TerrainAllocator> rects = std::move(m_heightDirtyRects);
    m_heightDirtyRects = Array<Vec4f, TerrainAllocator>();

    return rects;
}

SharedPtr<const Array<float, TerrainAllocator>> TerrainWorldGridLayer::GetOrGenerateCellHeights(const TerrainGenerator& generator, uint32 generationEpoch, const Vec2i& coord) const
{
    HYP_SCOPE;

    // runs on background threads; if the snapshot is newer than generationEpoch the result just isn't cached
    const WorldGridLayerInfo layerInfo = GetGenerationState().layerInfo;

    if (SharedPtr<const Array<float, TerrainAllocator>> cachedHeights = m_heightsCache.FindByCoord(coord))
    {
        return cachedHeights;
    }

    const Vec3f cellBoundsMin = ComputeCellBoundsMin(layerInfo, coord);

    auto heights = MakeSharedWithAllocator<Array<float, TerrainAllocator>, TerrainAllocator>();

    generator.GenerateCellHeights(
        Vec2f(cellBoundsMin.x, cellBoundsMin.z),
        Vec2f(layerInfo.scale.x, layerInfo.scale.z),
        layerInfo.cellSize,
        *heights);

    SharedPtr<const Array<float, TerrainAllocator>> insertedHeights = m_heightsCache.Insert(coord, heights, m_generatorState, generationEpoch);

    MarkHeightsDirty(layerInfo, coord);

    return insertedHeights;
}

SharedPtr<const Array<float, TerrainAllocator>> TerrainWorldGridLayer::TryGetCachedCellHeights(const Vec2i& coord) const
{
    return m_heightsCache.FindByCoord(coord);
}

void TerrainWorldGridLayer::WarmHeightsCache(const Vec2i& coord) const
{
    HYP_SCOPE;

    if (!m_heightsCache.TryBeginWarm(coord))
    {
        return;
    }

    Handle<TerrainWorldGridLayer> strongThis = HandleFromThis();

    if (!strongThis.IsValid())
    {
        m_heightsCache.EndWarm(coord);

        return;
    }

    TaskSystem::GetInstance().Enqueue(
        [strongThis, generator = m_generatorState.GetGenerator(), generationEpoch = m_generatorState.GetEpoch(), coord]()
        {
            strongThis->GetOrGenerateCellHeights(*generator, generationEpoch, coord);

            strongThis->m_heightsCache.EndWarm(coord);
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

uint64 TerrainWorldGridLayer::CombineCellFingerprint(uint64 baseFingerprint, const TerrainGenerator& generator, const Vec2i& coord)
{
    const uint64 localFingerprint = generator.ComputeLocalFingerprint(coord);

    if (localFingerprint == 0)
    {
        return baseFingerprint;
    }

    HashCode hashCode;
    hashCode.Add(baseFingerprint);
    hashCode.Add(localFingerprint);

    return uint64(hashCode.Value());
}

uint64 TerrainWorldGridLayer::GetCellFingerprint(const Vec2i& coord) const
{
    const SharedPtr<TerrainGenerator>& generator = m_generatorState.GetGenerator();

    if (!generator)
    {
        return m_generatorState.GetCellFingerprint();
    }

    return CombineCellFingerprint(m_generatorState.GetCellFingerprint(), *generator, coord);
}

uint64 TerrainWorldGridLayer::GetCellFingerprint(const TerrainGenerationState& generationState, const Vec2i& coord)
{
    if (!generationState.generator)
    {
        return generationState.cellFingerprint;
    }

    return CombineCellFingerprint(generationState.cellFingerprint, *generationState.generator, coord);
}

Handle<TerrainCellData> TerrainWorldGridLayer::FindCellData(const Vec2i& coord) const
{
    // also runs on streaming threads. loaded references are read under the lock; unloaded ones are copied out since
    // Resolve() can load the manifest from disk, then the resolved handle is cached back like an in-place Resolve()
    Array<AssetReference, StreamingAllocator> unloadedReferences;

    {
        Mutex::Guard guard(m_objectsByCoordMutex);

        auto objectsByCoordIt = m_objectsByCoord.Find(coord);

        if (objectsByCoordIt == m_objectsByCoord.End())
        {
            return Handle<TerrainCellData>();
        }

        const Array<AssetReference, StreamingAllocator>& assetReferences = objectsByCoordIt->second;

        for (size_t index = 0; index < assetReferences.Size(); index++)
        {
            const AssetReference& assetReference = assetReferences[index];

            if (!assetReference.IsLoaded())
            {
                // keep the rest in order so the first reference that resolves still wins
                for (size_t remainingIndex = index; remainingIndex < assetReferences.Size(); remainingIndex++)
                {
                    unloadedReferences.PushBack(assetReferences[remainingIndex]);
                }

                break;
            }

            if (Handle<TerrainCellData> cellData = TryCast<TerrainCellData>(assetReference.Resolve()); cellData.IsValid())
            {
                return cellData;
            }
        }
    }

    for (const AssetReference& assetReference : unloadedReferences)
    {
        const AssetPath assetPath = assetReference.GetAssetPath();

        Handle<TerrainCellData> cellData = TryCast<TerrainCellData>(assetReference.Resolve());

        if (!cellData.IsValid())
        {
            continue;
        }

        Mutex::Guard guard(m_objectsByCoordMutex);

        // const_cast: same logically-const caching Resolve() does through its mutable data
        auto& objectsByCoord = const_cast<FlatMap<Vec2i, Array<AssetReference, StreamingAllocator>, StreamingAllocator>&>(m_objectsByCoord);

        if (auto objectsByCoordIt = objectsByCoord.Find(coord); objectsByCoordIt != objectsByCoord.End())
        {
            for (AssetReference& existingReference : objectsByCoordIt->second)
            {
                if (!existingReference.IsLoaded() && existingReference.GetAssetPath() == assetPath)
                {
                    existingReference = assetReference;

                    break;
                }
            }
        }

        return cellData;
    }

    return Handle<TerrainCellData>();
}

Handle<TerrainStreamingCell> TerrainWorldGridLayer::FindLoadedCell(const Vec2i& coord) const
{
    return m_loadedCells.Find(coord);
}

bool TerrainWorldGridLayer::AreCellHeightsCurrent(const TerrainCellData& cellData) const
{
    return AreCellHeightsCurrent(cellData, m_layerInfo.cellSize, GetCellFingerprint(cellData.coord));
}

bool TerrainWorldGridLayer::AreCellHeightsCurrent(const TerrainCellData& cellData, uint32 cellSize, uint64 cellFingerprint)
{
    return cellData.HasHeights()
        && cellData.extent.x == cellSize
        && (cellData.isSculpted || cellData.generatorFingerprint == cellFingerprint);
}

void TerrainWorldGridLayer::GenerateCellPaddedHeights(const Vec2i& coord, Array<float, TerrainAllocator>& outPaddedHeights, Array<ubyte, TerrainAllocator>* outErosionMasks) const
{
    AssertDebug(m_generatorState.GetGenerator() != nullptr);

    GenerateCellPaddedHeights(*m_generatorState.GetGenerator(), m_layerInfo, coord, outPaddedHeights, outErosionMasks);
}

void TerrainWorldGridLayer::GenerateCellPaddedHeights(
    const TerrainGenerator& generator,
    const WorldGridLayerInfo& layerInfo,
    const Vec2i& coord,
    Array<float, TerrainAllocator>& outPaddedHeights,
    Array<ubyte, TerrainAllocator>* outErosionMasks)
{
    HYP_SCOPE;

    const Vec3f cellBoundsMin = ComputeCellBoundsMin(layerInfo, coord);

    generator.GeneratePaddedCellHeights(
        Vec2f(cellBoundsMin.x, cellBoundsMin.z),
        Vec2f(layerInfo.scale.x, layerInfo.scale.z),
        layerInfo.cellSize,
        outPaddedHeights,
        outErosionMasks);
}

Handle<TerrainCellData> TerrainWorldGridLayer::StoreGeneratedCellHeights(const Vec2i& coord, Span<const float> paddedHeights, Span<const ubyte> erosionMasks)
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

    m_heightsCache.GetSampleCache().Invalidate();

    {
        auto cellDataWriteScope = cellData->GetWriteScope();

        cellData->extent = Vec3u(cellSize);
        cellData->generatorFingerprint = GetCellFingerprint(coord);
        cellData->isSculpted = false;
        cellData->SetHeights(paddedHeights);
        cellData->SetErosionMasks(ConstByteView(erosionMasks.Data(), erosionMasks.Size()));
    }

    if (isNewCellData)
    {
        GetCurrentAssetRegistry()->PutAssetUnique(cellData);

        AddCellData(coord, cellData);
    }

    return cellData;
}

float TerrainWorldGridLayer::SampleHeightAt(const Vec2f& worldXZ) const
{
    return SampleHeight(worldXZ, /* generateColdCells */ false);
}

float TerrainWorldGridLayer::SampleHeightAtBlocking(const Vec2f& worldXZ) const
{
    AssertOnThread(g_simThread);

    return SampleHeight(worldXZ, /* generateColdCells */ true);
}

float TerrainWorldGridLayer::SampleHeight(const Vec2f& worldXZ, bool generateColdCells) const
{
    HYP_SCOPE;

    const WorldGridLayerInfo& layerInfo = m_layerInfo;
    const uint32 cellSize = layerInfo.cellSize;

    const float cellWorldSizeX = (float(cellSize) - 1.0f) * layerInfo.scale.x;
    const float cellWorldSizeZ = (float(cellSize) - 1.0f) * layerInfo.scale.z;

    if (cellWorldSizeX <= 0.0f || cellWorldSizeZ <= 0.0f || !m_generatorState.GetGenerator())
    {
        return 0.0f;
    }

    const Vec2i coord = ComputeCellCoord(layerInfo, worldXZ);

    // the loaded tile's heights are what its mesh and collider were built from, so they are the surface that's on screen.
    // the saved cell data can lag behind them (blob not paged in, replaced by a newer generation, ...)
    if (Handle<TerrainStreamingCell> loadedCell = FindLoadedCell(coord); loadedCell.IsValid())
    {
        float loadedCellHeight;

        if (loadedCell->SampleSurfaceHeight(worldXZ, loadedCellHeight))
        {
            return loadedCellHeight;
        }
    }

    const Vec3f cellBoundsMin = ComputeCellBoundsMin(layerInfo, coord);

    const int32 lx = MathUtil::Clamp(int32(MathUtil::Floor((worldXZ.x - cellBoundsMin.x) / layerInfo.scale.x + 0.5f)), 0, int32(cellSize) - 1);
    const int32 lz = MathUtil::Clamp(int32(MathUtil::Floor((worldXZ.y - cellBoundsMin.z) / layerInfo.scale.z + 0.5f)), 0, int32(cellSize) - 1);

    if (Handle<TerrainCellData> cellData = FindCellData(coord); cellData.IsValid() && AreCellHeightsCurrent(*cellData))
    {
        TerrainHeightsCache::SampleCache& sampleCache = m_heightsCache.GetSampleCache();

        if (sampleCache.cell != cellData)
        {
            sampleCache.Invalidate();

            sampleCache.scope.Reset(*cellData);
            sampleCache.cell = cellData;
            sampleCache.heights = cellData->GetHeights();
        }

        const uint32 paddedSize = cellSize + TerrainGenerator::CellPadding * 2u;
        const Span<const float> heights = sampleCache.heights;

        if (heights.Size() == size_t(paddedSize) * size_t(paddedSize))
        {
            return layerInfo.offset.y + heights[size_t(lz + int32(TerrainGenerator::CellPadding)) * paddedSize + size_t(lx + int32(TerrainGenerator::CellPadding))];
        }
    }

    SharedPtr<const Array<float, TerrainAllocator>> heights = generateColdCells
        ? GetOrGenerateCellHeights(*m_generatorState.GetGenerator(), m_generatorState.GetEpoch(), coord)
        : TryGetCachedCellHeights(coord);

    if (heights.IsValid() && heights->Size() == size_t(cellSize) * size_t(cellSize))
    {
        return layerInfo.offset.y + (*heights)[size_t(lz) * size_t(cellSize) + size_t(lx)];
    }

    ///heat it up
    WarmHeightsCache(coord);

    // the base height leaves out erosion, so this is only a stand-in until the cache is warm
    return layerInfo.offset.y + m_generatorState.GetGenerator()->SampleBaseHeight(worldXZ);
}

float TerrainWorldGridLayer::SampleDrawnHeightAt(const Vec2f& worldXZ) const
{
    const WorldGridLayerInfo& layerInfo = m_layerInfo;

    const float cellWorldSizeX = (float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.x;
    const float cellWorldSizeZ = (float(layerInfo.cellSize) - 1.0f) * layerInfo.scale.z;

    if (cellWorldSizeX <= 0.0f || cellWorldSizeZ <= 0.0f)
    {
        return SampleHeightAt(worldXZ);
    }

    const Vec2i coord = ComputeCellCoord(layerInfo, worldXZ);

    if (Handle<TerrainStreamingCell> loadedCell = FindLoadedCell(coord); loadedCell.IsValid())
    {
        float drawnHeight;

        if (loadedCell->SampleDrawnHeight(worldXZ, drawnHeight))
        {
            return drawnHeight;
        }
    }

    return SampleHeightAt(worldXZ);
}

bool TerrainWorldGridLayer::RaycastSurface(const Ray& ray, Vec3f& outHitPoint) const
{
    HYP_SCOPE;

    const WorldGridLayerInfo& layerInfo = m_layerInfo;

    const SharedPtr<TerrainGenerator>& generator = m_generatorState.GetGenerator();

    const float maxHeight = generator
        ? generator->GetMaxHeightEstimate()
        : 256.0f;

    const float minHeight = generator
        ? MathUtil::Min(generator->GetMinHeightEstimate(), 0.0f)
        : 0.0f;

    const float slabMinY = layerInfo.offset.y + minHeight - 256.0f;
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

        return p.y - SampleDrawnHeightAt(Vec2f(p.x, p.z));
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

    Handle<TerrainStreamingCell> loadedCell = m_loadedCells.Find(coord);

    return !loadedCell.IsValid() || !loadedCell->HasCollider();
}

void TerrainWorldGridLayer::ApplyBrush(const Vec3f& worldPos, float radius, float strength, bool raise)
{
    m_brush.Sculpt(worldPos, radius, strength, raise);

    const float margin = radius + MathUtil::Max(m_layerInfo.scale.x, m_layerInfo.scale.z);

    MarkHeightsDirty(Vec4f(worldPos.x - margin, worldPos.z - margin, worldPos.x + margin, worldPos.z + margin));
}

void TerrainWorldGridLayer::PaintSplat(const Vec3f& worldPos, float radius, float strength, uint32 layerIndex, bool erase)
{
    m_brush.Paint(worldPos, radius, strength, layerIndex, erase);
}

void TerrainWorldGridLayer::PaintGroundCover(const Vec3f& worldPos, float radius, float strength, Name groundCoverLayer, bool erase)
{
    m_brush.PaintGroundCover(worldPos, radius, strength, groundCoverLayer, erase);
}

Array<Name> TerrainWorldGridLayer::GetPaintedGroundCoverLayers()
{
    AssertOnThread(g_simThread);

    return m_groundCoverResources.GetPaintedLayerNames();
}

Array<TerrainGroundCoverPaintEdit, TerrainAllocator> TerrainWorldGridLayer::TakeGroundCoverPaintEdits()
{
    AssertOnThread(g_simThread);

    return m_brush.TakeGroundCoverPaintEdits();
}

void TerrainWorldGridLayer::SetGroundCoverPaintState(const Vec2i& coord, const TerrainGroundCoverPaintState& state)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!m_scene.IsValid() || !m_scene->GetWorld())
    {
        return;
    }

    const uint32 cellSize = m_layerInfo.cellSize;

    Handle<TerrainCellData> cellData = FindCellData(coord);

    const bool isNewCellData = !cellData.IsValid();

    if (isNewCellData)
    {
        if (state.paint.Empty())
        {
            return;
        }

        cellData = MakeHandle<TerrainCellData>(NAME_FMT("TerrainCellData_{}_{}", coord.x, coord.y), coord, Vec3u(cellSize));
    }

    {
        auto cellDataWriteScope = cellData->GetWriteScope();

        cellData->SetGroundCoverPaint(state.layers.ToSpan(), ConstByteView(state.paint.Data(), state.paint.Size()));
    }

    if (isNewCellData)
    {
        AddCellData(coord, cellData);
    }

    if (Handle<TerrainStreamingCell> loadedCell = m_loadedCells.Find(coord); loadedCell.IsValid())
    {
        loadedCell->UpdateGroundCoverPaint(cellData, Vec2i(0, 0), Vec2i(int32(cellSize) - 1, int32(cellSize) - 1));
    }
}

void TerrainWorldGridLayer::EndBrushStroke()
{
    m_brush.EndStroke();

    if (!m_proceduralCellsBeforeStroke.HasValue())
    {
        return;
    }

    Array<Vec2i, TerrainAllocator> proceduralCellsBefore = std::move(*m_proceduralCellsBeforeStroke);
    m_proceduralCellsBeforeStroke.Unset();

    if (proceduralCellsBefore == m_settings.proceduralCells)
    {
        return;
    }

    m_proceduralCellsEditBefore = std::move(proceduralCellsBefore);

    MarkWorldDirty();

    if (IsAddedToWorld())
    {
        RefreshGenerator();
    }
}

#pragma endregion TerrainWorldGridLayer

} // namespace Hyperion
