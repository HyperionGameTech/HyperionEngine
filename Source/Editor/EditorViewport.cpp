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
#include <Rendering/RenderableAttributes.hpp>
#include <Rendering/ThumbnailCaptureState.hpp>
#include <Rendering/Framebuffer.hpp>
#include <Rendering/GBuffer.hpp>

#include <Util/Img/WritePng.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/EntityManager.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Thread.hpp>
#include <Core/Threading/Scheduler.hpp>

#include <Core/Utilities/Float16.hpp>
#include <Core/Utilities/Time.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <Framework/View.hpp>

#include <EditorViewport.generated.inl>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

namespace /* Helpers */ {

bool IsWorldSceneShownInViewport(const Scene& scene)
{
    return (scene.GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND;
}

RenderableAttributeSet GetWireframeOverrideAttributes()
{
    MaterialAttributes materialAttributes {};
    materialAttributes.shaderName = NAME("EditorWireframe");
    materialAttributes.fillMode = FillMode::Line;
    materialAttributes.cullFaces = FaceCullMode::None;
    materialAttributes.blendFunction = BlendFunction::None();
    materialAttributes.flags = MaterialAttributeFlags::None;

    return RenderableAttributeSet(MeshAttributes {}, materialAttributes);
}

Name GetCameraName(EditorViewportType viewportType)
{
    switch (viewportType)
    {
    case EditorViewportType::Top:
        return NAME("EditorCamera_Top");
    case EditorViewportType::Front:
        return NAME("EditorCamera_Front");
    case EditorViewportType::Side:
        return NAME("EditorCamera_Side");
    default:
        return NAME("EditorCamera");
    }
}

} // namespace

#pragma region EditorViewport

EditorViewport::EditorViewport(const Handle<Camera>& camera)
    : m_viewportType(EditorViewportType::Perspective),
      m_camera(camera),
      m_view(nullptr),
      m_isMainWindow(true)
{
}

EditorViewport::~EditorViewport() = default;

ApplicationWindow* EditorViewport::GetWindow() const
{
    return m_window.Get();
}

bool EditorViewport::ShowsWorldScene(const Scene* scene) const
{
    if (!scene)
    {
        return false;
    }

    if (!IsWorldSceneShownInViewport(*scene))
    {
        return false;
    }

    return !(IsOrthographic() && (scene->GetSceneFlags() & SceneFlags::NO_EDITOR_WIREFRAME));
}

void EditorViewport::RequestRedraw()
{
    if (m_view.IsValid())
    {
        m_view->RequestRedraw();
    }
}

void EditorViewport::SetViewportType(EditorViewportType viewportType)
{
    if (IsInitCalled())
    {
        HYP_LOG(Editor, Warning, "EditorViewport type can only be set before it is initialized");

        return;
    }

    m_viewportType = viewportType;
}

void EditorViewport::Init()
{
    if (!m_camera)
    {
        m_camera = MakeHandle<Camera>();
        m_camera->SetName(GetCameraName(m_viewportType));

        m_camera->AddTag<EntityTag::EditorCamera>();

        EnumFlags<CameraFlags> cameraFlags = CameraFlags::None;

        if (IsOrthographic())
        {
            m_camera->SetDimensions(Vec2i(512, 512));

            m_camera->AddCameraController(MakeHandle<EditorOrthoCameraController>(m_viewportType));
        }
        else
        {
            cameraFlags |= CameraFlags::HasStreamingVolume;
            
            if (m_isMainWindow)
            {
                cameraFlags |= CameraFlags::MatchWindowSize;
            }

            m_camera->SetFOV(75.0f);
            m_camera->SetNearClip(0.1f);
            m_camera->SetFarClip(3000.0f);

            m_camera->AddCameraController(MakeHandle<EditorCameraController>());
        }

        m_camera->SetCameraFlags(cameraFlags);
    }

    InitObject(m_camera);

    UpdateCameraDimensions();

    ViewDesc viewDesc {};

    if (IsOrthographic())
    {
        // scenes are managed here, so the world doesn't add NO_EDITOR_WIREFRAME scenes back
        viewDesc.flags = ViewFlags::COLLECT_ALL_ENTITIES
                        | ViewFlags::EDITOR_WIREFRAME_VIEW
                        | ViewFlags::EXTERNAL_RENDERTARGET
                        | ViewFlags::NO_SHADOW_VIEWS
                        | ViewFlags::SKIP_LIGHTS
                        | ViewFlags::SKIP_ENV_PROBES
                        | ViewFlags::SKIP_LIGHTMAP_VOLUMES
                        | ViewFlags::SKIP_PARTICLE_VOLUMES
                        | ViewFlags::SKIP_FOG_VOLUMES
                        | ViewFlags::SKIP_EFFECT_VOLUMES
                        | ViewFlags::SKIP_DECALS
                        | ViewFlags::SKIP_SPRITES;

        viewDesc.overrideAttributes = GetWireframeOverrideAttributes();
    }
    else
    {
        viewDesc.flags = ViewFlags::DEFAULT
                        | ViewFlags::GBUFFER
                        | ViewFlags::MATCH_CAMERA_DIMENSIONS
                        | ViewFlags::EDITOR_VIEW;
    }

    viewDesc.framebufferDesc = {};
    viewDesc.framebufferDesc.extent = Vec2u(MathUtil::Max(m_camera->GetDimensions(), Vec2i::One()));

    viewDesc.camera = m_camera;

    m_view = MakeHandle<View>(viewDesc);
    m_view->SetName(NAME_FMT("EditorViewportView_{}", uint32(m_viewportType)));

    if (m_window.IsValid())
    {
        m_view->SetOutputWindowId(m_window->Id().Value());
    }

    InitObject(m_view);

    SetReady(true);
}

Handle<ApplicationWindow> EditorViewport::CreateViewportWindow(const WindowOptions& options, bool isMainWindow)
{
    AssertOnThread(g_mainThread);

    Handle<ApplicationWindow> window = g_appContext->CreateSystemWindow(options);

    if (!window.IsValid())
    {
        return window;
    }

    if (isMainWindow)
    {
        g_appContext->SetMainWindow(window);
    }
    else
    {
        g_appContext->AddPresentWindow(window);
    }

    GetThreadById(g_simThread)->GetScheduler().Enqueue([weakThis = MakeWeakRef(this), window, isMainWindow]()
        {
            if (Handle<EditorViewport> strongThis = weakThis.Lock())
            {
                strongThis->SetWindow(window, isMainWindow);
            }
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);

    return window;
}

void EditorViewport::ReleaseViewportWindow()
{
    AssertOnThread(g_mainThread);

    GetThreadById(g_simThread)->GetScheduler().Enqueue([weakThis = MakeWeakRef(this)]()
        {
            if (Handle<EditorViewport> strongThis = weakThis.Lock())
            {
                strongThis->SetWindow(Handle<ApplicationWindow>::Null(), strongThis->m_isMainWindow);
            }
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);
}

void EditorViewport::SetWindow(const Handle<ApplicationWindow>& window, bool isMainWindow)
{
    AssertOnThread(g_simThread);

    m_windowSizeChangedHandler.Reset();

    if (m_window.IsValid() && m_window != window)
    {
        GetThreadById(g_mainThread)->GetScheduler().Enqueue([previousWindowWeak = m_window.ToWeak()]()
            {
                Handle<ApplicationWindow> previousWindow = previousWindowWeak.Lock();

                if (!previousWindow.IsValid() || !g_appContext.IsValid())
                {
                    return;
                }

                if (g_appContext->GetMainWindow() == previousWindow.Get())
                {
                    g_appContext->SetMainWindow(Handle<ApplicationWindow>::Null());
                }

                g_appContext->RemovePresentWindow(previousWindow.Get());
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }

    m_window = window;
    m_isMainWindow = isMainWindow;
    m_inputState = {};

    if (m_view.IsValid())
    {
        m_view->SetOutputWindowId(window.IsValid() ? window->Id().Value() : 0);
    }

    if (m_camera.IsValid() && !IsOrthographic())
    {
        EnumFlags<CameraFlags> cameraFlags = m_camera->GetCameraFlags();

        if (isMainWindow)
        {
            cameraFlags |= CameraFlags::MatchWindowSize;
        }
        else
        {
            cameraFlags &= ~CameraFlags::MatchWindowSize;
        }

        m_camera->SetCameraFlags(cameraFlags);
    }

    if (window.IsValid())
    {
        m_windowSizeChangedHandler = window->OnWindowSizeChanged.BindThreaded(
            window.Get(),
            [weakThis = MakeWeakRef(this)](Vec2i)
            {
                if (Handle<EditorViewport> strongThis = weakThis.Lock())
                {
                    strongThis->UpdateCameraDimensions();
                }
            },
            g_simThread);
    }

    UpdateCameraDimensions();
}

void EditorViewport::UpdateCameraDimensions()
{
    if (!m_camera.IsValid() || !m_window.IsValid())
    {
        return;
    }

    if (m_camera->GetCameraFlags() & CameraFlags::MatchWindowSize)
    {
        return;
    }

    m_camera->SetDimensions(MathUtil::Max(m_window->GetRenderSize(), Vec2i::One()));
}

void EditorViewport::OnAdded(EditorSubsystem* editorSubsystem)
{
    const Handle<Scene>& editorScene = editorSubsystem->GetEditorScene();
    Assert(editorScene.IsValid());

    editorScene->GetRoot()->AddChild(m_camera);

    if (IsOrthographic() && !m_hasBeenCentered)
    {
        EditorViewport* mainViewport = editorSubsystem->GetMainWindowViewport();

        EditorOrthoCameraController* orthoController = TryCast<EditorOrthoCameraController>(m_camera->GetCameraController().Get());

        if (mainViewport && mainViewport != this && !mainViewport->IsOrthographic() && orthoController)
        {
            orthoController->CenterOn(mainViewport->GetCamera()->GetWorldTranslation());

            m_hasBeenCentered = true;
        }
    }

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

            if (!ShowsWorldScene(scene.Get()))
            {
                continue;
            }

            m_view->AddScene(scene);
        }
    }

    world->AddView(m_view);
}

void EditorViewport::TryCenterOnMainViewport(EditorSubsystem* editorSubsystem)
{
    if (!IsOrthographic() || m_hasBeenCentered)
    {
        return;
    }

    EditorViewport* mainViewport = editorSubsystem->GetMainWindowViewport();

    EditorOrthoCameraController* orthoController = TryCast<EditorOrthoCameraController>(m_camera->GetCameraController().Get());

    if (mainViewport && mainViewport != this && !mainViewport->IsOrthographic() && orthoController)
    {
        orthoController->CenterOn(mainViewport->GetCamera()->GetWorldTranslation());

        m_hasBeenCentered = true;
    }
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

    if (m_isolatedScenes.Any() || !ShowsWorldScene(scene))
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
            if (!scene.IsValid() || !ShowsWorldScene(scene.Get()))
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

#pragma endregion EditorViewport

} // namespace Hyperion
