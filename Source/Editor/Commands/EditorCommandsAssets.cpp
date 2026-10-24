/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Editor/Commands/EditorCommandsCommon.hpp>
#include <Editor/EditorAssetDrop.hpp>
#include <Editor/EditorTemplateLibrary.hpp>
#include <Editor/EditorImport.hpp>
#include <Editor/EditorAssetFolders.hpp>

#include <Scene/Components/ScriptComponent.hpp>

#include <Scene/Instancing/InstanceGroup.hpp>

#include <Core/IO/ByteReader.hpp>

#include <Scene/WorldGrid/Terrain/GroundCover.hpp>

namespace Hyperion {
    
namespace /* Helpers*/ {

/// Where a dropped or added asset lands.
Vec3f CalculateAssetInsertionPoint(
    EditorSubsystem& subsystem,
    Scene& scene,
    const Optional<Vec2f>& viewportPosition)
{
    Vec3f insertionPoint = subsystem.CalculateSceneInsertionPoint();

    if (!viewportPosition.HasValue())
    {
        return insertionPoint;
    }

    EditorViewport* activeViewport = subsystem.GetActiveViewport();
    Camera* camera = activeViewport != nullptr ? activeViewport->GetCamera() : nullptr;

    if (!camera || !(scene.GetSceneFlags() & SceneFlags::HAS_OCTREE))
    {
        return insertionPoint;
    }

    const Ray ray = camera->GetPickRay(*viewportPosition);

    RayTestResults results;

    if (scene.GetOctree().TestRay(ray, results, RayTestFlags::TestBVH))
    {
        insertionPoint = results.Front().hitpoint;
    }
    else if (activeViewport->IsOrthographic())
    {
        insertionPoint = ray.position - ray.direction * ray.position.Dot(ray.direction);
    }

    return insertionPoint;
}

/// Parses the optional normalized viewport position that follows the bucket index and asset name
Optional<Vec2f> ParseViewportPositionArguments(const EditorCommandBase& command)
{
    if (command.NumArguments() < 4)
    {
        return {};
    }

    float nx = 0.5f;
    float ny = 0.5f;

    StringUtil::Parse(command.GetArgument(2), &nx);
    StringUtil::Parse(command.GetArgument(3), &ny);

    return Vec2f(nx, ny);
}

void AddAssetToScene(EditorSubsystem& subsystem, const Handle<Scene>& scene, const Handle<AssetObject>& asset, const Vec3f& insertionPoint)
{
    Handle<Node> clonedNode;

    Handle<Mesh> mesh = DynamicCast<Mesh>(asset);
    Handle<Material> meshMaterial;

    if (Handle<Prefab> prefab = DynamicCast<Prefab>(asset))
    {
        if (subsystem.IsEditingPrefab() && subsystem.GetPrefabEditState()->GetPrefab() == prefab)
        {
            HYP_LOG(Editor, Warning, "Cannot add prefab '{}' to itself", asset->GetName());
            return;
        }

        clonedNode = prefab->Spawn();
    }
    else if (mesh.IsValid())
    {
        MaterialAttributes attributes;
        attributes.shaderName = NAME("GeometryPass");

        meshMaterial = MakeHandle<Material>(NAME_FMT("Material_{}", mesh->GetName()), attributes);
        meshMaterial->SetIsDynamic(true);
        InitObject(meshMaterial);

        Handle<Entity> meshEntity = MakeHandle<Entity>();
        meshEntity->SetName(mesh->GetName());

        clonedNode = meshEntity;
    }
    else
    {
        HYP_LOG(Editor, Warning, "Expected prefab or mesh, got {}", asset->InstanceClass()->GetName());
        return;
    }

    if (!clonedNode.IsValid())
    {
        HYP_LOG(Editor, Error, "Failed to spawn asset '{}'", asset->GetName());
        return;
    }

    Handle<Node> parentNode = scene->GetRoot();
    Assert(parentNode.IsValid());

    if (!parentNode.IsValid())
    {
        HYP_LOG(Editor, Error, "No root on Scene {}!", scene->GetName());
        return;
    }

    clonedNode->SetWorldTranslation(insertionPoint);

    WeakHandle<Node> previousFocusedNode = subsystem.GetFocusedNode();

    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        HYP_FORMAT("Add {}", asset->GetName()),
        Proc<EditorActionFunctions()>(
            [clonedNode, parentNode, previousFocusedNode, mesh, meshMaterial]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [clonedNode, parentNode, mesh, meshMaterial](EditorSubsystem* editorSubsystem, EditorProject*)
                        {
                            parentNode->AddChild(clonedNode);

                            if (meshMaterial.IsValid())
                            {
                                GetCurrentAssetRegistry()->PutAssetUnique(meshMaterial);
                                editorSubsystem->OnAssetsChanged(AssetBuckets::Materials.GetIndex());

                                Handle<Entity> meshEntity = DynamicCast<Entity>(clonedNode);

                                if (!meshEntity->HasComponent<MeshComponent>())
                                {
                                    meshEntity->AddComponent<MeshComponent>(MeshComponent { mesh, meshMaterial });
                                }

                                meshEntity->SetLocalBounds(mesh->GetAABB());
                            }

                            editorSubsystem->SetFocusedNode(clonedNode, true);
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [clonedNode, previousFocusedNode, meshMaterial](EditorSubsystem* editorSubsystem, EditorProject*)
                        {
                            clonedNode->Remove();

                            if (meshMaterial.IsValid())
                            {
                                GetCurrentAssetRegistry()->RemoveAsset(meshMaterial);
                                editorSubsystem->OnAssetsChanged(AssetBuckets::Materials.GetIndex());
                            }

                            if (Handle<Node> focusedNode = previousFocusedNode.Lock(); focusedNode.IsValid())
                            {
                                editorSubsystem->SetFocusedNode(focusedNode, true);
                            }
                        })
                };
            }));

    InitObject(action);
    subsystem.GetSceneActionStack()->PushAction(action);
}

} // namespace

#pragma region NewScript

class EditorCommandNewScript final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandNewScript);

public:
    virtual ~EditorCommandNewScript() override = default;

    virtual String GetText() const override
    {
        return "New Script";
    }

    static bool IsValidScriptName(const ANSIString& name)
    {
        if (name.Empty())
        {
            return false;
        }

        for (size_t index = 0; index < name.Size(); index++)
        {
            const char character = name.Data()[index];

            const bool isLetter = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || character == '_';
            const bool isDigit = character >= '0' && character <= '9';

            if (!isLetter && !(isDigit && index != 0))
            {
                return false;
            }
        }

        return true;
    }

    static bool IsScriptNameTaken(const ANSIString& name)
    {
        const Handle<AssetRegistry> registry = GetCurrentAssetRegistry();

        if (registry->GetUniqueAssetName(AssetBuckets::Scripts.GetIndex(), HYP_FORMAT("{}", name)) != Name(name))
        {
            return true;
        }

        const FilePath scriptsDir = registry->GetRootPath() / "Scripts";

        return (scriptsDir / HYP_FORMAT("{}.cs", name)).Exists()
            || (scriptsDir / HYP_FORMAT("{}.strata", name)).Exists();
    }

    static void CreateScriptFile(EditorProject& project, ScriptAsset& scriptAsset, const String& extension, const String& templateCode)
    {
        ScriptDesc& desc = scriptAsset.GetScriptDesc();

        const FilePath rootDir = GetCurrentAssetRegistry()->GetRootPath();
        if (!rootDir.Exists())
        {
            HYP_LOG(Editor, Info, "Asset registry root dir at {} does not exist, saving the package to create it.", rootDir);

            Result saveResult = project.Save();
            if (saveResult.HasError())
            {
                HYP_LOG(Editor, Warning, "Failed to save project; script file will not be created. Reason was: {}", saveResult.GetError().GetMessage());

                return;
            }
            else if (!rootDir.Exists())
            {
                HYP_LOG(Editor, Warning, "Asset registry root dir still does not exist after saving project. Will not create script asset. (path: {})", rootDir);

                return;
            }
        }
        else if (!rootDir.IsDirectory())
        {
            HYP_LOG(Editor, Warning, "Asset registry root dir is not a directory. Will not create script asset. (path: {})", rootDir);

            return;
        }

        const FilePath scriptsDir = rootDir / "Scripts";
        if (!scriptsDir.Exists())
        {
            if (!scriptsDir.MkDir())
            {
                HYP_LOG(Editor, Warning, "Failed to create scripts dir at {}", scriptsDir);

                return;
            }
        }
        else if (!scriptsDir.IsDirectory())
        {
            HYP_LOG(Editor, Warning, "Scripts dir exists but is not a directory at {}", scriptsDir);

            return;
        }

        FilePath scriptFilePath = scriptsDir / (String(*scriptAsset.GetName()) + extension);

        if (scriptFilePath.Exists())
        {
            HYP_LOG(Editor, Warning, "File at path {} already exists, not creating to prevent overwriting the file.", scriptFilePath);

            return;
        }

        const String relativeScriptFilePath = scriptFilePath.ToRelative(rootDir).ToCanonical();

        if (relativeScriptFilePath.Size() >= desc.path.Size())
        {
            HYP_LOG(Editor, Warning, "Relative file path is too long, will not fit into script desc! Path: {}", relativeScriptFilePath);

            // Zero it out, don't want to point to an invalid path.
            desc.path.Data()[0] = '\0';
        }
        else
        {
            desc.DeserializePath(relativeScriptFilePath);
        }

        FileByteWriter writer { scriptFilePath };
        writer.WriteString(templateCode);
        writer.Close();
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot create script asset!");

            return;
        }

        const String& languageArg = GetArgument(0);
        const String& nameArg = GetArgument(1);

        const ANSIString assetName = nameArg.Any()
            ? ANSIString(nameArg.Data(), nameArg.Data() + nameArg.Size())
            : "NewScript";

        // name becomes both a file name and a class name, so it has to be a plain identifier
        if (!IsValidScriptName(assetName))
        {
            HYP_LOG(Editor, Error, "Invalid script name '{}'; use letters, digits and underscores only, not starting with a digit", assetName);

            return;
        }

        if (IsScriptNameTaken(assetName))
        {
            HYP_LOG(Editor, Error, "A script named '{}' already exists", assetName);

            return;
        }

        ScriptDesc scriptDesc;
        String extension;
        String templateCode;

        // @TODO Move this to Templates/ ?

        if (languageArg == "strata")
        {
#ifndef HYP_STRATA
            HYP_LOG(Editor, Error, "Strata scripting is not enabled in this build; cannot create a Strata script");

            return;
#endif


            scriptDesc.language = ScriptLanguage::Strata;
            extension = ".strata";

            templateCode = String("// ") + assetName + "\n\n";
            templateCode += "import Engine;\n\n";
            templateCode += "Entity g_entity;\n\n";
            templateCode += "void OnAdded(const Entity entity)\n{\n    g_entity = entity;\n}\n\n";
            templateCode += "void Update(float delta)\n{\n}\n\n";
            templateCode += "void Destroy()\n{\n}\n";
        }
        else // csharp
        {
            scriptDesc.language = ScriptLanguage::CSharp;
            extension = ".cs";

            scriptDesc.DeserializeClassName(assetName);

            templateCode = String("using Hyperion;\n\npublic class ") + assetName + " : Script\n";
            templateCode += "{\n";
            templateCode += "    [HypField(Editor = true, Serialize = true)]\n    public int DummyValue = 0; /* Value that can be edited in the editor and saved/loaded */\n\n";
            templateCode += "    public override void OnAdded(Entity entity)\n    {\n    /* Called on entity added */\n    }\n\n";
            templateCode += "    public override void Update(float deltaTime)\n    {\n        /* Called every tick */\n     }\n\n";
            templateCode += "    public override void Destroy()\n    {\n    /* Called on entity removed or world shutdown */\n    }\n";
            templateCode += "}\n";
        }

        Handle<ScriptAsset> scriptAsset = MakeHandle<ScriptAsset>(Name(assetName), scriptDesc);
        InitObject(scriptAsset);

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [scriptAsset, extension, templateCode]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [scriptAsset, extension, templateCode](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                CreateScriptFile(*project, *scriptAsset, extension, templateCode);
                                GetCurrentAssetRegistry()->PutAssetUnique(scriptAsset);
                                editorSubsystem->OnAssetsChanged(AssetBuckets::Scripts.GetIndex());
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [scriptAsset](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->RemoveAsset(scriptAsset);
                                editorSubsystem->OnAssetsChanged(AssetBuckets::Scripts.GetIndex());

                                // @TODO: Move to trash/recycling bin? And move back out of there when redo?
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(NewScript);

#pragma endregion NewScript

#pragma region NewAsset

class EditorCommandNewAsset final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandNewAsset);

public:
    virtual ~EditorCommandNewAsset() override = default;

    virtual String GetText() const override
    {
        return m_text.Length() ? m_text : EditorCommandBase::GetText();
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 2)
        {
            HYP_LOG(Editor, Warning, "EditorCommandNewAsset requires a class name and a base name");
            return;
        }

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot create asset!");
            return;
        }

        const Class* assetClass = ClassRegistry::GetInstance().GetClass(Name(ANSIString(GetArgument(0))));
        if (!assetClass
            || !assetClass->IsDerivedFrom(AssetObject::StaticClass())
            || assetClass->IsAbstract() || !assetClass->CanCreateInstance())
        {
            HYP_LOG(Editor, Error, "'{}' is not a concrete asset class", GetArgument(0));
            return;
        }

        BoxedValue boxed;
        if (!assetClass->CreateInstance(boxed) || !boxed.Is<Handle<AssetObject>>())
        {
            HYP_LOG(Editor, Error, "could not create an instance of '{}'", GetArgument(0));
            return;
        }

        Handle<AssetObject> asset = boxed.Get<Handle<AssetObject>>();
        asset->SetName(Name(ANSIString(GetArgument(1))));
        InitObject(asset);

        // A prefab is only usable with a root node, which is named after the prefab
        if (asset->IsA<Prefab>())
        {
            Handle<Prefab> prefab = static_cast<Handle<Prefab>>(asset);

            Handle<Node> root = MakeHandle<Node>();
            root->SetName(prefab->GetName());
            InitObject(root);

            prefab->SetRoot(root);
        }

        m_text = HYP_FORMAT("New {}", assetClass->GetName());

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [asset]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [asset](EditorSubsystem*, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->PutAssetUnique(asset);

                                // the registry may have renamed the asset to keep it unique, and the root node follows the name
                                if (asset->IsA<Prefab>())
                                {
                                    Handle<Prefab> prefab = static_cast<Handle<Prefab>>(asset);
                                    prefab->SyncRootName();
                                }
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [asset](EditorSubsystem*, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->RemoveAsset(asset);
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }

private:
    String m_text;
};

DEFINE_EDITOR_COMMAND(NewAsset);

#pragma endregion NewAsset

#pragma region AddAsset

class EditorCommandAddAsset final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAddAsset);

public:
    virtual ~EditorCommandAddAsset() override = default;

    virtual String GetText() const override
    {
        return "Add Asset";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 2)
        {
            HYP_LOG(Editor, Warning, "EditorCommandAddAsset requires bucket index and asset name");
            return;
        }

        uint32 bucketIndex = 0;
        if (!StringUtil::Parse(GetArgument(0), &bucketIndex) || bucketIndex == AssetBuckets::None.GetIndex())
        {
            HYP_LOG(Editor, Warning, "EditorCommandAddAsset: invalid bucket index '{}'", GetArgument(0));
            return;
        }

        const ANSIString assetName = GetArgument(1);

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandAddAsset: no project loaded");
            return;
        }

        Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandAddAsset: no active scene");
            return;
        }

        Handle<AssetObject> asset = GetCurrentAssetRegistry()->GetAsset(*AssetBuckets::AllBuckets[bucketIndex], Name(assetName));
        if (!asset.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandAddAsset: asset '{}' in bucket {} is not valid", assetName, GetAssetBucketName(bucketIndex));
            return;
        }

        AddAssetToScene(*subsystem, activeScene, asset, CalculateAssetInsertionPoint(*subsystem, *activeScene, ParseViewportPositionArguments(*this)));
    }
};

DEFINE_EDITOR_COMMAND(AddAsset);

#pragma endregion AddAsset

#pragma region ImportAndPlace

/// Arguments: normalized viewport x, y, then the model file paths separated by '|' (paths may contain spaces).
class EditorCommandImportAndPlace final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandImportAndPlace);

public:
    virtual ~EditorCommandImportAndPlace() override = default;

    virtual String GetText() const override
    {
        return "Import and Place";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 3)
        {
            HYP_LOG(Editor, Warning, "EditorCommandImportAndPlace requires viewport x, y and file paths");
            return;
        }

        float nx = 0.5f;
        float ny = 0.5f;
        StringUtil::Parse(GetArgument(0), &nx);
        StringUtil::Parse(GetArgument(1), &ny);

        String joinedPaths = GetArgument(2);

        for (int i = 3; i < NumArguments(); i++)
        {
            joinedPaths += ' ';
            joinedPaths += GetArgument(i);
        }

        Array<FilePath> files;

        for (String& path : joinedPaths.Split('|'))
        {
            FilePath filePath(std::move(path));

            if (!IsEditorImportModelFile(filePath) || !filePath.Exists())
            {
                HYP_LOG(Editor, Warning, "EditorCommandImportAndPlace: skipping '{}'", filePath);
                continue;
            }

            files.PushBack(std::move(filePath));
        }

        if (files.Empty())
        {
            return;
        }

        if (!subsystem->GetCurrentProject().IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandImportAndPlace: no project loaded");
            return;
        }

        Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandImportAndPlace: no active scene");
            return;
        }

        const Vec3f insertionPoint = CalculateAssetInsertionPoint(*subsystem, *activeScene, Vec2f(nx, ny));

        ImportEditorContent(
            subsystem,
            files,
            subsystem->GetCurrentAssetFolder(),
            [weakScene = WeakHandle<Scene>(activeScene), insertionPoint](EditorSubsystem& editorSubsystem, const Array<Handle<AssetObject>>& assets)
            {
                Handle<Scene> scene = editorSubsystem.GetActiveScene();

                if (!scene.IsValid() || scene != weakScene.Lock())
                {
                    HYP_LOG(Editor, Warning, "Active scene changed during import; imported models were not placed");
                    return;
                }

                for (const Handle<AssetObject>& asset : assets)
                {
                    if (asset.IsValid() && asset->IsA<Prefab>())
                    {
                        AddAssetToScene(editorSubsystem, scene, asset, insertionPoint);
                    }
                }
            });
    }
};

DEFINE_EDITOR_COMMAND(ImportAndPlace);

#pragma endregion ImportAndPlace

#pragma region ImportFiles

/// Arguments are "<folder>|<path>|<path>...": files from disk imported into an asset folder
class EditorCommandImportFiles final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandImportFiles);

public:
    virtual ~EditorCommandImportFiles() override = default;

    virtual String GetText() const override
    {
        return "Import Files";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (!subsystem->CanCreateAssets())
        {
            HYP_LOG(Editor, Warning, "EditorCommandImportFiles: assets can only be imported while a project is open and the simulation is stopped");
            return;
        }

        String joinedArguments;

        for (int i = 0; i < NumArguments(); i++)
        {
            if (i != 0)
            {
                joinedArguments += ' ';
            }

            joinedArguments += GetArgument(i);
        }

        Array<String> fields = joinedArguments.Split('|');

        if (fields.Size() < 2)
        {
            HYP_LOG(Editor, Warning, "EditorCommandImportFiles requires a folder and file paths");
            return;
        }

        Array<FilePath> files;

        for (size_t i = 1; i < fields.Size(); i++)
        {
            FilePath filePath(fields[i]);

            if (!filePath.Exists())
            {
                HYP_LOG(Editor, Warning, "EditorCommandImportFiles: skipping '{}'", filePath);
                continue;
            }

            files.PushBack(std::move(filePath));
        }

        ImportEditorContent(subsystem, files, fields[0]);
    }
};

DEFINE_EDITOR_COMMAND(ImportFiles);

#pragma endregion ImportFiles

#pragma region PlaceAsInstance

/// Places a prefab as one more instance of the active scene's InstanceGroup for it, creating the group if the scene has none.
/// Arguments match AddAsset: bucket index, asset name, and optionally the normalized viewport position to drop at.
class EditorCommandPlaceAsInstance final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandPlaceAsInstance);

public:
    virtual ~EditorCommandPlaceAsInstance() override = default;

    virtual String GetText() const override
    {
        return "Place as Instance";
    }

    virtual bool AllowedWhileEditingPrefab() const override
    {
        return false;
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 2)
        {
            HYP_LOG(Editor, Warning, "EditorCommandPlaceAsInstance requires bucket index and asset name");
            return;
        }

        uint32 bucketIndex = 0;
        if (!StringUtil::Parse(GetArgument(0), &bucketIndex) || bucketIndex == AssetBuckets::None.GetIndex())
        {
            HYP_LOG(Editor, Warning, "EditorCommandPlaceAsInstance: invalid bucket index '{}'", GetArgument(0));
            return;
        }

        const ANSIString assetName = GetArgument(1);

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandPlaceAsInstance: no project loaded");
            return;
        }

        Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid() || !activeScene->GetRoot().IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandPlaceAsInstance: no active scene");
            return;
        }

        Handle<Prefab> prefab = DynamicCast<Prefab>(GetCurrentAssetRegistry()->GetAsset(*AssetBuckets::AllBuckets[bucketIndex], Name(assetName)));
        if (!prefab.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandPlaceAsInstance: '{}' is not a prefab", assetName);
            return;
        }

        Handle<InstanceGroup> group = InstanceGroup::Find(activeScene.Get(), prefab);
        const bool createsGroup = !group.IsValid();

        if (createsGroup)
        {
            group = InstanceGroup::Create(activeScene.Get(), prefab);
        }

        if (!group.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandPlaceAsInstance: failed to create an instance group for '{}'", assetName);
            return;
        }

        const Transform transform { CalculateAssetInsertionPoint(*subsystem, *activeScene, ParseViewportPositionArguments(*this)) };

        // the same id on every redo, so later actions that refer to this instance still find it
        const InstanceId instanceId = group->ReserveInstanceId();

        Handle<Node> sceneRoot = activeScene->GetRoot();

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            HYP_FORMAT("Place {} as Instance", assetName),
            Proc<EditorActionFunctions()>(
                [group, sceneRoot, createsGroup, instanceId, transform]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [group, sceneRoot, createsGroup, instanceId, transform](EditorSubsystem*, EditorProject*)
                            {
                                if (createsGroup)
                                {
                                    sceneRoot->AddChild(group);
                                }

                                group->AddInstanceWithId(instanceId, transform);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [group, createsGroup, instanceId](EditorSubsystem*, EditorProject*)
                            {
                                group->RemoveInstance(instanceId);

                                if (createsGroup)
                                {
                                    group->Remove();
                                }
                            })
                    };
                }));

        InitObject(action);
        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(PlaceAsInstance);

#pragma endregion PlaceAsInstance

#pragma region PlaceInstances

/// Adds a list of instances of a prefab to the active scene's InstanceGroup for it.
/// Arguments:
///     bucket index, asset name, then a text file with one instance per line: tx ty tz qx qy qz qw sx sy sz
class EditorCommandPlaceInstances final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandPlaceInstances);

public:
    virtual ~EditorCommandPlaceInstances() override = default;

    virtual String GetText() const override
    {
        return "Place Instances";
    }

    virtual bool AllowedWhileEditingPrefab() const override
    {
        return false;
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 3)
        {
            HYP_LOG(Editor, Warning, "EditorCommandPlaceInstances requires bucket index, asset name and a transforms file");
            return;
        }

        uint32 bucketIndex = 0;
        if (!StringUtil::Parse(GetArgument(0), &bucketIndex) || bucketIndex == AssetBuckets::None.GetIndex())
        {
            HYP_LOG(Editor, Warning, "EditorCommandPlaceInstances: invalid bucket index '{}'", GetArgument(0));
            return;
        }

        const ANSIString assetName = GetArgument(1);

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        Handle<Scene> activeScene = subsystem->GetActiveScene();

        if (!currentProject.IsValid() || !activeScene.IsValid() || !activeScene->GetRoot().IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandPlaceInstances: no project or active scene");
            return;
        }

        Handle<Prefab> prefab = DynamicCast<Prefab>(GetCurrentAssetRegistry()->GetAsset(*AssetBuckets::AllBuckets[bucketIndex], Name(assetName)));
        if (!prefab.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandPlaceInstances: '{}' is not a prefab", assetName);
            return;
        }

        Array<Transform> transforms;

        if (!ReadTransforms(FilePath(GetArgument(2)), transforms))
        {
            return;
        }

        Handle<InstanceGroup> group = InstanceGroup::Find(activeScene.Get(), prefab);
        const bool createsGroup = !group.IsValid();

        if (createsGroup)
        {
            group = InstanceGroup::Create(activeScene.Get(), prefab);
        }

        if (!group.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandPlaceInstances: failed to create an instance group for '{}'", assetName);
            return;
        }

        // the same ids on every redo, so later actions that refer to these instances still find them
        Array<InstanceId> instanceIds;
        instanceIds.Reserve(transforms.Size());

        for (size_t index = 0; index < transforms.Size(); index++)
        {
            instanceIds.PushBack(group->ReserveInstanceId());
        }

        Handle<Node> sceneRoot = activeScene->GetRoot();

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            HYP_FORMAT("Place {} Instances of {}", transforms.Size(), assetName),
            Proc<EditorActionFunctions()>(
                [group, sceneRoot, createsGroup, instanceIds, transforms]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [group, sceneRoot, createsGroup, instanceIds, transforms](EditorSubsystem*, EditorProject*)
                            {
                                if (createsGroup)
                                {
                                    sceneRoot->AddChild(group);
                                }

                                for (size_t index = 0; index < instanceIds.Size(); index++)
                                {
                                    group->AddInstanceWithId(instanceIds[index], transforms[index]);
                                }
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [group, createsGroup, instanceIds](EditorSubsystem*, EditorProject*)
                            {
                                for (InstanceId instanceId : instanceIds)
                                {
                                    group->RemoveInstance(instanceId);
                                }

                                if (createsGroup)
                                {
                                    group->Remove();
                                }
                            })
                    };
                }));

        InitObject(action);
        currentProject->GetActionStack()->PushAction(action);

        HYP_LOG(Editor, Info, "EditorCommandPlaceInstances: placed {} instances of {}", transforms.Size(), assetName);
    }

private:
    static bool ReadTransforms(const FilePath& path, Array<Transform>& outTransforms)
    {
        FileByteReader reader { path };

        if (reader.Eof())
        {
            HYP_LOG(Editor, Warning, "EditorCommandPlaceInstances: could not read '{}'", path);
            return false;
        }

        const ByteBuffer buffer = reader.Read();
        reader.Close();

        const String text(buffer.ToByteView());

        for (const String& line : text.Split('\n'))
        {
            float values[10];
            uint32 numValues = 0;

            for (const String& token : line.Trimmed().Split(' '))
            {
                if (token.Empty() || numValues >= 10 || !StringUtil::Parse(token, &values[numValues]))
                {
                    continue;
                }

                numValues++;
            }

            if (numValues == 0)
            {
                continue;
            }

            if (numValues != 10)
            {
                HYP_LOG(Editor, Warning, "EditorCommandPlaceInstances: skipped a line of '{}' without 10 numbers", path);
                continue;
            }

            outTransforms.PushBack(Transform(
                Vec3f(values[0], values[1], values[2]),
                Vec3f(values[7], values[8], values[9]),
                Quat4f(values[3], values[4], values[5], values[6])));
        }

        return true;
    }
};

DEFINE_EDITOR_COMMAND(PlaceInstances);

#pragma endregion PlaceInstances

#pragma region ScatterInstances

/// Scatters instances of a prefab over the terrain around a point, all in one undoable action.
/// Arguments: bucket index, asset name, center x, center z, radius, count, then optionally
/// seed, min scale, max scale, min spacing, max slope (0..1, 1 - normal.y) and sink depth.
class EditorCommandScatterInstances final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandScatterInstances);

public:
    virtual ~EditorCommandScatterInstances() override = default;

    virtual String GetText() const override
    {
        return "Scatter Instances";
    }

    virtual bool AllowedWhileEditingPrefab() const override
    {
        return false;
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 6)
        {
            HYP_LOG(Editor, Warning, "EditorCommandScatterInstances requires bucket index, asset name, center x, center z, radius and count");
            return;
        }

        const auto parseFloat = [this](uint32 index, float defaultValue)
        {
            float value = defaultValue;

            if (index < NumArguments())
            {
                StringUtil::Parse(GetArgument(index), &value);
            }

            return value;
        };

        uint32 bucketIndex = 0;
        if (!StringUtil::Parse(GetArgument(0), &bucketIndex) || bucketIndex == AssetBuckets::None.GetIndex())
        {
            HYP_LOG(Editor, Warning, "EditorCommandScatterInstances: invalid bucket index '{}'", GetArgument(0));
            return;
        }

        const ANSIString assetName = GetArgument(1);
        const Vec2f center { parseFloat(2, 0.0f), parseFloat(3, 0.0f) };
        const float radius = parseFloat(4, 50.0f);
        const uint32 count = uint32(MathUtil::Max(parseFloat(5, 10.0f), 0.0f));
        uint32 seed = uint32(parseFloat(6, 1.0f));
        const float minScale = parseFloat(7, 0.8f);
        const float maxScale = parseFloat(8, 1.2f);
        const float minSpacing = parseFloat(9, 0.0f);
        const float maxSlope = parseFloat(10, 1.0f);
        const float sinkDepth = parseFloat(11, 0.2f);

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        Handle<Scene> activeScene = subsystem->GetActiveScene();

        if (!currentProject.IsValid() || !activeScene.IsValid() || !activeScene->GetRoot().IsValid() || !activeScene->GetWorld())
        {
            HYP_LOG(Editor, Error, "EditorCommandScatterInstances: no project or active scene");
            return;
        }

        Handle<Prefab> prefab = DynamicCast<Prefab>(GetCurrentAssetRegistry()->GetAsset(*AssetBuckets::AllBuckets[bucketIndex], Name(assetName)));
        if (!prefab.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandScatterInstances: '{}' is not a prefab", assetName);
            return;
        }

        Handle<TerrainWorldGridLayer> terrainLayer;

        if (const Handle<WorldGrid>& worldGrid = activeScene->GetWorld()->GetWorldGrid(); worldGrid.IsValid())
        {
            for (const Handle<WorldGridLayer>& layer : worldGrid->GetLayers())
            {
                if ((terrainLayer = DynamicCast<TerrainWorldGridLayer>(layer)).IsValid())
                {
                    break;
                }
            }
        }

        const auto nextRandom = [&seed]()
        {
            seed = seed * 747796405u + 2891336453u;
            uint32 word = ((seed >> ((seed >> 28u) + 4u)) ^ seed) * 277803737u;
            word = (word >> 22u) ^ word;

            return float(word) / 4294967295.0f;
        };

        const auto sampleHeight = [&terrainLayer](const Vec2f& position)
        {
            return terrainLayer.IsValid() ? terrainLayer->SampleHeightAtBlocking(position) : 0.0f;
        };

        Array<Transform> transforms;
        Array<Vec2f> placedPositions;

        const uint32 maxAttempts = count * 30;

        for (uint32 attempt = 0; attempt < maxAttempts && transforms.Size() < count; attempt++)
        {
            const float angle = nextRandom() * 2.0f * 3.14159265f;
            const float distance = MathUtil::Sqrt(nextRandom()) * radius;
            const Vec2f position = center + Vec2f(MathUtil::Cos(angle), MathUtil::Sin(angle)) * distance;

            bool tooClose = false;

            for (const Vec2f& placedPosition : placedPositions)
            {
                if ((placedPosition - position).Length() < minSpacing)
                {
                    tooClose = true;
                    break;
                }
            }

            if (tooClose)
            {
                continue;
            }

            const float height = sampleHeight(position);
            const float step = 1.0f;
            const Vec3f normal = Vec3f(
                sampleHeight(position - Vec2f(step, 0.0f)) - sampleHeight(position + Vec2f(step, 0.0f)),
                2.0f * step,
                sampleHeight(position - Vec2f(0.0f, step)) - sampleHeight(position + Vec2f(0.0f, step)))
                                     .Normalized();

            if (1.0f - normal.y > maxSlope)
            {
                continue;
            }

            const float scale = MathUtil::Lerp(minScale, maxScale, nextRandom());
            const Quat4f rotation = Quat4f::AxisAngles(Vec3f::UnitY(), nextRandom() * 2.0f * 3.14159265f);

            transforms.PushBack(Transform(Vec3f(position.x, height - sinkDepth * scale, position.y), Vec3f(scale), rotation));
            placedPositions.PushBack(position);
        }

        if (transforms.Empty())
        {
            HYP_LOG(Editor, Warning, "EditorCommandScatterInstances: no valid positions found for '{}'", assetName);
            return;
        }

        Handle<InstanceGroup> group = InstanceGroup::Find(activeScene.Get(), prefab);
        const bool createsGroup = !group.IsValid();

        if (createsGroup)
        {
            group = InstanceGroup::Create(activeScene.Get(), prefab);
        }

        if (!group.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandScatterInstances: failed to create an instance group for '{}'", assetName);
            return;
        }

        Array<InstanceId> instanceIds;

        for (uint32 index = 0; index < transforms.Size(); index++)
        {
            instanceIds.PushBack(group->ReserveInstanceId());
        }

        HYP_LOG(Editor, Info, "EditorCommandScatterInstances: placing {} instances of '{}'", transforms.Size(), assetName);

        Handle<Node> sceneRoot = activeScene->GetRoot();

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            HYP_FORMAT("Scatter {}", assetName),
            Proc<EditorActionFunctions()>(
                [group, sceneRoot, createsGroup, instanceIds, transforms]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [group, sceneRoot, createsGroup, instanceIds, transforms](EditorSubsystem*, EditorProject*)
                            {
                                if (createsGroup)
                                {
                                    sceneRoot->AddChild(group);
                                }

                                for (uint32 index = 0; index < instanceIds.Size(); index++)
                                {
                                    group->AddInstanceWithId(instanceIds[index], transforms[index]);
                                }
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [group, createsGroup, instanceIds](EditorSubsystem*, EditorProject*)
                            {
                                for (InstanceId instanceId : instanceIds)
                                {
                                    group->RemoveInstance(instanceId);
                                }

                                if (createsGroup)
                                {
                                    group->Remove();
                                }
                            })
                    };
                }));

        InitObject(action);
        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(ScatterInstances);

#pragma endregion ScatterInstances

#pragma region SampleTerrainHeight

class EditorCommandSampleTerrainHeight final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSampleTerrainHeight);

public:
    virtual ~EditorCommandSampleTerrainHeight() override = default;

    virtual String GetText() const override
    {
        return "Sample Terrain Height";
    }

    virtual bool AllowedWhileSimulating() const override
    {
        return true;
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        Handle<Scene> activeScene = subsystem->GetActiveScene();

        if (!activeScene.IsValid() || !activeScene->GetWorld() || !activeScene->GetWorld()->GetWorldGrid().IsValid())
        {
            HYP_LOG(Editor, Warning, "SampleTerrainHeight: no world grid");
            return;
        }

        Handle<TerrainWorldGridLayer> terrainLayer;

        for (const Handle<WorldGridLayer>& layer : activeScene->GetWorld()->GetWorldGrid()->GetLayers())
        {
            if ((terrainLayer = DynamicCast<TerrainWorldGridLayer>(layer)).IsValid())
            {
                break;
            }
        }

        if (!terrainLayer.IsValid())
        {
            HYP_LOG(Editor, Warning, "SampleTerrainHeight: no terrain layer");
            return;
        }

        const auto parseFloat = [this](int index)
        {
            float value = 0.0f;

            if (index < NumArguments())
            {
                StringUtil::Parse(GetArgument(index), &value);
            }

            return value;
        };

        if (NumArguments() >= 1 && GetArgument(0) == "grid")
        {
            const Vec2f center { parseFloat(1), parseFloat(2) };
            const float extent = parseFloat(3);
            const int steps = MathUtil::Clamp(int(parseFloat(4)), 1, 64);

            for (int row = 0; row <= steps; row++)
            {
                const float z = center.y - extent + 2.0f * extent * float(row) / float(steps);

                String rowText;

                for (int column = 0; column <= steps; column++)
                {
                    const float x = center.x - extent + 2.0f * extent * float(column) / float(steps);

                    rowText = rowText + HYP_FORMAT("{} ", int(MathUtil::Round(terrainLayer->SampleHeightAtBlocking(Vec2f(x, z)))));
                }

                HYP_LOG(Editor, Info, "SampleTerrainHeight z={}: {}", z, rowText);
            }

            return;
        }

        for (int index = 0; index + 1 < NumArguments(); index += 2)
        {
            const Vec2f position { parseFloat(index), parseFloat(index + 1) };

            HYP_LOG(Editor, Info, "SampleTerrainHeight {} {}: {}", position.x, position.y, terrainLayer->SampleHeightAtBlocking(position));
        }
    }
};

DEFINE_EDITOR_COMMAND(SampleTerrainHeight);

#pragma endregion SampleTerrainHeight

#pragma region NewGroundCover

/// Creates a ground cover asset from a list of prefab types.
///
/// The first argument may optionally be the asset's name; otherwise a unique name is generated.
///
/// Each remaining argument adds a prefab type and accepts up to three parts, separated by colons:
///   - The prefab name (required).
///   - The weight: this type's share of its layer's patches, relative to the layer's other types. Defaults to 1.
///   - The placement: either a splat layer index (defaults to 0), or "paint" to place it using terrain painting.
///
/// Passing --apply additionally assigns the resulting ground cover to the active world's terrain.
///
/// Painted types each get their own layer, named after their prefab, while the remaining types share
/// one layer per splat layer.
class EditorCommandNewGroundCover final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandNewGroundCover);

public:
    virtual ~EditorCommandNewGroundCover() override = default;

    virtual String GetText() const override
    {
        return "New Ground Cover";
    }

    virtual bool AllowedWhileEditingPrefab() const override
    {
        return false;
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();

        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot create ground cover asset!");

            return;
        }

        Name name = Name::Unique("NewGroundCover");
        bool apply = false;

        Array<Pair<ANSIString, GroundCoverType>> typeSpecs;
        Array<uint32> typeSplatLayers;
        Array<bool> typeIsPainted;

        for (int argumentIndex = 0; argumentIndex < NumArguments(); argumentIndex++)
        {
            const String& argument = GetArgument(argumentIndex);

            if (argument == "--apply")
            {
                apply = true;

                continue;
            }

            const Array<String> parts = argument.Split(':');

            if (parts.Size() == 1 && argumentIndex == 0)
            {
                name = CreateNameFromDynamicString(ANSIString(argument));

                continue;
            }

            GroundCoverType type;

            Handle<Prefab> prefab = DynamicCast<Prefab>(GetCurrentAssetRegistry()->GetAsset(AssetBuckets::Prefabs, Name(ANSIString(parts[0]))));

            if (!prefab.IsValid())
            {
                HYP_LOG(Editor, Warning, "EditorCommandNewGroundCover: '{}' is not a prefab", parts[0]);

                continue;
            }

            type.prefab = prefab;

            if (parts.Size() > 1)
            {
                StringUtil::Parse(parts[1], &type.weight);
            }

            uint32 splatLayer = 0;
            const bool isPainted = parts.Size() > 2 && parts[2] == "paint";

            if (parts.Size() > 2 && !isPainted)
            {
                StringUtil::Parse(parts[2], &splatLayer);
            }

            typeSpecs.EmplaceBack(ANSIString(parts[0]), type);
            typeSplatLayers.PushBack(MathUtil::Min(splatLayer, TerrainNumSplatLayers - 1));
            typeIsPainted.PushBack(isPainted);
        }

        Handle<GroundCover> groundCover = MakeHandle<GroundCover>(name);

        for (uint32 specIndex = 0; specIndex < uint32(typeSpecs.Size()); specIndex++)
        {
            if (typeIsPainted[specIndex])
            {
                GroundCoverLayer& paintedLayer = groundCover->layers.EmplaceBack();
                paintedLayer.name = CreateNameFromDynamicString(typeSpecs[specIndex].first);
                paintedLayer.source = GroundCoverSource::Painted;
                paintedLayer.types.PushBack(typeSpecs[specIndex].second);

                continue;
            }

            const auto layerIt = groundCover->layers.FindIf(
                [splatLayer = typeSplatLayers[specIndex]](const GroundCoverLayer& candidate)
                {
                    return candidate.source == GroundCoverSource::SplatLayer && candidate.splatLayer == splatLayer;
                });

            GroundCoverLayer* layer = layerIt != groundCover->layers.End() ? &*layerIt : nullptr;

            if (layer == nullptr)
            {
                GroundCoverLayer& newLayer = groundCover->layers.EmplaceBack();
                newLayer.splatLayer = typeSplatLayers[specIndex];

                layer = &newLayer;
            }

            layer->types.PushBack(typeSpecs[specIndex].second);
        }

        InitObject(groundCover);

        Array<Handle<TerrainWorldGridLayer>> terrainLayers;

        if (apply)
        {
            if (Handle<Scene> activeScene = subsystem->GetActiveScene(); activeScene.IsValid() && activeScene->GetWorld())
            {
                if (const Handle<WorldGrid>& worldGrid = activeScene->GetWorld()->GetWorldGrid(); worldGrid.IsValid())
                {
                    for (const Handle<WorldGridLayer>& worldGridLayer : worldGrid->GetLayers())
                    {
                        if (Handle<TerrainWorldGridLayer> terrainLayer = DynamicCast<TerrainWorldGridLayer>(worldGridLayer); terrainLayer.IsValid())
                        {
                            terrainLayers.PushBack(std::move(terrainLayer));
                        }
                    }
                }
            }
        }

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [groundCover, terrainLayers]() -> EditorActionFunctions
                {
                    Array<Handle<GroundCover>> previousGroundCovers;

                    for (const Handle<TerrainWorldGridLayer>& terrainLayer : terrainLayers)
                    {
                        previousGroundCovers.PushBack(terrainLayer->GetGroundCover());
                    }

                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [groundCover, terrainLayers](EditorSubsystem*, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->PutAssetUnique(groundCover);

                                for (const Handle<TerrainWorldGridLayer>& terrainLayer : terrainLayers)
                                {
                                    terrainLayer->SetGroundCover(groundCover);
                                }
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [groundCover, terrainLayers, previousGroundCovers](EditorSubsystem*, EditorProject*)
                            {
                                for (uint32 index = 0; index < uint32(terrainLayers.Size()); index++)
                                {
                                    terrainLayers[index]->SetGroundCover(previousGroundCovers[index]);
                                }

                                GetCurrentAssetRegistry()->RemoveAsset(groundCover);
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(NewGroundCover);

#pragma endregion NewGroundCover

#pragma region DropAssetOnEntity

class EditorCommandDropAssetOnEntity final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandDropAssetOnEntity);

public:
    virtual ~EditorCommandDropAssetOnEntity() override = default;

    virtual String GetText() const override
    {
        return "Drop Asset On Entity";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 3)
        {
            HYP_LOG(Editor, Warning, "EditorCommandDropAssetOnEntity requires bucket index, asset name and either a node UUID or viewport coordinates");
            return;
        }

        uint32 bucketIndex = 0;
        if (!StringUtil::Parse(GetArgument(0), &bucketIndex) || bucketIndex == AssetBuckets::None.GetIndex())
        {
            HYP_LOG(Editor, Warning, "EditorCommandDropAssetOnEntity: invalid bucket index '{}'", GetArgument(0));
            return;
        }

        const ANSIString assetName = GetArgument(1);

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandDropAssetOnEntity: no project loaded");
            return;
        }

        Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandDropAssetOnEntity: no active scene");
            return;
        }

        Handle<AssetObject> asset = GetCurrentAssetRegistry()->GetAsset(*AssetBuckets::AllBuckets[bucketIndex], Name(assetName));

        if (!EditorEntityAssetDrop::TargetsEntity(asset.Get()))
        {
            HYP_LOG(Editor, Warning, "EditorCommandDropAssetOnEntity: asset '{}' in bucket {} can't be applied to an entity", assetName, GetAssetBucketName(bucketIndex));
            return;
        }

        Node* targetNode = nullptr;

        if (NumArguments() >= 4)
        {
            float screenX = 0.5f;
            float screenY = 0.5f;
            StringUtil::Parse(GetArgument(2), &screenX);
            StringUtil::Parse(GetArgument(3), &screenY);

            targetNode = subsystem->PickNodeAtViewport(Vec2f(screenX, screenY));
        }
        else
        {
            targetNode = ResolveNodeUuidArgument(subsystem, GetArgument(2));
        }

        Entity* targetEntity = DynamicCast<Entity>(targetNode);

        if (!targetEntity || targetEntity->GetScene() != activeScene.Get() || !EditorEntityAssetDrop::CanApplyToEntity(asset.Get(), targetEntity))
        {
            HYP_LOG(Editor, Info, "EditorCommandDropAssetOnEntity: no entity in the active scene under the drop position accepts '{}'", assetName);
            return;
        }

        Handle<EditorActionBase> action = EditorEntityAssetDrop::CreateApplyAction(subsystem, asset, MakeStrongRef(targetEntity));

        if (!action.IsValid())
        {
            return;
        }

        subsystem->GetSceneActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(DropAssetOnEntity);

#pragma endregion DropAssetOnEntity

#pragma region Prefab

struct MakePrefabNodeRecord
{
    Handle<Node> node;
    WeakHandle<Node> originalParent;
    Transform originalLocalTransform;
    Vec3f worldTranslation;
    Vec3f worldScale;
    Quat4f worldRotation;
};

static bool ResolvePrefabSourceNodes(EditorSubsystem* subsystem, Scene* activeScene, const String& nodeUuidArg, Array<Handle<Node>>& outValidNodes)
{
    Array<Handle<Node>> sourceNodes;

    if (!nodeUuidArg.Empty())
    {
        Node* node = ResolveNodeUuidArgument(subsystem, nodeUuidArg);

        if (!node)
        {
            HYP_LOG(Editor, Warning, "Prefab command: could not find node with UUID '{}'", nodeUuidArg);
            return false;
        }

        sourceNodes.PushBack(MakeStrongRef(node));
    }
    else
    {
        sourceNodes = subsystem->GetSelectedNodes();

        if (sourceNodes.Empty())
        {
            Handle<Node> focusedNode = subsystem->GetFocusedNode();

            if (focusedNode.IsValid())
            {
                sourceNodes.PushBack(focusedNode);
            }
        }
    }

    // Filter: skip nodes whose parent is also in the source set (they'll be regrouped implicitly)
    Array<Handle<Node>> topLevelNodes;
    for (const Handle<Node>& node : sourceNodes)
    {
        if (!node.IsValid())
        {
            continue;
        }

        bool hasAncestorInSet = false;
        for (Node* p = node->GetParent(); p; p = p->GetParent())
        {
            for (const Handle<Node>& other : sourceNodes)
            {
                if (other.Get() == p)
                {
                    hasAncestorInSet = true;
                    break;
                }
            }
            if (hasAncestorInSet)
            {
                break;
            }
        }

        if (!hasAncestorInSet)
        {
            topLevelNodes.PushBack(node);
        }
    }

    // Exclude nodes that cannot be regrouped: the scene root, a parentless node, or one with a locked transform
    for (const Handle<Node>& node : topLevelNodes)
    {
        if (!node->GetParent() || node.Get() == activeScene->GetRoot().Get())
        {
            HYP_LOG(Editor, Warning, "Prefab command: skipping scene root or parentless node '{}'", node->GetName());
            continue;
        }

        if (node->IsTransformLocked())
        {
            HYP_LOG(Editor, Warning, "Prefab command: skipping transform-locked node '{}'", node->GetName());
            continue;
        }

        outValidNodes.PushBack(node);
    }

    return outValidNodes.Any();
}

class EditorCommandMakePrefab final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandMakePrefab);

public:
    virtual ~EditorCommandMakePrefab() override = default;

    virtual String GetText() const override
    {
        return "Save as Prefab";
    }

    virtual bool AllowedWhileEditingPrefab() const override
    {
        return false;
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 1 || GetArgument(0).Empty())
        {
            HYP_LOG(Editor, Error, "EditorCommandMakePrefab: missing required prefab name argument");
            return;
        }

        const ANSIString prefabNameStr = GetArgument(0);
        const Name prefabName = Name(prefabNameStr);

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandMakePrefab: no project loaded");
            return;
        }

        Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandMakePrefab: no active scene");
            return;
        }

        Array<Handle<Node>> validNodes;
        if (!ResolvePrefabSourceNodes(subsystem, activeScene.Get(), NumArguments() >= 2 ? GetArgument(1) : String::empty, validNodes))
        {
            HYP_LOG(Editor, Warning, "EditorCommandMakePrefab: no valid nodes to make a prefab from");
            return;
        }

        // Capture per-node undo data and world transforms before anything is mutated
        Array<MakePrefabNodeRecord> records;
        Vec3f centroid = Vec3f::Zero();

        for (const Handle<Node>& node : validNodes)
        {
            MakePrefabNodeRecord record;
            record.node = node;
            record.originalParent = MakeWeakRef(node->GetParent());
            record.originalLocalTransform = node->GetLocalTransform();
            record.worldTranslation = node->GetWorldTranslation();
            record.worldScale = node->GetWorldScale();
            record.worldRotation = node->GetWorldRotation();

            centroid += record.worldTranslation;

            records.PushBack(record);
        }

        centroid /= float(records.Size());

        Node* commonParent = records[0].originalParent.Lock().Get();
        bool allSameParent = commonParent != nullptr;

        if (allSameParent)
        {
            for (const MakePrefabNodeRecord& record : records)
            {
                if (record.originalParent.Lock().Get() != commonParent)
                {
                    allSameParent = false;
                    break;
                }
            }
        }

        const Handle<Node> parentForGroup = allSameParent ? MakeStrongRef(commonParent) : activeScene->GetRoot();

        // Always wrap in a fresh group node (even for a single node) so the instance root is never one of
        // the user's own nodes - "Add to Prefab" relies on instances being plain containers it can append to
        Handle<Node> groupNode = MakeHandle<Node>();
        groupNode->SetName(prefabName);
        InitObject(groupNode);

        // Build the detached object that backs the asset - never the live scene nodes themselves,
        // matching every other Prefab in the engine (its root is always a template, never simultaneously live)
        Handle<Node> prefabRoot = MakeHandle<Node>();
        prefabRoot->SetName(prefabName);
        InitObject(prefabRoot);

        for (const MakePrefabNodeRecord& record : records)
        {
            Handle<Node> clonedChild = record.node->Clone();

            if (!clonedChild.IsValid())
            {
                HYP_LOG(Editor, Error, "EditorCommandMakePrefab: failed to clone node '{}'", record.node->GetName());
                continue;
            }

            prefabRoot->AddChild(clonedChild);
            clonedChild->SetWorldScale(record.worldScale);
            clonedChild->SetWorldRotation(record.worldRotation);
            clonedChild->SetWorldTranslation(record.worldTranslation - centroid);
        }

        Handle<Prefab> prefab = MakeHandle<Prefab>(prefabName, prefabRoot);
        InitObject(prefab);

        Array<Handle<Node>> previousSelectedNodes = subsystem->GetSelectedNodes();
        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [records, groupNode, centroid, parentForGroup, prefab,
                    previousSelectedNodes, previousFocusedNode]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [records, groupNode, centroid, parentForGroup, prefab](
                                EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->PutAssetsDeep(prefab);
                                prefab->SyncRootName();

                                groupNode->SetName(prefab->GetName());

                                Prefab::TagAsPrefabInstance(groupNode.Get(), prefab->GetUUID());

                                parentForGroup->AddChild(groupNode);
                                groupNode->SetWorldScale(Vec3f::One());
                                groupNode->SetWorldRotation(Quat4f::Identity());
                                groupNode->SetWorldTranslation(centroid);

                                for (const MakePrefabNodeRecord& record : records)
                                {
                                    record.node->Remove();
                                    groupNode->AddChild(record.node);
                                    record.node->SetWorldScale(record.worldScale);
                                    record.node->SetWorldRotation(record.worldRotation);
                                    record.node->SetWorldTranslation(record.worldTranslation);
                                }

                                editorSubsystem->SetSelectedNodes({ groupNode });
                                editorSubsystem->SetFocusedNode(groupNode, true);

                                editorSubsystem->OnAssetsChanged(AssetBuckets::Prefabs.GetIndex());
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [records, groupNode, prefab, previousSelectedNodes, previousFocusedNode](
                                EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->RemoveAsset(prefab);
                                editorSubsystem->OnAssetsChanged(AssetBuckets::Prefabs.GetIndex());

                                for (int i = records.Size() - 1; i >= 0; --i)
                                {
                                    const MakePrefabNodeRecord& record = records[i];

                                    record.node->Remove();

                                    Handle<Node> originalParent = record.originalParent.Lock();
                                    if (originalParent.IsValid())
                                    {
                                        originalParent->AddChild(record.node);
                                        record.node->SetLocalTransform(record.originalLocalTransform);
                                    }
                                }

                                groupNode->Remove();

                                editorSubsystem->SetSelectedNodes(previousSelectedNodes);

                                if (Handle<Node> focusedNode = previousFocusedNode.Lock(); focusedNode.IsValid())
                                {
                                    editorSubsystem->SetFocusedNode(focusedNode, true);
                                }
                            })
                    };
                }));

        InitObject(action);
        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(MakePrefab);

struct AddToPrefabNodeRecord
{
    Handle<Node> node;
    WeakHandle<Node> originalParent;
    Handle<Node> templateClone;
};

static bool IsOrContainsPrefabInstance(const Node* node, const UUID& prefabUUID)
{
    if (Prefab::GetSourcePrefabUUID(node) == prefabUUID)
    {
        return true;
    }

    for (Node* descendant : node->GetDescendants())
    {
        if (Prefab::GetSourcePrefabUUID(descendant) == prefabUUID)
        {
            return true;
        }
    }

    return false;
}

// Scene Hierarchy context menu action: moves the current selection into an existing Prefab. The nodes are
// cloned into the Prefab's root and into every live instance of it, then the originals are removed.
class EditorCommandAddToPrefab final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAddToPrefab);

public:
    virtual ~EditorCommandAddToPrefab() override = default;

    virtual String GetText() const override
    {
        return "Add to Prefab";
    }

    virtual bool AllowedWhileEditingPrefab() const override
    {
        return false;
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 1 || GetArgument(0).Empty())
        {
            HYP_LOG(Editor, Warning, "EditorCommandAddToPrefab requires a prefab name argument");
            return;
        }

        const ANSIString prefabNameArg = GetArgument(0);

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandAddToPrefab: no project loaded");
            return;
        }

        Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandAddToPrefab: no active scene");
            return;
        }

        Handle<Prefab> prefab = GetCurrentAssetRegistry()->GetAsset<Prefab>(AssetBuckets::Prefabs, Name(prefabNameArg));

        if (!prefab.IsValid() || !prefab->GetRoot().IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandAddToPrefab: could not find existing prefab '{}'", prefabNameArg);
            return;
        }

        Array<Handle<Node>> validNodes;
        if (!ResolvePrefabSourceNodes(subsystem, activeScene.Get(), String::empty, validNodes))
        {
            HYP_LOG(Editor, Warning, "EditorCommandAddToPrefab: no valid nodes to add to prefab '{}'", prefabNameArg);
            return;
        }

        const UUID prefabUUID = prefab->GetUUID();

        Array<Handle<Node>> sourceNodes;

        for (const Handle<Node>& node : validNodes)
        {
            if (IsOrContainsPrefabInstance(node.Get(), prefabUUID))
            {
                HYP_LOG(Editor, Warning, "EditorCommandAddToPrefab: skipping '{}', it is or contains an instance of prefab '{}'", node->GetName(), prefabNameArg);
                continue;
            }

            sourceNodes.PushBack(node);
        }

        if (sourceNodes.Empty())
        {
            HYP_LOG(Editor, Warning, "EditorCommandAddToPrefab: no valid nodes to add to prefab '{}'", prefabNameArg);
            return;
        }

        const Array<Handle<Node>> instances = prefab->FindLiveInstances(currentProject->GetWorld().Get());

        Vec3f centroid = Vec3f::Zero();

        for (const Handle<Node>& node : sourceNodes)
        {
            centroid += node->GetWorldTranslation();
        }

        centroid /= float(sourceNodes.Size());

        // Nodes are re-expressed relative to one instance so they stay put visually there: the instance they
        // already live under if any, otherwise the closest one
        Handle<Node> referenceInstance;

        for (const Handle<Node>& instance : instances)
        {
            if (AnyOf(sourceNodes, [&instance](const Handle<Node>& node) { return node->IsOrHasParent(instance.Get()); }))
            {
                referenceInstance = instance;
                break;
            }
        }

        if (!referenceInstance.IsValid())
        {
            float closestDistanceSquared = 0.0f;

            for (const Handle<Node>& instance : instances)
            {
                const float distanceSquared = instance->GetWorldTranslation().DistanceSquared(centroid);

                if (!referenceInstance.IsValid() || distanceSquared < closestDistanceSquared)
                {
                    referenceInstance = instance;
                    closestDistanceSquared = distanceSquared;
                }
            }
        }

        Array<AddToPrefabNodeRecord> records;
        Array<Pair<Handle<Node>, Handle<Node>>> instanceClones;
        Array<Handle<Node>> referenceClones;

        for (const Handle<Node>& node : sourceNodes)
        {
            Transform prefabLocalTransform;

            if (referenceInstance.IsValid())
            {
                prefabLocalTransform = Transform(
                    referenceInstance->GetWorldMatrix().Inverse().TransformVector(node->GetWorldTranslation()),
                    node->GetWorldScale() / referenceInstance->GetWorldScale(),
                    referenceInstance->GetWorldRotation().Inverse() * node->GetWorldRotation());
            }
            else
            {
                prefabLocalTransform = Transform(node->GetWorldTranslation() - centroid, node->GetWorldScale(), node->GetWorldRotation());
            }

            Handle<Node> templateClone = node->Clone();

            if (!templateClone.IsValid())
            {
                HYP_LOG(Editor, Error, "EditorCommandAddToPrefab: failed to clone node '{}'", node->GetName());
                continue;
            }

            templateClone->SetLocalTransform(prefabLocalTransform);

            for (const Handle<Node>& instance : instances)
            {
                Handle<Node> instanceClone = node->Clone();

                if (!instanceClone.IsValid())
                {
                    HYP_LOG(Editor, Error, "EditorCommandAddToPrefab: failed to clone node '{}'", node->GetName());
                    continue;
                }

                instanceClone->SetLocalTransform(prefabLocalTransform);
                instanceClones.PushBack({ instance, instanceClone });

                if (instance == referenceInstance)
                {
                    referenceClones.PushBack(instanceClone);
                }
            }

            AddToPrefabNodeRecord record;
            record.node = node;
            record.originalParent = MakeWeakRef(node->GetParent());
            record.templateClone = templateClone;

            records.PushBack(record);
        }

        if (records.Empty())
        {
            HYP_LOG(Editor, Warning, "EditorCommandAddToPrefab: nothing was cloned to append to prefab '{}'", prefabNameArg);
            return;
        }

        Array<Handle<Node>> previousSelectedNodes = subsystem->GetSelectedNodes();
        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();

        const uint32 previousRevision = prefab->GetRevision();

        Array<Pair<Handle<Node>, uint32>> instanceRevisions;

        for (const Handle<Node>& instance : instances)
        {
            instanceRevisions.PushBack({ instance, Prefab::GetInstanceRevision(instance.Get()) });
        }

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            HYP_FORMAT("Add to Prefab {}", prefabNameArg),
            Proc<EditorActionFunctions()>(
                [prefab, records, instanceClones, referenceClones, previousSelectedNodes, previousFocusedNode, previousRevision, instanceRevisions]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [prefab, records, instanceClones, referenceClones, previousRevision, instanceRevisions](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                prefab->SetRevision(previousRevision + 1);

                                for (const Pair<Handle<Node>, uint32>& instanceRevision : instanceRevisions)
                                {
                                    Prefab::TagAsPrefabInstance(instanceRevision.first.Get(), prefab->GetUUID(), previousRevision + 1);
                                }

                                Handle<Node> existingRoot = prefab->GetRoot();

                                for (const AddToPrefabNodeRecord& record : records)
                                {
                                    record.node->Remove();
                                    existingRoot->AddChild(record.templateClone);
                                }

                                for (const Pair<Handle<Node>, Handle<Node>>& pair : instanceClones)
                                {
                                    pair.first->AddChild(pair.second);
                                }

                                editorSubsystem->SetSelectedNodes(referenceClones);

                                if (referenceClones.Any())
                                {
                                    editorSubsystem->SetFocusedNode(referenceClones[0], true);
                                }
                                else
                                {
                                    editorSubsystem->SetFocusedNode(nullptr, true);
                                }

                                GetCurrentAssetRegistry()->PutAssetsDeep(prefab);
                                prefab->MarkDirty();

                                prefab->OnPrefabChanged(prefab.Get());
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [prefab, records, instanceClones, previousSelectedNodes, previousFocusedNode, previousRevision, instanceRevisions](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                prefab->SetRevision(previousRevision);

                                for (const Pair<Handle<Node>, uint32>& instanceRevision : instanceRevisions)
                                {
                                    Prefab::TagAsPrefabInstance(instanceRevision.first.Get(), prefab->GetUUID(), instanceRevision.second);
                                }

                                for (const Pair<Handle<Node>, Handle<Node>>& pair : instanceClones)
                                {
                                    pair.second->Remove();
                                }

                                for (int i = records.Size() - 1; i >= 0; --i)
                                {
                                    const AddToPrefabNodeRecord& record = records[i];

                                    record.templateClone->Remove();

                                    if (Handle<Node> originalParent = record.originalParent.Lock(); originalParent.IsValid())
                                    {
                                        originalParent->AddChild(record.node);
                                    }
                                }

                                editorSubsystem->SetSelectedNodes(previousSelectedNodes);

                                if (Handle<Node> focusedNode = previousFocusedNode.Lock(); focusedNode.IsValid())
                                {
                                    editorSubsystem->SetFocusedNode(focusedNode, true);
                                }

                                prefab->MarkDirty();

                                prefab->OnPrefabChanged(prefab.Get());
                            })
                    };
                }));

        InitObject(action);
        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(AddToPrefab);

class EditorCommandSavePrefab final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSavePrefab);

public:
    virtual ~EditorCommandSavePrefab() override = default;

    virtual String GetText() const override
    {
        return "Sync Prefab";
    }

    virtual bool AllowedWhileEditingPrefab() const override
    {
        return false;
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandSavePrefab: no project loaded");
            return;
        }

        Node* rawNode = ResolveNodeUuidArgument(subsystem, NumArguments() >= 1 ? GetArgument(0) : String::empty);
        Handle<Node> node = rawNode ? MakeStrongRef(rawNode) : subsystem->GetFocusedNode();

        if (!node.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandSavePrefab: no node to save");
            return;
        }

        const UUID prefabUUID = Prefab::GetSourcePrefabUUID(node.Get());

        if (prefabUUID == UUID::Invalid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandSavePrefab: node '{}' is not a Prefab instance", node->GetName());
            return;
        }

        Handle<Prefab> prefab = Prefab::FindByUUID(prefabUUID);

        if (!prefab.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandSavePrefab: could not find source Prefab for node '{}'", node->GetName());
            return;
        }

        Handle<Node> newRoot = node->Clone();

        if (!newRoot.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandSavePrefab: failed to clone node '{}'", node->GetName());
            return;
        }

        // Author at local origin
        Transform rootLocalTransform = newRoot->GetLocalTransform();
        rootLocalTransform.SetTranslation(Vec3f::Zero());
        newRoot->SetLocalTransform(rootLocalTransform);

        Handle<Node> previousRoot = prefab->GetRoot();

        const uint32 previousRevision = prefab->GetRevision();
        const uint32 previousNodeRevision = Prefab::GetInstanceRevision(node.Get());

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            HYP_FORMAT("Sync Prefab {}", prefab->GetName()),
            Proc<EditorActionFunctions()>(
                [prefab, node, newRoot, previousRoot, previousRevision, previousNodeRevision]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [prefab, node, newRoot, previousRevision](EditorSubsystem*, EditorProject*)
                            {
                                prefab->SetRoot(newRoot);
                                prefab->SetRevision(previousRevision + 1);

                                Prefab::TagAsPrefabInstance(node.Get(), prefab->GetUUID(), previousRevision + 1);

                                GetCurrentAssetRegistry()->PutAssetsDeep(prefab);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [prefab, node, previousRoot, previousRevision, previousNodeRevision](EditorSubsystem*, EditorProject*)
                            {
                                prefab->SetRoot(previousRoot);
                                prefab->SetRevision(previousRevision);

                                Prefab::TagAsPrefabInstance(node.Get(), prefab->GetUUID(), previousNodeRevision);
                            })
                    };
                }));

        InitObject(action);
        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(SavePrefab);

#pragma endregion Prefab

#pragma region Template

class EditorCommandSaveAsTemplate final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSaveAsTemplate);

public:
    virtual ~EditorCommandSaveAsTemplate() override = default;

    virtual String GetText() const override
    {
        return "Save as Template";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 1 || GetArgument(0).Empty())
        {
            HYP_LOG(Editor, Error, "EditorCommandSaveAsTemplate: missing required template name argument");
            return;
        }

        const Name templateName = Name(GetArgument(0).ToAnsi());

        Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandSaveAsTemplate: no active scene");
            return;
        }

        Array<Handle<Node>> validNodes;
        if (!ResolvePrefabSourceNodes(subsystem, activeScene.Get(), NumArguments() >= 2 ? GetArgument(1) : String::empty, validNodes))
        {
            HYP_LOG(Editor, Warning, "EditorCommandSaveAsTemplate: no valid nodes to make a template from");
            return;
        }

        Handle<Node> templateRoot;

        if (validNodes.Size() == 1)
        {
            const Handle<Node>& node = validNodes[0];

            templateRoot = node->Clone();

            if (!templateRoot.IsValid())
            {
                HYP_LOG(Editor, Error, "EditorCommandSaveAsTemplate: failed to clone node '{}'", node->GetName());
                return;
            }

            templateRoot->SetWorldScale(node->GetWorldScale());
            templateRoot->SetWorldRotation(node->GetWorldRotation());
            templateRoot->SetWorldTranslation(Vec3f::Zero());
        }
        else
        {
            Vec3f centroid = Vec3f::Zero();

            for (const Handle<Node>& node : validNodes)
            {
                centroid += node->GetWorldTranslation();
            }

            centroid /= float(validNodes.Size());

            templateRoot = MakeHandle<Node>();
            templateRoot->SetName(templateName);
            InitObject(templateRoot);

            for (const Handle<Node>& node : validNodes)
            {
                Handle<Node> clonedChild = node->Clone();

                if (!clonedChild.IsValid())
                {
                    HYP_LOG(Editor, Error, "EditorCommandSaveAsTemplate: failed to clone node '{}'", node->GetName());
                    continue;
                }

                templateRoot->AddChild(clonedChild);
                clonedChild->SetWorldScale(node->GetWorldScale());
                clonedChild->SetWorldRotation(node->GetWorldRotation());
                clonedChild->SetWorldTranslation(node->GetWorldTranslation() - centroid);
            }
        }

        if (Result saveResult = EditorTemplateLibrary::SaveTemplate(templateName, templateRoot); saveResult.HasError())
        {
            HYP_LOG(Editor, Error, "EditorCommandSaveAsTemplate: {}", saveResult.GetError().GetMessage());
        }
    }
};

DEFINE_EDITOR_COMMAND(SaveAsTemplate);

class EditorCommandSavePrefabAsTemplate final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSavePrefabAsTemplate);

public:
    virtual ~EditorCommandSavePrefabAsTemplate() override = default;

    virtual String GetText() const override
    {
        return "Save Prefab as Template";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 2 || GetArgument(0).Empty() || GetArgument(1).Empty())
        {
            HYP_LOG(Editor, Error, "EditorCommandSavePrefabAsTemplate: expected a template name and a prefab name");
            return;
        }

        const Name templateName = Name(GetArgument(0).ToAnsi());
        const ANSIString prefabNameArg = GetArgument(1);

        Handle<Prefab> prefab = GetCurrentAssetRegistry()->GetAsset<Prefab>(AssetBuckets::Prefabs, Name(prefabNameArg));

        if (!prefab.IsValid() || !prefab->GetRoot().IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandSavePrefabAsTemplate: could not find prefab '{}'", prefabNameArg);
            return;
        }

        Handle<Node> templateRoot = prefab->GetRoot()->Clone();

        if (!templateRoot.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandSavePrefabAsTemplate: failed to clone the root of prefab '{}'", prefabNameArg);
            return;
        }

        if (Result saveResult = EditorTemplateLibrary::SaveTemplate(templateName, templateRoot); saveResult.HasError())
        {
            HYP_LOG(Editor, Error, "EditorCommandSavePrefabAsTemplate: {}", saveResult.GetError().GetMessage());
        }
    }
};

DEFINE_EDITOR_COMMAND(SavePrefabAsTemplate);

#pragma endregion Template

#pragma region DeleteAsset

class EditorCommandDeleteAsset final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandDeleteAsset);

public:
    virtual ~EditorCommandDeleteAsset() override = default;

    virtual String GetText() const override
    {
        return "Delete Asset";
    }

    /// Arguments are pairs of "<bucketIndex> <assetName>"
    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 2)
        {
            HYP_LOG(Editor, Warning, "EditorCommandDeleteAsset requires bucket index and asset name");
            return;
        }

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandDeleteAsset: no project loaded");
            return;
        }

        Array<Handle<AssetObject>> assets;

        for (int i = 0; i + 1 < NumArguments(); i += 2)
        {
            uint32 bucketIndex = 0;
            if (!StringUtil::Parse(GetArgument(i), &bucketIndex) || bucketIndex == AssetBuckets::None.GetIndex() || bucketIndex >= MaxAssetBuckets)
            {
                HYP_LOG(Editor, Warning, "EditorCommandDeleteAsset: invalid bucket index '{}'", GetArgument(i));
                continue;
            }

            const ANSIString assetName = GetArgument(i + 1);

            Handle<AssetObject> asset = GetCurrentAssetRegistry()->GetAsset(*AssetBuckets::AllBuckets[bucketIndex], Name(assetName));
            if (!asset.IsValid())
            {
                HYP_LOG(Editor, Warning, "EditorCommandDeleteAsset: asset '{}' in bucket {} is not valid", assetName, GetAssetBucketName(bucketIndex));
                continue;
            }

            assets.PushBack(std::move(asset));
        }

        if (assets.Empty())
        {
            return;
        }

        const String description = assets.Size() == 1
            ? String("the asset ") + *assets[0]->GetName()
            : HYP_FORMAT("{} assets", assets.Size());

        bool cancelled = false;

        // Show confirm box
        SystemMessageBox(MessageBoxType::INFO)
            .Title("Confirm Delete")
            .Text("Are you sure you want to delete " + description + "?")
            .Button("Delete", []()
                    {
                    })
            .Button("Cancel", [&cancelled]()
                    {
                        cancelled = true;
                    })
            .Show();

        if (cancelled)
        {
            return;
        }

        Handle<FunctionalEditorAction> action = MakeDeleteAssetsAction(
            assets.Size() == 1 ? HYP_FORMAT("Delete {}", assets[0]->GetName()) : HYP_FORMAT("Delete {} assets", assets.Size()),
            std::move(assets),
            {},
            {});

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }

    /// Removes the assets (and their manifests) and, if given, a folder subtree. Action is reversible
    static Handle<FunctionalEditorAction> MakeDeleteAssetsAction(String text, Array<Handle<AssetObject>> assets, String folderPath, Array<String> folderSubtree)
    {
        Array<AssetPath> assetPaths;
        Array<FilePath> manifestPaths;
        Array<String> assetFolderPaths;

        const Handle<EditorProject> project = g_editorState->GetCurrentProject();
        EditorAssetFolders* assetFolders = project.IsValid() ? project->GetAssetFolders().Get() : nullptr;

        for (const Handle<AssetObject>& asset : assets)
        {
            assetPaths.PushBack(asset->GetPath());
            manifestPaths.PushBack(GetCurrentAssetRegistry()->GetManifestPath(asset->GetPath()));
            assetFolderPaths.PushBack(assetFolders ? assetFolders->GetAssetFolder(asset->GetPath()) : String::empty);
        }

        return MakeHandle<FunctionalEditorAction>(
            std::move(text),
            Proc<EditorActionFunctions()>(
                [assets, assetPaths, manifestPaths, assetFolderPaths, folderPath, folderSubtree]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [assetPaths, manifestPaths, folderPath](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                Set<uint32, EditorAllocator> changedBuckets;

                                for (size_t i = 0; i < assetPaths.Size(); i++)
                                {
                                    const AssetBucket& bucket = assetPaths[i].GetBucket();

                                    GetCurrentAssetRegistry()->RemoveAsset(bucket, assetPaths[i].assetName);

                                    if (manifestPaths[i].Exists())
                                    {
                                        manifestPaths[i].Remove();
                                    }

                                    changedBuckets.Insert(bucket.GetIndex());
                                }

                                if (folderPath.Any() && project != nullptr)
                                {
                                    project->GetAssetFolders()->RemoveFolder(folderPath);
                                }

                                for (uint32 bucketIndex : changedBuckets)
                                {
                                    editorSubsystem->OnAssetsChanged(bucketIndex);
                                }
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [assets, assetPaths, assetFolderPaths, folderSubtree](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                Set<uint32, EditorAllocator> changedBuckets;

                                if (project != nullptr)
                                {
                                    for (const String& folder : folderSubtree)
                                    {
                                        project->GetAssetFolders()->CreateFolder(folder);
                                    }
                                }

                                for (size_t i = 0; i < assets.Size(); i++)
                                {
                                    const AssetBucket& bucket = assetPaths[i].GetBucket();

                                    GetCurrentAssetRegistry()->PutAsset(bucket, assets[i]);

                                    if (project != nullptr)
                                    {
                                        project->GetAssetFolders()->SetAssetFolder(assetPaths[i], assetFolderPaths[i]);
                                    }

                                    changedBuckets.Insert(bucket.GetIndex());
                                }

                                for (uint32 bucketIndex : changedBuckets)
                                {
                                    editorSubsystem->OnAssetsChanged(bucketIndex);
                                }
                            })
                    };
                }));
    }
};

DEFINE_EDITOR_COMMAND(DeleteAsset);

#pragma endregion DeleteAsset

#pragma region RenameAsset

class EditorCommandRenameAsset final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandRenameAsset);

public:
    virtual ~EditorCommandRenameAsset() override = default;

    virtual String GetText() const override
    {
        return m_text.Length() ? m_text : EditorCommandBase::GetText();
    }

    /// Arguments are "<bucketIndex> <assetName> <newName>"
    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 3)
        {
            HYP_LOG(Editor, Warning, "EditorCommandRenameAsset requires bucket index, asset name and new name");
            return;
        }

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "no project loaded");
            return;
        }

        uint32 bucketIndex = 0;
        if (!StringUtil::Parse(GetArgument(0), &bucketIndex) || bucketIndex == AssetBuckets::None.GetIndex() || bucketIndex >= MaxAssetBuckets)
        {
            HYP_LOG(Editor, Warning, "invalid bucket index '{}'", GetArgument(0));
            return;
        }

        const AssetBucket& bucket = *AssetBuckets::AllBuckets[bucketIndex];

        const ANSIString assetName = GetArgument(1);

        Handle<AssetObject> asset = GetCurrentAssetRegistry()->GetAsset(bucket, Name(assetName));
        if (!asset.IsValid())
        {
            HYP_LOG(Editor, Warning, "asset '{}' in bucket {} is not valid", assetName, GetAssetBucketName(bucketIndex));
            return;
        }

        const Name previousName = asset->GetName();
        const Name newName = GetCurrentAssetRegistry()->SanitizeAssetName(GetArgument(2));

        if (newName == previousName)
        {
            return;
        }

        if (GetCurrentAssetRegistry()->HasAsset(bucket, newName))
        {
            HYP_LOG(Editor, Warning, "an asset named '{}' already exists in bucket {}", newName, GetAssetBucketName(bucketIndex));
            return;
        }

        m_text = HYP_FORMAT("Rename {} to {}", previousName, newName);

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [asset, previousName, newName]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [asset, previousName, newName](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                ApplyRename(editorSubsystem, asset, previousName, newName);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [asset, previousName, newName](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                ApplyRename(editorSubsystem, asset, newName, previousName);
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }

private:
    static void ApplyRename(EditorSubsystem* editorSubsystem, const Handle<AssetObject>& asset, Name fromName, Name toName)
    {
        const AssetBucket& bucket = asset->GetPath().GetBucket();

        if (GetCurrentAssetRegistry()->HasAsset(bucket, toName))
        {
            HYP_LOG(Editor, Warning, "Cannot rename asset '{}': an asset named '{}' already exists in bucket {}", fromName, toName, bucket.GetName());
            return;
        }

        if (Result renameResult = asset->Rename(toName); renameResult.HasError())
        {
            HYP_LOG(Editor, Error, "Failed to rename asset '{}' to '{}': {}", fromName, toName, renameResult.GetError().GetMessage());
            return;
        }

        // the registry keeps the old name until the next save otherwise, and the content browser lists from the registry
        GetCurrentAssetRegistry()->SyncAssetName(bucket, fromName, asset->GetName());

        editorSubsystem->OnAssetsChanged(bucket.GetIndex());
    }

    String m_text;
};

DEFINE_EDITOR_COMMAND(RenameAsset);

#pragma endregion RenameAsset

#pragma region DeleteAssetFolder

class EditorCommandDeleteAssetFolder final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandDeleteAssetFolder);

public:
    virtual ~EditorCommandDeleteAssetFolder() override = default;

    virtual String GetText() const override
    {
        return "Delete Folder";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandDeleteAssetFolder: no project loaded");
            return;
        }

        String joinedArguments;

        for (int i = 0; i < NumArguments(); i++)
        {
            if (i != 0)
            {
                joinedArguments += ' ';
            }

            joinedArguments += GetArgument(i);
        }

        EditorAssetFolders& assetFolders = *currentProject->GetAssetFolders();

        const String folderPath = EditorAssetFolders::NormalizeFolderPath(joinedArguments);

        if (folderPath.Empty() || !assetFolders.HasFolder(folderPath))
        {
            HYP_LOG(Editor, Warning, "EditorCommandDeleteAssetFolder: no folder '{}'", joinedArguments);
            return;
        }

        Array<Handle<AssetObject>> assets;

        for (const AssetPath& assetPath : assetFolders.GetAssetsInFolder(folderPath, /* recursive */ true))
        {
            if (Handle<AssetObject> asset = GetCurrentAssetRegistry()->GetAsset(assetPath.GetBucket(), assetPath.assetName); asset.IsValid())
            {
                assets.PushBack(std::move(asset));
            }
        }

        Array<String> folderSubtree;

        for (const String& folder : assetFolders.GetAllFolders())
        {
            if (EditorAssetFolders::IsSameOrDescendantFolder(folder, folderPath))
            {
                folderSubtree.PushBack(folder);
            }
        }

        const String folderName = EditorAssetFolders::GetFolderName(folderPath);

        if (assets.Any())
        {
            bool cancelled = false;

            SystemMessageBox(MessageBoxType::INFO)
                .Title("Confirm Delete")
                .Text(HYP_FORMAT("Are you sure you want to delete the folder {} and the {} asset(s) in it?", folderName, assets.Size()))
                .Button("Delete", []()
                        {
                        })
                .Button("Cancel", [&cancelled]()
                        {
                            cancelled = true;
                        })
                .Show();

            if (cancelled)
            {
                return;
            }
        }

        Handle<FunctionalEditorAction> action = EditorCommandDeleteAsset::MakeDeleteAssetsAction(
            HYP_FORMAT("Delete {}", folderName),
            std::move(assets),
            folderPath,
            std::move(folderSubtree));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(DeleteAssetFolder);

#pragma endregion DeleteAssetFolder

#pragma region NewPhysicsShape

class EditorCommandNewPhysicsShape final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandNewPhysicsShape);

public:
    virtual ~EditorCommandNewPhysicsShape() override = default;

    virtual String GetText() const override
    {
        return "New Physics Shape";
    }

    static PhysicsShapeType ParseShapeType(const String& str)
    {
        for (uint8 i = 0; i < uint8(PhysicsShapeType::Max); i++)
        {
            if (EnumToString(PhysicsShapeType(i)) == str)
            {
                return PhysicsShapeType(i);
            }
        }

        return PhysicsShapeType::Box;
    }

    static const Class* GetPhysicsShapeClass(PhysicsShapeType type)
    {
        static const Class* s_classes[uint8(PhysicsShapeType::Max)] = {
            BoxPhysicsShape::StaticClass(),
            SpherePhysicsShape::StaticClass(),
            PlanePhysicsShape::StaticClass(),
            ConvexHullPhysicsShape::StaticClass(),
            CapsulePhysicsShape::StaticClass()
        };

        const uint8 index = uint8(type);

        return index < GetArrayCount(s_classes)
            ? s_classes[index]
            : nullptr;
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot create physics shape!");

            return;
        }

        const PhysicsShapeType shapeType = NumArguments() > 0
            ? ParseShapeType(GetArgument(0))
            : PhysicsShapeType::Box;

        const Class* shapeClass = GetPhysicsShapeClass(shapeType);
        if (!shapeClass)
        {
            HYP_LOG(Editor, Warning, "No class registered for physics shape type '{}'", EnumToString(shapeType));

            return;
        }

        BoxedValue boxed;
        if (!shapeClass->CreateInstance(boxed))
        {
            HYP_LOG(Editor, Error, "Failed to create instance of physics shape class '{}'", shapeClass->GetName().LookupString());

            return;
        }

        Handle<PhysicsShape> shape = boxed.Get<Handle<PhysicsShape>>();

        shape->SetName(NAME_FMT("New{}", shapeClass->GetName()));
        InitObject(shape);

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [shape]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [shape](EditorSubsystem*, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->PutAssetUnique(shape);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [shape](EditorSubsystem*, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->RemoveAsset(shape);
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(NewPhysicsShape);

#pragma endregion NewPhysicsShape

#pragma region PrefabEdit

/// opens a Prefab in prefab edit mode (the same as Edit on a prefab in the content browser)
class EditorCommandEditPrefab final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandEditPrefab);

public:
    virtual ~EditorCommandEditPrefab() override = default;

    virtual String GetText() const override
    {
        return "Edit Prefab";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 1 || GetArgument(0).Empty())
        {
            HYP_LOG(Editor, Warning, "EditorCommandEditPrefab requires a prefab name argument");
            return;
        }

        Handle<Prefab> prefab = GetCurrentAssetRegistry()->GetAsset<Prefab>(AssetBuckets::Prefabs, Name(ANSIString(GetArgument(0))));

        if (!prefab.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandEditPrefab: could not find prefab '{}'", GetArgument(0));
            return;
        }

        const Handle<EditorPrefabEditState>& prefabEditState = subsystem->GetPrefabEditState();

        if (prefabEditState->IsActive() && prefabEditState->GetPrefab() != prefab)
        {
            prefabEditState->Exit(/* apply */ true);
        }

        prefabEditState->Enter(prefab);
    }
};

DEFINE_EDITOR_COMMAND(EditPrefab);

class EditorCommandApplyPrefabEdit final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandApplyPrefabEdit);

public:
    virtual ~EditorCommandApplyPrefabEdit() override = default;

    virtual String GetText() const override
    {
        return "Apply Prefab Edit";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        const Handle<EditorPrefabEditState>& prefabEditState = subsystem->GetPrefabEditState();

        const bool updateInstances = prefabEditState->IsUpdateInstancesOnApply();
        prefabEditState->SetUpdateInstancesOnApply(!(NumArguments() >= 1 && GetArgument(0) == "noinstances"));

        prefabEditState->Apply();

        prefabEditState->SetUpdateInstancesOnApply(updateInstances);
    }
};

DEFINE_EDITOR_COMMAND(ApplyPrefabEdit);

class EditorCommandRevertPrefabEdit final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandRevertPrefabEdit);

public:
    virtual ~EditorCommandRevertPrefabEdit() override = default;

    virtual String GetText() const override
    {
        return "Revert Prefab Edit";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        subsystem->GetPrefabEditState()->Revert();
    }
};

DEFINE_EDITOR_COMMAND(RevertPrefabEdit);

/// leaves prefab edit mode; pass "apply" to write pending edits back first, otherwise they're discarded
class EditorCommandExitPrefabEdit final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandExitPrefabEdit);

public:
    virtual ~EditorCommandExitPrefabEdit() override = default;

    virtual String GetText() const override
    {
        return "Exit Prefab Edit";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        subsystem->GetPrefabEditState()->Exit(/* apply */ NumArguments() >= 1 && GetArgument(0) == "apply");
    }
};

DEFINE_EDITOR_COMMAND(ExitPrefabEdit);

#pragma endregion PrefabEdit

} // namespace Hyperion
