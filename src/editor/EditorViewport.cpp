/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <EditorPch.hpp>

#include <editor/EditorViewport.hpp>
#include <editor/EditorSubsystem.hpp>
#include <editor/EditorProject.hpp>
#include <editor/EditorCamera.hpp>

#include <system/AppContext.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <scene/View.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>

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
    SafeDelete(std::move(m_camera));
}

void EditorViewport::Init()
{
    ObjectBase::Init();

    if (!m_camera)
    {
        m_camera = CreateObject<Camera>();
        m_camera->SetWindow(m_window);
        m_camera->SetCameraFlags(CameraFlags::MATCH_WINDOW_SIZE);
        m_camera->AddCameraController(CreateObject<EditorCameraController>());
        m_camera->SetName(NAME("EditorViewportCamera"));
        m_camera->SetFOV(60.0f);
        m_camera->SetNear(0.1f);
        m_camera->SetFar(3000.0f);
    }

    InitObject(m_camera);

    const ViewDesc viewDesc {
        .flags = ViewFlags::DEFAULT
            | ViewFlags::GBUFFER
            | ViewFlags::MATCH_CAMERA_DIMENSIONS,
        .viewport = Viewport {
            .extent = Vec2u(m_camera->GetDimensions()),
            .position = Vec2i::Zero()
        },
        .renderTargetDesc = {
            .extent = Vec2u(m_camera->GetDimensions())
        },
        .camera = m_camera
    };

    m_view = CreateObject<View>(viewDesc);
    InitObject(m_view);

    SetReady(true);
}

Handle<ApplicationWindow> EditorViewport::CreateViewportWindow(const WindowOptions& options)
{
    AssertOnThread(g_mainThread);

    Handle<ApplicationWindow> window = g_appContext->CreateSystemWindow(options);
    m_window = window;

    if (m_camera)
    {
        m_camera->SetWindow(m_window);
    }

    return window;
}

void EditorViewport::OnAdded(EditorSubsystem* editorSubsystem)
{
    AssertReady();

    const Handle<Scene>& editorScene = editorSubsystem->GetEditorScene();
    Assert(editorScene.IsValid());

    editorScene->GetRoot()->AddChild(m_camera);
    m_view->AddScene(editorScene);

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();

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
    AssertReady();

    const Handle<Scene>& editorScene = editorSubsystem->GetEditorScene();
    Assert(editorScene.IsValid());

    editorScene->GetRoot()->RemoveChild(m_camera);
    m_view->RemoveScene(editorScene);

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();

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
    AssertReady();
    Assert(scene != nullptr);

    if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
    {
        return;
    }

    m_view->AddScene(MakeStrongRef(scene));
}

void EditorViewport::OnSceneRemoved(Scene* scene)
{
    AssertReady();
    Assert(scene != nullptr);

    m_view->RemoveScene(scene);
}

} // namespace Hyperion
