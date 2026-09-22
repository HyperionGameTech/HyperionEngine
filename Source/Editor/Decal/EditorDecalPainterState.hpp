/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Transform.hpp>

#include <Core/Name/Name.hpp>

#include <Editor/EditorMemory.hpp>

#include <Scene/Decal/DecalTypes.hpp>

namespace Hyperion {

class EditorSubsystem;
class DebugDrawCommandList;
class Scene;
class Decal;
class DecalProxy;

HYP_CLASS(Serialize = false)
class EDITOR_API EditorDecalPainterState : public ObjectBase
{
    HYP_OBJECT_BODY(EditorDecalPainterState);

public:
    EditorDecalPainterState();
    ~EditorDecalPainterState() override;

    void Initialize(EditorSubsystem* subsystem);

    HYP_METHOD()
    bool IsEnabled() const;

    HYP_METHOD()
    void SetEnabled(bool enabled);

    HYP_METHOD()
    void Toggle();

    HYP_METHOD()
    bool CanEnterDecalTools() const;

    HYP_METHOD(Property = "ActiveDecal", Editor)
    const Handle<Decal>& GetActiveDecal() const;

    HYP_METHOD(Property = "ActiveDecal", Editor)
    void SetActiveDecal(const Handle<Decal>& decal);

    HYP_METHOD()
    void SetActiveDecalByName(Name assetName);

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

    void DebugDrawCursor(DebugDrawCommandList& debugDrawCommandList);

private:
    struct SurfaceHit
    {
        Handle<Scene> scene;
        Vec3f position;
        Vec3f normal;
    };

    bool TryGetSurfaceHit(const Vec2f& relativePos, SurfaceHit& outHit) const;
    Transform MakePlacementTransform(const SurfaceHit& hit) const;

    struct StrokeProxyEdit
    {
        Handle<DecalProxy> proxy;
        
        bool createdByStroke = false;

        Array<DecalInstance, EditorAllocator> added;
        Array<DecalInstance, EditorAllocator> removed;
    };

    bool AcceptStrokeScene(const Handle<Scene>& scene);
    StrokeProxyEdit& GetStrokeEdit(const Handle<DecalProxy>& proxy, bool createdByStroke = false);
    Handle<DecalProxy> FindOrCreateDecalProxy(const Handle<Scene>& scene);

    void StampAt(const SurfaceHit& hit);
    void EraseAt(const SurfaceHit& hit);

    EditorSubsystem* m_subsystem = nullptr;

    bool m_enabled = false;

    Handle<Decal> m_activeDecal;

    float m_scale = 1.0f;
    float m_rotationDegrees = 0.0f;
    bool m_randomRotation = true;
    float m_spacing = 0.5f;
    float m_eraseRadius = 1.0f;
    bool m_alignToSurface = true;

    bool m_hasHover = false;
    SurfaceHit m_hover;

    // rotation of the next stamp
    float m_nextRotationDegrees = 0.0f;

    bool m_isStroking = false;
    bool m_strokeErase = false;
    bool m_hasLastStamp = false;
    Vec3f m_lastStampPosition;

    WeakHandle<Scene> m_strokeScene;
    Array<StrokeProxyEdit> m_strokeEdits;
};

} // namespace Hyperion
