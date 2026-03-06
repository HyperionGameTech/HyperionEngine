/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

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

struct EditorProjectSaveContext
{
};

HYP_DECLARE_LOG_CHANNEL(Editor);

static const String s_defaultProjectName = "Project";

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
    HYP_SCOPE;

    if (Result createPackageResult = CreatePackage(); createPackageResult.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to create asset package for project '{}': {}", m_name.IsValid() ? *m_name : "<unnamed project>", createPackageResult.GetError().GetMessage());
    }

    InitObject(m_package);
    InitObject(m_actionStack);

    if (!m_gameInstance)
    {
        m_gameInstance = MakeHandle<Game>(); // base game instance
    }

    InitObject(m_gameInstance);

    SetReady(true);
}

void EditorProject::SetName(Name name)
{
    if (m_name == name)
    {
        return;
    }

    m_name = name;

    if (m_package.IsValid())
    {
        m_package->Rename(name);
    }
    else if (IsInitCalled())
    {
        if (Result createPackageResult = CreatePackage(); createPackageResult.HasError())
        {
            HYP_LOG(Editor, Error, "Failed to create asset package for project '{}': {}", m_name, createPackageResult.GetError().GetMessage());
        }
    }
}

const Handle<World>& EditorProject::GetWorld() const
{
    Assert(m_gameInstance != nullptr);

    return m_gameInstance->GetWorld();
}

void EditorProject::SetGame(const Handle<Game>& gameInstance)
{
    m_gameInstance = gameInstance;

    if (IsInitCalled())
    {
        InitObject(m_gameInstance);
    }
}

Result EditorProject::CreatePackage()
{
    // only create if it isn't created yet
    if (m_package.IsValid())
    {
        return {};
    }

    if (m_name.IsValid())
    {
        // if name is already set, use that
        // @NOTE: if package already exists at this path it will be used
        m_package = g_assetManager->GetAssetRegistry()->GetPackageFromPath(*m_name, /* createIfNotExist */ true);

        OnPackageCreated(m_package);

        return {};
    }

    int currentCounter = 0;
    String currentName = s_defaultProjectName;

    do
    {
        Handle<AssetPackage> tmpPackage = g_assetManager->GetAssetRegistry()->GetPackageFromPath(currentName, /* createIfNotExist */ false);

        if (!tmpPackage)
        {
            m_package = g_assetManager->GetAssetRegistry()->GetPackageFromPath(currentName, /* createIfNotExist */ true);
            m_name = CreateNameFromDynamicString(currentName);

            OnPackageCreated(m_package);

            return {};
        }

        currentName = s_defaultProjectName + String::ToString(++currentCounter);
    }
    while (true);

    return HYP_MAKE_ERROR(Error, "Failed to create package");
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

    if (!m_package.IsValid())
    {
        if (Result createPackageResult = CreatePackage(); createPackageResult.HasError())
        {
            return createPackageResult.GetError();
        }
    }

    EditorTaskScope taskScope(
        TickableEditorTask::StaticClass(),
        []() { /* do nothing on tick */ },
        "Saving project",
        "Registering assets",
        /* isForegroundTask */ true);

    g_assetManager->GetAssetRegistry()->RegisterAssetsRecursively(
        m_package->BuildPackagePath(),
        BoxedValue(AnyRef(*this)),
        /* forceRelocation */ false,
        [](const AssetObject& assetObject) -> String
        {
            // Instances of objects without a pre-defined path (e.g Media/Meshes) go under
            //  PkgName/Objects/Types/<ObjectClassName>/ObjectName
            return HYP_FORMAT("Objects/Types/{}", assetObject.InstanceClass()->GetName());
        });

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

    if (Result packageSaveResult = m_package->Save(dir / *m_name); packageSaveResult.HasError())
    {
        return packageSaveResult;
    };

    if (Result saveManifestResult = g_assetManager->GetAssetRegistry()->GetBlobStorage().SaveManifest(); saveManifestResult.HasError())
    {
        return HYP_MAKE_ERROR(Error, "Failed to save BlobStorage manifest: {}", saveManifestResult.GetError().GetMessage());
    }

    if (Result saveTOCResult = g_assetManager->GetAssetRegistry()->GetBlobStorage().SaveTOC(); saveTOCResult.HasError())
    {
        return HYP_MAKE_ERROR(Error, "Failed to save BlobStorage table of contents: {}", saveTOCResult.GetError().GetMessage());
    }
    
    OnProjectSaved(MakeStrongRef(this));

    return {};
}

TResult<Handle<EditorProject>> EditorProject::Load(const FilePath& filepath)
{
    HYP_SCOPE;

    // registry to load assets into
    AssetRegistry* registry = g_assetManager->GetAssetRegistry();
    AssertDebug(registry != nullptr);

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
        FileBufferedReaderSource source { projectFilepath };
        BufferedReader reader { &source };

        if (!reader.IsOpen())
        {
            return HYP_MAKE_ERROR(Error, "Failed to open project file: {}", projectFilepath);
        }

        JSON::ParseResult parseResult = JSON::Parse(String(reader.ReadBytes().ToByteView()));

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

    const FilePath packageDir = dir / FilePath(projectFilepath.StripExtension()).Basename();
    const FilePath packageManifestPath = packageDir / "PackageManifest.json";

    TResult<Handle<AssetPackage>> loadPackageResult = registry->LoadPackageFromManifest(packageManifestPath, /* loadSubpackages */ true, /* forceLoad */ true);

    if (loadPackageResult.HasError())
    {
        return loadPackageResult.GetError();
    }

    project->m_package = std::move(*loadPackageResult);

    // set transient properties
    project->m_lastSavedTime = projectFilepath.LastModifiedTimestamp();
    project->m_filepath = projectFilepath;

    InitObject(project);

    return project;
}

#pragma endregion EditorProject

} // namespace Hyperion
