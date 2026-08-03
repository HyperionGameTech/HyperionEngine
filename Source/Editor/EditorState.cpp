/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorState.hpp>
#include <Editor/EditorProject.hpp>

#include <Framework/Game.hpp>

#include <Framework/Threads/SimThread.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetObject.hpp>

#include <EditorState.generated.inl>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

#ifdef HYP_EDITOR

const Handle<EditorState>& EditorState::GetInstance()
{
    return g_editorState;
}

EditorState::EditorState()
{
}

EditorState::~EditorState()
{
    if (m_currentProject.IsValid() && m_currentProject->GetGame().IsValid())
    {
        PopAssetRegistry(m_currentProject->GetGame()->GetAssetRegistry());
    }
}

void EditorState::Initialize()
{
    m_taskManager.OnTaskAdded.Bind([this]<class... Args>(Args&&... args) { OnTaskStarted(std::forward<Args>(args)...); }).Detach();
    m_taskManager.OnTaskRemoved.Bind([this]<class... Args>(Args&&... args) { OnTaskEnded(std::forward<Args>(args)...); }).Detach();
    m_taskManager.OnTaskProgressUpdated.Bind([this]<class... Args>(Args&&... args) { OnTaskProgressUpdated(std::forward<Args>(args)...); }).Detach();
}

Handle<EditorSubsystem> EditorState::GetEditorSubsystem() const
{
    Mutex::Guard guard(m_mutex);

    if (!m_currentProject.IsValid())
    {
        return Handle<EditorSubsystem>::Null();
    }

    return m_currentProject->GetEditorSubsystem().Lock();
}

Handle<EditorProject> EditorState::GetCurrentProject() const
{
    Mutex::Guard guard(m_mutex);

    return m_currentProject;
}

void EditorState::SetCurrentProject(const Handle<EditorProject>& project, bool isSimulationStateChange)
{
    {
        Mutex::Guard guard(m_mutex);

        if (m_currentProject == project)
        {
            return;
        }

        if (m_currentProject.IsValid() && m_currentProject->GetGame().IsValid())
        {
            PopAssetRegistry(m_currentProject->GetGame()->GetAssetRegistry());
        }

        m_currentProject = project;

        if (project.IsValid())
        {
            HYP_LOG(Editor, Verbose, "Current project set to '{}'", *project->GetName());

            Game* game = m_currentProject->GetGame();
            Assert(game != nullptr);

            PushAssetRegistry(game->GetAssetRegistry());
        }
        else
        {
            HYP_LOG(Editor, Verbose, "Current project cleared");
        }
    }

    OnCurrentProjectChanged(project, isSimulationStateChange);
}

void EditorState::AddTask(const Handle<EditorTaskBase>& task)
{
    if (!task)
    {
        return;
    }

    m_taskManager.AddTask(task);
}

void EditorState::Update(float delta)
{
    HYP_SCOPE;

    m_pickCache.Update(delta);

    m_taskManager.Tick();
}

Array<Handle<Node>> EditorState::GetClipboardNodes() const
{
    AssertOnThread(g_simThread);

    return m_clipboardNodes;
}

void EditorState::SetClipboardNodes(const Array<Handle<Node>>& nodes)
{
    AssertOnThread(g_simThread);

    m_clipboardNodes = nodes;

    OnClipboardChanged();
}

#endif

} // namespace Hyperion
