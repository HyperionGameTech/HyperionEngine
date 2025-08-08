/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <editor/ui/EditorUI.hpp>
#include <editor/EditorActionStack.hpp>
#include <editor/EditorTask.hpp>

#include <scene/Subsystem.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashSet.hpp>

#include <core/math/BoundingBox.hpp>

#include <core/functional/Delegate.hpp>

#include <core/threading/Threads.hpp>

#include <core/utilities/Uuid.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/RefCountedPtr.hpp>

namespace hyperion {

class World;
class Scene;
class Camera;
class Texture;
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
class AssetPackage;
struct MouseEvent;
struct KeyboardEvent;
class View;
class ConsoleUI;

namespace sys {
class AppContextBase;
} // namespace sys

using sys::AppContextBase;

class RunningEditorTask
{
public:
    friend class EditorSubsystem;

    RunningEditorTask(const Handle<EditorTaskBase>& task)
        : m_task(task)
    {
    }

    RunningEditorTask(const Handle<EditorTaskBase>& task, const Handle<UIObject>& uiObject)
        : m_task(task),
          m_uiObject(uiObject)
    {
    }

    HYP_FORCE_INLINE const Handle<EditorTaskBase>& GetTask() const
    {
        return m_task;
    }

    HYP_FORCE_INLINE const Handle<UIObject>& GetUIObject() const
    {
        return m_uiObject;
    }

    Handle<UIObject> CreateUIObject(UIStage* uiStage) const;

private:
    Handle<EditorTaskBase> m_task;
    Handle<UIObject> m_uiObject;
};

HYP_CLASS()
class GenerateLightmapsEditorTask : public TickableEditorTask
{
    HYP_OBJECT_BODY(GenerateLightmapsEditorTask);

public:
    GenerateLightmapsEditorTask();

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
    HYP_FORCE_INLINE const BoundingBox& GetAABB() const
    {
        return m_aabb;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetAABB(const BoundingBox& aabb)
    {
        m_aabb = aabb;
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
    Handle<World> m_world;
    Handle<Scene> m_scene;
    BoundingBox m_aabb;
    Task<void>* m_task;
};

enum class EditorManipulationMode
{
    NONE = 0,

    TRANSLATE,
    ROTATE,
    SCALE
};

/*! \brief A widget that can manipulate the selected object. (e.g translate, rotate, scale) */
HYP_CLASS(Abstract)
class HYP_API EditorManipulationWidgetBase : public HypObjectBase
{
    HYP_OBJECT_BODY(EditorManipulationWidgetBase);

public:
    EditorManipulationWidgetBase();
    virtual ~EditorManipulationWidgetBase() = default;

    HYP_FORCE_INLINE const Handle<Node>& GetNode() const
    {
        return m_node;
    }

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

    virtual EditorManipulationMode GetManipulationMode() const = 0;

    virtual int GetPriority() const
    {
        return -1;
    }

    virtual void UpdateWidget(const Handle<Node>& focusedNode);

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
class NullEditorManipulationWidget : public EditorManipulationWidgetBase
{
    HYP_OBJECT_BODY(NullEditorManipulationWidget);

public:
    virtual ~NullEditorManipulationWidget() override = default;

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
class TranslateEditorManipulationWidget : public EditorManipulationWidgetBase
{
    HYP_OBJECT_BODY(TranslateEditorManipulationWidget);

public:
    virtual ~TranslateEditorManipulationWidget() override = default;

    virtual EditorManipulationMode GetManipulationMode() const override
    {
        return EditorManipulationMode::TRANSLATE;
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

class HYP_API EditorManipulationWidgetHolder
{
    using EditorManipulationWidgetSet = HashSet<Handle<EditorManipulationWidgetBase>, &EditorManipulationWidgetBase::GetManipulationMode>;

public:
    using OnSelectedManipulationWidgetChangeDelegate = Delegate<void, EditorManipulationWidgetBase&, EditorManipulationWidgetBase&>;

    EditorManipulationWidgetHolder(EditorSubsystem* editorSubsystem);

    HYP_FORCE_INLINE const EditorManipulationWidgetSet& GetManipulationWidgets() const
    {
        return m_manipulationWidgets;
    }
    
    HYP_FORCE_INLINE EditorManipulationWidgetBase& GetManipulationWidget(EditorManipulationMode mode) const
    {
        Assert(mode != EditorManipulationMode::NONE);
        
        return *m_manipulationWidgets.At(mode);
    }

    EditorManipulationMode GetSelectedManipulationMode() const;
    void SetSelectedManipulationMode(EditorManipulationMode mode);

    EditorManipulationWidgetBase& GetSelectedManipulationWidget() const;

    void SetCurrentProject(const WeakHandle<EditorProject>& project);

    void Initialize();
    void Shutdown();

    OnSelectedManipulationWidgetChangeDelegate OnSelectedManipulationWidgetChange;

private:
    EditorSubsystem* m_editorSubsystem;
    WeakHandle<EditorProject> m_currentProject;

    EditorManipulationMode m_selectedManipulationMode;
    EditorManipulationWidgetSet m_manipulationWidgets;
};

HYP_CLASS()
class HYP_API EditorSubsystem : public Subsystem
{
    HYP_OBJECT_BODY(EditorSubsystem);

public:
    EditorSubsystem(const Handle<AppContextBase>& appContext);
    virtual ~EditorSubsystem() override;

    void OnAddedToWorld() override;
    void OnRemovedFromWorld() override;
    void Update(float delta) override;

    void OnSceneAttached(const Handle<Scene>& scene) override;
    void OnSceneDetached(const Handle<Scene>& scene) override;

    HYP_FORCE_INLINE const Handle<AppContextBase>& GetAppContext() const
    {
        return m_appContext;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<EditorProject>& GetCurrentProject() const
    {
        return m_currentProject;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE Handle<Scene> GetActiveScene() const
    {
        return m_activeScene.Lock();
    }

    HYP_FORCE_INLINE EditorManipulationWidgetHolder& GetManipulationWidgetHolder()
    {
        return m_manipulationWidgetHolder;
    }

    HYP_FORCE_INLINE const EditorManipulationWidgetHolder& GetManipulationWidgetHolder() const
    {
        return m_manipulationWidgetHolder;
    }

    HYP_METHOD()
    void NewProject();

    HYP_METHOD()
    void OpenProject(const Handle<EditorProject>& project);

    HYP_METHOD()
    void ShowOpenProjectDialog();

    HYP_METHOD()
    void ShowImportContentDialog();

    HYP_METHOD()
    void AddTask(const Handle<EditorTaskBase>& task);

    HYP_METHOD()
    void SetFocusedNode(const Handle<Node>& focusedNode, bool shouldSelectInOutline = true);

    HYP_METHOD()
    void AddDebugOverlay(const Handle<EditorDebugOverlayBase>& debugOverlay);

    HYP_METHOD()
    bool RemoveDebugOverlay(WeakName name);

    HYP_METHOD()
    Handle<Node> GetFocusedNode() const;

    HYP_FORCE_INLINE EditorDelegates* GetEditorDelegates()
    {
        return m_editorDelegates;
    }

    HYP_FIELD()
    ScriptableDelegate<void, const Handle<Node>&, const Handle<Node>&, bool> OnFocusedNodeChanged;

    HYP_FIELD()
    ScriptableDelegate<void, const Handle<EditorProject>&> OnProjectClosing;

    HYP_FIELD()
    ScriptableDelegate<void, const Handle<EditorProject>&> OnProjectOpened;

    HYP_FIELD()
    ScriptableDelegate<void, const Handle<Scene>&> OnActiveSceneChanged;

private:
    void LoadEditorUIDefinitions();

    void CreateHighlightNode();

    void InitViewport();
    void InitSceneOutline();
    void InitContentBrowser();
    void InitDetailView();
    void InitConsoleUI();
    void InitDebugOverlays();
    void InitManipulationWidgetSelection();
    void InitActiveSceneSelection();

    TResult<RC<FontAtlas>> CreateFontAtlas();

    void UpdateCamera(float delta);
    void UpdateTasks(float delta);
    void UpdateDebugOverlays(float delta);

    void StartWatchingNode(const Handle<Node>& node);
    void StopWatchingNode(const Handle<Node>& node);
    void ClearWatchedNodes();
    void UpdateWatchedNodes();

    void AddPackageToContentBrowser(const Handle<AssetPackage>& package, bool nested);
    void RemovePackageFromContentBrowser(AssetPackage* package);
    void SetSelectedPackage(const Handle<AssetPackage>& package);

    void SetHoveredManipulationWidget(
        const MouseEvent& event,
        EditorManipulationWidgetBase* manipulationWidget,
        const Handle<Node>& manipulationWidgetNode);

    HYP_FORCE_INLINE bool IsHoveringManipulationWidget() const
    {
        return m_hoveredManipulationWidget.IsValid() && m_hoveredManipulationWidgetNode.IsValid();
    }

    void SetActiveScene(const WeakHandle<Scene>& scene);

    Handle<AppContextBase> m_appContext;
    Handle<Scene> m_editorScene;
    Handle<Camera> m_camera;

    Handle<EditorProject> m_currentProject;
    WeakHandle<Scene> m_activeScene;

    Array<RunningEditorTask> m_tasks;

    EditorManipulationWidgetHolder m_manipulationWidgetHolder;

    WeakHandle<EditorManipulationWidgetBase> m_hoveredManipulationWidget;
    WeakHandle<Node> m_hoveredManipulationWidgetNode;

    Handle<UIObject> m_mainPanel;

    WeakHandle<Node> m_focusedNode;
    // the actual node that displays the highlight for the focused item
    Handle<Node> m_highlightNode;

    bool m_editorCameraEnabled;
    bool m_shouldCancelNextClick;

    EditorDelegates* m_editorDelegates;

    Array<Handle<EditorDebugOverlayBase>> m_debugOverlays;
    Handle<UIObject> m_debugOverlayUiObject;

    Handle<ConsoleUI> m_consoleUi;

    Handle<UIListView> m_contentBrowserDirectoryList;
    Handle<AssetPackage> m_selectedPackage;

    Handle<UIGrid> m_contentBrowserContents;
    Handle<UIObject> m_contentBrowserContentsEmpty;

    Array<Handle<View>> m_views;

    DelegateHandlerSet m_delegateHandlers;
};

} // namespace hyperion

