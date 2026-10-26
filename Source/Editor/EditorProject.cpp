/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorProject.hpp>
#include <Editor/EditorActionStack.hpp>
#include <Editor/EditorAssetFolders.hpp>
#include <Editor/EditorTask.hpp>
#include <Editor/EditorState.hpp>
#include <Editor/EditorNativeModule.hpp>
#include <Editor/EditorProjectLayout.hpp>
#include <Editor/EditorGameModuleCommon.hpp>

#include <Core/Core.hpp>

#include <Framework/Game.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetObject.hpp>
#include <Asset/BlobStorage.hpp>
#include <Asset/SerializationUtils.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Swatch.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Systems/ScriptSystem.hpp>

#include <DotNET/Assembly.hpp>
#include <Scene/Systems/SwatchOverrideSystem.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <Core/Utilities/DeferredScope.hpp>
#include <Core/Utilities/GlobalContext.hpp>
#include <Core/Utilities/StringUtil.hpp>

#include <Core/Reflection/Class.hpp>

#include <Core/IO/ByteReader.hpp>
#include <Core/IO/ByteWriter.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <HyperionEngine.hpp>

#include <Plugins/PluginManager.hpp>

#include <EditorProject.generated.inl>

namespace Hyperion {

struct EditorProjectSaveContext {};

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

    return Name(candidateName);
}

static bool CopyLooseProjectFiles(const FilePath& sourceDir, const FilePath& targetDir)
{
    if (!sourceDir.Exists() || !sourceDir.IsDirectory())
    {
        return true;
    }

    bool allCopied = true;

    for (const FilePath& subdir : sourceDir.GetSubdirectories())
    {
        const FilePath targetSubdir = targetDir / String(subdir.Basename());

        if (!targetSubdir.Exists() && !targetSubdir.MkDir())
        {
            HYP_LOG(Editor, Warning, "Failed to create directory '{}' while migrating project files", targetSubdir);

            allCopied = false;

            continue;
        }

        if (!CopyLooseProjectFiles(subdir, targetSubdir))
        {
            allCopied = false;
        }
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

        // empty files read as Eof too, those are fine to copy
        if (reader.Eof() && file.FileSize() != 0)
        {
            HYP_LOG(Editor, Warning, "Failed to read file '{}' while migrating project files", file);

            allCopied = false;

            continue;
        }

        const ByteBuffer fileContents = reader.Read();

        FileByteWriter writer { targetFile };

        if (!writer.IsOpen())
        {
            HYP_LOG(Editor, Warning, "Failed to open file '{}' for writing while migrating project files", targetFile);

            allCopied = false;

            continue;
        }

        writer.Write(fileContents.Data(), fileContents.Size());
        writer.Close();
    }

    return allCopied;
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
      m_lastSavedTime(~0ull),
      m_gameInstance(gameInstance)
{
    m_actionStack = MakeHandle<EditorActionStack>(WeakHandleFromThis());
    m_assetFolders = MakeHandle<EditorAssetFolders>();

    // replaced by the project file's own settings when it has them
    m_packagingSettings = PackagingSettings::CreateDefault();
    m_projectSettings = MakeHandle<EditorProjectSettings>();
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

Name EditorProject::GetEditWorldName() const
{
    if (m_editWorld.IsValid())
    {
        return m_editWorld->GetName();
    }

    return m_editWorldName.IsValid() ? m_editWorldName : Game::s_nameMainWorld;
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

FilePath EditorProject::GetProjectDirectory() const
{
    if (m_filepath.Any())
    {
        return m_filepath.BasePath();
    }

    return m_tempDirectory;
}

FilePath EditorProject::GetContentDirectory() const
{
    const FilePath projectDirectory = GetProjectDirectory();

    if (projectDirectory.Empty() || m_contentSubfolder.Empty())
    {
        return projectDirectory;
    }

    return projectDirectory / m_contentSubfolder;
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

    // follows the World asset if it was renamed
    m_editWorldName = m_editWorld->GetName();

    ///Save EditorCamera position/direction
    Camera* editorCamera = g_editorState->GetEditorCamera();

    if (editorCamera)
    {
        ///set these -- they get serialized to disk
        m_editorCameraPosition = editorCamera->GetWorldTranslation();
        m_editorCameraDirection = editorCamera->GetDirection();
    }
    else
    {
        HYP_LOG(Editor, Warning, "No EditorCamera found when saving scene, next load will have Camera at 0,0,0");
    }

    ///Ensure we have a valid name, unique among existing project subdirs.
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

        m_name = Name(candidateName);
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

        // project is named after its file, so saving under a new file name renames it
        m_name = CreateNameFromDynamicString(ANSIString(FilePath(filepath.Basename()).StripExtension()));
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

    const FilePath contentDir = m_contentSubfolder.Any() ? dir / m_contentSubfolder : dir;

    if (!contentDir.MkDir())
    {
        return HYP_MAKE_ERROR(Error, "Failed to create project content directory '{}'", contentDir);
    }

    const FilePath oldProjectDir = GetProjectDirectory();
    const bool projectDirChanged = oldProjectDir != dir;

    AssetRegistry& registry = *m_gameInstance->GetAssetRegistry();

    GlobalContextScope assetRegistryContextScope { AssetRegistryContext { MakeStrongRef(&registry) } };

    const FilePath oldRootPath = registry.GetRootPath();
    const bool rootPathChanged = oldRootPath != contentDir;

    if (rootPathChanged)
    {
        // load all into memory before we re-assign paths
        registry.MakeAllAssetsResident();

        registry.SetRootPath(contentDir);
        registry.MarkAllDirty();
    }

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

    if (!wri.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Failed to open project file '{}' for writing", filepath);
    }

    wri.WriteString(projectHmf);
    wri.Close();

    // renamed in place, don't leave two project files in the same directory
    if (!projectDirChanged && m_filepath.Any() && String(m_filepath.Basename()) != String(filepath.Basename()) && m_filepath.Exists())
    {
        if (!m_filepath.Remove())
        {
            HYP_LOG(Editor, Warning, "Failed to remove old project file '{}'", m_filepath);
        }
    }

    m_filepath = filepath;

    taskScope.GetEditorTask()->SetDescription("Saving package data");

    // Ensure base property values are written to manifests, not values from applied swatch overrides.
    if (m_editWorld.IsValid())
    {
        if (SwatchOverrideSystem* overrideSystem = m_editWorld->GetSystem<SwatchOverrideSystem>())
        {
            overrideSystem->RevertAll();
        }
    }

    registry.SaveDirtyAssets();

    // failed assets stay dirty; their data may still only live in the temp dir
    const bool allAssetsSaved = !registry.HasDirtyAssets();

    m_assetFolders->SyncWithRegistry();
    m_assetFolders->PruneMissingAssets(registry);

    if (Result saveFoldersResult = m_assetFolders->Save(dir / EditorAssetFolders::s_fileName); saveFoldersResult.HasError())
    {
        HYP_LOG(Editor, Warning, "Failed to save asset folders: {}", saveFoldersResult.GetError().GetMessage());
    }

    // Re-apply the active swatch's overrides now that all assets have been saved
    if (m_editWorld.IsValid())
    {
        if (SwatchOverrideSystem* overrideSystem = m_editWorld->GetSystem<SwatchOverrideSystem>())
        {
            overrideSystem->ApplyActive();
        }
    }

    // Move files not managed by the registry (eg. script sources in Scripts/) from the old location
    // (eg. the temporary directory of an unsaved project) to the new one.
    bool looseFilesCopied = true;

    if (projectDirChanged && oldProjectDir.Any())
    {
        looseFilesCopied = CopyLooseProjectFiles(oldProjectDir, dir);
    }

    if (rootPathChanged)
    {
        // Re-point script file watchers at the new project directory
        if (m_editWorld.IsValid())
        {
            if (ScriptSystem* scriptSystem = m_editWorld->GetSystem<ScriptSystem>())
            {
                scriptSystem->RefreshScriptSourceDirectories();
            }
        }
    }

    if (!looseFilesCopied)
    {
        return HYP_MAKE_ERROR(Error, "Failed to copy some project files from '{}' to '{}'; the old directory was kept", oldProjectDir, dir);
    }

    if (!allAssetsSaved && !m_tempDirectory.Empty())
    {
        HYP_LOG(Editor, Warning, "Some assets failed to save; keeping temporary project directory '{}'", m_tempDirectory);
    }

    // Moved from temp; remove old dir
    if (!m_tempDirectory.Empty() && allAssetsSaved)
    {
        if (!m_tempDirectory.RemoveRecursively())
        {
            HYP_LOG(Editor, Warning, "Failed to remove temporary project directory '{}'", m_tempDirectory);
        }

        m_tempDirectory = FilePath();
    }

    m_lastSavedTime = Time::Now();
    m_hasUnsavedSettings = false;

    if (Result argsResult = WriteGameArgs(); argsResult.HasError())
    {
        HYP_LOG(Editor, Warning, "Failed to write the game's args.txt: {}", argsResult.GetError().GetMessage());
    }

    OnProjectSaved(MakeStrongRef(this));

    return {};
}

bool EditorProject::IsDirty() const
{
    if (!IsSaved() && !m_actionStack->EverTouched())
    {
        return false;
    }

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

    return !IsSaved() || m_hasUnsavedSettings || m_assetFolders->IsDirty() || registry->HasDirtyAssets();
}

String EditorProject::GetGameTitle() const
{
    const String title = m_projectSettings->gameTitle.Trimmed();

    if (title.Any())
    {
        return title;
    }

    return m_name.IsValid() ? String(*m_name) : String::empty;
}

String EditorProject::GetModuleName() const
{
    if (!IsSaved())
    {
        const String name = StringUtil::ToIdentifier(GetGameTitle());

        return name.Any() ? name : String("MyGame");
    }

    return ResolveModuleName(m_projectSettings->moduleName, m_projectSettings->gameTitle, m_filepath);
}

String EditorProject::GetModuleNameFromTitle() const
{
    return StringUtil::ToIdentifier(GetGameTitle());
}

bool EditorProject::HasGameCode() const
{
    return IsSaved() && GetGameProjectLanguage(m_filepath) != GameProjectLanguage::None;
}

Result EditorProject::WriteGameArgs() const
{
    if (!HasGameCode())
    {
        return {};
    }

    const FilePath binariesDir = m_filepath.BasePath() / "Binaries" / String(GetModulePlatformDirectory());

    if (!binariesDir.Exists())
    {
        return {};
    }

    Array<String> args {
        HYP_FORMAT("--basedir=\"{}\"", CoreApi::GetBaseDirectory().ReplaceAll("\\", "/")),
        HYP_FORMAT("--contentdir=\"{}\"", GetContentDirectory().ReplaceAll("\\", "/")),
        "--cooked=false"
    };

    args.Concat(m_projectSettings->netSettings.GetGameArgs());

    return WriteGameArgsFile(binariesDir, args);
}

Result EditorProject::RenameGameCode(const String& newModuleName)
{
    if (!HasGameCode())
    {
        return HYP_MAKE_ERROR(Error, "The project has no game code to rename");
    }

    const String oldName = GetModuleName();
    const String newName = StringUtil::ToIdentifier(newModuleName);

    if (newName.Empty())
    {
        return HYP_MAKE_ERROR(Error, "'{}' has no characters a class name can use", newModuleName);
    }

    if (newName == oldName)
    {
        return {};
    }

    if (Result renameResult = RenameGameProjectFiles(m_filepath, oldName, newName); renameResult.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to rename game code from {} to {}: {}", oldName, newName, renameResult.GetError().GetMessage());

        return renameResult;
    }

    HYP_LOG(Editor, Info, "Renamed game code from {} to {}", oldName, newName);

    // the module name is read back from the project file
    m_projectSettings->moduleName = newName;

    return Save();
}

BakeLayer& EditorProject::GetActiveBakeLayer()
{
    const Handle<World>& world = GetWorld();
    Assert(world.IsValid(), "No World set on the project!");

    Handle<Swatch> swatch = world->GetActiveSwatch();
    Assert(swatch.IsValid());

    return swatch->bakeLayer;
}

Array<Name> EditorProject::GetSwatchNames() const
{
    const Handle<World>& world = GetWorld();
    Assert(world.IsValid(), "No World set on the project!");

    return world->GetSwatchNames();
}

Name EditorProject::GetActiveSwatchName() const
{
    const Handle<World>& world = GetWorld();
    Assert(world.IsValid(), "No World set on the project!");

    return world->GetActiveSwatchName();
}

void EditorProject::SetActiveSwatch(Name swatchName)
{
    const Handle<World>& world = GetWorld();
    Assert(world.IsValid(), "No World set on the project!");

    world->SetActiveSwatch(swatchName);

    OnActiveSwatchChanged(world->GetActiveSwatchName());
}

TResult<Handle<EditorProject>> EditorProject::Load(const FilePath& filepath)
{
    FilePath dir;
    FilePath projectFilepath;

    if (filepath.IsDirectory())
    {
        dir = filepath;
        projectFilepath = FindProjectFile(filepath);
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

    TResult<String> contentSubfolderResult = ReadProjectContentSubfolder(projectFilepath);

    if (contentSubfolderResult.HasError())
    {
        return contentSubfolderResult.GetError();
    }

    const String contentSubfolder = contentSubfolderResult.GetValue();
    const FilePath registryDir = contentSubfolder.Any() ? dir / contentSubfolder : dir;

    if (contentSubfolder.Any() && !registryDir.IsDirectory())
    {
        return HYP_MAKE_ERROR(Error, "Content folder '{}' of project {} does not exist", registryDir, projectFilepath);
    }

    HYP_LOG(Editor, Info, "Loading registry assets from {}", registryDir);

    g_editorState->ReportProjectLoadStage(ProjectLoadStage::ReadAssetRegistry);

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

    g_editorState->ReportProjectLoadStage(ProjectLoadStage::ReadProjectFile);

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

    if (project->m_contentSubfolder != contentSubfolder)
    {
        HYP_LOG(Editor, Warning, "Project {} has ContentSubfolder '{}' but its content was loaded from '{}'", projectFilepath, project->m_contentSubfolder, contentSubfolder);

        project->m_contentSubfolder = contentSubfolder;
    }

    g_editorState->ReportProjectLoadStage(ProjectLoadStage::LoadGameModule);

    if (IsManagedProject(projectFilepath))
    {
        if (Result propsResult = WriteManagedProjectProps(projectFilepath); propsResult.HasError())
        {
            HYP_LOG(Editor, Warning,
                "Could not point {} at this editor: {}",
                projectFilepath,
                propsResult.GetError().GetMessage());
        }
    }

    if (const String versionError = CheckNativeModuleEngineVersion(projectFilepath); versionError.Any())
    {
        HYP_LOG(Editor, Info, "{} Rebuilding the game.", versionError);

        g_editorState->ReportProjectLoadStage(ProjectLoadStage::RebuildGameModule);

        if (!g_editorState->OnRebuildNativeGameRequested(String(projectFilepath)))
        {
            HYP_LOG(Editor, Warning,
                "Rebuilding the game of {} failed or isn't available here",
                projectFilepath);
        }
    }

    // a project with a built C++ game module plays with the module's Game class
    TResult<Handle<Game>> moduleGameResult = CreateGameFromNativeModule(projectFilepath);

    if (moduleGameResult.HasError())
    {
        return moduleGameResult.GetError();
    }

    if (!moduleGameResult.GetValue().IsValid())
    {
        moduleGameResult = CreateGameFromManagedModule(projectFilepath);

        if (moduleGameResult.HasError())
        {
            HYP_LOG(Editor, Error,
                "{} Opening the project without it; build the game to load it.",
                moduleGameResult.GetError().GetMessage());

            moduleGameResult = Handle<Game>();
        }
    }

    if (moduleGameResult.GetValue().IsValid())
    {
        if (project->m_gameInstance.IsValid())
        {
            project->m_gameInstance->ReleaseManagedObject();
        }

        project->m_gameInstance = std::move(moduleGameResult.GetValue());
    }

    if (!project->m_gameInstance.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "{} uses an overridden Game class, but game module {} isn't built. Build the game ({}) and open the project again.",
            projectFilepath, GetNativeModulePath(projectFilepath), GetNativeBuildScriptPath(projectFilepath));
    }

    // hand registry over to the game instance on the project
    Assert(project->m_gameInstance != nullptr);

    project->m_gameInstance->SetAssetRegistry(registry);

    g_editorState->ReportProjectLoadStage(ProjectLoadStage::LoadScripts);

    // C# classes the scenes may contain (e.g Entity subclasses) must exist before any world is read
    project->m_scriptAssembly = ScriptSystem::LoadProjectScriptAssembly(registryDir);

    g_editorState->ReportProjectLoadStage(ProjectLoadStage::LoadAssetFolders);

    if (Result loadFoldersResult = project->m_assetFolders->Load(dir / EditorAssetFolders::s_fileName); loadFoldersResult.HasError())
    {
        HYP_LOG(Editor, Warning, "Failed to load asset folders: {}", loadFoldersResult.GetError().GetMessage());
    }

    project->m_assetFolders->BindRegistry(registry);
    project->m_assetFolders->SyncWithRegistry();

    if (!project->m_projectSettings.IsValid())
    {
        project->m_projectSettings = MakeHandle<EditorProjectSettings>();
    }

    if (project->m_legacyPlatformSettings.GetHashCode() != EditorPlatformSettings().GetHashCode())
    {
        project->m_projectSettings->platformSettings = std::move(project->m_legacyPlatformSettings);
        project->m_legacyPlatformSettings = EditorPlatformSettings();
    }

    // files saved before overrides recorded their fields have none listed
    if (project->m_packagingSettings.IsValid())
    {
        project->m_packagingSettings->UpdateOverriddenFields();
    }

    // set transient properties
    project->m_lastSavedTime = projectFilepath.LastModifiedTimestamp();
    project->m_filepath = projectFilepath;

    return project;
}

Array<Name> EditorProject::GetEnabledPlugins() const
{
    if (!m_projectSettings.IsValid())
    {
        return PluginManager::GetInstance().GetDefaultEnabledPlugins();
    }

    return m_projectSettings->ResolveEnabledPlugins();
}

Array<Name> EditorProject::GetEnabledPluginRuntimeModules() const
{
    return PluginManager::GetInstance().GetRuntimeModules(GetEnabledPlugins());
}

Handle<EditorProject> EditorProject::CreateNew()
{
    Name projectName = GetUniqueProjectName();

    // Create Game instance
    Handle<Game> gameInstance = MakeHandle<Game>();

    // Create World.
    // Like a loaded project, it's only the edit world. the Game only owns a World while playing
    Handle<World> world = MakeHandle<World>(NAME("MainWorld"), WorldFlags::Default);
    world->SetGame(gameInstance);

    // Until the project is saved for the first time, keep all of its contentin a temporary
    // directory, so that no project directory is created under Projects/ before the project is explicitly saved.
    const Optional<FilePath> tempDirOpt = EngineGlobals::CreateTempDirectory("UnsavedProject");

    FilePath contentDir;

    if (tempDirOpt.HasValue())
    {
        contentDir = *tempDirOpt / g_defaultProjectContentSubfolder;

        if (!contentDir.MkDir())
        {
            HYP_LOG(Editor, Warning, "Failed to create content directory '{}' for project '{}'!", contentDir, *projectName);
        }
    }
    else
    {
        HYP_LOG(Editor, Warning, "Failed to create temp directory for project '{}'!", *projectName);
    }

    Handle<AssetRegistry> registry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, contentDir);
    registry->Initialize();

    gameInstance->SetAssetRegistry(registry);

    Handle<EditorProject> project = MakeHandle<EditorProject>(projectName, gameInstance);
    project->m_tempDirectory = tempDirOpt.GetOr(FilePath());
    project->m_contentSubfolder = g_defaultProjectContentSubfolder;
    project->m_assetFolders->BindRegistry(registry);
    project->SetEditWorld(world);

    return project;
}

#pragma endregion EditorProject

} // namespace Hyperion
