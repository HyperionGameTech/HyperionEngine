/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorProject.hpp>
#include <Editor/EditorActionStack.hpp>
#include <Editor/EditorTask.hpp>
#include <Editor/EditorState.hpp>

#include <Framework/Game.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetObject.hpp>
#include <Asset/BlobStorage.hpp>
#include <Asset/SerializationUtils.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Systems/ScriptSystem.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <Core/Utilities/DeferredScope.hpp>
#include <Core/Utilities/GlobalContext.hpp>

#include <Core/Reflection/Class.hpp>

#include <Core/IO/ByteReader.hpp>
#include <Core/IO/ByteWriter.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <HyperionEngine.hpp>

#include <EditorProject.generated.inl>

namespace Hyperion {

struct EditorProjectSaveContext
{
};

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

static const ANSIString s_defaultProjectName = "Project";

#pragma region EditorProject

static Name GetUniqueProjectName()
{
    const FilePath projectsDir = EngineGlobals::GetProjectsDirectory();

    ANSIString candidateName = s_defaultProjectName;
    uint32 counter = 0;

    while ((projectsDir / candidateName).Exists())
    {
        ++counter;
        candidateName = HYP_FORMAT("{}{}", s_defaultProjectName, counter);
    }

    return CreateNameFromDynamicString(candidateName);
}

/*! \brief Copy files not managed by the AssetRegistry (eg. script sources in Scripts/) from an old project
 *  directory to a new one. Manifests (.hmf), blob data (.blob) and project files (.hypproject) are skipped,
 *  as those are (re-)written as part of saving the project. */
static void CopyLooseProjectFiles(const FilePath& sourceDir, const FilePath& targetDir)
{
    if (!sourceDir.Exists() || !sourceDir.IsDirectory())
    {
        return;
    }

    for (const FilePath& subdir : sourceDir.GetSubdirectories())
    {
        const FilePath targetSubdir = targetDir / String(subdir.Basename());

        if (!targetSubdir.Exists() && !targetSubdir.MkDir())
        {
            HYP_LOG(Editor, Warning, "Failed to create directory '{}' while migrating project files", targetSubdir);

            continue;
        }

        CopyLooseProjectFiles(subdir, targetSubdir);
    }

    for (const FilePath& file : sourceDir.GetAllFilesInDirectory())
    {
        const String extension = file.GetExtension();

        if (extension == "hmf" || extension == "blob" || extension == "hypproject")
        {
            continue;
        }

        const FilePath targetFile = targetDir / String(file.Basename());

        if (targetFile.Exists())
        {
            // Never overwrite anything at the target location
            continue;
        }

        FileByteReader reader { file };

        if (reader.Eof())
        {
            HYP_LOG(Editor, Warning, "Failed to read file '{}' while migrating project files", file);

            continue;
        }

        const ByteBuffer fileContents = reader.Read();

        FileByteWriter writer { targetFile };

        if (!writer.IsOpen())
        {
            HYP_LOG(Editor, Warning, "Failed to open file '{}' for writing while migrating project files", targetFile);

            continue;
        }

        writer.Write(fileContents.Data(), fileContents.Size());
        writer.Close();
    }
}

EditorProject::EditorProject()
    : EditorProject(Handle<Game>::Null())
{
}

EditorProject::EditorProject(const Handle<Game>& gameInstance)
    : EditorProject(Name::Invalid(), gameInstance)
{
}

EditorProject::EditorProject(Name name, const Handle<Game>& gameInstance)
    : m_name(name),
      m_gameInstance(gameInstance),
      m_lastSavedTime(~0ull)
{
    m_actionStack = MakeHandle<EditorActionStack>(WeakHandleFromThis());
}

EditorProject::~EditorProject()
{
    // If the project was never saved, discard the temporary directory its content was kept in
    if (!m_tempDirectory.Empty())
    {
        if (!m_tempDirectory.RemoveRecursively())
        {
            HYP_LOG(Editor, Warning, "Failed to remove temporary project directory '{}'", m_tempDirectory);
        }

        m_tempDirectory = FilePath();
    }

    if (m_gameInstance)
    {
        m_gameInstance->Shutdown();

        EnqueueDeletion(std::move(m_gameInstance));
    }

    if (m_editWorld.IsValid())
    {
        m_editWorld->Shutdown();
        m_editWorld.Reset();
    }
}

void EditorProject::SetName(Name name)
{
    m_name = name;
}

const Handle<World>& EditorProject::GetWorld() const
{
    Assert(m_gameInstance != nullptr);

    const Handle<World>& gameWorld = m_gameInstance->GetWorld();
    if (gameWorld.IsValid())
    {
        return gameWorld;
    }

    return m_editWorld;
}

void EditorProject::SetGame(const Handle<Game>& gameInstance)
{
    if (m_gameInstance == gameInstance)
    {
        return;
    }

    m_gameInstance = gameInstance;
}

void EditorProject::AddScene(const Handle<Scene>& scene)
{
    if (!scene)
    {
        return;
    }

    World* world = m_editWorld;
    Assert(world != nullptr, "No edit World set on the project!");

    world->AddScene(scene, /* addToStreamingLayer */ true);
    world->MarkDirty();
}

void EditorProject::RemoveScene(Scene* scene)
{
    if (!scene)
    {
        return;
    }

    World* world = m_editWorld;
    Assert(world != nullptr, "No edit World set on the project!");

    world->RemoveScene(scene, /* removeFromStreamingLayer */ true);
    world->MarkDirty();
}

FilePath EditorProject::GetProjectsDirectory() const
{
    return EngineGlobals::GetProjectsDirectory();
}

bool EditorProject::IsSaved() const
{
    return uint64(m_lastSavedTime) != ~0ull;
}

void EditorProject::Close(bool shutdownWorld)
{
    if (m_gameInstance)
    {
        m_gameInstance->Shutdown(/* shutdownWorld */ shutdownWorld);
    }

    if (shutdownWorld && m_editWorld.IsValid())
    {
        m_editWorld->Shutdown();
        m_editWorld.Reset();
    }
}

Result EditorProject::Save()
{
    return SaveAs(m_filepath);
}

Result EditorProject::SaveAs(FilePath filepath)
{
    if (!m_editWorld.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "No World set on the project");
    }

    // Ensure we have a valid name, unique among existing project subdirs.
    if (!m_name.IsValid())
    {
        const FilePath projectsDir = EngineGlobals::GetProjectsDirectory();
        ANSIString candidateName = s_defaultProjectName;
        uint32 counter = 0;

        while ((projectsDir / candidateName).Exists())
        {
            ++counter;
            candidateName = HYP_FORMAT("{}{}", s_defaultProjectName, counter);
        }

        m_name = CreateNameFromDynamicString(candidateName);
    }

    FilePath dir;

    if (filepath.Empty())
    {
        dir = EngineGlobals::GetProjectsDirectory() / String(*m_name);
        filepath = dir / (String(*m_name) + ".hypproject");
    }
    else if (filepath.GetExtension().Any())
    {
        dir = filepath.BasePath();
    }
    else
    {
        dir = filepath / String(*m_name);
        filepath = dir / (String(*m_name) + ".hypproject");
    }

    if (!dir.Exists() && !dir.MkDir())
    {
        return HYP_MAKE_ERROR(Error, "Failed to create project directory '{}'", dir);
    }

    if (!dir.IsDirectory())
    {
        return HYP_MAKE_ERROR(Error, "Path '{}' is not a directory", dir);
    }

    AssetRegistry& registry = *m_gameInstance->GetAssetRegistry();

    GlobalContextScope assetRegistryContextScope { AssetRegistryContext { MakeStrongRef(&registry) } };

    const FilePath oldRootPath = registry.GetRootPath();
    const bool rootPathChanged = oldRootPath != dir;

    if (rootPathChanged)
    {
        // Blob data that has been unpaged only exists on disk at the current root (eg. the temporary
        // directory of an unsaved project). Page it back in while the root path still points there,
        // so that it is re-written to the new project directory on save.
        registry.MakeAllAssetsResident();
    }

    registry.SetRootPath(dir);

    // Content may already have been written while the project was unsaved (eg. imported assets persisted into the
    // temporary directory, which unmarked them as dirty). Re-mark everything dirty so all of it is written
    // to the final project directory.
    registry.MarkAllDirty();

    EditorTaskScope taskScope(
        TickableEditorTask::StaticClass(),
        "Saving project",
        "Registering assets",
        /* isForegroundTask */ true);

    // Put the world asset, root of all assets.
    registry.PutAssetsDeep(m_editWorld);

    GlobalContextScope saveContextScope { EditorProjectSaveContext {} };

    taskScope.GetEditorTask()->SetDescription("Saving project metadata");

    ToHMFOptions opts;

    String projectHmf;
    if (!ObjectToHMF(EditorProject::StaticClass(), BoxedValue(MakeStrongRef(this)), projectHmf, &opts))
    {
        return HYP_MAKE_ERROR(Error, "Failed to save project!");
    }

    FileByteWriter wri { filepath };
    HYP_DEFER({ wri.Close(); });

    wri.WriteString(projectHmf);
    wri.Close();

    m_filepath = filepath;

    taskScope.GetEditorTask()->SetDescription("Saving package data");

    registry.SaveDirtyAssets();

    // Move files not managed by the registry (eg. script sources in Scripts/) from the old location
    // (eg. the temporary directory of an unsaved project) to the new one.
    if (rootPathChanged)
    {
        CopyLooseProjectFiles(oldRootPath, dir);

        // Re-point script file watchers at the new project directory
        if (m_editWorld.IsValid())
        {
            if (ScriptSystem* scriptSystem = m_editWorld->GetSystem<ScriptSystem>())
            {
                scriptSystem->RefreshScriptSourceDirectories();
            }
        }
    }

    // Moved from temp; remove old dir
    if (!m_tempDirectory.Empty())
    {
        if (!m_tempDirectory.RemoveRecursively())
        {
            HYP_LOG(Editor, Warning, "Failed to remove temporary project directory '{}'", m_tempDirectory);
        }

        m_tempDirectory = FilePath();
    }

    OnProjectSaved(MakeStrongRef(this));

    m_lastSavedTime = Time::Now();

    return {};
}

bool EditorProject::IsDirty() const
{
    AssertDebug(m_gameInstance.IsValid());

    if (!m_gameInstance.IsValid())
    {
        return false;
    }

    const Handle<AssetRegistry>& registry = m_gameInstance->GetAssetRegistry();
    AssertDebug(registry.IsValid());

    if (!registry.IsValid())
    {
        return false;
    }

    return !IsSaved() || registry->HasDirtyAssets();
}

TResult<Handle<EditorProject>> EditorProject::Load(const FilePath& filepath)
{
    FilePath dir;
    FilePath projectFilepath;

    if (filepath.IsDirectory())
    {
        dir = filepath;

        for (const FilePath& file : filepath.GetAllFilesInDirectory())
        {
            if (file.EndsWith(".hypproject"))
            {
                projectFilepath = file;

                break;
            }
        }
    }
    else
    {
        dir = filepath.BasePath();
        projectFilepath = filepath;
    }

    if (!dir.Exists())
    {
        return HYP_MAKE_ERROR(Error, "Directory does not exist: {}", dir);
    }

    if (!projectFilepath.Exists())
    {
        return HYP_MAKE_ERROR(Error, "Project file does not exist: {}", projectFilepath);
    }

    const FilePath registryDir = dir;

    HYP_LOG(Editor, Info, "Loading registry assets from {}", registryDir);

    // create registry to load assets into
    Handle<AssetRegistry> registry = MakeHandle<AssetRegistry>(
        AssetRegistryId::Game, registryDir);

    registry->Initialize();

    // Load asset descs for the registry before attempting to load the assets themselves
    if (!registry->LoadAssetDescs())
    {
        return HYP_MAKE_ERROR(Error, "Failed to load asset descs for registry! Does the path exists at {} ?", registryDir);
    }

    Handle<EditorProject> project;

    { // load project and all its assets into the registry we just created.
        GlobalContextScope assetRegistryContextScope { AssetRegistryContext { registry } };

        {
            FileByteReader stream { projectFilepath };

            if (stream.Eof())
            {
                return HYP_MAKE_ERROR(Error, "Failed to open project file: {}", projectFilepath);
            }

            HMF::ParseResult parseResult = HMF::Parse(projectFilepath, stream);

            if (parseResult.HasError())
            {
                return parseResult.GetError();
            }

            BoxedValue boxed = std::move(parseResult.GetValue());

            if (!boxed.Is<Handle<EditorProject>>())
            {
                return HYP_MAKE_ERROR(Error, "Project file data is invalid!");
            }

            project = std::move(boxed.Get<Handle<EditorProject>>());
        }
    }

    // hand registry over to the game instance on the project
    Assert(project->m_gameInstance != nullptr);

    project->m_gameInstance->SetAssetRegistry(registry);

    // set transient properties
    project->m_lastSavedTime = projectFilepath.LastModifiedTimestamp();
    project->m_filepath = projectFilepath;

    return project;
}

Handle<EditorProject> EditorProject::CreateNew()
{
    Name projectName = GetUniqueProjectName();

    // Create Game instance
    Handle<Game> gameInstance = MakeHandle<Game>();

    // Create World
    Handle<World> world = MakeHandle<World>(NAME("MainWorld"), WorldFlags::Default);
    gameInstance->SetWorld(world);

    // Until the project is saved for the first time, keep all of its contentin a temporary
    // directory, so that no project directory is created under Projects/ before the project is explicitly saved.
    const Optional<FilePath> tempDirOpt = EngineGlobals::CreateTempDirectory("UnsavedProject");

    FilePath projectDir;

    if (tempDirOpt.HasValue())
    {
        projectDir = *tempDirOpt;
    }
    else
    {
        HYP_LOG(Editor, Warning, "Failed to create temp directory for project '{}'!", *projectName);
    }

    Handle<AssetRegistry> registry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, projectDir);
    registry->Initialize();

    gameInstance->SetAssetRegistry(registry);

    Handle<EditorProject> project = MakeHandle<EditorProject>(projectName, gameInstance);
    project->m_tempDirectory = tempDirOpt.GetOr(FilePath());
    project->SetEditWorld(world);

    return project;
}

#pragma endregion EditorProject

} // namespace Hyperion
