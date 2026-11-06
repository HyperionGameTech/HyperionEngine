/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Editor/Painting/EditorSurfacePainterState.hpp>
#include <Editor/Painting/SurfacePainterStroke.hpp>

#include <Core/Math/BoundingBox.hpp>

#include <Scene/Instancing/InstanceTypes.hpp>
#include <Scene/Instancing/InstanceGroup.hpp>
#include <Scene/Instancing/PrefabSet.hpp>

namespace Hyperion {

class Prefab;

struct InstanceStrokeTraits
{
    using Target = InstanceGroup;
    using Record = InstanceRecord;

    static constexpr const char* Noun = "instance";

    static InstanceId GetId(const InstanceRecord& record)
    {
        return record.GetId();
    }

    static void Add(InstanceGroup& group, const InstanceRecord& record)
    {
        group.AddInstanceWithId(record.GetId(), record.GetTransform());
    }

    static void Remove(InstanceGroup& group, const InstanceRecord& record)
    {
        group.RemoveInstance(record.GetId());
    }

    static bool IsEmpty(const InstanceGroup& group)
    {
        return group.NumInstances() == 0;
    }
};

HYP_CLASS(Serialize = false)
class EDITOR_API EditorInstancePainterState final : public EditorSurfacePainterState
{
    HYP_OBJECT_BODY(EditorInstancePainterState);

public:
    EditorInstancePainterState();
    ~EditorInstancePainterState() override;

    HYP_METHOD(Property = "ActivePrefab", Editor)
    const Handle<Prefab>& GetActivePrefab() const;

    HYP_METHOD(Property = "ActivePrefab", Editor)
    void SetActivePrefab(const Handle<Prefab>& prefab);

    HYP_METHOD(Property = "ActivePrefabSet", Editor)
    const Handle<PrefabSet>& GetActivePrefabSet() const;

    HYP_METHOD(Property = "ActivePrefabSet", Editor)
    void SetActivePrefabSet(const Handle<PrefabSet>& prefabSet);

    HYP_METHOD()
    void SetActivePrefabByName(Name assetName);

    HYP_METHOD()
    void SetActivePrefabSetByName(Name assetName);

    ///Palette

    HYP_METHOD()
    uint32 GetPaletteCount() const;

    HYP_METHOD()
    Name GetPaletteName(uint32 index) const;

    HYP_METHOD()
    float GetPaletteWeight(uint32 index) const;

    HYP_METHOD()
    bool IsPaletteEntryIncluded(uint32 index) const;

    HYP_METHOD()
    void SetPaletteEntryIncluded(uint32 index, bool included);

    ///Scale

    HYP_METHOD()
    float GetScaleMin() const;

    HYP_METHOD()
    void SetScaleMin(float scaleMin);

    HYP_METHOD()
    float GetScaleMax() const;

    HYP_METHOD()
    void SetScaleMax(float scaleMax);

    //////////

    /// Radius around the placement point probed for the lowest ground, so nothing overhangs a slope. 0 = off
    HYP_METHOD()
    float GetFootprintRadius() const;

    HYP_METHOD()
    void SetFootprintRadius(float footprintRadius);

    /// How far below the ground an instance is placed
    HYP_METHOD()
    float GetSinkDepth() const;

    HYP_METHOD()
    void SetSinkDepth(float sinkDepth);

    HYP_METHOD()
    bool GetOnlyOnGround() const;

    HYP_METHOD()
    void SetOnlyOnGround(bool onlyOnGround);

protected:
    const char* GetToolName() const override
    {
        return "instance painter";
    }

    const char* GetAssetTypeName() const override
    {
        return "prefab";
    }

    bool HasActiveAsset() const override;
    Vec3f GetPlacementScale() const override;
    Transform GetCursorTransform(const SurfaceHit& hit) const override;
    bool ShouldIgnoreHit(const RayHit& hit) const override;
    Vec3f GetPlacementPosition(const SurfaceHit& hit) const override;
    bool CanPaintSurface(const SurfaceHit& hit) const override;

    void PaintAt(const SurfaceHit& hit) override;
    void EraseAt(const SurfaceHit& hit) override;

    void ResetStrokeEdits() override;
    void CommitStrokeEdits(const WeakHandle<Scene>& strokeScene) override;

private:
    Handle<InstanceGroup> FindOrCreateInstanceGroup(const Handle<Scene>& scene, const Handle<Prefab>& prefab);

    bool IsPaintedPrefab(const Handle<Prefab>& prefab) const;

    Array<PrefabSetEntry> GetPalette() const;
    bool IsExcluded(const Handle<Prefab>& prefab) const;

    void DrawNextStamp();

    Handle<Prefab> m_activePrefab;
    Handle<PrefabSet> m_activePrefabSet;
    Array<WeakHandle<Prefab>> m_excludedPrefabs;

    Handle<Prefab> m_nextPrefab;
    float m_nextScale = 1.0f;
    BoundingBox m_prefabBounds;

    float m_scaleMin = 1.0f;
    float m_scaleMax = 1.0f;

    float m_footprintRadius = 0.5f;
    float m_sinkDepth = 0.1f;

    bool m_onlyOnGround = false;

    SurfacePainterStroke<InstanceStrokeTraits> m_stroke;
};

} // namespace Hyperion
