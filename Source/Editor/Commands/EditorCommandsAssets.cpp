#include <Editor/Commands/EditorCommandsCommon.hpp>
#include <Editor/EditorAssetDrop.hpp>
#include <Editor/EditorTemplateLibrary.hpp>

#include <Scene/Components/ScriptComponent.hpp>

namespace Hyperion {

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

        ScriptDesc scriptDesc;
        scriptDesc.language = ScriptLanguage::Strata;

        String extension = ".strata";
        String templateCode;

        if (languageArg == "strata")
        {
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
            templateCode += "    public override void OnAdded(Entity entity)\n    {\n    }\n\n";
            templateCode += "    public override void Update(float deltaTime)\n    {\n    }\n\n";
            templateCode += "    public override void Destroy()\n    {\n    }\n";
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
                            [scriptAsset, extension, templateCode](EditorSubsystem*, EditorProject* project)
                            {
                                CreateScriptFile(*project, *scriptAsset, extension, templateCode);
                                GetCurrentAssetRegistry()->PutAssetUnique(scriptAsset);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [scriptAsset](EditorSubsystem*, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->RemoveAsset(scriptAsset);

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

#pragma region NewWeapon

class EditorCommandNewWeapon final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandNewWeapon);

public:
    virtual ~EditorCommandNewWeapon() override = default;

    virtual String GetText() const override
    {
        return "New Weapon";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot create weapon asset!");

            return;
        }

        
        const String& weaponTypeArg = GetArgument(0);

        uint32 weaponTypeIndex;
        if (!StringUtil::Parse(weaponTypeArg, &weaponTypeIndex) || (weaponTypeIndex >= uint32(WeaponType::Max)))
        {
            HYP_LOG(Editor, Error, "Invalid WeaponType passed");

            return;
        }

        Handle<Weapon> weapon = MakeHandle<Weapon>(Name::Unique("NewWeapon"), WeaponType(weaponTypeIndex));
        InitObject(weapon);

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [weapon]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [weapon](EditorSubsystem*, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->PutAssetUnique(weapon);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [weapon](EditorSubsystem*, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->RemoveAsset(weapon);
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(NewWeapon);

#pragma endregion NewWeapon

#pragma region NewDecal

class EditorCommandNewDecal final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandNewDecal);

public:
    virtual ~EditorCommandNewDecal() override = default;

    virtual String GetText() const override
    {
        return "New Decal";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot create decal asset!");

            return;
        }

        Handle<Decal> decal = MakeHandle<Decal>(Name::Unique("NewDecal"), DecalDesc {});
        InitObject(decal);

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [decal]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [decal](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->PutAssetUnique(decal);

                                editorSubsystem->OnAssetsChanged(AssetBuckets::Decals.GetIndex());
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [decal](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->RemoveAsset(decal);

                                editorSubsystem->OnAssetsChanged(AssetBuckets::Decals.GetIndex());
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(NewDecal);

#pragma endregion NewDecal

#pragma region NewMaterial

class EditorCommandNewMaterial final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandNewMaterial);

public:
    virtual ~EditorCommandNewMaterial() override = default;

    virtual String GetText() const override
    {
        return "New Material";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot create material asset!");

            return;
        }

        Handle<Material> material = MakeHandle<Material>(Name::Unique("NewMaterial"));
        InitObject(material);

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [material]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [material](EditorSubsystem*, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->PutAssetUnique(material);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [material](EditorSubsystem*, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->RemoveAsset(material);
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(NewMaterial);

#pragma endregion NewMaterial

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

        Handle<Prefab> prefab = DynamicCast<Prefab>(asset);
        if (!prefab.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandAddAsset: Expected prefab, got {}", asset->InstanceClass()->GetName());
            return;
        }

        Handle<Node> clonedNode = prefab->Spawn();
        if (!clonedNode.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandAddAsset: failed to spawn asset '{}'", assetName);
            return;
        }

        Handle<Node> parentNode = activeScene->GetRoot();
        Assert(parentNode.IsValid());

        if (!parentNode.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandAddAsset: no root on Scene {}!", activeScene->GetName());
            return;
        }

        Vec3f insertionPoint = subsystem->CalculateSceneInsertionPoint();

        // If viewport coordinates are provided, try raycasting from the camera
        if (NumArguments() >= 4)
        {
            float nx = 0.5f, ny = 0.5f;
            StringUtil::Parse(GetArgument(2), &nx);
            StringUtil::Parse(GetArgument(3), &ny);

            if (EditorViewport* activeViewport = subsystem->GetActiveViewport())
            {
                if (Camera* camera = activeViewport->GetCamera())
                {
                    const Vec4f worldPos = camera->TransformScreenToWorld(Vec2f(nx, ny));
                    const Vec3f rayDir = worldPos.GetXYZ().Normalize();
                    const Ray ray { camera->GetWorldTranslation(), rayDir };

                    if (activeScene->GetSceneFlags() & SceneFlags::HAS_OCTREE)
                    {
                        RayTestResults results;
                        if (activeScene->GetOctree().TestRay(ray, results, RayTestFlags::TestBVH))
                        {
                            insertionPoint = results.Front().hitpoint;
                        }
                    }
                }
            }
        }

        clonedNode->SetWorldTranslation(insertionPoint);

        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            HYP_FORMAT("Add {}", assetName),
            Proc<EditorActionFunctions()>(
                [clonedNode, parentNode, previousFocusedNode]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [clonedNode, parentNode](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                parentNode->AddChild(clonedNode);
                                editorSubsystem->SetFocusedNode(clonedNode, true);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [clonedNode, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                clonedNode->Remove();

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

DEFINE_EDITOR_COMMAND(AddAsset);

#pragma endregion AddAsset

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

        currentProject->GetActionStack()->PushAction(action);
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

// Every live instance of the given Prefab across the world's foreground scenes
static Array<Handle<Node>> FindPrefabInstances(const World* world, const UUID& prefabUUID)
{
    Array<Handle<Node>> instances;

    if (!world)
    {
        return instances;
    }

    for (const Handle<Scene>& scene : world->GetScenes())
    {
        if (!scene.IsValid() || (scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
        {
            continue;
        }

        const Handle<Node>& sceneRoot = scene->GetRoot();

        if (!sceneRoot.IsValid())
        {
            continue;
        }

        for (Node* node : sceneRoot->GetDescendants())
        {
            if (Prefab::GetSourcePrefabUUID(node) == prefabUUID)
            {
                instances.PushBack(MakeStrongRef(node));
            }
        }
    }

    return instances;
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

        const Array<Handle<Node>> instances = FindPrefabInstances(currentProject->GetWorld().Get(), prefabUUID);

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

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            HYP_FORMAT("Add to Prefab {}", prefabNameArg),
            Proc<EditorActionFunctions()>(
                [prefab, records, instanceClones, referenceClones, previousSelectedNodes, previousFocusedNode]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [prefab, records, instanceClones, referenceClones](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
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
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [prefab, records, instanceClones, previousSelectedNodes, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
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
                            })
                    };
                }));

        InitObject(action);
        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(AddToPrefab);

// Content Browser "New" action: creates a blank Prefab with an empty root node. Deliberately ignores
// scene selection entirely (the Content Browser has no notion of selected scene nodes), unlike
// EditorCommandMakePrefab above.
class EditorCommandNewPrefab final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandNewPrefab);

public:
    virtual ~EditorCommandNewPrefab() override = default;

    virtual String GetText() const override
    {
        return "New Prefab";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot create prefab asset!");
            return;
        }

        ANSIString prefabNameStr = "NewPrefab";

        if (NumArguments() >= 1 && !GetArgument(0).Empty())
        {
            prefabNameStr = GetArgument(0);
        }

        const Name prefabName = Name(prefabNameStr);

        Handle<Node> root = MakeHandle<Node>();
        root->SetName(prefabName);
        InitObject(root);

        Handle<Prefab> prefab = MakeHandle<Prefab>(prefabName, root);
        InitObject(prefab);

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [prefab]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [prefab](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->PutAssetUnique(prefab);
                                prefab->SyncRootName(); //sets the Node to have the same name as the Prefab we created
                                                        //it may have a different name due to needing to be unique.

                                editorSubsystem->OnAssetsChanged(AssetBuckets::Prefabs.GetIndex());
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [prefab](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->RemoveAsset(prefab);
                                editorSubsystem->OnAssetsChanged(AssetBuckets::Prefabs.GetIndex());
                            })
                    };
                }));

        InitObject(action);
        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(NewPrefab);

class EditorCommandSavePrefab final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSavePrefab);

public:
    virtual ~EditorCommandSavePrefab() override = default;

    virtual String GetText() const override
    {
        return "Sync Prefab";
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

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            HYP_FORMAT("Sync Prefab {}", prefab->GetName()),
            Proc<EditorActionFunctions()>(
                [prefab, newRoot, previousRoot]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [prefab, newRoot](EditorSubsystem*, EditorProject*)
                            {
                                prefab->SetRoot(newRoot);
                                GetCurrentAssetRegistry()->PutAssetsDeep(prefab);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [prefab, previousRoot](EditorSubsystem*, EditorProject*)
                            {
                                prefab->SetRoot(previousRoot);
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

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 2)
        {
            HYP_LOG(Editor, Warning, "EditorCommandDeleteAsset requires bucket index and asset name");
            return;
        }

        uint32 bucketIndex = 0;
        if (!StringUtil::Parse(GetArgument(0), &bucketIndex) || bucketIndex == AssetBuckets::None.GetIndex())
        {
            HYP_LOG(Editor, Warning, "EditorCommandDeleteAsset: invalid bucket index '{}'", GetArgument(0));
            return;
        }

        const ANSIString assetName = GetArgument(1);
        const AssetBucket& bucket = *AssetBuckets::AllBuckets[bucketIndex];

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandDeleteAsset: no project loaded");
            return;
        }

        Handle<AssetObject> asset = GetCurrentAssetRegistry()->GetAsset(bucket, Name(assetName));
        if (!asset.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandDeleteAsset: asset '{}' in bucket {} is not valid", assetName, GetAssetBucketName(bucketIndex));
            return;
        }

        bool cancelled = false;

        // Show confirm box
        SystemMessageBox(MessageBoxType::INFO)
            .Title("Confirm Delete")
            .Text("Are you sure you want to delete the asset " + assetName + "?")
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

        const FilePath manifestPath = GetCurrentAssetRegistry()->GetManifestPath(asset->GetPath());

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            HYP_FORMAT("Delete {}", assetName),
            Proc<EditorActionFunctions()>(
                [asset, &bucket, manifestPath]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [asset, &bucket, manifestPath](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->RemoveAsset(bucket, asset->GetName());

                                if (manifestPath.Exists())
                                {
                                    manifestPath.Remove();
                                }

                                editorSubsystem->OnAssetsChanged(bucket.GetIndex());
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [asset, &bucket](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->PutAsset(bucket, asset);

                                editorSubsystem->OnAssetsChanged(bucket.GetIndex());
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(DeleteAsset);

#pragma endregion DeleteAsset

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

} // namespace Hyperion
