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

#include <Csg/MeshBoolean.hpp>

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
class Material;

struct Ray;

HYP_ENUM()
enum class CsgBrushShape : uint8
{
    Box = 0,
    Sphere,
    Cylinder,

    Max
};

static constexpr uint32 NumCsgBrushShapes = uint32(CsgBrushShape::Max);

static constexpr const char* CsgBrushShapeNames[NumCsgBrushShapes] = {
    "Box",
    "Sphere",
    "Cylinder"
};

HYP_ENUM()
enum class CsgOperation : uint8
{
    Union = 0,
    Subtract,
    Intersect,

    Max
};

static constexpr uint32 NumCsgOperations = uint32(CsgOperation::Max);

enum class CsgSessionId : uint32;

static constexpr const char* CsgOperationNames[NumCsgOperations] = {
    "Union",
    "Subtract",
    "Intersect"
};

HYP_CLASS(Serialize = false)
class CSGEDITOR_API EditorCsgState : public ObjectBase
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
    uint32 GetTargetCount() const;

    HYP_METHOD()
    Node* GetTargetNodeAt(uint32 index) const;

    HYP_METHOD()
    String GetTargetName(uint32 index) const;

    HYP_METHOD()
    String GetTargetIssue(uint32 index) const;

    HYP_METHOD()
    bool IsTargetIncluded(uint32 index) const;

    HYP_METHOD()
    void SetTargetIncluded(uint32 index, bool included);

    HYP_METHOD()
    bool AreTargetsPinned() const;

    HYP_METHOD()
    void SetTargetsPinned(bool pinned);

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
    bool IsAlignToSurface() const;

    HYP_METHOD()
    void SetAlignToSurface(bool alignToSurface);

    HYP_METHOD()
    bool AreOutlinesShown() const;

    HYP_METHOD()
    void SetOutlinesShown(bool outlinesShown);

    HYP_METHOD()
    void ApplyBrush();

    HYP_METHOD()
    void CancelBrush();

    bool IsSessionActive(CsgSessionId sessionId) const;

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
    struct CsgTarget
    {
        WeakHandle<Node> node;
        Handle<Mesh> baselineMesh;
        Handle<Material> createdMaterial;
        Handle<Mesh> validatedMesh;
        MeshBooleanError error = MeshBooleanError::None;
        float overlap = 0.0f;
        bool overlapping = false;
        bool pinned = false;
        bool included = false;
        bool includedByUser = false;
        bool created = false;
    };

    struct ValidatedMesh
    {
        Handle<Mesh> mesh;
        MeshBooleanError error = MeshBooleanError::None;
    };

    const Handle<Mesh>& GetBrushMesh(CsgBrushShape shape);

    CsgTarget* FindTarget(const Node* node);
    CsgTarget& AddTarget(const Handle<Node>& node);
    const CsgTarget* GetListedTarget(uint32 index) const;
    bool IsTargetListed(const CsgTarget& target) const;

    MeshBooleanError ValidateMesh(const Handle<Mesh>& mesh);
    void ValidateTarget(CsgTarget& target);

    Transform GetPlacementTransform() const;
    bool GetActiveBrushTransform(CsgBrushShape& outShape, Transform& outTransform) const;

    void SaveSettings() const;
    void RefreshTargets();
    void UpdateDefaultInclusion();

    static void RefreshBrushAfterApply(EditorSubsystem* editorSubsystem, const Handle<Node>& brushNode, bool keepBrush, CsgSessionId sessionId);
    static void RestoreBrushAfterUndo(EditorSubsystem* editorSubsystem, const Handle<Node>& brushNode, CsgBrushShape shape, CsgSessionId sessionId);

    void ApplyToTargets(const Handle<Node>& brushNode, CsgBrushShape shape, CsgOperation operation, bool keepBrush);
    void CreateUnionMesh(const Handle<Node>& brushNode, CsgBrushShape shape, bool keepBrush);

    Vec3f GetBrushHalfExtent() const;
    Transform MakeDefaultBrushTransform() const;

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

    void FinalizeMeshes();

    EditorSubsystem* m_subsystem = nullptr;

    bool m_enabled = false;

    Array<CsgTarget, EditorAllocator> m_targets;
    Array<uint32, EditorAllocator> m_listedTargets;
    Array<ValidatedMesh, EditorAllocator> m_validatedMeshes;

    WeakHandle<Node> m_placedOnNode;
    WeakHandle<Node> m_primaryTarget;
    bool m_targetsPinned = false;
    HashCode m_targetsHash;

    bool m_hasGathered = false;
    CsgBrushShape m_gatheredShape = CsgBrushShape::Box;
    Mat4f m_gatheredBrushMatrix;
    uint32 m_framesSinceGather = 0;

    CsgSessionId m_sessionId = CsgSessionId(0);

    Array<Handle<EditorActionBase>, EditorAllocator> m_brushActions;
    Array<Handle<EditorActionBase>, EditorAllocator> m_applyActions;

    DelegateHandler m_actionAddedHandler;

    Handle<Node> m_brushNode;
    CsgBrushShape m_brushShape = CsgBrushShape::Box;
    CsgOperation m_operation = CsgOperation::Subtract;
    bool m_keepBrushAfterApply = false;
    bool m_alignToSurface = false;
    bool m_outlinesShown = true;

    Optional<Vec3f> m_lastBrushHalfExtent;

    FixedArray<Handle<Mesh>, NumCsgBrushShapes> m_brushMeshes;

    String m_statusText;

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
        WeakHandle<Node> hoverNode;
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
