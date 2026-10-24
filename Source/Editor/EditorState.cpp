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
#include <Editor/EditorViewport.hpp>

#include <Framework/Game.hpp>

#include <Framework/Threads/SimThread.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetObject.hpp>

#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/EntityTag.hpp>

#include <EditorState.generated.inl>

#include <chrono>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

#ifdef HYP_EDITOR

namespace {

static constexpr const char* RecentProjectsConfigKey = "Projects.Recent";
static constexpr const char* RecentProjectsOpenedConfigKey = "Projects.RecentOpened";
static constexpr const char* RecentProjectsPinnedConfigKey = "Projects.RecentPinned";
static constexpr const char* AlwaysOpenLastProjectConfigKey = "Projects.AlwaysOpenLastProject";
static constexpr uint32 MaxRecentProjects = 10;

struct ProjectLoadStageInfo
{
    ProjectLoadStage stage;
    ANSIStringView description;
    uint32 weight;
};

String NormalizeProjectFilepath(const String& projectFilepath)
{
    const String trimmed = projectFilepath.Trimmed();

    if (trimmed.Empty())
    {
        return String::empty;
    }

    return String(FilePath::Canonical(FilePath(trimmed)).Data()).ReplaceAll("/", HYP_FILESYSTEM_SEPARATOR).ReplaceAll("\\", HYP_FILESYSTEM_SEPARATOR);
}

bool IsSameProjectFilepath(const String& a, const String& b)
{
#ifdef HYP_WINDOWS
    return a.ToLower() == b.ToLower();
#else
    return a == b;
#endif
}

int64 GetUnixTimeSeconds()
{
    return int64(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

static constexpr ProjectLoadStageInfo ProjectLoadStages[] = {
    { ProjectLoadStage::CloseProject, "Closing current project", 1 },
    { ProjectLoadStage::ReadAssetRegistry, "Reading asset registry", 1 },
    { ProjectLoadStage::ReadProjectFile, "Reading project file", 3 },
    { ProjectLoadStage::LoadGameModule, "Loading game module", 2 },
    { ProjectLoadStage::RebuildGameModule, "This project's code needs to be recompiled before it can be opened. Please wait", 12 },
    { ProjectLoadStage::LoadScripts, "Loading scripts", 1 },
    { ProjectLoadStage::LoadAssetFolders, "Loading asset folders", 1 },
    { ProjectLoadStage::LoadWorld, "Loading world", 8 },
    { ProjectLoadStage::StartEditor, "Starting editor", 2 }
};

} // namespace

#pragma region EditorState

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

    if (EditorViewport* mainViewport = ess->GetMainWindowViewport(); mainViewport && !mainViewport->IsOrthographic())
    {
        return mainViewport->GetCamera().Get();
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

void EditorState::ReportProjectLoadStage(ProjectLoadStage stage)
{
    const ProjectLoadStageInfo* info = nullptr;
    uint32 weightBefore = 0;
    uint32 totalWeight = 0;

    for (const ProjectLoadStageInfo& it : ProjectLoadStages)
    {
        if (it.stage == stage)
        {
            info = &it;
            weightBefore = totalWeight;
        }

        totalWeight += it.weight;
    }

    Assert(info != nullptr);

    OnProjectLoadStatusChanged(String(info->description), weightBefore * 100 / totalWeight);
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

    return m_recentProjects[index].filepath;
}

int64 EditorState::GetRecentProjectLastOpened(int32 index) const
{
    Mutex::Guard guard(m_recentProjectsMutex);

    if (index < 0 || index >= int32(m_recentProjects.Size()))
    {
        return 0;
    }

    return m_recentProjects[index].lastOpened;
}

bool EditorState::IsRecentProjectPinned(int32 index) const
{
    Mutex::Guard guard(m_recentProjectsMutex);

    if (index < 0 || index >= int32(m_recentProjects.Size()))
    {
        return false;
    }

    return m_recentProjects[index].pinned;
}

void EditorState::SetRecentProjectPinned(const String& projectFilepath, bool pinned)
{
    const String normalizedFilepath = NormalizeProjectFilepath(projectFilepath);

    {
        Mutex::Guard guard(m_recentProjectsMutex);

        const int32 existingIndex = FindRecentProject(normalizedFilepath);

        if (existingIndex < 0 || m_recentProjects[existingIndex].pinned == pinned)
        {
            return;
        }

        RecentProjectEntry entry = std::move(m_recentProjects[existingIndex]);
        m_recentProjects.EraseAt(existingIndex);

        entry.pinned = pinned;
        InsertRecentProject(std::move(entry));

        TrimRecentProjects();

        SaveRecentProjects();
    }

    OnRecentProjectsChanged();
}

int32 EditorState::FindRecentProject(const String& normalizedFilepath) const
{
    for (size_t index = 0; index < m_recentProjects.Size(); index++)
    {
        if (IsSameProjectFilepath(m_recentProjects[index].filepath, normalizedFilepath))
        {
            return int32(index);
        }
    }

    return -1;
}

void EditorState::InsertRecentProject(RecentProjectEntry&& entry)
{
    size_t index = 0;

    for (; index < m_recentProjects.Size(); index++)
    {
        const RecentProjectEntry& other = m_recentProjects[index];

        if (entry.pinned != other.pinned ? entry.pinned : entry.lastOpened > other.lastOpened)
        {
            break;
        }
    }

    m_recentProjects.Insert(m_recentProjects.Begin() + index, std::move(entry));
}

void EditorState::TrimRecentProjects()
{
    size_t numUnpinned = 0;

    for (const RecentProjectEntry& entry : m_recentProjects)
    {
        numUnpinned += entry.pinned ? 0 : 1;
    }

    for (; numUnpinned > MaxRecentProjects; numUnpinned--)
    {
        m_recentProjects.PopBack();
    }
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

        const int32 existingIndex = FindRecentProject(normalizedFilepath);

        bool pinned = false;

        if (existingIndex >= 0)
        {
            pinned = m_recentProjects[existingIndex].pinned;

            m_recentProjects.EraseAt(existingIndex);
        }

        InsertRecentProject(RecentProjectEntry { normalizedFilepath, GetUnixTimeSeconds(), pinned });

        TrimRecentProjects();

        SaveRecentProjects();
    }

    OnRecentProjectsChanged();
}

void EditorState::RemoveRecentProject(const String& projectFilepath)
{
    const String normalizedFilepath = NormalizeProjectFilepath(projectFilepath);

    {
        Mutex::Guard guard(m_recentProjectsMutex);

        const int32 existingIndex = FindRecentProject(normalizedFilepath);

        if (existingIndex < 0)
        {
            return;
        }

        m_recentProjects.EraseAt(existingIndex);

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

    const RecentProjectEntry* lastEntry = &m_recentProjects[0];

    for (const RecentProjectEntry& entry : m_recentProjects)
    {
        if (entry.lastOpened > lastEntry->lastOpened)
        {
            lastEntry = &entry;
        }
    }

    if (!FilePath(lastEntry->filepath).Exists())
    {
        return String::empty;
    }

    return lastEntry->filepath;
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

    const ConfigValue& openedValue = config.Get(RecentProjectsOpenedConfigKey);
    const bool hasOpenedTimes = openedValue.IsArray();

    const ConfigValue& pinnedValue = config.Get(RecentProjectsPinnedConfigKey);
    const bool hasPinned = pinnedValue.IsArray();

    if (const ConfigValue& recentProjectsValue = config.Get(RecentProjectsConfigKey); recentProjectsValue.IsArray())
    {
        const JSON::JArray& projectValues = recentProjectsValue.AsArray();

        for (size_t index = 0; index < projectValues.Size(); index++)
        {
            const ConfigValue& projectValue = projectValues[index];

            if (!projectValue.IsString())
            {
                continue;
            }

            const String normalizedFilepath = NormalizeProjectFilepath(projectValue.ToString());

            if (normalizedFilepath.Empty() || FindRecentProject(normalizedFilepath) >= 0)
            {
                continue;
            }

            int64 lastOpened = 0;

            if (hasOpenedTimes && index < openedValue.AsArray().Size())
            {
                lastOpened = openedValue.AsArray()[index].ToInt64(0);
            }

            const bool pinned = hasPinned && index < pinnedValue.AsArray().Size() && pinnedValue.AsArray()[index].ToBool(false);

            InsertRecentProject(RecentProjectEntry { normalizedFilepath, lastOpened, pinned });
        }

        TrimRecentProjects();
    }

    m_alwaysOpenLastProject = config.Get(AlwaysOpenLastProjectConfigKey).ToBool(false);
}

void EditorState::SaveRecentProjects() const
{
    // Caller holds m_recentProjectsMutex
    EditorConfig config;
    config.Load();

    JSON::JArray recentProjectsArray;
    JSON::JArray openedArray;
    JSON::JArray pinnedArray;

    for (const RecentProjectEntry& entry : m_recentProjects)
    {
        recentProjectsArray.PushBack(ConfigValue(entry.filepath));
        openedArray.PushBack(ConfigValue(entry.lastOpened));
        pinnedArray.PushBack(ConfigValue(entry.pinned));
    }

    config.Set(RecentProjectsConfigKey, ConfigValue(std::move(recentProjectsArray)));
    config.Set(RecentProjectsOpenedConfigKey, ConfigValue(std::move(openedArray)));
    config.Set(RecentProjectsPinnedConfigKey, ConfigValue(std::move(pinnedArray)));
    config.Set(AlwaysOpenLastProjectConfigKey, ConfigValue(m_alwaysOpenLastProject));

    if (!config.Save())
    {
        HYP_LOG(Editor, Warning, "Failed to save recent projects");
    }
}

#pragma endregion EditorState

#endif

} // namespace Hyperion
