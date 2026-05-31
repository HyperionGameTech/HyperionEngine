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

#include <Rendering/util/DeletionQueue.hpp>

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

EditorViewport::~EditorViewport()
{
    EnqueueDeletion(std::move(m_camera));
}

void EditorViewport::Init()
{
    if (!m_camera)
    {
        m_camera = MakeHandle<Camera>();
        m_camera->SetName(NAME("EditorViewportCamera"));
        m_camera->SetIsTransient(true);

        m_camera->AddTag<EntityTag::EditorCamera>();

        m_camera->SetCameraFlags(CameraFlags::MATCH_WINDOW_SIZE);

        m_camera->AddCameraController(MakeHandle<EditorCameraController>());

        m_camera->SetFOV(60.0f);
        m_camera->SetNearClip(0.1f);
        m_camera->SetFarClip(3000.0f);
    }

    InitObject(m_camera);

    ViewDesc viewDesc {};
    viewDesc.flags = ViewFlags::DEFAULT | ViewFlags::GBUFFER | ViewFlags::MATCH_CAMERA_DIMENSIONS;

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
    AssertDebug(currentProject.IsValid());

    if (!currentProject)
    {
        return;
    }

    for (const Handle<Scene>& scene : currentProject->GetWorld()->GetScenes())
    {
        Assert(scene != nullptr);

        if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
        {
            continue;
        }

        m_view->AddScene(scene);
    }

    currentProject->GetWorld()->AddView(m_view);
}

void EditorViewport::OnRemoved(EditorSubsystem* editorSubsystem)
{
    const Handle<Scene>& editorScene = editorSubsystem->GetEditorScene();
    Assert(editorScene.IsValid());

    editorScene->GetRoot()->RemoveChild(m_camera);
    m_view->RemoveScene(editorScene);

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();
    AssertDebug(currentProject.IsValid());

    if (!currentProject)
    {
        return;
    }

    for (const Handle<Scene>& scene : currentProject->GetWorld()->GetScenes())
    {
        Assert(scene != nullptr);

        m_view->RemoveScene(scene);
    }

    currentProject->GetWorld()->RemoveView(m_view);
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
