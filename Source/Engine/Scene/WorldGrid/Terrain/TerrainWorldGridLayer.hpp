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

#include <Asset/AssetObject.hpp>

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
class Scene;
class Texture;
class TerrainStreamingCell;
class TerrainCellData;
struct AssetPath;

/// @TODO: Make a TerrainAllocator - use it throughout here.

HYP_CLASS()
class ENGINE_API TerrainWorldGridLayer : public WorldGridLayer
{
    HYP_OBJECT_BODY(TerrainWorldGridLayer);

    friend class TerrainBrush;

public:
    TerrainWorldGridLayer();

    explicit TerrainWorldGridLayer(Name name, const WorldGridLayerInfo& layerInfo = {});

    virtual ~TerrainWorldGridLayer() override;

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Scene>& GetScene() const
    {
        return m_scene;
    }

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

    ///not shown in editor because the editor for this shows LayerInfo's Seed property anyway
    HYP_METHOD(Property = "Seed", Editor = false)
    HYP_FORCE_INLINE uint32 GetSeed() const
    {
        return m_layerInfo.seed;
    }

    HYP_METHOD(Property = "Seed")
    void SetSeed(uint32 seed);

    HYP_METHOD()
    virtual void SetLayerInfo(const WorldGridLayerInfo& layerInfo) override;

    ///result is only cached while \p generationEpoch is still current
    SharedPtr<const Array<float>> GetOrGenerateCellHeights(const TerrainGenerator& generator, uint32 generationEpoch, const Vec2i& coord) const;
    SharedPtr<const Array<float>> TryGetCachedCellHeights(const Vec2i& coord) const;

    void WarmHeightsCache(const Vec2i& coord) const;

    HYP_FORCE_INLINE uint64 GetCellFingerprint() const
    {
        return m_generatorState.GetCellFingerprint();
    }

    ///first of the coord's references that resolves
    Handle<TerrainCellData> FindCellData(const Vec2i& coord) const;

    ///sim thread - the loaded tile at \p coord, if there is one
    Handle<TerrainStreamingCell> FindLoadedCell(const Vec2i& coord) const;

    ///saved heights are usable if they match the current generator, or were sculpted (frozen) at the current cell size
    bool AreCellHeightsCurrent(const TerrainCellData& cellData) const;
    ///thread safe when \p cellSize and \p cellFingerprint come from a GetGenerationState() snapshot
    static bool AreCellHeightsCurrent(const TerrainCellData& cellData, uint32 cellSize, uint64 cellFingerprint);

    void GenerateCellPaddedHeights(const Vec2i& coord, Array<float>& outPaddedHeights, Array<ubyte>* outErosionMasks = nullptr) const;
    ///thread safe when \p generator and \p layerInfo come from a GetGenerationState() snapshot
    static void GenerateCellPaddedHeights(
        const TerrainGenerator& generator,
        const WorldGridLayerInfo& layerInfo,
        const Vec2i& coord,
        Array<float>& outPaddedHeights,
        Array<ubyte>* outErosionMasks = nullptr);

    ///saves freshly generated heights and their erosion masks into the cell's data, creating it if needed; leaves current heights untouched
    Handle<TerrainCellData> StoreGeneratedCellHeights(const Vec2i& coord, Span<const float> paddedHeights, Span<const ubyte> erosionMasks);

    HYP_METHOD()
    void ApplyBrush(const Vec3f& worldPos, float radius, float strength, bool raise);

    
    HYP_METHOD()
    void PaintSplat(const Vec3f& worldPos, float radius, float strength, uint32 layerIndex, bool erase);

    /// paints the painted GroundCover layer named \p groundCoverLayer - see GetPaintedGroundCoverLayers()
    HYP_METHOD()
    void PaintGroundCover(const Vec3f& worldPos, float radius, float strength, Name groundCoverLayer, bool erase);

    /// sim thread only - the layers of the terrain's GroundCover that grow where painted
    HYP_METHOD()
    Array<Name> GetPaintedGroundCoverLayers();

    Array<TerrainGroundCoverPaintEdit> TakeGroundCoverPaintEdits();
    void SetGroundCoverPaintState(const Vec2i& coord, const TerrainGroundCoverPaintState& state);

    ///world space height of the full resolution surface - the one the collider and the brush work on
    float SampleHeightAt(const Vec2f& worldXZ) const;

    ///world XZ rects (xy = min, zw = max) where SampleHeightAt() may return something new since the last call.
    ///this method is thread safe
    Array<Vec4f> TakeHeightDirtyRects();

    ///sim thread only - SampleHeightAt(), but generates a cold cell's heights instead of returning the un-eroded stand-in
    float SampleHeightAtBlocking(const Vec2f& worldXZ) const;

    ///world space height of the surface currently drawn at \p worldXZ, CDLOD morph included. Falls back to
    ///SampleHeightAt() where no tile is loaded. Editor picking uses this so the cursor lands on what's on screen
    float SampleDrawnHeightAt(const Vec2f& worldXZ) const;

    bool RaycastSurface(const Ray& ray, Vec3f& outHitPoint) const;

    virtual bool IsCollisionPendingAt(const Vec3f& worldPosition) const override;

    void EndBrushStroke();

    void RegisterLoadedCell(const Vec2i& coord, const WeakHandle<TerrainStreamingCell>& cell);

    ///only unregisters if \p cell is the one registered at \p coord
    void UnregisterLoadedCell(const Vec2i& coord, const TerrainStreamingCell* cell);

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

    ///CDLOD - tuned through the Terrain.Lod.* cvars, shared by every terrain layer

    ///the tile's quadtree needs (cellSize - 1) to be a power of two
    static Vec3f ComputeCellBoundsMin(const WorldGridLayerInfo& layerInfo, const Vec2i& coord);

    ///inverse of ComputeCellBoundsMin - caller must ensure the cell's world size is non-zero
    static Vec2i ComputeCellCoord(const WorldGridLayerInfo& layerInfo, const Vec2f& worldXZ);

    static uint32 RoundCellSizeForQuadtree(uint32 cellSize);

    ///reads the Terrain.Lod cvars, so each tile keeps the layout it was built with
    static TerrainQuadtreeLayout MakeQuadtreeLayout(uint32 cellSize);

    static float GetLodRangeMultiplier();

    static float CalculateLodRange(uint8 level, const TerrainQuadtreeLayout& layout, const Vec3f& scale);
    static float CalculateLodMorphStart(uint8 level, const TerrainQuadtreeLayout& layout, const Vec3f& scale);

    void MarkHeightsDirty(const WorldGridLayerInfo& layerInfo, const Vec2i& coord) const;
    void MarkHeightsDirty(const Vec4f& worldRect) const;

    ///sim thread only - reselects the drawn patches of every loaded tile
    void UpdateLodSelection(Span<const Vec3f> viewpoints);

    ///sim thread only - lets tiles streaming in pick which quadtree levels to build before the LOD system sees them
    void SetLodViewpoints(Span<const Vec3f> viewpoints);

    Array<Vec3f> GetLodViewpoints() const;

    ///infinity until the LOD system has run
    float GetNearestLodViewpointDistance(const BoundingBox& worldBounds) const;

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

    virtual void GetDescAssets(Array<WGLayerAsset>& outAssets) const override;
    virtual void SetDescAssets(Span<const WGLayerAsset> assets) override;

    /////////////////////////

protected:
    virtual void OnAdded(WorldGrid* worldGrid) override;
    virtual void OnRemoved(WorldGrid* worldGrid) override;

    virtual Handle<StreamingCell> CreateStreamingCell(const StreamingCellInfo& cellInfo) override;

    virtual void StreamPrefetch(Span<const Vec2i> cellCoords) override;

    TerrainGenerationParams MakeGenerationParams() const;
    static uint64 ComputeCellFingerprint(const TerrainGenerator& generator, const WorldGridLayerInfo& layerInfo);

    ///swaps in a freshly configured generator and bumps the epoch so work started with the old one is discarded
    void ReplaceGenerator();

    float SampleHeight(const Vec2f& worldXZ, bool generateColdCells) const;

    void DetachLoadedCells();
    void DiscardAllCellData();
    void DeletePersistedCellData(const AssetPath& assetPath);

    ///for cell data created because none of the coord's references resolved - replaces those references
    void AddCellData(const Vec2i& coord, const Handle<TerrainCellData>& cellData);

    ///drops references to cell data that isn't registered (never saved) and duplicate references; true if any were dropped
    bool RemoveUnregisteredCellData();

    Handle<Scene> m_scene;
    Handle<Material> m_material;

    TerrainGeneratorState m_generatorState;

    mutable TerrainHeightsCache m_heightsCache;

    mutable Mutex m_heightDirtyMutex;
    mutable Array<Vec4f> m_heightDirtyRects;

    TerrainLoadedCells m_loadedCells;

    TerrainBrush m_brush { *this };

    TerrainLodViewpoints m_lodViewpoints;

    TerrainGroundCoverResources m_groundCoverResources;
};

} // namespace Hyperion
