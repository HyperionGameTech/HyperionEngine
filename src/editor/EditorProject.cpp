/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorProject.hpp>
#include <editor/EditorActionStack.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <scene/Scene.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/CameraComponent.hpp>

#include <scene/camera/Camera.hpp>

#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>
#include <core/serialization/fbom/FBOMDeserializedObject.hpp>

#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/GlobalContext.hpp>

#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <HyperionEngine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

static const String g_default_project_name = "UntitledProject";

EditorProject::EditorProject()
    : EditorProject(Name::Invalid())
{
}

EditorProject::EditorProject(Name name)
    : m_name(name),
      m_last_saved_time(~0ull)
{
    m_asset_registry = CreateObject<AssetRegistry>();
    m_action_stack = CreateObject<EditorActionStack>();
}

EditorProject::~EditorProject()
{
}

void EditorProject::Init()
{
    InitObject(m_asset_registry);
    InitObject(m_action_stack);

    for (const Handle<Scene>& scene : m_scenes)
    {
        InitObject(scene);

        OnSceneAdded(scene);
    }

    SetReady(true);
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
    return uint64(m_last_saved_time) != ~0ull;
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
        m_name = GetNextDefaultProjectName(g_default_project_name);

        if (!m_name.IsValid())
        {
            return HYP_MAKE_ERROR(Error, "Failed to generate a project name");
        }
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

    const Time previous_last_saved_time = m_last_saved_time;
    m_last_saved_time = Time::Now();

    const FilePath project_filepath = filepath / (String(*m_name) + ".hypproj");

    FileByteWriter byte_writer(project_filepath);

    HYP_DEFER({
        byte_writer.Close();
    });

    FBOMWriter writer { FBOMWriterConfig {} };
    writer.Append(*this);

    if (auto err = writer.Emit(&byte_writer))
    {
        m_last_saved_time = previous_last_saved_time;

        return HYP_MAKE_ERROR(Error, "Failed to write project to disk");
    }

    Proc<Result(const FilePath&, const Handle<AssetPackage>&)> create_asset_package_directory;

    create_asset_package_directory = [&create_asset_package_directory](const FilePath& parent_directory, const Handle<AssetPackage>& package) -> Result
    {
        const FilePath directory = parent_directory / package->GetName().LookupString();

        if (!directory.Exists())
        {
            if (!directory.MkDir())
            {
                return HYP_MAKE_ERROR(Error, "Failed to create directory");
            }
        }
        else if (!directory.IsDirectory())
        {
            return HYP_MAKE_ERROR(Error, "Path is not a directory");
        }

        for (const Handle<AssetPackage>& subpackage : package->GetSubpackages())
        {
            Result subpackage_result = create_asset_package_directory(directory, subpackage);

            if (subpackage_result.HasError())
            {
                return subpackage_result;
            }
        }

        return {};
    };

    Result result;

    {
        // temporary scope to set the root path for the asset registry
        GlobalContextScope scope { AssetRegistryRootPathContext { filepath } };

        m_asset_registry->ForEachPackage([&create_asset_package_directory, &filepath, &result](const Handle<AssetPackage>& package)
            {
                if (Result package_result = create_asset_package_directory(filepath, package); package_result.HasError())
                {
                    result = package_result;

                    return IterationResult::STOP;
                }

                // Save each individual object in the package
                package->ForEachAssetObject([&result](const Handle<AssetObject>& asset_object)
                    {
                        HYP_LOG(Editor, Debug, "Saving asset object '{}', Path: '{}', FilePath: '{}'", asset_object->GetName(), asset_object->GetPath(), asset_object->GetFilePath());
                        if (Result object_result = asset_object->Save(); object_result.HasError())
                        {
                            result = object_result;

                            return IterationResult::STOP;
                        }

                        return IterationResult::CONTINUE;
                    });

                return IterationResult::CONTINUE;
            });
    }

    if (result)
    {
        // Update m_filepath when save was successful.
        m_filepath = filepath;

        m_asset_registry->SetRootPath(FilePath::Relative(filepath, FilePath::Current()));
    }

    OnProjectSaved(HandleFromThis());

    return result;
}

TResult<Handle<EditorProject>> EditorProject::Load(const FilePath& filepath)
{
    HYP_SCOPE;

    FilePath directory;
    FilePath project_filepath;

    if (filepath.IsDirectory())
    {
        directory = filepath;

        for (const FilePath& file : filepath.GetAllFilesInDirectory())
        {
            if (file.EndsWith(".hypproj"))
            {
                project_filepath = file;

                break;
            }
        }
    }
    else
    {
        directory = filepath.BasePath();
        project_filepath = filepath;
    }

    if (!directory.Exists())
    {
        return HYP_MAKE_ERROR(Error, "Directory does not exist");
    }

    if (!project_filepath.Exists())
    {
        return HYP_MAKE_ERROR(Error, "Project file does not exist");
    }

    FBOMObject project_object;
    FBOMReader reader({});

    if (FBOMResult err = reader.LoadFromFile(project_filepath, project_object))
    {
        return HYP_MAKE_ERROR(Error, "Failed to load project");
    }

    Optional<const Handle<EditorProject>&> project_opt = project_object.m_deserialized_object->TryGet<Handle<EditorProject>>();

    if (!project_opt)
    {
        return HYP_MAKE_ERROR(Error, "Failed to get project");
    }

    Handle<EditorProject> project = *project_opt;

    Proc<TResult<Handle<AssetPackage>>(const FilePath& directory)> initialize_package;

    initialize_package = [&initialize_package](const FilePath& directory) -> TResult<Handle<AssetPackage>>
    {
        HYP_NAMED_SCOPE_FMT("Initialize package %s", *directory);

        Handle<AssetPackage> package = CreateObject<AssetPackage>();

        package->SetName(CreateNameFromDynamicString(directory.Basename()));

        AssetPackageSet subpackages;

        for (const FilePath& subdirectory : directory.GetSubdirectories())
        {
            TResult<Handle<AssetPackage>> subpackage_result = initialize_package(subdirectory);

            if (subpackage_result.HasError())
            {
                return subpackage_result;
            }

            subpackages.Insert(std::move(subpackage_result.GetValue()));
        }

        // @TODO Load assets from files in directory

        package->SetSubpackages(std::move(subpackages));

        return package;
    };

    AssetPackageSet packages;

    for (const FilePath& subdirectory : directory.GetSubdirectories())
    {
        TResult<Handle<AssetPackage>> package_result = initialize_package(subdirectory);

        if (package_result.HasError())
        {
            return package_result.GetError();
        }

        packages.Insert(std::move(package_result.GetValue()));
    }

    project->m_asset_registry->SetPackages(std::move(packages));

    return project;
}

Name EditorProject::GetNextDefaultProjectName_Impl(const String& default_project_name) const
{
    return Name::Invalid();
}

} // namespace hyperion
