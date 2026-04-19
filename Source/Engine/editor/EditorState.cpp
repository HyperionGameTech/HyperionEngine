/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <editor/EditorState.hpp>
#include <editor/EditorProject.hpp>

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
    m_onProjectPackageChangedHandle.Reset();
    m_onAssetObjectAddedHandle.Reset();
}

void EditorState::Init()
{
    m_taskManager.OnTaskAdded.Bind([this]<class... Args>(Args&&... args) { OnTaskStarted(std::forward<Args>(args)...); }).Detach();
    m_taskManager.OnTaskRemoved.Bind([this]<class... Args>(Args&&... args) { OnTaskEnded(std::forward<Args>(args)...); }).Detach();
    m_taskManager.OnTaskProgressUpdated.Bind([this]<class... Args>(Args&&... args) { OnTaskProgressUpdated(std::forward<Args>(args)...); }).Detach();

#if 0
    // add newly imported assets to the current project's asset registry
    m_onAssetObjectAddedHandle = importsPackage->OnAssetObjectAdded
        .Bind([weakThis = WeakHandleFromThis()](const AssetDesc& assetDesc, bool isDirect, AssetPackage* parentPackage)
        {
            Handle<EditorState> editorState = weakThis.Lock();

            if (!editorState)
            {
                return;
            }

            Mutex::Guard guard(editorState->m_mutex);

            if (editorState->m_currentProject && editorState->m_currentProject->GetPackage().IsValid())
            {
                Handle<AssetObject> assetObject = parentPackage->GetAssetObject(assetDesc.name);
                AssertDebug(assetObject.IsValid());

                if (!assetObject.IsValid())
                {
                    return;
                }

                RegisterImportedAsset(editorState->m_currentProject, assetObject);
            }
        });

    Mutex::Guard guard(m_mutex);

    ImportAssetsOrSetCallback(m_currentProject);
#endif

    SetReady(true);
}

void EditorState::ImportAssetsOrSetCallback(const Handle<EditorProject>& project)
{
    m_onProjectPackageChangedHandle.Reset();

    if (!IsInitCalled())
    {
        // defer until init
        return;
    }

    if (!m_currentProject.IsValid())
    {
        return;
    }

    if (m_currentProject->GetPackage().IsValid())
    {
        RegisterPackageAssets(m_currentProject, GetImportsPackage());

        return;
    }

    m_onProjectPackageChangedHandle = m_currentProject->OnPackageCreated.Bind([weakProject = m_currentProject.ToWeak()](const Handle<AssetPackage>&)
        {
            Handle<EditorProject> project = weakProject.Lock();
            if (!project)
            {
                return;
            }

            RegisterPackageAssets(project, GetImportsPackage());
        });
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

        m_onProjectPackageChangedHandle.Reset();

        m_currentProject = project;

        if (project.IsValid())
        {
            HYP_LOG(Editor, Verbose, "Current project set to '{}'", *project->GetName());
        }
        else
        {
            HYP_LOG(Editor, Verbose, "Current project cleared");
        }

        ImportAssetsOrSetCallback(m_currentProject);
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
