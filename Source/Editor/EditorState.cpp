/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorState.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorConfig.hpp>

#include <Framework/Game.hpp>

#include <Framework/Threads/SimThread.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetObject.hpp>

#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/EntityTag.hpp>

#include <EditorState.generated.inl>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

#ifdef HYP_EDITOR

static constexpr const char* RecentProjectsConfigKey = "Projects.Recent";
static constexpr const char* AlwaysOpenLastProjectConfigKey = "Projects.AlwaysOpenLastProject";
static constexpr uint32 MaxRecentProjects = 10;

static String NormalizeProjectFilepath(const String& projectFilepath)
{
    return projectFilepath.Trimmed().ReplaceAll("/", HYP_FILESYSTEM_SEPARATOR).ReplaceAll("\\", HYP_FILESYSTEM_SEPARATOR);
}

const Handle<EditorState>& EditorState::GetInstance()
{
    return g_editorState;
}

EditorState::EditorState()
    : m_alwaysOpenLastProject(false)
{
    LoadRecentProjects();
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
Camera* EditorState::GetEditorCamera() const
{
    AssertOnThread(g_simThread); // only callable on sim thread as we iterate nodes on the scene

    Handle<EditorSubsystem> ess = GetEditorSubsystem();
    if (!ess.IsValid())
    {
        return nullptr;
    }

    const Handle<Scene>& editorScene = ess->GetEditorScene();
    if (!editorScene.IsValid())
    {
        return nullptr;
    }

    Assert(editorScene->GetEntityManager().IsValid());

    if (!editorScene->GetEntityManager().IsValid())
    {
        return nullptr;
    }

    // Find a Camera entity with the EditorCamera tag
    for (auto [camera, _] : editorScene->GetEntityManager()->GetEntitySet<EntityType<Camera>, TagComponent<EntityTag::EditorCamera>>().GetScopedView(DataAccessFlags::ACCESS_READ))
    {
        return camera;
    }

    return nullptr;
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

int32 EditorState::GetNumRecentProjects() const
{
    Mutex::Guard guard(m_recentProjectsMutex);

    return int32(m_recentProjects.Size());
}

String EditorState::GetRecentProject(int32 index) const
{
    Mutex::Guard guard(m_recentProjectsMutex);

    if (index < 0 || index >= int32(m_recentProjects.Size()))
    {
        return String::empty;
    }

    return m_recentProjects[index];
}

void EditorState::AddRecentProject(const String& projectFilepath)
{
    const String normalizedFilepath = NormalizeProjectFilepath(projectFilepath);

    if (normalizedFilepath.Empty())
    {
        return;
    }

    {
        Mutex::Guard guard(m_recentProjectsMutex);

        if (m_recentProjects.Any() && m_recentProjects[0] == normalizedFilepath)
        {
            return;
        }

        m_recentProjects.Erase(normalizedFilepath);
        m_recentProjects.PushFront(normalizedFilepath);

        while (m_recentProjects.Size() > MaxRecentProjects)
        {
            m_recentProjects.PopBack();
        }

        SaveRecentProjects();
    }

    OnRecentProjectsChanged();
}

void EditorState::RemoveRecentProject(const String& projectFilepath)
{
    const String normalizedFilepath = NormalizeProjectFilepath(projectFilepath);

    {
        Mutex::Guard guard(m_recentProjectsMutex);

        if (!m_recentProjects.Contains(normalizedFilepath))
        {
            return;
        }

        m_recentProjects.Erase(normalizedFilepath);

        SaveRecentProjects();
    }

    OnRecentProjectsChanged();
}

void EditorState::ClearRecentProjects()
{
    {
        Mutex::Guard guard(m_recentProjectsMutex);

        if (m_recentProjects.Empty())
        {
            return;
        }

        m_recentProjects.Clear();

        SaveRecentProjects();
    }

    OnRecentProjectsChanged();
}

bool EditorState::GetAlwaysOpenLastProject() const
{
    Mutex::Guard guard(m_recentProjectsMutex);

    return m_alwaysOpenLastProject;
}

void EditorState::SetAlwaysOpenLastProject(bool alwaysOpenLastProject)
{
    Mutex::Guard guard(m_recentProjectsMutex);

    if (m_alwaysOpenLastProject == alwaysOpenLastProject)
    {
        return;
    }

    m_alwaysOpenLastProject = alwaysOpenLastProject;

    SaveRecentProjects();
}

String EditorState::GetStartupProjectPath() const
{
    Mutex::Guard guard(m_recentProjectsMutex);

    if (!m_alwaysOpenLastProject || m_recentProjects.Empty())
    {
        return String::empty;
    }

    if (!FilePath(m_recentProjects[0]).Exists())
    {
        return String::empty;
    }

    return m_recentProjects[0];
}

void EditorState::LoadRecentProjects()
{
    EditorConfig config;

    // A missing file just means nothing has been saved yet
    if (!config.Load())
    {
        return;
    }

    Mutex::Guard guard(m_recentProjectsMutex);

    m_recentProjects.Clear();

    if (const ConfigValue& recentProjectsValue = config.Get(RecentProjectsConfigKey); recentProjectsValue.IsArray())
    {
        for (const ConfigValue& projectValue : recentProjectsValue.AsArray())
        {
            if (!projectValue.IsString())
            {
                continue;
            }

            const String normalizedFilepath = NormalizeProjectFilepath(projectValue.ToString());

            if (normalizedFilepath.Empty() || m_recentProjects.Contains(normalizedFilepath))
            {
                continue;
            }

            m_recentProjects.PushBack(normalizedFilepath);

            if (m_recentProjects.Size() >= MaxRecentProjects)
            {
                break;
            }
        }
    }

    m_alwaysOpenLastProject = config.Get(AlwaysOpenLastProjectConfigKey).ToBool(false);
}

void EditorState::SaveRecentProjects() const
{
    // Caller holds m_recentProjectsMutex
    EditorConfig config;
    config.Load();

    JSON::JArray recentProjectsArray;

    for (const String& projectFilepath : m_recentProjects)
    {
        recentProjectsArray.PushBack(ConfigValue(projectFilepath));
    }

    config.Set(RecentProjectsConfigKey, ConfigValue(std::move(recentProjectsArray)));
    config.Set(AlwaysOpenLastProjectConfigKey, ConfigValue(m_alwaysOpenLastProject));

    if (!config.Save())
    {
        HYP_LOG(Editor, Warning, "Failed to save recent projects");
    }
}

#endif

} // namespace Hyperion
