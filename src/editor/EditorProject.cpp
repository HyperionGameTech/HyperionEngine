/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorProject.hpp>
#include <editor/EditorActionStack.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/AssetObject.hpp>

#include <scene/Scene.hpp>

#include <scene/EntityManager.hpp>

#include <scene/camera/Camera.hpp>

#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>
#include <core/serialization/fbom/FBOMDeserializedObject.hpp>

#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/GlobalContext.hpp>

#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/object/HypClass.hpp>

#include <engine/EngineGlobals.hpp>

#include <HyperionEngine.hpp>

namespace hyperion {

struct EditorProjectSaveContext
{
};

HYP_DECLARE_LOG_CHANNEL(Editor);

static const String s_defaultProjectName = "UntitledProject";

EditorProject::EditorProject()
    : EditorProject(Name::Invalid())
{
}

EditorProject::EditorProject(Name name)
    : m_name(name),
      m_lastSavedTime(~0ull)
{
    m_actionStack = CreateObject<EditorActionStack>(WeakHandleFromThis());
}

EditorProject::~EditorProject()
{
}

void EditorProject::Init()
{
    if (m_name.IsValid())
    {
        if (Result createPackageResult = CreatePackage(); createPackageResult.HasError())
        {
            HYP_LOG(Editor, Error, "Failed to create asset package for project '{}': {}", m_name, createPackageResult.GetError().GetMessage());
        }
    }

    if (m_package)
    {
        InitObject(m_package);
    }

    InitObject(m_actionStack);

    for (const Handle<Scene>& scene : m_scenes)
    {
        InitObject(scene);

        OnSceneAdded(scene);
    }

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

Result EditorProject::CreatePackage()
{
    // only create if it isn't created yet
    if (m_package.IsValid())
    {
        return {};
    }

    Name packageName = GetName();

    if (!packageName.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Project name is not set");
    }

    Handle<AssetRegistry> assetRegistry = g_assetManager->GetAssetRegistry();

    Handle<AssetPackage> rootPackage = assetRegistry->GetPackageFromPath(*packageName, true);
    Assert(rootPackage.IsValid());

    if (IsInitCalled())
    {
        InitObject(rootPackage);
    }

    m_package = rootPackage;

    OnPackageCreated(m_package);

    return {};
}

void EditorProject::AddScene(const Handle<Scene>& scene)
{
    HYP_SCOPE;

    if (!scene.IsValid())
    {
        return;
    }

    if (m_scenes.Contains(scene))
    {
        return;
    }

    m_scenes.PushBack(scene);

    if (IsInitCalled())
    {
        OnSceneAdded(scene);
    }
}

void EditorProject::RemoveScene(const Handle<Scene>& scene)
{
    HYP_SCOPE;

    if (!scene.IsValid())
    {
        return;
    }

    if (!m_scenes.Contains(scene))
    {
        return;
    }

    if (IsInitCalled())
    {
        OnSceneRemoved(scene);
    }

    m_scenes.Erase(scene);
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

    if (!m_name.IsValid())
    {
        m_name = GetNextDefaultProjectName(s_defaultProjectName);

        if (!m_name.IsValid())
        {
            return HYP_MAKE_ERROR(Error, "Failed to generate a project name");
        }
    }

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
            HypData(AnyRef(*this)),
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

    TResult<Handle<AssetPackage>> loadPackageResult = registry->LoadPackageFromManifest(packageManifestPath, /* loadSubpackages */ true).Await();

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

Name EditorProject::GetNextDefaultProjectName_Impl(const String& defaultProjectName) const
{
    return Name::Invalid();
}

} // namespace hyperion
