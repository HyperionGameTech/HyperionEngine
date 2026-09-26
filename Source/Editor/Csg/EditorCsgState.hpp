/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/String.hpp>
#include <Core/Containers/FixedArray.hpp>

#include <Core/Math/Transform.hpp>
#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

#include <Rendering/Util/MeshBoolean.hpp>

#include <Scripting/ScriptableDelegate.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Core/Utilities/Optional.hpp>

#include <Editor/EditorMemory.hpp>
#include <Editor/EditorAction.hpp>
#include <Editor/Gizmo/EditorGizmoBase.hpp>

namespace Hyperion {

class EditorSubsystem;
class EditorActionStack;
class DebugDrawCommandList;
class Node;
class Mesh;

struct Ray;

HYP_ENUM()
enum class CsgBrushShape : uint8
{
    Box = 0,
    Sphere,
    Cylinder
};

static constexpr uint32 NumCsgBrushShapes = 3;

HYP_ENUM()
enum class CsgOperation : uint8
{
    Union = 0,
    Subtract,
    Intersect
};

HYP_CLASS(Serialize = false)
class EDITOR_API EditorCsgState : public ObjectBase
{
    HYP_OBJECT_BODY(EditorCsgState);

public:
    EditorCsgState();
    ~EditorCsgState() override;

    void Initialize(EditorSubsystem* subsystem);

    HYP_METHOD()
    bool IsEnabled() const;

    HYP_METHOD()
    bool CanEnter() const;

    HYP_METHOD()
    void Enter();

    HYP_METHOD()
    void Exit(bool saveEdits);

    HYP_METHOD()
    Node* GetTargetNode() const;

    HYP_METHOD()
    bool HasBrush() const;

    HYP_METHOD()
    bool HasPendingEdits() const;

    HYP_METHOD()
    bool CanApply() const;

    HYP_METHOD()
    String GetStatusText() const;

    HYP_METHOD()
    CsgBrushShape GetBrushShape() const;

    HYP_METHOD()
    CsgOperation GetOperation() const;

    HYP_METHOD()
    void SetOperation(CsgOperation operation);

    HYP_METHOD()
    void AddBrush(CsgBrushShape shape);

    HYP_METHOD()
    void SelectBrushShape(CsgBrushShape shape);

    HYP_METHOD()
    bool IsPlacing() const;

    HYP_METHOD()
    CsgBrushShape GetPlacementShape() const;

    HYP_METHOD()
    void CancelPlacement();

    HYP_METHOD()
    bool IsBrushSelected() const;

    HYP_METHOD()
    void SelectBrush();

    HYP_METHOD()
    void ResetBrush();

    HYP_METHOD()
    CsgBrushShape GetSizingShape() const;

    HYP_METHOD()
    Vec3f GetSizingHalfExtents() const;

    HYP_METHOD()
    void SetSizingHalfExtents(Vec3f halfExtents);

    HYP_METHOD()
    bool IsKeepBrushAfterApply() const;

    HYP_METHOD()
    void SetKeepBrushAfterApply(bool keepBrushAfterApply);

    HYP_METHOD()
    void ApplyBrush();

    HYP_METHOD()
    void CancelBrush();

    bool IsSessionActive(uint32 sessionId) const;

    bool IsSessionNode(const Node* node) const;

    void OnFocusedNodeChanged(const Handle<Node>& focusedNode);

    EditorManipulationMode ResolveManipulationMode(EditorManipulationMode requestedMode);

    void UpdatePlacementHover(const Vec2f& relativePos);
    bool CommitPlacement();
    void ScalePlacement(float factor);

    bool TryPickBrush(const Ray& ray);

    void UpdateHandleHover(const Vec2f& relativePos);
    bool IsHandleHovered() const;
    bool BeginHandleDrag(const Vec2f& relativePos);
    void UpdateHandleDrag(const Vec2f& relativePos);
    void EndHandleDrag();
    bool IsHandleDragActive() const;

    bool BackOut();

    void Update();

    void DebugDraw(DebugDrawCommandList& debugDrawCommandList);

    HYP_FIELD()
    ScriptableDelegate<void> OnStateChanged;

private:
    Node* ResolveTarget() const;
    Handle<Mesh> GetTargetMesh() const;

    const Handle<Mesh>& GetBrushMesh(CsgBrushShape shape);

    void ValidateTarget();

    Vec3f GetBrushHalfExtent() const;
    Transform MakeDefaultBrushTransform(Node* target) const;

    void BeginPlacement(CsgBrushShape shape);
    void PlaceBrush(CsgBrushShape shape, const Transform& transform);

    void DrawBrush(DebugDrawCommandList& debugDrawCommandList, CsgBrushShape shape, const Transform& transform, float fillAlpha, float outlineAlpha) const;
    void DrawSizeHandles(DebugDrawCommandList& debugDrawCommandList) const;
    void DrawTargetHighlight(DebugDrawCommandList& debugDrawCommandList) const;

    bool CanUseSizeHandles() const;
    float GetHandleRadius(const Vec3f& worldPosition) const;
    int32 PickSizeHandle(const Vec2f& relativePos) const;
    void SetBrushTransformUndoable(const Transform& previousTransform, const Transform& newTransform, const char* actionText);

    void AttachBrush(const Handle<Node>& brushNode, CsgBrushShape shape);
    void DetachBrush(const Handle<Node>& brushNode);

    EditorActionStack* GetProjectActionStack() const;

    void PushBrushAction(const Handle<EditorActionBase>& action);
    void PushApplyAction(const Handle<EditorActionBase>& action);
    bool IsSessionAction(const EditorActionBase* action) const;
    void RemoveSessionActions(bool includeApplyActions);

    void FinalizeMesh();

    EditorSubsystem* m_subsystem = nullptr;

    bool m_enabled = false;

    WeakHandle<Node> m_targetNode;
    Handle<Mesh> m_baselineMesh;

    uint32 m_sessionId = 0;

    Array<Handle<EditorActionBase>, EditorAllocator> m_brushActions;
    Array<Handle<EditorActionBase>, EditorAllocator> m_applyActions;

    DelegateHandler m_actionAddedHandler;

    Handle<Node> m_brushNode;
    CsgBrushShape m_brushShape = CsgBrushShape::Box;
    CsgOperation m_operation = CsgOperation::Subtract;
    bool m_keepBrushAfterApply = false;

    Optional<Vec3f> m_lastBrushHalfExtent;

    FixedArray<Handle<Mesh>, NumCsgBrushShapes> m_brushMeshes;

    WeakHandle<Mesh> m_validatedMesh;
    MeshBooleanError m_targetError = MeshBooleanError::None;
    MeshBooleanError m_lastApplyError = MeshBooleanError::None;

    EditorManipulationMode m_manipulationModeBeforeCsg = EditorManipulationMode::Translate;
    EditorManipulationMode m_brushManipulationMode = EditorManipulationMode::Translate;

    struct PlacementState
    {
        bool active = false;
        CsgBrushShape shape = CsgBrushShape::Box;
        Vec3f halfExtent = Vec3f(0.5f);

        bool hasHover = false;
        Vec3f hoverPosition;
        Vec3f hoverNormal = Vec3f::UnitY();
    } m_placement;

    struct HandleDragState
    {
        bool active = false;
        int32 handleIndex = -1;

        Transform startTransform;
        Vec3f worldAxis;
        Vec3f planePoint;
        Vec3f planeNormal;
        float startProjection = 0.0f;
    } m_handleDrag;

    int32 m_hoveredHandle = -1;
};

} // namespace Hyperion
