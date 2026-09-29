/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Transform.hpp>

#include <Core/Functional/Proc.hpp>

namespace Hyperion {

class EditorSubsystem;
class DebugDrawCommandList;
class Scene;
class RenderableAttributeSet;
struct RayHit;
struct Ray;

HYP_CLASS(Abstract)
class EDITOR_API EditorSurfacePainterState : public ObjectBase
{
    HYP_OBJECT_BODY(EditorSurfacePainterState);

public:
    EditorSurfacePainterState();
    ~EditorSurfacePainterState() override;

    void Initialize(EditorSubsystem* subsystem);

    HYP_METHOD()
    bool IsEnabled() const;

    HYP_METHOD()
    void SetEnabled(bool enabled);

    HYP_METHOD()
    void Toggle();

    HYP_METHOD()
    bool CanEnterTool() const;

    HYP_METHOD()
    float GetScale() const;

    HYP_METHOD()
    void SetScale(float scale);

    HYP_METHOD()
    float GetRotationDegrees() const;

    HYP_METHOD()
    void SetRotationDegrees(float rotationDegrees);

    HYP_METHOD()
    bool GetRandomRotation() const;

    HYP_METHOD()
    void SetRandomRotation(bool randomRotation);

    HYP_METHOD()
    float GetSpacing() const;

    HYP_METHOD()
    void SetSpacing(float spacing);

    HYP_METHOD()
    float GetEraseRadius() const;

    HYP_METHOD()
    void SetEraseRadius(float eraseRadius);

    HYP_METHOD()
    bool GetAlignToSurface() const;

    HYP_METHOD()
    void SetAlignToSurface(bool alignToSurface);

    void BeginStroke(const Vec2f& relativePos, bool erase);
    void UpdateStroke(const Vec2f& relativePos, bool erase);
    void EndStroke();

    HYP_FORCE_INLINE bool IsStroking() const
    {
        return m_isStroking;
    }

    void Update();

    void UpdateHover(const Vec2f& relativePos);

    virtual void DebugDrawCursor(DebugDrawCommandList& debugDrawCommandList);

protected:
    struct SurfaceHit
    {
        Handle<Scene> scene;
        Vec3f position;
        Vec3f normal;
    };

    static const RenderableAttributeSet& GetCursorDrawAttributes();

    /// Runs \p proc now when already on the sim thread, otherwise queues it there
    static void DispatchToSimThread(Proc<void()>&& proc);

    /// e.g. "decal painter", for log messages
    virtual const char* GetToolName() const = 0;

    /// e.g. "decal", for log messages
    virtual const char* GetAssetTypeName() const = 0;

    virtual bool HasActiveAsset() const = 0;

    /// Scale of a placed stamp; the placement transform carries it
    virtual Vec3f GetPlacementScale() const = 0;

    /// The box drawn under the cursor, as a transform of the unit (-1..1) cube
    virtual Transform GetCursorTransform(const SurfaceHit& hit) const;

    virtual bool ShouldIgnoreHit(const RayHit& hit) const
    {
        return false;
    }

    virtual Vec3f GetPlacementPosition(const SurfaceHit& hit) const
    {
        return hit.position;
    }

    /// Closest surface along \p ray in any scene of the world, skipping approximate and ignored hits
    bool RaycastSurface(const Ray& ray, SurfaceHit& outHit) const;

    virtual void StampAt(const SurfaceHit& hit) = 0;
    virtual void EraseAt(const SurfaceHit& hit) = 0;

    virtual void ResetStrokeEdits() = 0;

    /// Pushes one undo entry for everything the stroke changed in \p strokeScene
    virtual void CommitStrokeEdits(const WeakHandle<Scene>& strokeScene) = 0;

    Transform MakePlacementTransform(const SurfaceHit& hit) const;

    /// One undo entry per stroke keeps to a single scene
    bool AcceptStrokeScene(const Handle<Scene>& scene);

    /// Call after each successful stamp
    void OnStamped(const SurfaceHit& hit);

    EditorSubsystem* m_subsystem = nullptr;

    bool m_enabled = false;

    float m_scale = 1.0f;
    float m_rotationDegrees = 0.0f;
    bool m_randomRotation = true;
    float m_spacing = 0.5f;
    float m_eraseRadius = 1.0f;
    bool m_alignToSurface = true;

private:
    bool TryGetSurfaceHit(const Vec2f& relativePos, SurfaceHit& outHit) const;

    bool m_hasHover = false;
    SurfaceHit m_hover;

    // rotation of the next stamp
    float m_nextRotationDegrees = 0.0f;
    uint32 m_rotationSeed = 0x9E3779B9u;

    bool m_isStroking = false;
    bool m_strokeErase = false;
    bool m_hasLastStamp = false;
    Vec3f m_lastStampPosition;

    WeakHandle<Scene> m_strokeScene;
};

} // namespace Hyperion
