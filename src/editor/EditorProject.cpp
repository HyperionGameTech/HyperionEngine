/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <EditorPch.hpp>

#include <editor/EditorProject.hpp>
#include <editor/EditorActionStack.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/AssetObject.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/EntityManager.hpp>

#include <scene/camera/Camera.hpp>

#include <engine/Game.hpp>

#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>
#include <core/serialization/fbom/FBOMDeserializedObject.hpp>

#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/GlobalContext.hpp>

#include <core/reflection/Class.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <HyperionEngine.hpp>

#include <EditorProject.generated.inl>

namespace Hyperion {

struct EditorProjectSaveContext
{
};

HYP_DECLARE_LOG_CHANNEL(Editor);

static const String s_defaultProjectName = "Project";

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
    SafeDelete(std::move(m_gameInstance));
}

void EditorProject::Init()
{
    HYP_SCOPE;

    ObjectBase::Init();

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
    return GetResourceDirectory() / "projects";
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

    { // register objects under PkgName/Objects
        Handle<AssetPackage> objectsSubpackage = g_assetManager->GetAssetRegistry()->GetSubpackage(
            m_package,
            NAME("Objects"),
            /* createIfNotExist */ true);

        Assert(objectsSubpackage.IsValid());

        g_assetManager->GetAssetRegistry()->RegisterAssetsRecursively(
            objectsSubpackage->BuildPackagePath(),
            BoxedValue(AnyRef(*this)),
            /* forceRelocation */ false,
            [](const AssetObject& assetObject) -> String
            {
                //  PkgName/Objects/Types/<ObjectClassName>/ObjectName
                return HYP_FORMAT("Types/{}", assetObject.InstanceClass()->GetName());
            });
    }

    if (filepath.Empty())
    {
        filepath = GetProjectsDirectory() / *m_name;
    }

    if (!filepath.Exists() && !filepath.MkDir())
    {
        return HYP_MAKE_ERROR(Error, "Failed to create directory");
    }

    if (!filepath.IsDirectory())
    {
        return HYP_MAKE_ERROR(Error, "Path '{}' is not a directory", filepath);
    }

    GlobalContextScope contextScope { EditorProjectSaveContext {} };

    const Time previousLastSavedTime = m_lastSavedTime;
    m_lastSavedTime = Time::Now();

    const FilePath projectFilepath = filepath / (String(*m_name) + ".hypproj");

    FileByteWriter byteWriter(projectFilepath);
    HYP_DEFER({ byteWriter.Close(); });

    FBOMWriter writer { FBOMWriterConfig {} };
    writer.Append(*this);

    if (FBOMResult err = writer.Emit(&byteWriter); !err.IsOK())
    {
        m_lastSavedTime = previousLastSavedTime;

        return HYP_MAKE_ERROR(Error, "Failed to write project to disk at '{}': {}", projectFilepath, err.message);
    }

    Result result;

    {
        if (Result packageSaveResult = m_package->Save(filepath / *m_name); packageSaveResult.HasError())
        {
            result = packageSaveResult;
        }
    }

    if (result)
    {
        // Update m_filepath when save was successful.
        m_filepath = filepath;

        OnProjectSaved(HandleFromThis());
    }

    return result;
}

TResult<Handle<EditorProject>> EditorProject::Load(const FilePath& filepath)
{
    HYP_SCOPE;

    // registry to load assets into
    const Handle<AssetRegistry> registry = g_assetManager->GetAssetRegistry();
    Assert(registry.IsValid());

    FilePath dir;
    FilePath projectFilepath;

    if (filepath.IsDirectory())
    {
        dir = filepath;

        for (const FilePath& file : filepath.GetAllFilesInDirectory())
        {
            if (file.EndsWith(".hypproj"))
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

    const FilePath packageDir = dir / FilePath(projectFilepath.StripExtension()).Basename();
    const FilePath packageManifestPath = packageDir / "PackageManifest.json";

    TResult<Handle<AssetPackage>> loadPackageResult = registry->LoadPackageFromManifest(packageManifestPath, /* loadSubpackages */ true, /* forceLoad */ true).Await();

    if (loadPackageResult.HasError())
    {
        return loadPackageResult.GetError();
    }

    Handle<AssetPackage> rootPackage = std::move(*loadPackageResult);

    Handle<EditorProject> project;

    {
        FBOMObject projectObject;

        FBOMReader reader {
            FBOMReaderConfig { .basePath = dir }
        };

        if (FBOMResult err = reader.LoadFromFile(projectFilepath, projectObject); !err.IsOK())
        {
            return HYP_MAKE_ERROR(Error, "Failed to read project data: {}", err.message);
        }

        Optional<Handle<EditorProject>&> projectOpt = projectObject.m_deserializedObject->TryGet<Handle<EditorProject>>();

        if (!projectOpt)
        {
            return HYP_MAKE_ERROR(Error, "Internal error: Deserialized object is not an EditorProject");
        }

        project = *projectOpt;
    }

    project->m_package = rootPackage;

    InitObject(project);

    return project;
}

} // namespace Hyperion
