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

#include <Core/Memory/UniquePtr.hpp>
#include <Core/Memory/SharedPtr.hpp>
#include <Core/Containers/FlatMap.hpp>
#include <Core/Threading/Mutex.hpp>

#include <Core/Math/Ray.hpp>
#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

namespace Hyperion {

class Material;
class Mesh;
class Scene;
class TerrainStreamingCell;
class TerrainCellData;

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

    HYP_FORCE_INLINE const TerrainGenerator& GetGenerator() const
    {
        return *m_generator;
    }

    HYP_METHOD(Property = "Seed")
    HYP_FORCE_INLINE uint32 GetSeed() const
    {
        return m_layerInfo.seed;
    }

    HYP_METHOD(Property = "Seed")
    void SetSeed(uint32 seed);

    HYP_METHOD()
    virtual void SetLayerInfo(const WorldGridLayerInfo& layerInfo) override;

    SharedPtr<const Array<float>> GetOrGenerateCellHeights(const Vec2i& coord) const;

    HYP_FORCE_INLINE uint64 GetCellFingerprint() const
    {
        return m_cellFingerprint;
    }

    Handle<TerrainCellData> FindCellData(const Vec2i& coord) const;

    ///saved heights are usable if they match the current generator, or were sculpted (frozen) at the current cell size
    bool AreCellHeightsCurrent(const TerrainCellData& cellData) const;

    void GenerateCellPaddedHeights(const Vec2i& coord, Array<float>& outPaddedHeights) const;

    ///saves freshly generated heights into the cell's data, creating it if needed; leaves current heights untouched
    Handle<TerrainCellData> StoreGeneratedCellHeights(const Vec2i& coord, Span<const float> paddedHeights);

    HYP_METHOD()
    void ApplyBrush(const Vec3f& worldPos, float radius, float strength, bool raise);

    
    HYP_METHOD()
    void PaintSplat(const Vec3f& worldPos, float radius, float strength, uint32 layerIndex, bool erase);

    float SampleHeightAt(const Vec2f& worldXZ) const;
    bool RaycastSurface(const Ray& ray, Vec3f& outHitPoint) const;

    void EndBrushStroke();

    void RegisterLoadedCell(const Vec2i& coord, const WeakHandle<TerrainStreamingCell>& cell);
    void UnregisterLoadedCell(const Vec2i& coord);

protected:
    virtual void OnAdded(WorldGrid* worldGrid) override;
    virtual void OnRemoved(WorldGrid* worldGrid) override;

    virtual Handle<StreamingCell> CreateStreamingCell(const StreamingCellInfo& cellInfo) override;

    void Regenerate();
    void UpdateCellFingerprint();

    Handle<Scene> m_scene;
    Handle<Material> m_material;
    UniquePtr<TerrainGenerator> m_generator;

    mutable Mutex m_heightCacheMutex;
    mutable FlatMap<Vec2i, SharedPtr<Array<float>>> m_cellHeightsCache;

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
