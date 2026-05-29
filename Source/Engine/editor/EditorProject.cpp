/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <editor/EditorProject.hpp>
#include <editor/EditorActionStack.hpp>
#include <editor/EditorTask.hpp>
#include <editor/EditorState.hpp>

#include <engine/Game.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/AssetObject.hpp>
#include <asset/BlobStorage.hpp>
#include <asset/SerializationUtils.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/EntityManager.hpp>

#include <scene/camera/Camera.hpp>

#include <Core/utilities/DeferredScope.hpp>
#include <Core/utilities/GlobalContext.hpp>

#include <Core/reflection/Class.hpp>

#include <Core/io/ByteReader.hpp>
#include <Core/io/ByteWriter.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <HyperionEngine.hpp>

#include <EditorProject.generated.inl>

namespace Hyperion {

struct EditorProjectSaveContext { };

HYP_DECLARE_LOG_CHANNEL(Editor);

static const ANSIString s_defaultProjectName = "Project";

#pragma region EditorProject

static Name GetUniqueProjectName()
{
    const FilePath projectsDir = ::Hyperion::GetProjectsDirectory();

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

    AssertDebug(m_gameInstance != nullptr && m_gameInstance->GetWorld() != nullptr);

    m_gameInstance->GetWorld()->AddScene(scene, /* addToStreamingLayer */ true);
}

void EditorProject::RemoveScene(Scene* scene)
{
    if (!scene)
    {
        return;
    }

    AssertDebug(m_gameInstance != nullptr && m_gameInstance->GetWorld() != nullptr);

    m_gameInstance->GetWorld()->RemoveScene(scene, /* removeFromStreamingLayer */ true);
}

FilePath EditorProject::GetProjectsDirectory() const
{
    return ::Hyperion::GetProjectsDirectory();
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
        const FilePath projectsDir = GetProjectsDirectory();
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
        dir = GetProjectsDirectory() / String(*m_name);
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

    registry.PutAssetsDeep(m_gameInstance->GetWorld());

    GlobalContextScope saveContextScope { EditorProjectSaveContext {} };

    taskScope.GetEditorTask()->SetDescription("Saving project metadata");

    ToJSONOptions opts;
    opts.skipTransientProperties = true;
    opts.writeClassNames = true;

    JSON::Object projectJson;
    if (!ObjectToJSON(EditorProject::StaticClass(), BoxedValue(MakeStrongRef(this)), projectJson, opts))
    {
        return HYP_MAKE_ERROR(Error, "Failed to save project!");
    }

    FileByteWriter wri { filepath };
    HYP_DEFER({ wri.Close(); });

    wri.WriteString(JSON::Value(projectJson).ToString(true));
    wri.Close();

    m_filepath = filepath;

    taskScope.GetEditorTask()->SetDescription("Saving package data");

    registry.SaveDirtyAssets();

    if (registry.HasBlobStorage())
    {
        if (Result saveManifestResult = registry.GetBlobStorage().SaveManifest(); saveManifestResult.HasError())
        {
            return HYP_MAKE_ERROR(Error, "Failed to save BlobStorage manifest: {}", saveManifestResult.GetError().GetMessage());
        }

        if (Result saveTOCResult = registry.GetBlobStorage().SaveTOC(); saveTOCResult.HasError())
        {
            return HYP_MAKE_ERROR(Error, "Failed to save BlobStorage table of contents: {}", saveTOCResult.GetError().GetMessage());
        }
    }

    OnProjectSaved(MakeStrongRef(this));

    m_lastSavedTime = Time::Now();

    return {};
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

    // create registry to load assets into
    Handle<AssetRegistry> registry = MakeHandle<AssetRegistry>(
        AssetRegistryId::Game, registryDir);

    registry->Initialize();

    // Load asset descs for the registry before attempting to load the assets themselves
    registry->LoadAssetDescs();

    Handle<EditorProject> project;

    { // load project and all its assets into the registry we just created.
        GlobalContextScope assetRegistryContextScope { AssetRegistryContext { registry } };

        {
            FileByteReader stream { projectFilepath };

            if (stream.Eof())
            {
                return HYP_MAKE_ERROR(Error, "Failed to open project file: {}", projectFilepath);
            }

            JSON::ParseResult parseResult = JSON::Parse(String(stream.Read().ToByteView()));

            if (!parseResult.ok)
            {
                return HYP_MAKE_ERROR(Error, "Failed to parse project file at {}: {}", projectFilepath, parseResult.message);
            }

            JSON::Value projectJson = parseResult.value;

            if (!projectJson.IsObject())
            {
                return HYP_MAKE_ERROR(Error, "Project file data is invalid!");
            }

            BoxedValue boxed;
            if (!ObjectFromJSON(projectJson.AsObject(), EditorProject::StaticClass(), boxed))
            {
                return HYP_MAKE_ERROR(Error, "Project file data could not be imported. Check the log for details.");
            }

            Assert(boxed.Is<Handle<EditorProject>>());

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
    Handle<World> world = MakeHandle<World>(NAME("MainWorld"), WorldFlags::DEFAULT);
    gameInstance->SetWorld(world);

    const FilePath projectDir = ::Hyperion::GetProjectsDirectory() / *projectName;

    Handle<AssetRegistry> registry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, projectDir);
    registry->Initialize();

    gameInstance->SetAssetRegistry(registry);

    Handle<EditorProject> project = MakeHandle<EditorProject>(projectName, gameInstance);

    return project;
}

#pragma endregion EditorProject

} // namespace Hyperion
