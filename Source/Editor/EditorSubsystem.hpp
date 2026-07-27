/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Editor/UI/EditorUI.hpp>

#include <Editor/EditorActionStack.hpp>
#include <Editor/EditorTask.hpp>
#include <Editor/EditorMemory.hpp>

#include <Scene/Subsystem.hpp>

#include <Core/Math/BoundingBox.hpp>

#include <Core/Functional/Delegate.hpp>
#include <Core/Containers/Set.hpp>

namespace Hyperion {

class Game;
class World;
class Scene;
class Camera;
class Entity;
class Mesh;
class Material;
class Texture;
class EnvProbe;
class InputManager;
class UIStage;
class UIObject;
class UIListView;
class UIGrid;
class FontAtlas;
class EditorDelegates;
class EditorSubsystem;
class EditorProject;
class EditorCommandBase;
class ApplicationWindow;
struct MouseEvent;
struct KeyboardEvent;
struct MeshComponent;
class View;
class EditorViewport;
class LightmapVolume;
class VolumeBase;
class AppContextBase;
struct Ray;

HYP_ENUM()
enum class EditorManipulationMode : uint8
{
    None = 0,

    Translate,
    Rotate,
    Scale,
    ReshapeVolume
};

HYP_ENUM()
enum class MeshEditFaceMode : uint8
{
    Triangle = 0,
    Quad
};

struct MeshEditFaceSelection
{
    WeakHandle<Node> node;
    Array<uint32, EditorAllocator> vertexIndices;
    uint8 lodIndex = 0;

    HYP_FORCE_INLINE bool operator==(const MeshEditFaceSelection& other) const
    {
        return node == other.node
            && vertexIndices.Size() == other.vertexIndices.Size()
            && Memory::Compare(vertexIndices.Data(), other.vertexIndices.Data(), sizeof(uint32) * vertexIndices.Size()) == 0
            && lodIndex == other.lodIndex;
    }

    HYP_FORCE_INLINE bool operator!=(const MeshEditFaceSelection& other) const
    {
        return !(*this == other);
    }
};

/*! \brief A widget that can manipulate the selected object. (e.g translate, rotate, scale) */
HYP_CLASS(Abstract)
class EDITOR_API EditorGizmoBase : public ObjectBase
{
    HYP_OBJECT_BODY(EditorGizmoBase);

public:
    static Pool* GetAllocator() { return g_editorPool; }

    EditorGizmoBase();
    virtual ~EditorGizmoBase();

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Node>& GetNode() const
    {
        return m_node;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsDragging() const
    {
        return m_isDragging;
    }

    HYP_FORCE_INLINE void SetCurrentProject(const WeakHandle<EditorProject>& project)
    {
        m_currentProject = project;
    }

    HYP_FORCE_INLINE void SetEditorSubsystem(EditorSubsystem* editorSubsystem)
    {
        m_editorSubsystem = editorSubsystem;
    }

    void Shutdown();

    HYP_METHOD()
    virtual EditorManipulationMode GetManipulationMode() const = 0;

    HYP_METHOD()
    virtual int GetPriority() const
    {
        return -1;
    }

    HYP_METHOD()
    virtual String GetMenuText() const = 0;

    virtual void SetFocusedNode(const Handle<Node>& focusedNode);

    virtual void OnDragStart(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node, const Vec3f& hitpoint);
    virtual void OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent);

    virtual bool OnMouseHover(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
    {
        return false;
    }

    virtual bool OnMouseLeave(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
    {
        return false;
    }

    virtual bool OnMouseMove(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node)
    {
        return false;
    }

    virtual bool OnKeyPress(const Handle<Camera>& camera, const KeyboardEvent& keyboardEvent, const Handle<Node>& node)
    {
        return false;
    }

protected:
    virtual void Init() override;

    virtual Handle<Node> Load_Internal() const = 0;

    Handle<EditorProject> GetCurrentProject() const;

    HYP_FORCE_INLINE EditorSubsystem* GetEditorSubsystem() const
    {
        return m_editorSubsystem;
    }

    WeakHandle<Node> m_focusedNode;
    Handle<Node> m_node;
    struct InputMouseLockScope* m_mouseLockScope;

private:
    EditorSubsystem* m_editorSubsystem;
    WeakHandle<EditorProject> m_currentProject;

    bool m_isDragging;
};

HYP_CLASS()
class NullEditorGizmo : public EditorGizmoBase
{
    HYP_OBJECT_BODY(NullEditorGizmo);

public:
    virtual ~NullEditorGizmo() override = default;

    virtual String GetMenuText() const override
    {
        return "<null>";
    }

    virtual EditorManipulationMode GetManipulationMode() const override
    {
        return EditorManipulationMode::None;
    }

protected:
    virtual Handle<Node> Load_Internal() const override
    {
        return Handle<Node>::empty;
    }
};

HYP_CLASS()
class TranslateEditorGizmo : public EditorGizmoBase
{
    HYP_OBJECT_BODY(TranslateEditorGizmo);

public:
    virtual ~TranslateEditorGizmo() override = default;

    virtual EditorManipulationMode GetManipulationMode() const override
    {
        return EditorManipulationMode::Translate;
    }

    virtual String GetMenuText() const override
    {
        return "Translate";
    }

    virtual int GetPriority() const override
    {
        return 0;
    }

    virtual void OnDragStart(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node, const Vec3f& hitpoint) override;
    virtual void OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent) override;

    virtual bool OnMouseHover(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;
    virtual bool OnMouseLeave(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;
    virtual bool OnMouseMove(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;

    virtual bool OnKeyPress(const Handle<Camera>& camera, const KeyboardEvent& keyboardEvent, const Handle<Node>& node) override;

protected:
    struct DragData
    {
        Vec3f axisDirection;
        Vec3f planeNormal;
        Vec3f planePoint;
        Vec3f hitpointOrigin;
        Vec3f nodeOrigin;
    };

    virtual Handle<Node> Load_Internal() const override;

    Optional<DragData> m_dragData;
    Array<Pair<Handle<Node>, Vec3f>> m_selectedNodes;
};

HYP_CLASS()
class RotateEditorGizmo : public EditorGizmoBase
{
    HYP_OBJECT_BODY(RotateEditorGizmo);

public:
    virtual ~RotateEditorGizmo() override = default;

    virtual EditorManipulationMode GetManipulationMode() const override
    {
        return EditorManipulationMode::Rotate;
    }

    virtual String GetMenuText() const override
    {
        return "Rotate";
    }

    virtual int GetPriority() const override
    {
        return 0;
    }

    virtual void OnDragStart(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node, const Vec3f& hitpoint) override;
    virtual void OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent) override;

    virtual bool OnMouseHover(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;
    virtual bool OnMouseLeave(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;
    virtual bool OnMouseMove(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;

    virtual bool OnKeyPress(const Handle<Camera>& camera, const KeyboardEvent& keyboardEvent, const Handle<Node>& node) override;

protected:
    struct DragData
    {
        Vec3f axis;
        Vec3f planePoint;
        Vec3f startVector;
        Quat4f startRotation;
        Quat4f currentRotation;
    };

    virtual Handle<Node> Load_Internal() const override;

    Optional<DragData> m_dragData;
    Array<Pair<Handle<Node>, Quat4f>> m_selectedNodes;
};

HYP_CLASS()
class ScaleEditorGizmo : public EditorGizmoBase
{
    HYP_OBJECT_BODY(ScaleEditorGizmo);

public:
    virtual ~ScaleEditorGizmo() override = default;

    virtual EditorManipulationMode GetManipulationMode() const override
    {
        return EditorManipulationMode::Scale;
    }

    virtual String GetMenuText() const override
    {
        return "Scale";
    }

    virtual int GetPriority() const override
    {
        return 0;
    }

    virtual void OnDragStart(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node, const Vec3f& hitpoint) override;
    virtual void OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent) override;

    virtual bool OnMouseHover(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;
    virtual bool OnMouseLeave(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;
    virtual bool OnMouseMove(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;

protected:
    struct DragData
    {
        Vec3f axisDirection;
        Vec3f planeNormal;
        Vec3f planePoint;
        Vec3f hitpointOrigin;
        Vec3f nodeOrigin;
        Vec3f initialScale;
        int axis = -1;
    };

    virtual Handle<Node> Load_Internal() const override;

    Optional<DragData> m_dragData;
    Array<Pair<Handle<Node>, Pair<Vec3f, Vec3f>>> m_selectedNodes; // node + (origin scale, origin translation)
};

/*! \brief A gizmo for editing axis-aligned bounding boxes by dragging individual faces.
 *  Used for resizing volumes such as LightmapVolume, FogVolume, etc.
 *  Each face of the AABB is represented as a draggable quad handle.
 */
HYP_CLASS()
class VolumeEditorGizmo : public EditorGizmoBase
{
    HYP_OBJECT_BODY(VolumeEditorGizmo);

public:
    VolumeEditorGizmo();
    virtual ~VolumeEditorGizmo() override = default;

    virtual EditorManipulationMode GetManipulationMode() const override
    {
        return EditorManipulationMode::ReshapeVolume;
    }

    virtual String GetMenuText() const override
    {
        return "Volume Edit";
    }

    virtual int GetPriority() const override
    {
        return 0;
    }

    virtual void SetFocusedNode(const Handle<Node>& focusedNode) override;

    virtual void OnDragStart(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node, const Vec3f& hitpoint) override;
    virtual void OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent) override;

    virtual bool OnMouseHover(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;
    virtual bool OnMouseLeave(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;
    virtual bool OnMouseMove(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;

    virtual bool OnKeyPress(const Handle<Camera>& camera, const KeyboardEvent& keyboardEvent, const Handle<Node>& node) override;

protected:
    struct DragData
    {
        int faceIndex;
        Vec3f faceNormal;
        Vec3f planePoint;
        Vec3f planeNormal;
        float hitOffset;
        BoundingBox originalBounds;
    };

    virtual Handle<Node> Load_Internal() const override;

private:
    void UpdateFaceGeometry(const BoundingBox& localBounds, const Vec3f& worldTranslation);

    Optional<DragData> m_dragData;
    BoundingBox m_currentBounds;
};

HYP_CLASS()
class EDITOR_API EditorSubsystem : public Subsystem
{
    HYP_OBJECT_BODY(EditorSubsystem);

public:
    using EditorGizmoSet = HashTable<Handle<EditorGizmoBase>, &EditorGizmoBase::GetManipulationMode, EditorAllocator>;
    
    static Pool* GetAllocator() { return g_editorPool; }

    EditorSubsystem();
    virtual ~EditorSubsystem() override;

    void OnAddedToWorld() override;
    void OnRemovedFromWorld() override;
    void Update(float delta) override;

    void OnSceneAttached(const Handle<Scene>& scene) override;
    void OnSceneDetached(Scene* scene) override;

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<EditorProject>& GetCurrentProject() const
    {
        return m_currentProject;
    }

    HYP_METHOD()
    EditorViewport* GetActiveViewport() const;

    HYP_METHOD()
    void SetActiveViewport(EditorViewport* viewport);

    HYP_METHOD()
    void AddViewport(const Handle<EditorViewport>& viewport);

    HYP_METHOD()
    void RemoveViewport(EditorViewport* viewport);

    /*! \brief Get the main editor scene used for editor-specific objects (e.g cameras, gizmos, etc). */
    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Scene>& GetEditorScene() const
    {
        return m_editorScene;
    }

    HYP_METHOD()
    bool ExecuteCommand(const Handle<EditorCommandBase>& command);

    HYP_METHOD()
    bool ExecuteCommandByName(Name name, const String& arguments);

    HYP_METHOD()
    void NewProject();

    HYP_METHOD()
    void OpenProject(const Handle<EditorProject>& project);

    HYP_METHOD()
    void CloseProject(bool shutdownWorld = true);

    HYP_METHOD()
    void ShowImportContentDialog();

    HYP_METHOD()
    void SetFocusedNode(const Handle<Node>& focusedNode, bool shouldSelectInOutline = true);

    HYP_METHOD()
    Handle<Node> GetFocusedNode() const;

    HYP_METHOD()
    void AddToSelection(const Handle<Node>& node);

    HYP_METHOD()
    void RemoveFromSelection(const Handle<Node>& node);

    HYP_METHOD()
    void ClearSelection();

    HYP_METHOD()
    void SetSelectedNodes(const Array<Handle<Node>>& nodes);

    HYP_METHOD()
    bool IsNodeSelected(const Handle<Node>& node) const;

    HYP_METHOD()
    Array<Handle<Node>> GetSelectedNodes() const;

    HYP_METHOD()
    Handle<Scene> GetActiveScene() const;

    HYP_METHOD()
    void SetActiveScene(const Handle<Scene>& scene);

    HYP_METHOD()
    EditorManipulationMode GetSelectedManipulationMode() const;

    HYP_METHOD()
    void SetSelectedManipulationMode(EditorManipulationMode mode);

    HYP_METHOD()
    EditorGizmoBase* GetSelectedGizmo() const;

    HYP_METHOD()
    EditorGizmoBase* GetGizmo(EditorManipulationMode mode) const;

    const EditorGizmoSet& GetGizmos() const;

    HYP_METHOD()
    bool IsMeshEditModeEnabled() const;

    HYP_METHOD()
    void EnterMeshEditMode();

    HYP_METHOD()
    void ExitMeshEditMode(bool saveEdits);

    HYP_METHOD()
    bool IsSimulating() const;

    HYP_METHOD()
    bool CanEnableMeshEditMode() const;

    HYP_METHOD()
    Node* GetMeshEditTargetNode() const;

    HYP_METHOD()
    bool HasMeshEditFaceSelected() const;

    HYP_METHOD()
    bool IsMeshEditDragActive() const;

    HYP_METHOD()
    bool HasPendingMeshEdits() const;

    HYP_METHOD()
    int GetMeshEditLockedAxis() const;

    EditorActionStack* GetActiveActionStack() const;

    HYP_METHOD()
    MeshEditFaceMode GetMeshEditFaceMode() const;

    HYP_METHOD()
    void SetMeshEditFaceMode(MeshEditFaceMode faceMode);

    HYP_METHOD()
    bool IsMeshEditAlignToNormal() const;

    HYP_METHOD()
    void SetMeshEditAlignToNormal(bool alignToNormal);

    HYP_METHOD()
    bool IsSnapToGridEnabled() const;

    HYP_METHOD()
    void SetSnapToGridEnabled(bool snapToGrid);

    HYP_METHOD()
    void SetSelectedBucket(uint32 bucketIndex);

    /*! \brief Calculate an appropriate position for inserting a new object into the scene.
     *  Uses raycasting from the camera to find a suitable location that doesn't intersect with existing geometry.
     *
     *  \param desiredDistance The preferred distance from the camera. If no geometry is hit within this range,
     *                         the position will be placed at this distance. Default is 5.0 units.
     *  \param offsetFromSurface If geometry is hit, the object will be placed this distance in front of the surface
     *                           to prevent clipping through. Default is 0.5 units.
     *  \return The calculated world position for object insertion.
     */
    HYP_METHOD()
    Vec3f CalculateSceneInsertionPoint(float desiredDistance = 5.0f, float offsetFromSurface = 0.5f) const;

    /*! \brief Create or update an in-progress, non-undoable preview entity showing a normalized cube sphere
     *  with the given number of subdivisions. Used to live-preview a shape while a creation dialog is open.
     *  Call \ref{CommitMeshPreview} to turn the preview into a permanent, undoable scene entity, or
     *  \ref{CancelMeshPreview} to discard it. */
    HYP_METHOD()
    void UpdateNormalizedCubeSpherePreview(uint32 numDivisions);

    /*! \brief Commit the current mesh preview entity (if any) as a permanent scene entity, pushing an
     *  undoable "Add" action onto the current project's action stack. No-op if there is no active preview. */
    HYP_METHOD()
    void CommitMeshPreview();

    /*! \brief Discard the current mesh preview entity (if any), removing it from the scene. */
    HYP_METHOD()
    void CancelMeshPreview();

    HYP_FORCE_INLINE EditorDelegates* GetEditorDelegates()
    {
        return m_editorDelegates;
    }

    HYP_FIELD()
    ScriptableDelegate<void, Handle<Node>, Handle<Node>, bool> OnFocusedNodeChanged;

    HYP_FIELD()
    ScriptableDelegate<void, Handle<EditorProject>> OnProjectClosing;

    HYP_FIELD()
    ScriptableDelegate<void, Handle<EditorProject>> OnProjectOpened;

    HYP_FIELD()
    ScriptableDelegate<void, Handle<Scene>> OnActiveSceneChanged;

    HYP_FIELD()
    ScriptableDelegate<void, EditorGizmoBase*, EditorGizmoBase*> OnSelectedGizmoChanged;

    HYP_FIELD()
    ScriptableDelegate<void, uint32> OnSelectedBucketChanged;

    HYP_FIELD()
    ScriptableDelegate<void, Handle<EditorViewport>> OnActiveViewportChanged;

    HYP_FIELD()
    ScriptableDelegate<void> OnSelectionChanged;

    HYP_FIELD()
    ScriptableDelegate<void> OnMeshEditSelectionChanged;

    HYP_FIELD()
    ScriptableDelegate<void> OnMeshEditStateChanged;

private:
    void InitViewport();

    void InitializeGizmos();
    void ShutdownGizmos();

    HYP_METHOD()
    bool StartSimulation();

    HYP_METHOD()
    bool StopSimulation();

    HYP_METHOD()
    bool PauseSimulation();

    void SetHoveredGizmo(
        const MouseEvent& event,
        EditorGizmoBase* gizmo,
        const Handle<Node>& gizmoNode);

    HYP_FORCE_INLINE bool IsHoveringGizmo() const
    {
        return m_hoveredGizmo.IsValid() && m_hoveredGizmoNode.IsValid();
    }

    struct MeshEditDragData
    {
        Array<uint32, EditorAllocator> affectedVertexIndices;
        Array<Vec3f, EditorAllocator> vertexOriginalPositions;

        Vec3f faceCentroidWorldOrigin;
        Vec3f planeNormal;
        Vec3f hitpointOrigin;
        Vec3f currentLocalDelta;
        Vec3f axisDirection;
        Vec3f defaultAxisDirection;

        // 0/1/2 when constrained to a world X/Y/Z axis via the keyboard, -1 when following defaultAxisDirection
        int lockedAxis = -1;
    };

    Node* ResolveMeshEditTarget(MeshComponent** outMeshComponent = nullptr) const;

    /*! \brief Record the target mesh's LOD 0 vertex positions as they were before any edit, if not
     *  already recorded. This is the "before" half of the single action pushed on save, and what
     *  Discard restores. Called lazily at the first edit so entering and leaving the mode without
     *  touching anything costs nothing. */
    void CaptureMeshEditBaseline();

    /*! \brief Collapse every edit made this session into one action on the project's action stack,
     *  then reset the per-session stack. No-op when nothing has been edited. */
    void CommitMeshEdits();

    /*! \brief Roll the target mesh back to the captured baseline and drop the per-session stack. */
    void DiscardMeshEdits();

    bool TryPickMeshEditFace(const Ray& ray, MeshEditFaceSelection& outSelection, bool ensureUniqueMesh);
    void SetSelectedMeshEditFace(Optional<MeshEditFaceSelection> selection);
    void UpdateHoveredMeshEditFace(const Ray& ray);
    void DebugDrawMeshEditSelection(class DebugDrawCommandList& debugDrawCommandList);

    void StartMeshEditDrag(const Handle<Camera>& camera, const MouseEvent& mouseEvent);
    void UpdateMeshEditDrag(const Handle<Camera>& camera, const MouseEvent& mouseEvent);
    void EndMeshEditDrag(bool saveEdits);
    void SetMeshEditDragLockedAxis(const Handle<Camera>& camera, const KeyboardEvent& keyboardEvent, int axis);

    bool BackOutOfMeshEditState();

    SubsystemUpdatePhase GetUpdatePhase_Internal() const override
    {
        return SubsystemUpdatePhase::AfterVis;
    }

    Handle<Scene> m_editorScene;

    // The project.
    Handle<EditorProject> m_currentProject;
    // The project, but only used when we start simulation and need to restore the pre-simulation state after we end simulation.
    Handle<EditorProject> m_preSimulationProject;

    WeakHandle<Scene> m_activeScene;

    EditorManipulationMode m_selectedManipulationMode;
    EditorGizmoSet m_gizmos;

    struct MeshEditState
    {
        bool enabled = false;
        MeshEditFaceMode faceMode = MeshEditFaceMode::Quad;
        bool alignToNormal = true;

        WeakHandle<Node> targetNode;

        // Hacky gross gross
        EditorManipulationMode manipulationModeBeforeMeshEdit = EditorManipulationMode::Translate;
        bool isChanging = false;

        Handle<EditorActionStack> actionStack;

        Array<Vec3f, EditorAllocator> baselinePositions;
        WeakHandle<Mesh> baselineMesh;

        Optional<MeshEditFaceSelection> selectedFace;
        Optional<MeshEditFaceSelection> hoveredFace;
        Optional<MeshEditDragData> dragData;
    } m_meshEditState;

    bool m_snapToGridEnabled;

    WeakHandle<EditorGizmoBase> m_hoveredGizmo;
    WeakHandle<Node> m_hoveredGizmoNode;

    WeakHandle<Node> m_focusedNode;
    // the actual node that displays the highlight for the focused item
    Handle<Node> m_highlightNode;

    Set<Handle<Node>, EditorAllocator> m_selectedNodes;

    bool m_editorCameraEnabled;
    bool m_shouldCancelNextClick;

    EditorDelegates* m_editorDelegates;

    uint32 m_selectedBucketIndex;

    Array<Handle<EditorViewport>, EditorAllocator> m_editorViewports;

    Handle<View> m_simulationView;
    FilePath m_simulationSnapshotPath;

    Handle<Entity> m_meshPreviewEntity;
    Handle<Material> m_meshPreviewMaterial;

    DelegateHandlerSet m_delegateHandlers;
};

} // namespace Hyperion
