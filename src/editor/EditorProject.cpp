/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

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
#include <asset/BlobStorageStructs.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/EntityManager.hpp>

#include <scene/camera/Camera.hpp>

#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>
#include <core/serialization/fbom/FBOMDeserializedObject.hpp>

#include <core/utilities/DeferredScope.hpp>
#include <core/utilities/GlobalContext.hpp>

#include <core/reflection/Class.hpp>
#include <core/serialization/SerializationUtils.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <HyperionEngine.hpp>

// temp
#include <rendering/asset/MeshAsset.hpp>

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

            // temp
            m_package->InitBlobStorage(/* readOnly */ false);
            //m_package->GetBlobStorage()->EnsureCapacity(1 * 1024 * 1024);

            // temp debug
            static const Array<Vertex> quadVertices = {
                Vertex { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
                Vertex { { 1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
                Vertex { { 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
                Vertex { { -1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } }
            };

            static const Array<uint32> quadIndices = {
                0, 3, 2,
                0, 2, 1
            };

            SizeType totalSize;
            MeshData2* md2 = MeshData2::CreateMeshData(quadVertices, quadIndices, totalSize);
            Vertex& vtx0 = md2->vertexData[0];
            HYP_LOG_TEMP("Vertex0: {}", vtx0.GetPosition());
            Vertex& vtx1 = md2->vertexData[1];
            HYP_LOG_TEMP("Vertex1: {}", vtx1.GetPosition());
            Vertex& vtx2 = md2->vertexData[2];
            HYP_LOG_TEMP("Vertex2: {}", vtx2.GetPosition());
            Vertex& vtx3 = md2->vertexData[3];
            HYP_LOG_TEMP("Vertex3: {}", vtx3.GetPosition());

            //BlobAllocation allocation;
            //Assert(m_package->GetBlobStorage()->Map(0, totalSize, allocation));
            //Memory::Copy(allocation.address, md2, totalSize);

            //// lets write it again just to see..
            //BlobAllocation allocation2;
            //Assert(m_package->GetBlobStorage()->Map(ByteUtil::AlignAs(totalSize, 16), totalSize, allocation2));
            //Memory::Copy(allocation2.address, md2, totalSize);

            //m_package->GetBlobStorage()->Write(0, totalSize, md2);
            //m_package->GetBlobStorage()->Write(ByteUtil::AlignAs(totalSize, 16), totalSize, md2);
            ByteWriter* writer = m_package->GetBlobStorage()->GetWriteStream();
            Assert(writer != nullptr);
            
            TBlobBuilder<MeshData2> builder;
            auto res = builder.Serialize(writer, md2);
            Assert(!res.HasError());

            auto& value = res.GetValue();
            HYP_LOG_TEMP("offset = {}, size = {}", value.offset, value.size);

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
            if (assetObject.InstanceClass()->GetName() == "TextureAsset"_sh)
            {
                HYP_BREAKPOINT;
            }

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
    }

    OnProjectSaved(MakeStrongRef(this));

    return {};
}

HYP_DISABLE_OPTIMIZATION;
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

    // temp debug
    if (BlobStorage* blobStorage = project->m_package->GetBlobStorage())
    {
        BlobResourceKey key { 0, 552 };

        void* blobPtr = blobStorage->Map(key);
        Assert(blobPtr != nullptr);

        //MeshData2* md2 = (MeshData2*)Memory::Allocate(552);
        //Memory::Copy(md2, blobPtr, 552);

        MeshData2* md2 = reinterpret_cast<MeshData2*>(blobPtr);

        HYP_LOG_TEMP("Md2 vertex ptr: {}", md2->vertexData.offset);
        HYP_LOG_TEMP("Md2 index ptr: {}", md2->indexData.offset);
        
        Vertex& vtx0 = md2->vertexData[0];
        HYP_LOG_TEMP("Vertex0: {}", vtx0.GetPosition());
        Vertex& vtx1 = md2->vertexData[1];
        HYP_LOG_TEMP("Vertex1: {}", vtx1.GetPosition());
        Vertex& vtx2 = md2->vertexData[2];
        HYP_LOG_TEMP("Vertex2: {}", vtx2.GetPosition());
        Vertex& vtx3 = md2->vertexData[3];
        HYP_LOG_TEMP("Vertex3: {}", vtx3.GetPosition());

        HYP_BREAKPOINT;

        blobStorage->Unmap(key);
    }

    // set transient properties
    project->m_lastSavedTime = projectFilepath.LastModifiedTimestamp();
    project->m_filepath = projectFilepath;

    InitObject(project);

    return project;
}

#pragma endregion EditorProject

} // namespace Hyperion
