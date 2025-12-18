/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <editor/ui/EditorUI.hpp>

#include <editor/EditorActionStack.hpp>
#include <editor/EditorTask.hpp>
#include <editor/EditorTaskManager.hpp>

#include <scene/Subsystem.hpp>

#include <core/functional/Delegate.hpp>

namespace hyperion {

class Game;
class World;
class Scene;
class Camera;
class Texture;
class ReflectionProbe;
class InputManager;
class UIStage;
class UIObject;
class UIListView;
class UIGrid;
class FontAtlas;
class EditorDelegates;
class EditorSubsystem;
class EditorProject;
class EditorDebugOverlayBase;
class EditorCommandBase;
class AssetPackage;
struct MouseEvent;
struct KeyboardEvent;
class View;
class EditorViewport;
class LightmapVolume;

namespace sys {
class AppContextBase;
} // namespace sys

using sys::AppContextBase;

HYP_CLASS()
class GenerateLightmapsEditorTask : public TickableEditorTask
{
    HYP_OBJECT_BODY(GenerateLightmapsEditorTask);

public:
    GenerateLightmapsEditorTask()
        : TickableEditorTask()
    {
    }

    explicit GenerateLightmapsEditorTask(const Handle<LightmapVolume>& volume);
    explicit GenerateLightmapsEditorTask(const Handle<ReflectionProbe>& probe);

    explicit GenerateLightmapsEditorTask(const Array<Handle<ObjectBase>>& sources);

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<World>& GetWorld() const
    {
        return m_world;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetWorld(const Handle<World>& world)
    {
        m_world = world;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Scene>& GetScene() const
    {
        return m_scene;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetScene(const Handle<Scene>& scene)
    {
        m_scene = scene;
    }

    HYP_METHOD()
    virtual void Process() override;

    HYP_METHOD()
    virtual void Cancel() override;

    HYP_METHOD()
    virtual bool IsCompleted() const override;

    HYP_METHOD()
    virtual void Tick(float delta) override;

private:
    Array<Handle<ObjectBase>> m_sources;
    Handle<World> m_world;
    Handle<Scene> m_scene;

    Array<Task<void>*> m_tasks;
};

HYP_ENUM()
enum class EditorManipulationMode
{
    NONE = 0,

    TRANSLATE,
    ROTATE,
    SCALE
};

/*! \brief A widget that can manipulate the selected object. (e.g translate, rotate, scale) */
HYP_CLASS(Abstract)
class HYP_API EditorGizmoBase : public ObjectBase
{
    HYP_OBJECT_BODY(EditorGizmoBase);

public:
    EditorGizmoBase();
    virtual ~EditorGizmoBase() = default;

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
    virtual void OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node);

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
        return EditorManipulationMode::NONE;
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
        return EditorManipulationMode::TRANSLATE;
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
    virtual void OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;

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
};

HYP_CLASS()
class RotateEditorGizmo : public EditorGizmoBase
{
    HYP_OBJECT_BODY(RotateEditorGizmo);

public:
    virtual ~RotateEditorGizmo() override = default;

    virtual EditorManipulationMode GetManipulationMode() const override
    {
        return EditorManipulationMode::ROTATE;
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
    virtual void OnDragEnd(const Handle<Camera>& camera, const MouseEvent& mouseEvent, const Handle<Node>& node) override;

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
        Quaternion startRotation;
        Quaternion currentRotation;
    };

    virtual Handle<Node> Load_Internal() const override;

    Optional<DragData> m_dragData;
};

HYP_CLASS()
class HYP_API EditorSubsystem : public Subsystem
{

    HYP_OBJECT_BODY(EditorSubsystem);

public:
    using EditorGizmoSet = HashSet<Handle<EditorGizmoBase>, &EditorGizmoBase::GetManipulationMode>;

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
    bool ExecuteCommandByName(Name name);

    HYP_METHOD()
    void NewProject();

    HYP_METHOD()
    void OpenProject(const Handle<EditorProject>& project);

    HYP_METHOD()
    void ShowImportContentDialog();

    HYP_METHOD()
    void AddTask(const Handle<EditorTaskBase>& task);

    HYP_METHOD()
    void SetFocusedNode(const Handle<Node>& focusedNode, bool shouldSelectInOutline = true);

    HYP_METHOD()
    void AddDebugOverlay(const Handle<EditorDebugOverlayBase>& debugOverlay);

    HYP_METHOD()
    bool RemoveDebugOverlay(EditorDebugOverlayBase* debugOverlay);

    HYP_METHOD()
    Handle<Node> GetFocusedNode() const;

    HYP_METHOD()
    Handle<Scene> GetActiveScene() const;

    void SetActiveScene(const WeakHandle<Scene>& scene);

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
    void SetSelectedPackage(const Handle<AssetPackage>& package);

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
    ScriptableDelegate<void, Handle<AssetPackage>> OnSelectedPackageChanged;

private:
    void LoadEditorUIDefinitions();

    void CreateHighlightNode();

    void InitViewport();
    void InitSceneOutline();
    void InitContentBrowser();
    void InitDetailView();
    void InitDebugOverlays();
    void InitGizmoSelection();
    void InitActiveSceneSelection();

    TResult<Handle<FontAtlas>> CreateFontAtlas();

    void UpdateCamera(float delta);
    void UpdateTasks(float delta);
    void UpdateDebugOverlays(float delta);

    void StartWatchingNode(const Handle<Node>& node);
    void StopWatchingNode(const Handle<Node>& node);
    void ClearWatchedNodes();
    void UpdateWatchedNodes();

    void AddPackageToContentBrowser(const Handle<AssetPackage>& package, bool nested);
    void RemovePackageFromContentBrowser(AssetPackage* package);

    void SetGizmoCurrentProject(const WeakHandle<EditorProject>& project);
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

    Handle<Scene> m_editorScene;

    Handle<EditorProject> m_currentProject;
    WeakHandle<Scene> m_activeScene;

    EditorTaskManager m_taskManager;

    EditorManipulationMode m_selectedManipulationMode;
    EditorGizmoSet m_gizmos;
    WeakHandle<EditorProject> m_gizmoCurrentProject;

    WeakHandle<EditorGizmoBase> m_hoveredGizmo;
    WeakHandle<Node> m_hoveredGizmoNode;

    WeakHandle<Node> m_focusedNode;
    // the actual node that displays the highlight for the focused item
    Handle<Node> m_highlightNode;

    bool m_editorCameraEnabled;
    bool m_shouldCancelNextClick;

    EditorDelegates* m_editorDelegates;

    Array<Handle<EditorDebugOverlayBase>> m_debugOverlays;

    // top-left, bottom-left, top-right, bottom-right
    FixedArray<Handle<UIObject>, 4> m_debugOverlayContainers;

    Handle<UIListView> m_contentBrowserDirectoryList;
    Handle<AssetPackage> m_selectedPackage;

    Handle<UIGrid> m_contentBrowserContents;
    Handle<UIObject> m_contentBrowserContentsEmpty;

    Array<Handle<EditorViewport>> m_editorViewports;

    DelegateHandlerSet m_delegateHandlers;
};

} // namespace hyperion
