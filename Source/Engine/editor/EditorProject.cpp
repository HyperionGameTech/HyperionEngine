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

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/EntityManager.hpp>

#include <scene/camera/Camera.hpp>

#include <Core/utilities/DeferredScope.hpp>
#include <Core/utilities/GlobalContext.hpp>

#include <Core/reflection/Class.hpp>
#include <Core/serialization/SerializationUtils.hpp>

#include <Core/io/ByteReader.hpp>
#include <Core/io/ByteWriter.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <HyperionEngine.hpp>

#include <EditorProject.generated.inl>

namespace Hyperion {

struct EditorProjectSaveContext { };

HYP_DECLARE_LOG_CHANNEL(Editor);

static const String s_defaultProjectName = "DefaultProject";

#pragma region EditorProject

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
    EnqueueDeletion(std::move(m_gameInstance));
}

void EditorProject::Init()
{
    InitObject(m_actionStack);

    if (!m_gameInstance)
    {
        m_gameInstance = MakeHandle<Game>(); // base game instance
    }

    m_gameInstance->Initialize();

    SetReady(true);
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

    if (m_gameInstance.IsValid())
    {
        m_gameInstance->Shutdown();
    }

    m_gameInstance = gameInstance;

    if (m_gameInstance.IsValid())
    {
        m_gameInstance->Initialize();
    }
}

void EditorProject::AddScene(const Handle<Scene>& scene)
{
    HYP_SCOPE;

    if (!scene)
    {
        return;
    }

    AssertDebug(m_gameInstance != nullptr && m_gameInstance->GetWorld() != nullptr);

    m_gameInstance->GetWorld()->AddScene(scene, /* addToStreamingLayer */ true);
}

void EditorProject::RemoveScene(Scene* scene)
{
    HYP_SCOPE;

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

void EditorProject::Close()
{
    HYP_SCOPE;
}

Result EditorProject::Save()
{
    return SaveAs(m_filepath);
}

Result EditorProject::SaveAs(FilePath filepath)
{
    HYP_SCOPE;

    EditorTaskScope taskScope(
        TickableEditorTask::StaticClass(),
        []() { /* do nothing on tick */ },
        "Saving project",
        "Registering assets",
        /* isForegroundTask */ true);
        
    GetCurrentAssetRegistry()->PutAssetsDeep(m_gameInstance->GetWorld());

    if (filepath.Empty())
    {
        filepath = GetProjectsDirectory() / *m_name;
    }

    FilePath dir;

    if (filepath.GetExtension().Any())
    {
        dir = filepath.BasePath();
    }
    else
    {
        dir = filepath;
        filepath = filepath / (String(*m_name) + ".hypproject");
    }

    if (!dir.Exists() && !dir.MkDir())
    {
        return HYP_MAKE_ERROR(Error, "Failed to create directory");
    }

    if (!dir.IsDirectory())
    {
        return HYP_MAKE_ERROR(Error, "Path '{}' is not a directory", dir);
    }

    GlobalContextScope contextScope { EditorProjectSaveContext {} };
    
    taskScope.GetEditorTask()->SetDescription("Saving project metadata");

    const Time previousLastSavedTime = m_lastSavedTime;
    m_lastSavedTime = Time::Now();
    
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

    m_lastSavedTime = previousLastSavedTime;
    m_filepath = filepath;
    
    taskScope.GetEditorTask()->SetDescription("Saving package data");

    GetCurrentAssetRegistry()->SaveDirtyAssets();

    if (Result saveManifestResult = GetCurrentAssetRegistry()->GetBlobStorage().SaveManifest(); saveManifestResult.HasError())
    {
        return HYP_MAKE_ERROR(Error, "Failed to save BlobStorage manifest: {}", saveManifestResult.GetError().GetMessage());
    }

    if (Result saveTOCResult = GetCurrentAssetRegistry()->GetBlobStorage().SaveTOC(); saveTOCResult.HasError())
    {
        return HYP_MAKE_ERROR(Error, "Failed to save BlobStorage table of contents: {}", saveTOCResult.GetError().GetMessage());
    }
    
    OnProjectSaved(MakeStrongRef(this));

    return {};
}

TResult<Handle<EditorProject>> EditorProject::Load(const FilePath& filepath)
{
    HYP_SCOPE;

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

    Handle<EditorProject> project;

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

    const FilePath registryDir = dir / FilePath(projectFilepath.StripExtension()).Basename();

    // create registry to load assets into
    Handle<AssetRegistry> registry = MakeHandle<AssetRegistry>(registryDir);
    registry->Initialize();

    GlobalContextScope contextScope { AssetRegistryContext { registry } };

    Assert(project->m_gameInstance != nullptr);

    registry->LoadAssetDescs();

    // hand registry over to the game instance on the project
    project->m_gameInstance->m_assetRegistry = std::move(registry);

    // set transient properties
    project->m_lastSavedTime = projectFilepath.LastModifiedTimestamp();
    project->m_filepath = projectFilepath;

    InitObject(project);

    return project;
}

#pragma endregion EditorProject

} // namespace Hyperion
