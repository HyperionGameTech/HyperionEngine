/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorGame.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorProject.hpp>

#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Node.hpp>

#include <UI/UISubsystem.hpp>
#include <UI/Overlays/DeviceDetailsOverlay.hpp>
#include <UI/Overlays/StatsOverlay.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <EditorGame.generated.inl>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

EditorGame::EditorGame()
    : Game(),
      m_editorSubsystem(nullptr)
{
}

EditorGame::~EditorGame()
{
}

void EditorGame::OnLaunch()
{
    HYP_LOG(Editor, Debug, "EditorGame Launched");

    Assert(GetWorld() != nullptr);

    UISubsystem* uiSubsystem = GetWorld()->GetSubsystem<UISubsystem>();

    if (!uiSubsystem)
    {
        uiSubsystem = GetWorld()->AddSubsystem<UISubsystem>().Get();
    }

    Assert(uiSubsystem != nullptr);

    uiSubsystem->AddDebugOverlay(MakeHandle<DeviceDetailsOverlay>());
    uiSubsystem->AddDebugOverlay(MakeHandle<StatsOverlay>());

    m_editorSubsystem = GetWorld()->AddSubsystem<EditorSubsystem>().Get();

    m_onFocusedNodeChanged = m_editorSubsystem->OnFocusedNodeChanged.Bind(
        this,
        [this](Handle<Node> newNode, Handle<Node> prevNode, bool shouldSelectInOutline)
        {
            HandleFocusedNodeChanged(newNode, prevNode, shouldSelectInOutline);
        });

    m_onProjectOpened = m_editorSubsystem->OnProjectOpened.Bind(
        this,
        [this](Handle<EditorProject> project)
        {
            HandleProjectOpened(project);
        });

    m_onProjectClosing = m_editorSubsystem->OnProjectClosing.Bind(
        this,
        [this](Handle<EditorProject> project)
        {
            HandleProjectClosing(project);
        });

    if (const Handle<EditorProject>& project = m_editorSubsystem->GetCurrentProject())
    {
        HandleProjectOpened(project);
    }
}

Handle<World> EditorGame::LoadWorld(Name unusedName)
{
    Handle<World> world = MakeHandle<World>(NAME("EditorWorld"), (WorldFlags::Default | WorldFlags::Editor) & ~WorldFlags::IsReplicated);
    world->SetIsTransient(true); // Editor world should not be saved or loaded from disk.

    return world;
}

void EditorGame::BeforeShutdown()
{
    HYP_LOG(Editor, Debug, "EditorGame BeforeShutdown");

    m_onProjectOpened.Reset();
    m_onProjectClosing.Reset();

    m_onFocusedNodeChanged.Reset();
    m_onRootNodeChanged.Reset();
    m_onChildAdded.Reset();
    m_onChildRemoved.Reset();
    m_onActiveSceneChanged.Reset();

    m_editorSubsystem = nullptr;
}

void EditorGame::OnUpdate(float delta)
{
    // @NOTE Intentionally not calling the base OnUpdate() to override default behaviour.
}

void EditorGame::HandleFocusedNodeChanged(const Handle<Node>& newNode, const Handle<Node>& prevNode, bool shouldSelectInOutline)
{
    HYP_LOG(Editor, Debug, "Focused node changed from {} to {}, shouldSelectInOutline: {}",
        prevNode.IsValid() ? prevNode->GetName().LookupString() : "null",
        newNode.IsValid() ? newNode->GetName().LookupString() : "null",
        shouldSelectInOutline);
}

void EditorGame::SetChildAddRemovedHandlers(const Handle<Node>& node)
{
    m_onChildAdded.Reset();
    m_onChildRemoved.Reset();

    if (!node.IsValid())
    {
        return;
    }

    m_onChildAdded = Node::OnChildAdded.Bind(
        this,
        [](Node* child, bool isDirect)
        {
            HYP_LOG(Editor, Debug, "Child node '{}' added (isDirect: {})", child->GetName().LookupString(), isDirect);
        });

    m_onChildRemoved = Node::OnChildRemoved.Bind(
        this,
        [](Node* child, bool isDirect)
        {
            HYP_LOG(Editor, Debug, "Child node '{}' removed (isDirect: {})", child->GetName().LookupString(), isDirect);
        });
}

void EditorGame::SetRootNodeChangedHandler(const Handle<Scene>& scene)
{
    m_onRootNodeChanged.Reset();

    if (!scene.IsValid())
    {
        return;
    }

    m_onRootNodeChanged = Scene::OnRootNodeChanged.Bind(
        this,
        [this](Handle<Node> newRoot, Handle<Node> oldRoot)
        {
            HYP_LOG(Editor, Info, "Root node changed from {} to {}",
                oldRoot.IsValid() ? oldRoot->GetName().LookupString() : "null",
                newRoot.IsValid() ? newRoot->GetName().LookupString() : "null");

            SetChildAddRemovedHandlers(newRoot);
        });

    SetChildAddRemovedHandlers(scene->GetRoot());
}

void EditorGame::HandleProjectOpened(const Handle<EditorProject>& project)
{
    Assert(project.IsValid() && project->GetGame().IsValid());
    Assert(project->GetGame()->GetAssetRegistry().IsValid());

    SetAssetRegistry(project->GetGame()->GetAssetRegistry());

    HYP_LOG(Editor, Info, "Project opened: {}", project->GetName());

    Handle<Scene> activeScene = m_editorSubsystem->GetActiveScene();

    if (activeScene.IsValid())
    {
        SetRootNodeChangedHandler(activeScene);
    }

    m_onActiveSceneChanged.Reset();
    m_onActiveSceneChanged = m_editorSubsystem->OnActiveSceneChanged.Bind(
        this,
        [this](Handle<Scene> scene)
        {
            HYP_LOG(Editor, Info, "Active scene changed to: {}", scene.IsValid() ? scene->GetName() : Name::Invalid());

            SetRootNodeChangedHandler(scene);
        });
}

void EditorGame::HandleProjectClosing(const Handle<EditorProject>& project)
{
    HYP_LOG(Editor, Info, "Project closing: {}", project.IsValid() ? project->GetName() : Name::Invalid());

    m_onRootNodeChanged.Reset();
    m_onChildAdded.Reset();
    m_onChildRemoved.Reset();
    m_onActiveSceneChanged.Reset();
}

} // namespace Hyperion
