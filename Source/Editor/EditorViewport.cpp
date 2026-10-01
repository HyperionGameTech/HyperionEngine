/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorViewport.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorCamera.hpp>

#include <System/AppContext.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/EntityManager.hpp>

#include <Framework/View.hpp>

#include <EditorViewport.generated.inl>

namespace Hyperion {

/// @TODO Move to a new EditorHelpers
static bool IsWorldSceneShownInViewport(const Scene* scene)
{
    return (scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND;
}

EditorViewport::EditorViewport(const Handle<Camera>& camera)
    : m_camera(camera),
      m_view(nullptr),
      m_window(nullptr)
{
}

EditorViewport::~EditorViewport() = default;

void EditorViewport::Init()
{
    if (!m_camera)
    {
        m_camera = MakeHandle<Camera>();
        m_camera->SetName(NAME("EditorCamera"));

        m_camera->AddTag<EntityTag::EditorCamera>();

        m_camera->SetCameraFlags(CameraFlags::MatchWindowSize | CameraFlags::HasStreamingVolume);

        m_camera->AddCameraController(MakeHandle<EditorCameraController>());

        m_camera->SetFOV(75.0f);
        m_camera->SetNearClip(0.1f);
        m_camera->SetFarClip(3000.0f);
    }

    InitObject(m_camera);

    ViewDesc viewDesc {};
    viewDesc.flags = ViewFlags::DEFAULT
        | ViewFlags::GBUFFER
        | ViewFlags::MATCH_CAMERA_DIMENSIONS
        | ViewFlags::EDITOR_VIEW;

    viewDesc.framebufferDesc = {};
    viewDesc.framebufferDesc.extent = Vec2u(m_camera->GetDimensions());

    viewDesc.camera = m_camera;

    m_view = MakeHandle<View>(viewDesc);
    m_view->SetName(NAME("EditorViewportView"));
    InitObject(m_view);

    SetReady(true);
}

Handle<ApplicationWindow> EditorViewport::CreateViewportWindow(const WindowOptions& options)
{
    AssertOnThread(g_mainThread);

    Handle<ApplicationWindow> window = g_appContext->CreateSystemWindow(options);
    m_window = window;

    return window;
}

void EditorViewport::OnAdded(EditorSubsystem* editorSubsystem)
{
    const Handle<Scene>& editorScene = editorSubsystem->GetEditorScene();
    Assert(editorScene.IsValid());

    editorScene->GetRoot()->AddChild(m_camera);

    m_view->AddScene(editorScene);

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();
    Assert(currentProject.IsValid());

    const Handle<World>& world = currentProject->GetWorld();
    Assert(world.IsValid());

    if (m_isolatedScenes.Any())
    {
        for (const WeakHandle<Scene>& isolatedSceneWeak : m_isolatedScenes)
        {
            m_view->AddScene(isolatedSceneWeak.Lock().Get());
        }
    }
    else
    {
        for (const Handle<Scene>& scene : world->GetScenes())
        {
            Assert(scene != nullptr);

            if (!IsWorldSceneShownInViewport(scene.Get()))
            {
                continue;
            }

            m_view->AddScene(scene);
        }
    }

    world->AddView(m_view);
}

void EditorViewport::OnRemoved(EditorSubsystem* editorSubsystem)
{
    const Handle<Scene>& editorScene = editorSubsystem->GetEditorScene();
    Assert(editorScene.IsValid());

    m_camera->Remove(/* moveToDetached */ false);

    m_view->RemoveScene(editorScene);

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();
    Assert(currentProject.IsValid());

    const Handle<World>& world = currentProject->GetWorld();
    Assert(world.IsValid());

    for (const Handle<Scene>& scene : world->GetScenes())
    {
        Assert(scene != nullptr);

        m_view->RemoveScene(scene);
    }

    for (const WeakHandle<Scene>& isolatedSceneWeak : m_isolatedScenes)
    {
        m_view->RemoveScene(isolatedSceneWeak.Lock().Get());
    }

    m_isolatedScenes.Clear();

    world->RemoveView(m_view);
}

void EditorViewport::OnSceneAdded(Scene* scene)
{
    Assert(scene != nullptr);

    if (m_isolatedScenes.Any() || !IsWorldSceneShownInViewport(scene))
    {
        return;
    }

    m_view->AddScene(scene);
}

void EditorViewport::OnSceneRemoved(Scene* scene)
{
    Assert(scene != nullptr);

    m_view->RemoveScene(scene);
}

void EditorViewport::SetIsolatedScenes(EditorSubsystem* editorSubsystem, const Array<Handle<Scene>>& scenes)
{
    for (const WeakHandle<Scene>& isolatedSceneWeak : m_isolatedScenes)
    {
        m_view->RemoveScene(isolatedSceneWeak.Lock().Get());
    }

    m_isolatedScenes.Clear();

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();
    const World* world = currentProject.IsValid() ? currentProject->GetWorld().Get() : nullptr;

    if (world)
    {
        for (const Handle<Scene>& scene : world->GetScenes())
        {
            if (!scene.IsValid() || !IsWorldSceneShownInViewport(scene.Get()))
            {
                continue;
            }

            if (scenes.Any())
            {
                m_view->RemoveScene(scene);
            }
            else
            {
                m_view->AddScene(scene);
            }
        }
    }

    for (const Handle<Scene>& scene : scenes)
    {
        if (!scene.IsValid())
        {
            continue;
        }

        m_isolatedScenes.PushBack(scene.ToWeak());
        m_view->AddScene(scene);
    }
}

} // namespace Hyperion
