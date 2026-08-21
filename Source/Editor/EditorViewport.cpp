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

#include <Scene/View.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/EntityManager.hpp>

#include <EditorViewport.generated.inl>

namespace Hyperion {

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
        m_camera->SetName(NAME("EditorViewportCamera"));

        m_camera->AddTag<EntityTag::EditorCamera>();

        m_camera->SetCameraFlags(CameraFlags::MatchWindowSize | CameraFlags::HasStreamingVolume);

        m_camera->AddCameraController(MakeHandle<EditorCameraController>());

        m_camera->SetFOV(60.0f);
        m_camera->SetNearClip(0.1f);
        m_camera->SetFarClip(1000.0f);
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

    for (const Handle<Scene>& scene : world->GetScenes())
    {
        Assert(scene != nullptr);

        if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
        {
            continue;
        }

        m_view->AddScene(scene);
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

    world->RemoveView(m_view);
}

void EditorViewport::OnSceneAdded(Scene* scene)
{
    Assert(scene != nullptr);

    if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
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

} // namespace Hyperion
