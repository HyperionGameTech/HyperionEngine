/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <editor/EditorState.hpp>
#include <editor/EditorProject.hpp>

#include <engine/Game.hpp>

#include <engine/threads/SimThread.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/AssetObject.hpp>

#include <EditorState.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

#if HYP_EDITOR

static constexpr size_t BlockSize = (16 * 1024 * 1024);

Pool s_editorPickCachePool { BlockSize };
HYP_API Pool* g_editorPickCachePool = &s_editorPickCachePool;

const Handle<EditorState>& EditorState::GetInstance()
{
    return g_editorState;
}

EditorState::EditorState()
{
}

EditorState::~EditorState()
{
    if (m_currentProject.IsValid())
    {
        PopCurrentAssetRegistry();
    }
}

void EditorState::Init()
{
    m_taskManager.OnTaskAdded.Bind([this]<class... Args>(Args&&... args) { OnTaskStarted(std::forward<Args>(args)...); }).Detach();
    m_taskManager.OnTaskRemoved.Bind([this]<class... Args>(Args&&... args) { OnTaskEnded(std::forward<Args>(args)...); }).Detach();
    m_taskManager.OnTaskProgressUpdated.Bind([this]<class... Args>(Args&&... args) { OnTaskProgressUpdated(std::forward<Args>(args)...); }).Detach();


    SetReady(true);
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

void EditorState::SetCurrentProject(const Handle<EditorProject>& project)
{
    {
        Mutex::Guard guard(m_mutex);

        if (m_currentProject == project)
        {
            return;
        }

        if (m_currentProject.IsValid())
        {
            PopCurrentAssetRegistry();
        }

        m_currentProject = project;

        if (project.IsValid())
        {
            HYP_LOG(Editor, Verbose, "Current project set to '{}'", *project->GetName());

            Game* game = m_currentProject->GetGame();
            Assert(game != nullptr);

            PushCurrentAssetRegistry(game->GetAssetRegistry());
        }
        else
        {
            HYP_LOG(Editor, Verbose, "Current project cleared");
        }
    }

    OnCurrentProjectChanged(project);
}

void EditorState::AddTask(const Handle<EditorTaskBase>& task)
{
    HYP_SCOPE;

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

#endif

} // namespace Hyperion
