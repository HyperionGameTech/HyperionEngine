/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <EditorPch.hpp>

#include <editor/EditorState.hpp>
#include <editor/EditorProject.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/AssetObject.hpp>

#include <EditorState.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

#ifdef HYP_EDITOR

static constexpr SizeType BlockSize = (16 * 1024 * 1024);

Pool s_editorPickCachePool { BlockSize };
HYP_API Pool* g_editorPickCachePool = &s_editorPickCachePool;

static Handle<AssetPackage> GetImportsPackage()
{
    return AssetManager::GetInstance()->GetAssetRegistry()->GetPackageFromPath("$Import", true);
}

static void RegisterImportedAsset(const Handle<EditorProject>& project, const Handle<AssetObject>& assetObject)
{
    Assert(project.IsValid() && assetObject.IsValid());

    Handle<AssetPackage> projectPackage = project->GetPackage();
    Assert(projectPackage.IsValid());

    Handle<AssetRegistry> registry = projectPackage->GetRegistry().Lock();
    Assert(registry.IsValid());

    Array<Name> subpackageNames;

    Handle<AssetPackage> previousPackage = assetObject->GetPackage();
    Assert(previousPackage.IsValid());

    // keep a copy around in case removing it from the package invalidates the reference
    Handle<AssetObject> assetObjectCopy = assetObject;

    // remove the asset from its current package
    if (Result removeResult = previousPackage->RemoveAssetObject(assetObject); removeResult.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to remove asset object '{}' from package '{}': {}", assetObject->GetName(), previousPackage->GetName(), removeResult.GetError().GetMessage());
    }

    Handle<AssetPackage> currentPackage = previousPackage;

    while (currentPackage.IsValid() && currentPackage->GetName() != "$Import"_sh)
    {
        subpackageNames.PushBack(currentPackage->GetName());
        currentPackage = currentPackage->GetParentPackage().Lock();
    }

    subpackageNames.Reverse();

    String newPath = projectPackage->BuildPackagePath() + '/' + String::Join(subpackageNames, '/', &Name::LookupString);
    HYP_LOG(Editor, Debug, "Adding imported asset '{}' to project package '{}'", *assetObject->GetName(), newPath);

    if (Result registerAssetResult = registry->RegisterAsset(newPath, assetObject); registerAssetResult.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to register imported asset '{}' at path '{}': {}", assetObject->GetName(), newPath, registerAssetResult.GetError().GetMessage());
    }
}

static void RegisterPackageAssets(const Handle<EditorProject>& project, const Handle<AssetPackage>& package)
{
    Assert(project.IsValid() && package.IsValid());

    package->ForEachAssetObject([&](const Handle<AssetObject>& assetObject)
        {
            RegisterImportedAsset(project, assetObject);

            return IterationResult::CONTINUE;
        });

    // recursively register assets in subpackages
    package->ForEachSubpackage([&](const Handle<AssetPackage>& subpackage)
        {
            RegisterPackageAssets(project, subpackage);

            return IterationResult::CONTINUE;
        });
}

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
    Handle<AssetPackage> importsPackage = GetImportsPackage();
    Assert(importsPackage.IsValid());

    m_taskManager.OnTaskAdded.Bind([this]<class... Args>(Args&&... args) { OnTaskStarted(std::forward<Args>(args)...); }).Detach();
    m_taskManager.OnTaskRemoved.Bind([this]<class... Args>(Args&&... args) { OnTaskEnded(std::forward<Args>(args)...); }).Detach();
    m_taskManager.OnTaskProgressUpdated.Bind([this]<class... Args>(Args&&... args) { OnTaskProgressUpdated(std::forward<Args>(args)...); }).Detach();

    // add newly imported assets to the current project's asset registry
    m_onAssetObjectAddedHandle = importsPackage->OnAssetObjectAdded.Bind([weakThis = WeakHandleFromThis()](Handle<AssetObject> assetObject, bool isDirect)
        {
            Handle<EditorState> editorState = weakThis.Lock();

            if (!editorState)
            {
                return;
            }

            Mutex::Guard guard(editorState->m_mutex);

            if (editorState->m_currentProject && editorState->m_currentProject->GetPackage().IsValid())
            {
                RegisterImportedAsset(editorState->m_currentProject, assetObject);
            }
        });

    Mutex::Guard guard(m_mutex);

    ImportAssetsOrSetCallback(m_currentProject);

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
            HYP_LOG(Editor, Info, "Current project set to '{}'", *project->GetName());
        }
        else
        {
            HYP_LOG(Editor, Info, "Current project cleared");
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

    AssertOnThread(g_simThread);

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
