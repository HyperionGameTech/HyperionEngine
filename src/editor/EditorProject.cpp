/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/EditorProject.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>
#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>
#include <core/serialization/fbom/FBOMDeserializedObject.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>

namespace hyperion {

EditorProject::EditorProject()
    : EditorProject(Name::Unique("Project_"))
{

}

EditorProject::EditorProject(Name name)
    : m_name(name),
      m_last_saved_time(~0ull)
{
    m_asset_registry = CreateObject<AssetRegistry>();
    InitObject(m_asset_registry);

    Handle<Camera> camera = CreateObject<Camera>();
    camera->SetFlags(CameraFlags::MATCH_WINDOW_SIZE);
    camera->SetFOV(70.0f);
    camera->SetNear(0.01f);
    camera->SetFar(30000.0f);

    m_scene = CreateObject<Scene>(nullptr, camera, SceneFlags::FOREGROUND | SceneFlags::HAS_TLAS);
}

EditorProject::~EditorProject()
{
}

bool EditorProject::IsSaved() const
{
    return uint64(m_last_saved_time) != ~0ull;
}

void EditorProject::Close()
{
    HYP_SCOPE;
}

Result<void> EditorProject::Save()
{
    if (m_filepath.Any()) {
        return Save(m_filepath);
    }

    return Save(AssetManager::GetInstance()->GetBasePath() / "projects" / *m_name);
}

Result<void> EditorProject::Save(const FilePath &filepath)
{
    HYP_SCOPE;

    if (filepath.Any()) {
        m_filepath = filepath;
    }

    if (m_filepath.Empty()) {
        return HYP_MAKE_ERROR(Error, "No filepath set");
    }

    if (!m_filepath.Exists() && !m_filepath.MkDir()) {
        return HYP_MAKE_ERROR(Error, "Failed to create directory");
    }

    const Time previous_last_saved_time = m_last_saved_time;
    m_last_saved_time = Time::Now();

    const FilePath project_filepath = m_filepath / (String(*m_name) + ".hypproj");

    FileByteWriter byte_writer(project_filepath);

    HYP_DEFER({
        byte_writer.Close();
    });

    fbom::FBOMWriter writer { fbom::FBOMWriterConfig { } };
    writer.Append(*this);

    if (auto err = writer.Emit(&byte_writer)) {
        m_last_saved_time = previous_last_saved_time;

        return HYP_MAKE_ERROR(Error, "Failed to write project to disk");
    }

    Proc<Result<void>, const FilePath &, const Handle<AssetPackage> &> CreateAssetPackageDirectory;

    CreateAssetPackageDirectory = [&CreateAssetPackageDirectory](const FilePath &parent_directory, const Handle<AssetPackage> &package) -> Result<void>
    {
        const FilePath directory = parent_directory / package->GetName().LookupString();

        if (!directory.Exists()) {
            if (!directory.MkDir()) {
                return HYP_MAKE_ERROR(Error, "Failed to create directory");
            }
        } else if (!directory.IsDirectory()) {
            return HYP_MAKE_ERROR(Error, "Path is not a directory");
        }

        for (const Handle<AssetPackage> &subpackage : package->GetSubpackages()) {
            Result<void> subpackage_result = CreateAssetPackageDirectory(directory, subpackage);

            if (subpackage_result.HasError()) {
                return subpackage_result;
            }
        }

        return { };
    };

    Result<void> result;

    m_asset_registry->ForEachPackage([&](const Handle<AssetPackage> &package)
    {
        if (Result<void> package_result = CreateAssetPackageDirectory(m_filepath, package); package_result.HasError()) {
            result = package_result;

            return IterationResult::STOP;
        }

        return IterationResult::CONTINUE;
    });

    return result;
}

Result<RC<EditorProject>> EditorProject::Load(const FilePath &filepath)
{
    HYP_SCOPE;

    FilePath directory;
    FilePath project_filepath;

    if (filepath.IsDirectory()) {
        directory = filepath;

        for (const FilePath &file : filepath.GetAllFilesInDirectory()) {
            if (file.EndsWith(".hypproj")) {
                project_filepath = file;

                break;
            }
        }
    } else {
        directory = filepath.BasePath();
        project_filepath = filepath;
    }

    if (!directory.Exists()) {
        return HYP_MAKE_ERROR(Error, "Directory does not exist");
    }

    if (!project_filepath.Exists()) {
        return HYP_MAKE_ERROR(Error, "Project file does not exist");
    }

    fbom::FBOMObject project_object;
    fbom::FBOMReader reader({ });

    if (fbom::FBOMResult err = reader.LoadFromFile(project_filepath, project_object)) {
        return HYP_MAKE_ERROR(Error, "Failed to load project");
    }

    Optional<const RC<EditorProject> &> project_opt = project_object.m_deserialized_object->TryGet<RC<EditorProject>>();

    if (!project_opt) {
        return HYP_MAKE_ERROR(Error, "Failed to get project");
    }

    RC<EditorProject> project = *project_opt;

    Proc<Result<Handle<AssetPackage>>, const FilePath &> InitializePackage;

    InitializePackage = [&InitializePackage](const FilePath &directory) -> Result<Handle<AssetPackage>>
    {
        HYP_NAMED_SCOPE_FMT("InitializePackage(%s)", *directory);

        Handle<AssetPackage> package = CreateObject<AssetPackage>();

        package->SetName(CreateNameFromDynamicString(directory.Basename()));

        AssetPackageSet subpackages;

        for (const FilePath &subdirectory : directory.GetSubdirectories()) {
            Result<Handle<AssetPackage>> subpackage_result = InitializePackage(subdirectory);

            if (subpackage_result.HasError()) {
                return subpackage_result;
            }

            subpackages.Insert(std::move(subpackage_result.GetValue()));
        }

        // @TODO Load assets from files in directory

        package->SetSubpackages(std::move(subpackages));

        return package;
    };

    AssetPackageSet packages;

    for (const FilePath &subdirectory : directory.GetSubdirectories()) {
        Result<Handle<AssetPackage>> package_result = InitializePackage(subdirectory);

        if (package_result.HasError()) {
            return package_result.GetError();
        }

        packages.Insert(std::move(package_result.GetValue()));
    }

    project->m_asset_registry->SetPackages(std::move(packages));

    return project;
}

} // namespace hyperion
