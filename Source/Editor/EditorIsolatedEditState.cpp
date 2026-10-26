/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorIsolatedEditState.hpp>
#include <Editor/Csg/EditorCsgState.hpp>
#include <Editor/Terrain/EditorTerrainState.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorViewport.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorActionStack.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Node.hpp>
#include <Scene/Light/Light.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Core/Math/MathUtil.hpp>

#include <EditorIsolatedEditState.generated.inl>

namespace Hyperion {

EditorIsolatedEditState::EditorIsolatedEditState() = default;

EditorIsolatedEditState::~EditorIsolatedEditState() = default;

void EditorIsolatedEditState::Initialize(EditorSubsystem* subsystem)
{
    m_subsystem = subsystem;
    m_subsystem->RegisterIsolatedEditState(HandleFromThis());
}

void EditorIsolatedEditState::Shutdown()
{
    if (!m_subsystem)
    {
        return;
    }

    Exit(/* apply */ false);

    m_subsystem->UnregisterIsolatedEditState(this);
    m_subsystem = nullptr;
}

bool EditorIsolatedEditState::CanEnter() const
{
    return m_subsystem != nullptr
        && m_subsystem->GetCurrentProject().IsValid()
        && m_subsystem->GetProjectWorld().IsValid()
        && !m_subsystem->IsSimulating();
}

bool EditorIsolatedEditState::BeginEnter()
{
    if (!CanEnter())
    {
        return false;
    }

    if (!m_subsystem->ExitIsolatedEditStates(/* apply */ true))
    {
        return false;
    }

    m_subsystem->ExitMeshEditMode(/* saveEdits */ true);
    m_subsystem->GetCsgState()->Exit(/* saveEdits */ true);
    m_subsystem->GetTerrainState()->SetEnabled(false);
    m_subsystem->DisableSurfacePainters();

    m_previousActiveScene = m_subsystem->GetActiveScene();

    m_subsystem->SetFocusedNode(Handle<Node>::Null());
    m_subsystem->ClearSelection();

    return true;
}

void EditorIsolatedEditState::FinishEnter()
{
    ResetActionStack();
    SetDirty(false);

    m_subsystem->UpdateViewportIsolation();

    FrameCamera();

    m_subsystem->SetActiveScene(m_editScene);

    OnActiveChanged(true);
}

bool EditorIsolatedEditState::Exit(bool apply)
{
    AssertOnThread(g_simThread);

    if (!IsActive())
    {
        return true;
    }

    if (apply && m_dirty && !Apply())
    {
        return false;
    }

    m_subsystem->SetFocusedNode(Handle<Node>::Null());
    m_subsystem->ClearSelection();

    Handle<Scene> previousActiveScene = m_previousActiveScene.Lock();

    if (!previousActiveScene.IsValid())
    {
        for (const Handle<Scene>& scene : m_subsystem->GetProjectWorld()->GetScenes())
        {
            if (scene.IsValid() && (scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND)
            {
                previousActiveScene = scene;

                break;
            }
        }
    }

    ClearTarget();
    m_previousActiveScene.Reset();

    m_actionStackStateHandler.Reset();
    m_actionStack.Reset();

    m_subsystem->UpdateViewportIsolation();

    RestoreCamera();

    m_subsystem->SetActiveScene(previousActiveScene);
    m_subsystem->OnActiveActionStackChanged();

    // removed only once the active scene has moved off it
    DestroyScenes();

    SetDirty(false);

    OnActiveChanged(false);

    return true;
}

bool EditorIsolatedEditState::IsDirty() const
{
    return m_dirty;
}

const Handle<Scene>& EditorIsolatedEditState::GetEditScene() const
{
    return m_editScene;
}

bool EditorIsolatedEditState::IsEditScene(const Handle<Scene>& scene) const
{
    return scene.IsValid() && scene == m_editScene;
}

EditorActionStack* EditorIsolatedEditState::GetActionStack() const
{
    return IsActive() ? m_actionStack.Get() : nullptr;
}

Array<Handle<Scene>> EditorIsolatedEditState::GetIsolatedScenes() const
{
    if (!IsActive())
    {
        return {};
    }

    return { m_editScene, m_stageScene };
}

void EditorIsolatedEditState::CreateScenes(Name editSceneName)
{
    const Handle<World>& world = m_subsystem->GetProjectWorld();
    Assert(world.IsValid());

    m_editScene = MakeHandle<Scene>(editSceneName, SceneFlags::EDITOR | SceneFlags::HAS_OCTREE);
    m_editScene->SetIsTransient(true);
    InitObject(m_editScene);

    world->AddScene(m_editScene, /* addToStreamingLayer */ false);

    m_stageScene = MakeHandle<Scene>(NAME_FMT("{}_Stage", editSceneName), SceneFlags::EDITOR);
    m_stageScene->SetIsTransient(true);
    InitObject(m_stageScene);

    Handle<DirectionalLight> keyLight = MakeHandle<DirectionalLight>(
        Vec3f(-0.4f, 0.75f, 0.5f).Normalized(),
        Color(1.0f, 0.98f, 0.94f, 1.0f),
        4.0f);

    keyLight->SetName(NAME("EditStageKeyLight"));
    keyLight->SetLightFlags(LightFlags::None);
    InitObject(keyLight);
    m_stageScene->GetRoot()->AddChild(keyLight);

    Handle<DirectionalLight> fillLight = MakeHandle<DirectionalLight>(
        Vec3f(0.5f, 0.2f, -0.8f).Normalized(),
        Color(0.6f, 0.7f, 0.85f, 1.0f),
        1.5f);

    fillLight->SetName(NAME("EditStageFillLight"));
    fillLight->SetLightFlags(LightFlags::None);
    InitObject(fillLight);
    m_stageScene->GetRoot()->AddChild(fillLight);

    world->AddScene(m_stageScene, /* addToStreamingLayer */ false);
}

void EditorIsolatedEditState::DestroyScenes()
{
    const Handle<World>& world = m_subsystem->GetProjectWorld();

    for (Handle<Scene>* scene : { &m_editScene, &m_stageScene })
    {
        if (scene->IsValid() && world.IsValid())
        {
            world->RemoveScene(scene->Get());
        }

        scene->Reset();
    }
}

void EditorIsolatedEditState::FrameCamera()
{
    EditorViewport* viewport = m_subsystem->GetActiveViewport();

    if (!viewport || !viewport->GetCamera().IsValid())
    {
        return;
    }

    const Handle<Camera>& camera = viewport->GetCamera();

    m_savedCameraTranslation = camera->GetWorldTranslation();
    m_savedCameraDirection = camera->GetDirection();
    m_hasSavedCamera = true;

    const BoundingBox bounds = m_editScene->GetRoot()->GetWorldBounds();

    const Vec3f center = bounds.IsValid() ? bounds.GetCenter() : Vec3f::Zero();
    const float radius = bounds.IsValid() ? MathUtil::Max(bounds.GetRadius(), 0.5f) : 1.0f;

    const Vec3f viewDirection = Vec3f(-1.0f, -0.6f, -1.0f).Normalized();

    camera->SetWorldTranslation(center - viewDirection * (radius * 2.5f));
    camera->SetDirection(viewDirection);
}

void EditorIsolatedEditState::RestoreCamera()
{
    if (!m_hasSavedCamera)
    {
        return;
    }

    m_hasSavedCamera = false;

    EditorViewport* viewport = m_subsystem->GetActiveViewport();

    if (!viewport || !viewport->GetCamera().IsValid())
    {
        return;
    }

    viewport->GetCamera()->SetWorldTranslation(m_savedCameraTranslation);
    viewport->GetCamera()->SetDirection(m_savedCameraDirection);
}

void EditorIsolatedEditState::ResetActionStack()
{
    m_actionStackStateHandler.Reset();

    m_actionStack = MakeHandle<EditorActionStack>(m_subsystem->GetCurrentProject().ToWeak());

    m_actionStackStateHandler = m_actionStack->OnStateChange.Bind(
        this,
        [this](EnumFlags<EditorActionStackState>, int)
        {
            SetDirty(m_actionStack.IsValid() && m_actionStack->CanUndo());
        });

    m_subsystem->OnActiveActionStackChanged();
}

void EditorIsolatedEditState::SetDirty(bool dirty)
{
    if (m_dirty == dirty)
    {
        return;
    }

    m_dirty = dirty;

    OnDirtyChanged(dirty);
}

} // namespace Hyperion
