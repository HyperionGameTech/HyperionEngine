/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/WorldGrid/WorldGridLayer.hpp>

#include <Scene/WorldGrid/Terrain/Generation/TerrainGenerator.hpp>
#include <Scene/WorldGrid/Terrain/TerrainQuadtree.hpp>
#include <Scene/WorldGrid/Terrain/TerrainGeneratorState.hpp>
#include <Scene/WorldGrid/Terrain/TerrainHeightsCache.hpp>
#include <Scene/WorldGrid/Terrain/TerrainLodViewpoints.hpp>
#include <Scene/WorldGrid/Terrain/TerrainLoadedCells.hpp>
#include <Scene/WorldGrid/Terrain/TerrainGrass.hpp>
#include <Scene/WorldGrid/Terrain/GroundCover.hpp>
#include <Scene/WorldGrid/Terrain/TerrainBrush.hpp>
#include <Scene/WorldGrid/Terrain/TerrainMemory.hpp>

#include <Asset/AssetObject.hpp>

#include <Rendering/RenderableAttributes.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Memory/SharedPtr.hpp>
#include <Core/Containers/FlatMap.hpp>
#include <Core/Threading/Mutex.hpp>
#include <Core/Threading/AtomicVar.hpp>

#include <Core/Math/Ray.hpp>
#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

namespace Hyperion {

class Material;
class Mesh;
class Entity;
class Scene;
class Texture;
class TerrainStreamingCell;
class TerrainCellData;
struct AssetPath;

HYP_STRUCT()
struct TerrainHeightmapTile
{
    HYP_STRUCT_BODY(TerrainHeightmapTile);

    /// tiles share their border samples, so tile (1, 0) starts on tile (0, 0)'s last column
    HYP_FIELD(Property = "Tile", Editor)
    Vec2i tile;

    HYP_FIELD(Property = "Texture", Editor)
    Handle<Texture> texture;
};

HYP_STRUCT()
struct TerrainLayerSettings
{
    HYP_STRUCT_BODY(TerrainLayerSettings);

    HYP_FIELD(Property = "HeightSource", Serialize)
    TerrainHeightSource heightSource = TerrainHeightSource::Procedural;

    HYP_FIELD(Property = "FlatHeight", Serialize)
    float flatHeight = 0.0f;

    HYP_FIELD(Property = "HeightmapCenter", Serialize)
    Vec2f heightmapCenter;

    /// zero for one sample per terrain vertex
    HYP_FIELD(Property = "HeightmapSize", Serialize)
    Vec2f heightmapSize;

    HYP_FIELD(Property = "HeightmapHeightRange", Serialize)
    Vec2f heightmapHeightRange { 0.0f, 200.0f };

    HYP_FIELD(Property = "ProceduralEdges", Serialize)
    bool proceduralEdges = true;

    HYP_FIELD(Property = "BlendDistance", Serialize)
    float blendDistance = 64.0f;

    HYP_FIELD(Property = "ProceduralCells", Serialize)
    Array<Vec2i> proceduralCells;

    /// in the order of the layer's heightmap tile textures
    HYP_FIELD(Property = "HeightmapTiles", Serialize)
    Array<Vec2i> heightmapTiles;

    HYP_FORCE_INLINE bool operator==(const TerrainLayerSettings& other) const = default;
    HYP_FORCE_INLINE bool operator!=(const TerrainLayerSettings& other) const = default;
};

HYP_CLASS()
class ENGINE_API TerrainWorldGridLayer : public WorldGridLayer
{
    HYP_OBJECT_BODY(TerrainWorldGridLayer);

    friend class TerrainBrush;

public:
    TerrainWorldGridLayer();
    explicit TerrainWorldGridLayer(Name name, const WorldGridLayerInfo& layerInfo = {});

    virtual ~TerrainWorldGridLayer() override;

    // the terrain's material is made at run time, so a cook takes its attributes from here
    static MaterialAttributes GetTerrainMaterialAttributes();

    static Vec3f ComputeCellBoundsMin(const WorldGridLayerInfo& layerInfo, const Vec2i& coord);
    static Vec2i ComputeCellCoord(const WorldGridLayerInfo& layerInfo, const Vec2f& worldXZ);

    static uint32 RoundCellSizeForQuadtree(uint32 cellSize);

    ///reads the Terrain.Lod cvars, so each tile keeps the layout it was built with
    static TerrainQuadtreeLayout MakeQuadtreeLayout(uint32 cellSize);

    static float GetLodRangeMultiplier();
    static float CalculateLodRange(uint8 level, const TerrainQuadtreeLayout& layout, const Vec3f& scale);
    static float CalculateLodMorphStart(uint8 level, const TerrainQuadtreeLayout& layout, const Vec3f& scale);

    static uint64 GetCellFingerprint(const TerrainGenerationState& generationState, const Vec2i& coord);
    static uint64 CombineCellFingerprint(uint64 baseFingerprint, const TerrainGenerator& generator, const Vec2i& coord);

    ///thread safe when \p cellSize and \p cellFingerprint come from a GetGenerationState() snapshot
    static bool AreCellHeightsCurrent(const TerrainCellData& cellData, uint32 cellSize, uint64 cellFingerprint);

    ///thread safe when \p generator and \p layerInfo come from a GetGenerationState() snapshot
    static void GenerateCellPaddedHeights(
        const TerrainGenerator& generator,
        const WorldGridLayerInfo& layerInfo,
        const Vec2i& coord,
        Array<float, TerrainAllocator>& outPaddedHeights,
        Array<ubyte, TerrainAllocator>* outErosionMasks = nullptr);

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Scene>& GetScene() const
    {
        return m_scene;
    }

    ///not shown in editor because the editor for this shows LayerInfo's Seed property anyway
    HYP_METHOD(Property = "Seed", Editor = false)
    HYP_FORCE_INLINE uint32 GetSeed() const
    {
        return m_layerInfo.seed;
    }

    HYP_METHOD(Property = "Seed", Editor = false)
    void SetSeed(uint32 seed);

    HYP_METHOD(NoScriptBindings)
    HYP_FORCE_INLINE const TerrainLayerSettings& GetSettings() const
    {
        return m_settings;
    }

    HYP_METHOD(NoScriptBindings)
    void SetSettings(const TerrainLayerSettings& settings);

    HYP_METHOD(Property = "HeightSource", Editor, Label = "Height Source")
    HYP_FORCE_INLINE TerrainHeightSource GetHeightSource() const
    {
        return m_settings.heightSource;
    }

    HYP_METHOD(Property = "HeightSource", Editor)
    void SetHeightSource(TerrainHeightSource heightSource);

    HYP_METHOD(Property = "FlatHeight", Editor, NoScriptBindings)
    HYP_FORCE_INLINE float GetFlatHeight() const
    {
        return m_settings.flatHeight;
    }

    HYP_METHOD(Property = "FlatHeight", Editor, NoScriptBindings)
    void SetFlatHeight(float flatHeight);

    HYP_METHOD(Property = "Heightmap", Editor)
    const Handle<Texture>& GetHeightmap() const;

    /// A greyscale texture. 16 bit images and .r16 files keep full precision
    HYP_METHOD(Property = "Heightmap", Editor)
    void SetHeightmap(const Handle<Texture>& heightmap);

    HYP_METHOD(Property = "HeightmapTiles", Editor, NoScriptBindings)
    Array<TerrainHeightmapTile> GetHeightmapTiles() const;

    /// Equal sized tiles that share their border samples. Tile y counts along +Z
    HYP_METHOD(Property = "HeightmapTiles", Editor, NoScriptBindings)
    void SetHeightmapTiles(const Array<TerrainHeightmapTile>& tiles);

    HYP_METHOD()
    uint32 GetNumHeightmapTiles() const;

    HYP_METHOD(Property = "HeightmapCenter", Editor, NoScriptBindings)
    HYP_FORCE_INLINE Vec2f GetHeightmapCenter() const
    {
        return m_settings.heightmapCenter;
    }

    HYP_METHOD(Property = "HeightmapCenter", Editor, NoScriptBindings)
    void SetHeightmapCenter(Vec2f heightmapCenter);

    HYP_METHOD(Property = "HeightmapSize", Editor, NoScriptBindings)
    HYP_FORCE_INLINE Vec2f GetHeightmapSize() const
    {
        return m_settings.heightmapSize;
    }

    HYP_METHOD(Property = "HeightmapSize", Editor, NoScriptBindings)
    void SetHeightmapSize(Vec2f heightmapSize);

    HYP_METHOD(Property = "HeightmapHeightRange", Editor, NoScriptBindings)
    HYP_FORCE_INLINE Vec2f GetHeightmapHeightRange() const
    {
        return m_settings.heightmapHeightRange;
    }

    HYP_METHOD(Property = "HeightmapHeightRange", Editor, NoScriptBindings)
    void SetHeightmapHeightRange(Vec2f heightmapHeightRange);

    HYP_METHOD(Property = "ProceduralEdges", Editor, NoScriptBindings)
    HYP_FORCE_INLINE bool GetProceduralEdges() const
    {
        return m_settings.proceduralEdges;
    }

    HYP_METHOD(Property = "ProceduralEdges", Editor, NoScriptBindings)
    void SetProceduralEdges(bool proceduralEdges);

    HYP_METHOD(Property = "BlendDistance", Editor, NoScriptBindings)
    HYP_FORCE_INLINE float GetBlendDistance() const
    {
        return m_settings.blendDistance;
    }

    HYP_METHOD(Property = "BlendDistance", Editor, NoScriptBindings)
    void SetBlendDistance(float blendDistance);

    ///sim thread only - other threads must use GetGenerationState()
    HYP_FORCE_INLINE const TerrainGenerator& GetGenerator() const
    {
        return *m_generatorState.GetGenerator();
    }

    TerrainGenerationState GetGenerationState() const;

    ///false once Regenerate() has replaced the generator that \p epoch belongs to
    HYP_FORCE_INLINE bool IsGenerationCurrent(uint32 epoch) const
    {
        return m_generatorState.IsCurrent(epoch);
    }

    uint64 GetCellFingerprint(const Vec2i& coord) const;

    /// Discards all edits and regenerates the tiles procedurally
    HYP_METHOD(EditorAction = "Regenerate")
    void Regenerate();

    /// Generate all tiles in the layer's defined range
    HYP_METHOD(EditorAction = "Generate All", EditCondition = "HasDiscreteRange")
    void GenerateAll();

    /// not finite, has a defined range where tiles are allowed to generate.
    HYP_METHOD()
    bool HasDiscreteRange() const
    {
        return !m_layerInfo.infinite && m_layerInfo.range.x < m_layerInfo.range.y;
    }

    ///result is only cached while \p generationEpoch is still current
    SharedPtr<const Array<float, TerrainAllocator>> GetOrGenerateCellHeights(const TerrainGenerator& generator, uint32 generationEpoch, const Vec2i& coord) const;
    SharedPtr<const Array<float, TerrainAllocator>> TryGetCachedCellHeights(const Vec2i& coord) const;

    void WarmHeightsCache(const Vec2i& coord) const;

    ///first of the coord's references that resolves
    Handle<TerrainCellData> FindCellData(const Vec2i& coord) const;

    ///sim thread - the loaded tile at \p coord, if there is one
    Handle<TerrainStreamingCell> FindLoadedCell(const Vec2i& coord) const;

    void RegisterLoadedCell(const Vec2i& coord, const WeakHandle<TerrainStreamingCell>& cell);

    ///only unregisters if \p cell is the one registered at \p coord
    void UnregisterLoadedCell(const Vec2i& coord, const TerrainStreamingCell* cell);

    ///saved heights are usable if they match the current generator, or were sculpted (frozen) at the current cell size
    bool AreCellHeightsCurrent(const TerrainCellData& cellData) const;

    void GenerateCellPaddedHeights(const Vec2i& coord, Array<float, TerrainAllocator>& outPaddedHeights, Array<ubyte, TerrainAllocator>* outErosionMasks = nullptr) const;

    ///saves freshly generated heights and their erosion masks into the cell's data, creating it if needed; leaves current heights untouched
    Handle<TerrainCellData> StoreGeneratedCellHeights(const Vec2i& coord, Span<const float> paddedHeights, Span<const ubyte> erosionMasks);

    ///world space height of the full resolution surface - the one the collider and the brush work on
    float SampleHeightAt(const Vec2f& worldXZ) const;

    ///sim thread only - SampleHeightAt(), but generates a cold cell's heights instead of returning the un-eroded stand-in
    float SampleHeightAtBlocking(const Vec2f& worldXZ) const;

    ///world space height of the surface currently drawn at \p worldXZ, CDLOD morph included. Falls back to
    ///SampleHeightAt() where no tile is loaded. Editor picking uses this so the cursor lands on what's on screen
    float SampleDrawnHeightAt(const Vec2f& worldXZ) const;

    bool RaycastSurface(const Ray& ray, Vec3f& outHitPoint) const;

    void MarkHeightsDirty(const WorldGridLayerInfo& layerInfo, const Vec2i& coord) const;
    void MarkHeightsDirty(const Vec4f& worldRect) const;

    ///world XZ rects (xy = min, zw = max) where SampleHeightAt() may return something new since the last call.
    ///this method is thread safe
    Array<Vec4f, TerrainAllocator> TakeHeightDirtyRects();

    HYP_METHOD()
    bool IsProceduralCell(const Vec2i& coord) const;

    HYP_METHOD()
    void MarkProceduralCell(const Vec3f& worldPos, bool isProcedural);

    HYP_METHOD()
    void SetProceduralCells(const Array<Vec2i>& proceduralCells);

    HYP_METHOD()
    uint32 GetNumProceduralCells() const;

    HYP_METHOD()
    void ClearProceduralCells();

    bool TakeProceduralCellsEdit(
        Array<Vec2i, TerrainAllocator>& outBefore,
        Array<Vec2i, TerrainAllocator>& outAfter);

    HYP_METHOD()
    void ApplyBrush(const Vec3f& worldPos, float radius, float strength, bool raise);

    HYP_METHOD()
    void PaintSplat(const Vec3f& worldPos, float radius, float strength, uint32 layerIndex, bool erase);

    /// paints the painted GroundCover layer named \p groundCoverLayer - see GetPaintedGroundCoverLayers()
    HYP_METHOD()
    void PaintGroundCover(const Vec3f& worldPos, float radius, float strength, Name groundCoverLayer, bool erase);

    void EndBrushStroke();

    /// sim thread only - the layers of the terrain's GroundCover that grow where painted
    HYP_METHOD()
    Array<Name> GetPaintedGroundCoverLayers();

    Array<TerrainGroundCoverPaintEdit, TerrainAllocator> TakeGroundCoverPaintEdits();
    void SetGroundCoverPaintState(const Vec2i& coord, const TerrainGroundCoverPaintState& state);

    HYP_FORCE_INLINE TerrainGroundCoverResources& GetGroundCoverResources()
    {
        return m_groundCoverResources;
    }

    HYP_METHOD(Property = "GroundCover", Editor)
    const Handle<GroundCover>& GetGroundCover() const;

    HYP_METHOD(Property = "GroundCover", Editor)
    void SetGroundCover(const Handle<GroundCover>& groundCover);

    HYP_FORCE_INLINE Handle<GroundCover> GetPlantedGroundCover() const
    {
        return m_groundCoverResources.GetPlantedGroundCover();
    }

    HYP_METHOD(EditorAction = "Replant Ground Cover")
    void ReplantGroundCover();

    void UpdateLodSelection(Span<const Vec3f> viewpoints);

    void RetireEntity(Handle<Entity>&& entity);

    void SetLodViewpoints(Span<const Vec3f> viewpoints);
    Array<Vec3f, TerrainAllocator> GetLodViewpoints() const;

    float GetNearestLodViewpointDistance(const BoundingBox& worldBounds) const;

    HYP_METHOD()
    virtual void SetLayerInfo(const WorldGridLayerInfo& layerInfo) override;

    virtual bool IsCollisionPendingAt(const Vec3f& worldPosition) const override;

    virtual void GetDescAssets(Array<WGLayerAsset>& outAssets) const override;
    virtual void SetDescAssets(Span<const WGLayerAsset> assets) override;

    virtual BoxedValue GetDescSettings() const override;
    virtual void SetDescSettings(const BoxedValue& settings) override;

protected:
    struct RetiredEntity
    {
        Handle<Entity> entity;
        uint32 frameCounter = 0;
    };

    static uint64 ComputeCellFingerprint(const TerrainGenerator& generator, const WorldGridLayerInfo& layerInfo);

    virtual void OnAdded(WorldGrid* worldGrid) override;
    virtual void OnRemoved(WorldGrid* worldGrid) override;

    virtual Handle<StreamingCell> CreateStreamingCell(const StreamingCellInfo& cellInfo) override;

    virtual void StreamPrefetch(Span<const Vec2i> cellCoords) override;

    TerrainGenerationParams MakeGenerationParams() const;
    TerrainHeightSourceDesc MakeHeightSourceDesc();

    ///swaps in a freshly configured generator and bumps the epoch so work started with the old one is discarded
    void ReplaceGenerator();
    void RefreshGenerator();

    bool IsAddedToWorld() const;
    void MarkWorldDirty() const;

    float SampleHeight(const Vec2f& worldXZ, bool generateColdCells) const;

    void DetachLoadedCells();

    void ReleaseRetiredEntities();

    void DiscardAllCellData();
    void DeletePersistedCellData(const AssetPath& assetPath);

    ///for cell data created because none of the coord's references resolved - replaces those references
    void AddCellData(const Vec2i& coord, const Handle<TerrainCellData>& cellData);

    ///drops references to cell data that isn't registered (never saved) and duplicate references; true if any were dropped
    bool RemoveUnregisteredCellData();

    Handle<Scene> m_scene;
    Handle<Material> m_material;

    TerrainGeneratorState m_generatorState;
    TerrainLayerSettings m_settings;

    ///in the order of m_settings.heightmapTiles
    Array<AssetReference, TerrainAllocator> m_heightmapTileTextures;

    ///the copy the generator samples, kept while the heightmap's content is the same
    SharedPtr<const TerrainHeightmapSamples> m_heightmapSamples;

    mutable TerrainHeightsCache m_heightsCache;

    mutable Mutex m_heightDirtyMutex;
    mutable Array<Vec4f, TerrainAllocator> m_heightDirtyRects;

    TerrainLoadedCells m_loadedCells;

    Array<RetiredEntity, TerrainAllocator> m_retiredEntities;

    TerrainBrush m_brush { *this };

    Optional<Array<Vec2i, TerrainAllocator>> m_proceduralCellsBeforeStroke;
    Optional<Array<Vec2i, TerrainAllocator>> m_proceduralCellsEditBefore;

    TerrainLodViewpoints m_lodViewpoints;

    TerrainGroundCoverResources m_groundCoverResources;
};

} // namespace Hyperion
