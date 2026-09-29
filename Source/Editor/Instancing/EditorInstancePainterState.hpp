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

    void StampAt(const SurfaceHit& hit) override;
    void EraseAt(const SurfaceHit& hit) override;

    void ResetStrokeEdits() override;
    void CommitStrokeEdits(const WeakHandle<Scene>& strokeScene) override;

private:
    Handle<InstanceGroup> FindOrCreateInstanceGroup(const Handle<Scene>& scene);

    Handle<Prefab> m_activePrefab;
    BoundingBox m_prefabBounds;

    float m_footprintRadius = 0.5f;
    float m_sinkDepth = 0.1f;

    SurfacePainterStroke<InstanceStrokeTraits> m_stroke;
};

} // namespace Hyperion
