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
    if (m_gameInstance)
    {
        m_gameInstance->Shutdown();

        EnqueueDeletion(std::move(m_gameInstance));
    }
}

void EditorProject::SetName(Name name)
{
    m_name = name;
}

const Handle<World>& EditorProject::GetWorld() const
{
    Assert(m_gameInstance != nullptr);

    return m_gameInstance->GetWorld();
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

    AssertDebug(m_gameInstance != nullptr);

    World* world = m_gameInstance->GetWorld();
    AssertDebug(world != nullptr);

    world->AddScene(scene, /* addToStreamingLayer */ true);
    world->MarkDirty();
}

void EditorProject::RemoveScene(Scene* scene)
{
    if (!scene)
    {
        return;
    }

    AssertDebug(m_gameInstance != nullptr);

    World* world = m_gameInstance->GetWorld();
    AssertDebug(world != nullptr);

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
}

Result EditorProject::Save()
{
    return SaveAs(m_filepath);
}

Result EditorProject::SaveAs(FilePath filepath)
{
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
    registry.SetRootPath(dir);

    GlobalContextScope assetRegistryContextScope { AssetRegistryContext { MakeStrongRef(&registry) } };

    EditorTaskScope taskScope(
        TickableEditorTask::StaticClass(),
        "Saving project",
        "Registering assets",
        /* isForegroundTask */ true);

    // Put the world asset, root of all assets.
    registry.PutAssetsDeep(m_gameInstance->GetWorld());

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

    const FilePath projectDir = EngineGlobals::GetProjectsDirectory() / *projectName;

    Handle<AssetRegistry> registry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, projectDir);
    registry->Initialize();

    gameInstance->SetAssetRegistry(registry);

    Handle<EditorProject> project = MakeHandle<EditorProject>(projectName, gameInstance);

    return project;
}

#pragma endregion EditorProject

} // namespace Hyperion
