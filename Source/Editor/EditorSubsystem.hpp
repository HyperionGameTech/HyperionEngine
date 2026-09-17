/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Editor/EditorActionStack.hpp>
#include <Editor/EditorTask.hpp>
#include <Editor/EditorMemory.hpp>

#include <Editor/Preview/AssetThumbnailService.hpp>
#include <Editor/Preview/MaterialPreviewRenderer.hpp>

#include <Scene/Subsystem.hpp>

#include <Core/Math/BoundingBox.hpp>

#include <Core/Functional/Delegate.hpp>
#include <Core/Containers/Set.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Utilities/ClockTimer.hpp>

namespace Hyperion {

class Game;
class World;
class Scene;
class Camera;
class Node;
class Entity;
class Mesh;
class Material;
class Texture;
class EnvProbe;
class PhysicsShape;
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
class MessagesOverlay;
class View;
class EditorViewport;
class LightmapVolume;
class VolumeBase;
class TerrainWorldGridLayer;
class WorldGridLayer;
class DynamicSkySystem;
class EditorTerrainState;
class AppContextBase;
class BVHNode;

struct Ray;
struct MouseEvent;
struct KeyboardEvent;
struct MeshComponent;

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

HYP_ENUM()
enum class EditorPlayNetMode : uint8
{
    Standalone = 0,
    Client,
    DedicatedServer
};

HYP_ENUM()
enum class EditorPlayNetState : uint8
{
    None = 0,
    Connecting,
    Connected,
    Failed,
    Disconnected,
    Hosting,
    StartingServer
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

    // Keeps the gizmo in sync when the focused node's transform changes externally
    // (e.g. swatch overrides applied on active-swatch switch)
    DelegateHandler m_focusedNodeTransformHandler;

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

    HYP_FORCE_INLINE void SetMessagesOverlay(const Handle<MessagesOverlay>& messagesOverlay)
    {
        m_messagesOverlay = messagesOverlay;
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

    /// Use GetProjectWorld() instead if you need the project's world
    /// Otherwise, you can cast to Subsystem and call GetWorld on that.
    World* GetWorld() const = delete;

    HYP_METHOD()
    const Handle<World>& GetProjectWorld() const;

    HYP_METHOD()
    bool StartSimulation();

    HYP_METHOD()
    bool StopSimulation();

    HYP_METHOD()
    bool PauseSimulation();

    ///Play net mode

    HYP_METHOD()
    EditorPlayNetMode GetPlayNetMode() const
    {
        return m_playNetMode;
    }

    HYP_METHOD()
    void SetPlayNetMode(EditorPlayNetMode mode);

    HYP_METHOD()
    String GetPlayNetHost() const
    {
        return m_playNetHost;
    }

    HYP_METHOD()
    void SetPlayNetHost(const String& host);

    HYP_METHOD()
    uint32 GetPlayNetPort() const
    {
        return m_playNetPort;
    }

    HYP_METHOD()
    void SetPlayNetPort(uint32 port);

    HYP_METHOD()
    bool GetPlayNetAutoLaunchServer() const
    {
        return m_playNetAutoLaunchServer;
    }

    HYP_METHOD()
    void SetPlayNetAutoLaunchServer(bool autoLaunchServer);

    HYP_METHOD()
    uint32 GetPlayNetCachePort() const
    {
        return m_playNetCachePort;
    }

    HYP_METHOD()
    void SetPlayNetCachePort(uint32 port);

    HYP_METHOD()
    EditorPlayNetState GetPlayNetState() const
    {
        return m_playNetState;
    }

    HYP_METHOD()
    bool IsPlayNetServerAutoLaunched() const
    {
        return m_activeAutoLaunchServer;
    }

    HYP_METHOD()
    String GetPlayNetProjectDirectory() const;

    HYP_METHOD()
    void OnPlayNetServerReady();

    HYP_METHOD()
    void OnPlayNetServerFailed();

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
    String GetCodeEditor() const;

    /*! \brief Names of all concrete WorldGridLayer-derived classes registered with the engine. */
    HYP_METHOD()
    Array<Name> GetAvailableWorldGridLayerClassNames() const;

    /*! \brief The current project world's DynamicSkySystem, or an empty handle if it has none. */
    HYP_METHOD()
    Handle<DynamicSkySystem> GetDynamicSkySystem() const;

    HYP_METHOD()
    EditorManipulationMode GetSelectedManipulationMode() const;

    HYP_METHOD()
    void SetSelectedManipulationMode(EditorManipulationMode mode);

    HYP_METHOD()
    EditorGizmoBase* GetSelectedGizmo() const;

    HYP_METHOD()
    EditorGizmoBase* GetGizmo(EditorManipulationMode mode) const;

    const EditorGizmoSet& GetGizmos() const;

    ///Terrain

    HYP_METHOD()
    Handle<EditorTerrainState> GetTerrainState();

    ///Mesh edits

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

    HYP_METHOD()
    void SetMeshEditFaceMode(MeshEditFaceMode faceMode);

    HYP_METHOD()
    bool IsMeshEditAlignToNormal() const;

    HYP_METHOD()
    void SetMeshEditAlignToNormal(bool alignToNormal);

    HYP_METHOD()
    MeshEditFaceMode GetMeshEditFaceMode() const;

    /*! \brief LOD currently being edited. Edits only affect this LOD. */
    HYP_METHOD()
    uint8 GetMeshEditLod() const;

    HYP_METHOD()
    void SetMeshEditLod(uint8 lodIndex);

    /*! \brief Number of LODs on the mesh being edited, or 0 when there is no target. */
    HYP_METHOD()
    uint8 GetMeshEditNumLods() const;

    /*! \brief True when LOD 0 was edited after the mesh's LODs were generated from it. */
    HYP_METHOD()
    bool AreMeshEditLodsOutOfDate() const;

    HYP_METHOD()
    void RegenerateMeshEditLods();

    ///action stack

    EditorActionStack* GetActiveActionStack() const;

    //- Snappy

    HYP_METHOD()
    bool IsSnapToGridEnabled() const;

    HYP_METHOD()
    void SetSnapToGridEnabled(bool snapToGrid);

    ///Swatch overrides


    HYP_METHOD()
    bool IsSwatchOverrideModeEnabled() const
    {
        return m_swatchOverrideMode;
    }

    HYP_METHOD()
    void SetSwatchOverrideMode(bool enabled)
    {
        m_swatchOverrideMode = enabled;
    }

    HYP_METHOD()
    Array<Name> GetEntitySwatchOverrideSets(Entity* entity) const;

    HYP_METHOD()
    bool EntityHasSwatchOverrideSet(Entity* entity, Name swatchName) const;

    HYP_METHOD()
    bool EntityHasSwatchOverrideValues(Entity* entity, Name swatchName) const;

    HYP_METHOD()
    void EntityAddSwatchOverrideSet(Entity* entity, Name swatchName) const;

    HYP_METHOD()
    bool EntityRemoveSwatchOverrideSet(Entity* entity, Name swatchName) const;

    HYP_METHOD()
    bool IsEntityPropertyOverridden(Entity* entity, Name swatchName, Name propertyName) const;

    HYP_METHOD()
    bool EntityRemoveSwatchOverrideValue(Entity* entity, Name swatchName, Name propertyName) const;

    HYP_METHOD()
    Name GetEntityAppliedOverrideSwatch(Entity* entity) const;

    HYP_METHOD()
    void EntityApplySwatchOverrides(Entity* entity, Name swatchName) const;

    HYP_METHOD()
    void EntityRevertSwatchOverrides(Entity* entity) const;

    ///Phys

    HYP_METHOD()
    bool IsPhysicsDebugDrawEnabled() const;

    HYP_METHOD()
    void SetPhysicsDebugDrawEnabled(bool enabled);

    /*! \brief True if the focused entity has a mesh and a BoxPhysicsShape whose AABB can be fitted
     *  to the mesh via \ref FitPhysicsShapeToMesh. */
    HYP_METHOD()
    bool CanFitPhysicsShapeToMesh() const;

    /*! \brief Resize the focused entity's BoxPhysicsShape so its local AABB matches the entity's
     *  mesh AABB. Undoable. */
    HYP_METHOD()
    void FitPhysicsShapeToMesh();

    void SyncBoxPhysicsShapeToLocalBounds(Entity* entity);

    /*! \brief True if the focused entity has a mesh and a rigid body that convex collision can be built for. */
    HYP_METHOD()
    bool CanGenerateConvexCollision() const;

    /*! \brief Decompose the focused entity's mesh into convex hulls and assign them as its collision shape.
     *  Runs in the background; the swap is undoable. \p presetIndex selects a decomposition preset,
     *  0 being a single hull. */
    HYP_METHOD()
    void GenerateConvexCollision(uint32 presetIndex);

    HYP_METHOD()
    uint32 GetNumConvexCollisionPresets() const;

    HYP_METHOD()
    String GetConvexCollisionPresetName(uint32 presetIndex) const;

    /*! \brief LOD every mesh renders at in the viewport: -1 selects automatically, otherwise the LOD index. */
    HYP_METHOD()
    int32 GetViewportForcedLod() const;

    HYP_METHOD()
    void SetViewportForcedLod(int32 lodIndex);

    ///

    HYP_METHOD()
    void SetSelectedBucket(uint32 bucketIndex);

    /*! \brief Queue a content browser thumbnail render for an asset. Returns immediately; OnThumbnailReady
     *  fires once the image is on disk. When a current thumbnail is already cached it fires right away. */
    HYP_METHOD()
    void RequestAssetThumbnail(uint32 bucketIndex, Name assetName);

    /*! \brief Absolute path of the cached thumbnail for an asset, or an empty string when none has been
     *  generated yet or the cached one is older than the asset itself. */
    HYP_METHOD()
    String GetAssetThumbnailPath(uint32 bucketIndex, Name assetName) const;

    /*! \brief Drop queued thumbnail requests that have not started yet, e.g. when the user switches to a
     *  different bucket and the queued assets are no longer on screen. */
    HYP_METHOD()
    void CancelPendingAssetThumbnails();

    /*! \brief Start rendering a live preview of a material for the asset editing panel. Pass an invalid
     *  name to stop. Frames are pulled with EditorSubsystem_CopyMaterialPreviewFrame once
     *  OnMaterialPreviewUpdated fires. */
    HYP_METHOD()
    void BeginMaterialPreview(uint32 bucketIndex, Name assetName);

    /*! \brief Stop the live material preview and release its last frame. */
    HYP_METHOD()
    void EndMaterialPreview();

    /*! \brief Aim the material preview's key light. Angles are radians, driven by the mouse position over
     *  the preview image so dragging across it relights the sphere. */
    HYP_METHOD()
    void SetMaterialPreviewLightAngles(float yaw, float pitch);

    /*! \brief Mark the material preview out of date, e.g. after a property edit, so it re-renders. */
    HYP_METHOD()
    void InvalidateMaterialPreview();

    HYP_FORCE_INLINE MaterialPreviewRenderer* GetMaterialPreviewRenderer() const
    {
        return m_materialPreviewRenderer.Get();
    }

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

    /*! \brief Fired when assets are added to or removed from a bucket in the current asset registry
     *  (e.g. by deleting an asset or importing content). The argument is the index of the affected bucket. */
    HYP_FIELD()
    ScriptableDelegate<void, uint32> OnAssetsChanged;

    /*! \brief Fired when a content browser thumbnail has been written to the cache and is ready to be
     *  displayed. Arguments are the asset's bucket index and name. */
    HYP_FIELD()
    ScriptableDelegate<void, uint32, Name> OnThumbnailReady;

    /*! \brief Fired when the live material preview has a newly rendered frame waiting to be copied. */
    HYP_FIELD()
    ScriptableDelegate<void> OnMaterialPreviewUpdated;

    HYP_FIELD()
    ScriptableDelegate<void, Handle<EditorViewport>> OnActiveViewportChanged;

    HYP_FIELD()
    ScriptableDelegate<void> OnSelectionChanged;

    HYP_FIELD()
    ScriptableDelegate<void> OnMeshEditSelectionChanged;

    HYP_FIELD()
    ScriptableDelegate<void> OnMeshEditStateChanged;

    HYP_FIELD()
    ScriptableDelegate<void, EditorPlayNetState> OnPlayNetStateChanged;

private:
    void InitViewport();

    void LoadPlayNetSettings();
    void SavePlayNetSettings();

    void ConnectPlayNetClient();
    void UpdatePlayNetState();
    void SetPlayNetState(EditorPlayNetState state);

    void InitializeProjectWorld(const Handle<EditorProject>& project, bool isStartSimulation = false);
    void ShutdownProjectWorld(const Handle<EditorProject>& project, bool shutdownWorld = true);

    void UpdateBakeStatus();

    ///Gizmos

    void InitializeGizmos();
    void ShutdownGizmos();

    void SetHoveredGizmo(
        const MouseEvent& event,
        EditorGizmoBase* gizmo,
        const Handle<Node>& gizmoNode);

    HYP_FORCE_INLINE bool IsHoveringGizmo() const
    {
        return m_hoveredGizmo.IsValid() && m_hoveredGizmoNode.IsValid();
    }

    void UpdateGizmoProximityVisibility();

    HYP_FORCE_INLINE bool AreGizmosHiddenByProximity() const
    {
        return m_gizmosHiddenByProximity;
    }

    ///Mesh edits

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

    /*! \brief Record the target mesh's LOD 0 vertex positions as they were before any edit . */
    void CaptureMeshEditBaseline();

    /*! \brief Collapse every edit made this session into one action on the project's action stack,
     *  then reset the per-session stack. No-op when nothing has been edited. */
    void CommitMeshEdits();

    /*! \brief Roll the target mesh back to the captured baseline and drop the per-session stack. */
    void DiscardMeshEdits();

    bool TryPickMeshEditFace(const Ray& ray, MeshEditFaceSelection& outSelection, bool ensureUniqueMesh);

    bool PickMeshEditFaceTriangle(const Ray& ray, const Handle<Node>& targetNode, Mesh* mesh, uint8 lodIndex, uint32& outTriangleIndex);

    uint8 ResolveMeshEditLod(Node* targetNode) const;
    void SetSelectedMeshEditFace(Optional<MeshEditFaceSelection> selection);
    void UpdateHoveredMeshEditFace(const Ray& ray);
    void DebugDrawMeshEditSelection(class DebugDrawCommandList& debugDrawCommandList);

    void StartMeshEditDrag(const Handle<Camera>& camera, const MouseEvent& mouseEvent);
    void UpdateMeshEditDrag(const Handle<Camera>& camera, const MouseEvent& mouseEvent);
    void EndMeshEditDrag(bool saveEdits);
    void SetMeshEditDragLockedAxis(const Handle<Camera>& camera, const KeyboardEvent& keyboardEvent, int axis);

    bool BackOutOfMeshEditState();

    ////////////////////

    void DebugDrawPhysicsShapes(class DebugDrawCommandList& debugDrawCommandList);

    void DebugDrawMeshLods(class DebugDrawCommandList& debugDrawCommandList);
    /*! \brief If the focused entity's physics shape is referenced by any other entity, clone it and
     *  assign the clone to this entity, so the shape can be mutated */
    Handle<PhysicsShape> EnsureUniquePhysicsShape(Entity* entity);

    bool IsPhysicsShapeShared(Entity* entity, const Handle<PhysicsShape>& shape) const;

    ////////////////////

    SubsystemUpdatePhase GetUpdatePhase_Internal() const override
    {
        return SubsystemUpdatePhase::AfterVis;
    }

    ///this whole thing is held together by ducktape and hopes & dreams
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

        ///LOD being edited
        uint8 lodIndex = 0;

        UniquePtr<BVHNode, EditorAllocator> lodPickBvh;
        WeakHandle<Mesh> lodPickBvhMesh;
        uint8 lodPickBvhLodIndex = 0;
        bool lodPickBvhDirty = true;

        Array<Vec3f, EditorAllocator> baselinePositions;
        WeakHandle<Mesh> baselineMesh;

        Optional<MeshEditFaceSelection> selectedFace;
        Optional<MeshEditFaceSelection> hoveredFace;
        Optional<MeshEditDragData> dragData;
    } m_meshEditState;

    ////////////////////

    Handle<EditorTerrainState> m_terrainSculpting;

    ////////////////////

    Handle<Scene> m_editorScene;

    // The project.
    Handle<EditorProject> m_currentProject;
    // The project, but only used when we start simulation and need to restore the pre-simulation state after we end simulation.
    Handle<EditorProject> m_preSimulationProject;

    WeakHandle<Scene> m_activeScene;

    EditorManipulationMode m_selectedManipulationMode;
    EditorGizmoSet m_gizmos;

    WeakHandle<EditorGizmoBase> m_hoveredGizmo;
    WeakHandle<Node> m_hoveredGizmoNode;

    WeakHandle<Node> m_focusedNode;
    // the actual node that displays the highlight for the focused item
    Handle<Node> m_highlightNode;

    Set<Handle<Node>, EditorAllocator> m_selectedNodes;

    EditorDelegates* m_editorDelegates;

    ClockTimer m_bakeStatusUpdateTimer;
    Handle<MessagesOverlay> m_messagesOverlay;

    uint32 m_selectedBucketIndex;

    Array<Handle<EditorViewport>, EditorAllocator> m_editorViewports;

    Handle<View> m_simulationView;
    FilePath m_simulationSnapshotPath;

    EditorPlayNetMode m_playNetMode;
    String m_playNetHost;
    uint32 m_playNetPort;
    bool m_playNetAutoLaunchServer;
    uint32 m_playNetCachePort;

    // latched from the settings above when a simulation starts
    EditorPlayNetMode m_activeNetMode;
    bool m_activeAutoLaunchServer;
    EditorPlayNetState m_playNetState;

    Handle<Entity> m_meshPreviewEntity;
    Handle<Material> m_meshPreviewMaterial;

    UniquePtr<AssetThumbnailService> m_thumbnailService;
    UniquePtr<MaterialPreviewRenderer> m_materialPreviewRenderer;

    DelegateHandlerSet m_delegateHandlers;

    ////////////////////

    bool m_snapToGridEnabled;
    bool m_swatchOverrideMode;

    bool m_gizmosHiddenByProximity;

    bool m_editorCameraEnabled;
    bool m_shouldCancelNextClick;
};

} // namespace Hyperion
