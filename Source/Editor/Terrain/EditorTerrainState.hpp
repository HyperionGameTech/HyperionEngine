/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Utilities/ClockTimer.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

namespace Hyperion {

class EditorSubsystem;
class DebugDrawCommandList;
class Scene;
class World;
class WorldGrid;
class TerrainWorldGridLayer;

HYP_ENUM()
enum class TerrainSculptMode : uint8
{
    Raise,
    Lower,
    PaintSplat
};

HYP_CLASS(Serialize = false)
class EDITOR_API EditorTerrainState : public ObjectBase
{
    HYP_OBJECT_BODY(EditorTerrainState);

public:
    EditorTerrainState();
    ~EditorTerrainState() override;

    void Initialize(EditorSubsystem* subsystem);

    HYP_METHOD()
    bool IsEnabled() const;

    HYP_METHOD()
    void SetEnabled(bool enabled);

    HYP_METHOD()
    float GetRadius() const;

    HYP_METHOD()
    void SetRadius(float radius);

    HYP_METHOD()
    float GetStrength() const;

    HYP_METHOD()
    void SetStrength(float strength);

    HYP_METHOD()
    TerrainSculptMode GetMode() const;

    HYP_METHOD()
    void SetMode(TerrainSculptMode mode);

    HYP_METHOD()
    bool IsSculptActive() const;

    HYP_METHOD()
    bool IsPaintActive() const;

    HYP_METHOD()
    void ActivateSculpt();

    HYP_METHOD()
    void ActivatePaint();

    HYP_METHOD()
    int GetPaintLayer() const;

    HYP_METHOD()
    void SetPaintLayer(int paintLayer);

    HYP_METHOD()
    bool CanSculptTerrainForWorld(const Handle<World>& world) const;

    void BeginStroke(const Vec2f& relativePos, bool invert);
    void UpdateStroke(const Vec2f& relativePos, bool invert);
    void EndStroke();

    HYP_FORCE_INLINE bool IsStroking() const
    {
        return m_isStroking;
    }

    void Update();

    void UpdateHover(const Vec2f& relativePos);

    void DebugDrawCursor(DebugDrawCommandList& debugDrawCommandList);

private:
    bool TryGetTerrainHit(const Vec2f& relativePos, Handle<TerrainWorldGridLayer>& outLayer, Vec3f& outWorldPos) const;
    void ApplyBrushAt(const Handle<TerrainWorldGridLayer>& layer, const Vec3f& worldPos, bool invert, float dt);
    bool TryApplyAtScreenPos(const Vec2f& relativePos, bool invert, float dt);

    ///one dab of the held stroke, at the anchored position unless the cursor has moved since the last one
    bool ApplyStroke(float dt);

    EditorSubsystem* m_subsystem = nullptr;

    bool m_enabled = false;
    
    TerrainSculptMode m_mode = TerrainSculptMode::Raise;
    TerrainSculptMode m_sculptDirection = TerrainSculptMode::Raise;

    float m_radius = 5.0f;
    float m_strength = 2.0f;
    uint32 m_paintLayer = 0;

    bool m_hasHover = false;
    Vec3f m_hoverWorldPos;

    // for projection of the effective edit region.
    WeakHandle<TerrainWorldGridLayer> m_hoveredLayer;

    bool m_isStroking = false;
    bool m_strokeInvert = false;
    Vec2f m_strokeScreenPos;

    ///a held stroke re-picks only when the cursor moves. Picking every frame would walk the brush along the pick ray,
    ///because the surface it just raised comes up to meet the ray closer to the camera
    bool m_strokeNeedsPick = true;
    Vec3f m_strokeWorldPos;
    WeakHandle<TerrainWorldGridLayer> m_strokeLayer;

    ClockTimer m_strokeTimer;
};

} // namespace Hyperion
