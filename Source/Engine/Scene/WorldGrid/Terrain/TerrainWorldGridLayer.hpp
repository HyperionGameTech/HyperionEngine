/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/WorldGrid/WorldGridLayer.hpp>

#include <Scene/WorldGrid/Terrain/Generation/TerrainGenerator.hpp>

#include <Asset/AssetObject.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Memory/SharedPtr.hpp>
#include <Core/Containers/FlatMap.hpp>
#include <Core/Threading/Mutex.hpp>
#include <Core/Threading/AtomicVar.hpp>

#include <Core/Math/Ray.hpp>
#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

namespace Hyperion {

class Material;
class Mesh;
class Scene;
class TerrainStreamingCell;
class TerrainCellData;
struct AssetPath;

struct TerrainGenerationState
{
    SharedPtr<TerrainGenerator> generator;
    uint64 cellFingerprint = 0;
    uint32 epoch = 0;

    ///taken together with the epoch - cell geometry (size, scale, offset) changes bump the epoch
    WorldGridLayerInfo layerInfo;
};

HYP_CLASS()
class ENGINE_API TerrainWorldGridLayer : public WorldGridLayer
{
    HYP_OBJECT_BODY(TerrainWorldGridLayer);

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
        return *m_generator;
    }

    TerrainGenerationState GetGenerationState() const;

    ///false once Regenerate() has replaced the generator that \p epoch belongs to
    HYP_FORCE_INLINE bool IsGenerationCurrent(uint32 epoch) const
    {
        return m_generationEpoch.Get(MemoryOrder::ACQUIRE) == epoch;
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
        return m_cellFingerprint;
    }

    Handle<TerrainCellData> FindCellData(const Vec2i& coord) const;

    ///saved heights are usable if they match the current generator, or were sculpted (frozen) at the current cell size
    bool AreCellHeightsCurrent(const TerrainCellData& cellData) const;
    ///thread safe when \p cellSize and \p cellFingerprint come from a GetGenerationState() snapshot
    static bool AreCellHeightsCurrent(const TerrainCellData& cellData, uint32 cellSize, uint64 cellFingerprint);

    void GenerateCellPaddedHeights(const Vec2i& coord, Array<float>& outPaddedHeights) const;
    ///thread safe when \p generator and \p layerInfo come from a GetGenerationState() snapshot
    static void GenerateCellPaddedHeights(const TerrainGenerator& generator, const WorldGridLayerInfo& layerInfo, const Vec2i& coord, Array<float>& outPaddedHeights);

    ///saves freshly generated heights into the cell's data, creating it if needed; leaves current heights untouched
    Handle<TerrainCellData> StoreGeneratedCellHeights(const Vec2i& coord, Span<const float> paddedHeights);

    HYP_METHOD()
    void ApplyBrush(const Vec3f& worldPos, float radius, float strength, bool raise);

    
    HYP_METHOD()
    void PaintSplat(const Vec3f& worldPos, float radius, float strength, uint32 layerIndex, bool erase);

    float SampleHeightAt(const Vec2f& worldXZ) const;
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

    ///CDLOD//////////////////
    
    HYP_METHOD()
    uint8 GetEffectiveLodCount() const;

    HYP_METHOD()
    uint32 GetEffectiveLodStrideMultiplier() const;

    HYP_METHOD()
    float GetEffectiveLodRangeMultiplier() const;

    HYP_METHOD()
    float GetLodRange(uint8 lodIndex) const;

    HYP_METHOD()
    float GetLodMorphStart(uint8 lodIndex) const;

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

    ///call after bumping the epoch, so in-flight work from the previous epoch can't repopulate the caches
    void ClearHeightCaches();

    void DetachLoadedCells();
    void DiscardAllCellData();
    void DeletePersistedCellData(const AssetPath& assetPath);

    Handle<Scene> m_scene;
    Handle<Material> m_material;

    HYP_FIELD(Property = "LodCount", Editor = true,
        Description = "Number of mesh LODs per cell")
    uint8 m_lodCount = 3;

    HYP_FIELD(Property = "LodStrideMultiplier", Editor = true,
        Description = "How much sparser each LOD's vertex grid is than the previous one")
    uint32 m_lodStrideMultiplier = 4;

    HYP_FIELD(Property = "LodBaseRange", Editor = true,
        Description = "World-space distance from the camera where full-resolution cells finish morphing into LOD 1.")
    float m_lodBaseRange = 96.0f;

    HYP_FIELD(Property = "LodRangeMultiplier", Editor = true,
        Description = "Each LOD's range is the previous LOD's range times this value.")
    float m_lodRangeMultiplier = 2.0f;

    HYP_FIELD(Property = "LodMorphStartRatio", Editor = true,
        Description = "Fraction of each LOD's distance band spent at full detail before it starts morphing into the next LOD. Lower values == longer, smoother transitions")
    float m_lodMorphStartRatio = 0.7f;
    
    ///written on the sim thread only, under m_generationStateMutex
    SharedPtr<TerrainGenerator> m_generator;
    mutable Mutex m_generationStateMutex;
    AtomicVar<uint32> m_generationEpoch { 0 };

    mutable Mutex m_heightCacheMutex;
    mutable FlatMap<Vec2i, SharedPtr<Array<float>>> m_cellHeightsCache;

    mutable Mutex m_pendingWarmsMutex;
    mutable FlatMap<Vec2i, bool> m_pendingHeightWarms;

    FlatMap<Vec2i, WeakHandle<TerrainStreamingCell>> m_loadedCells;
    FlatMap<Vec2i, bool> m_cellsModifiedSinceStrokeEnd;

    uint64 m_cellFingerprint = 0;

    struct HeightsSampleCache
    {
        Handle<TerrainCellData> cell;
        TSharedResLock<AssetObject> scope;
        Span<const float> heights;

        void Invalidate();
    };

    mutable HeightsSampleCache m_heightsSampleCache;
};

} // namespace Hyperion
