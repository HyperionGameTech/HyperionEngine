#include <Editor/Commands/EditorCommandsCommon.hpp>

namespace Hyperion {

template <class T>
static constexpr bool ShouldAddNodeAsChild()
{
    return std::is_same_v<T, InstancedMeshProxy>;
}

template <class EditorCommandType, class T>
static void AddNodeOfTypeImpl(EditorSubsystem* subsystem, Name defaultNodeName)
{
    const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
    if (!currentProject.IsValid())
    {
        HYP_LOG(Editor, Error, "No project loaded; cannot add entity");

        return;
    }

    Handle<Scene> activeScene = subsystem->GetActiveScene();
    if (!activeScene.IsValid())
    {
        HYP_LOG(Editor, Error, "No active scene found");

        return;
    }

    WeakHandle<Node> currentFocusedNode = subsystem->GetFocusedNode();

    Handle<T> n = MakeHandle<T>();
    n->SetName(defaultNodeName);
    InitObject(n);

    if constexpr (!std::is_same_v<T, InstancedMeshProxy>)
    {
        // Calculate appropriate insertion point in front of camera
        const Vec3f insertionPoint = subsystem->CalculateSceneInsertionPoint(5.0f, 0.5f);
        n->SetWorldTranslation(insertionPoint);
    }

    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        HYP_FORMAT("Add {}", defaultNodeName),
        Proc<EditorActionFunctions()>(
            [n, currentFocusedNode, activeScene]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [n, currentFocusedNode, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            if constexpr (ShouldAddNodeAsChild<T>())
                            {
                                Handle<Node> parentNode = currentFocusedNode.Lock();

                                if (!parentNode.IsValid())
                                {
                                    parentNode = activeScene->GetRoot();
                                }

                                parentNode->AddChild(n);
                            }
                            else
                            {
                                activeScene->GetRoot()->AddChild(n);
                            }

                            if constexpr (std::is_base_of_v<Light, T>)
                            {
                                project->GetActiveBakeLayer().Add<Baking::BakeLayerCategory::LightProvider>(*n);
                            }

                            editorSubsystem->SetSelectedNodes({ n });
                            editorSubsystem->SetFocusedNode(n, true);
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [n, currentFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            if constexpr (std::is_base_of_v<Light, T>)
                            {
                                project->GetActiveBakeLayer().Remove<Baking::BakeLayerCategory::LightProvider>(*n);
                            }

                            n->Remove();

                            if (editorSubsystem->GetFocusedNode() == n)
                            {
                                editorSubsystem->SetFocusedNode(nullptr, true);

                                Handle<Node> focusedNode = currentFocusedNode.Lock();

                                if (focusedNode.IsValid())
                                {
                                    editorSubsystem->SetFocusedNode(focusedNode, true);
                                }
                            }
                        })
                };
            }));

    InitObject(action);

    currentProject->GetActionStack()->PushAction(action);
}

template <class Derived>
class EditorCommandAddNodeBase : public EditorCommandBase
{
public:
    virtual ~EditorCommandAddNodeBase() override = default;

    virtual String GetText() const override
    {
        static const auto s_typeNameNoNamespace = TypeNameWithoutNamespace<typename Derived::NodeType>();

        return HYP_FORMAT("Add {}", s_typeNameNoNamespace.Data());
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        static const auto s_typeNameNoNamespace = TypeNameWithoutNamespace<typename Derived::NodeType>();

        AddNodeOfTypeImpl<Derived, typename Derived::NodeType>(subsystem, NAME_FMT("New{}", s_typeNameNoNamespace.Data()));
    }
};

#pragma region EditorCommandAddEntity

class EditorCommandAddEntity final : public EditorCommandAddNodeBase<EditorCommandAddEntity>
{
    HYP_OBJECT_BODY(EditorCommandAddEntity);

public:
    using NodeType = Entity;
};

DEFINE_EDITOR_COMMAND(AddEntity);

#pragma endregion EditorCommandAddEntity

#pragma region EditorCommandAddEmptyNode

class EditorCommandAddEmptyNode final : public EditorCommandAddNodeBase<EditorCommandAddEmptyNode>
{
    HYP_OBJECT_BODY(EditorCommandAddEmptyNode);

public:
    using NodeType = Node;
};

DEFINE_EDITOR_COMMAND(AddEmptyNode);

#pragma endregion EditorCommandAddEmptyNode

#pragma region EditorCommandAddInstance

class EditorCommandAddInstance final : public EditorCommandAddNodeBase<EditorCommandAddInstance>
{
    HYP_OBJECT_BODY(EditorCommandAddInstance);

public:
    using NodeType = InstancedMeshProxy;
};

DEFINE_EDITOR_COMMAND(AddInstance);

#pragma region EditorCommandAddCamera

class EditorCommandAddCamera final : public EditorCommandAddNodeBase<EditorCommandAddCamera>
{
    HYP_OBJECT_BODY(EditorCommandAddCamera);

public:
    using NodeType = Camera;
};

DEFINE_EDITOR_COMMAND(AddCamera);

#pragma endregion EditorCommandAddCamera

#pragma region EditorCommandAddSprite

class EditorCommandAddSprite final : public EditorCommandAddNodeBase<EditorCommandAddSprite>
{
    HYP_OBJECT_BODY(EditorCommandAddSprite);

public:
    using NodeType = Sprite;
};

DEFINE_EDITOR_COMMAND(AddSprite);

#pragma endregion EditorCommandAddSprite

#pragma region EditorCommandAddTextSprite

class EditorCommandAddTextSprite final : public EditorCommandAddNodeBase<EditorCommandAddTextSprite>
{
    HYP_OBJECT_BODY(EditorCommandAddTextSprite);

public:
    using NodeType = TextSprite;
};

DEFINE_EDITOR_COMMAND(AddTextSprite);

#pragma endregion EditorCommandAddTextSprite

#pragma region EditorCommandAddPointLight

class EDITOR_API EditorCommandAddPointLight final : public EditorCommandAddNodeBase<EditorCommandAddPointLight>
{
    HYP_OBJECT_BODY(EditorCommandAddPointLight);

public:
    using NodeType = PointLight;
};

DEFINE_EDITOR_COMMAND(AddPointLight);

#pragma endregion EditorCommandAddPointLight

#pragma region EditorCommandAddDirectionalLight

class EditorCommandAddDirectionalLight final : public EditorCommandAddNodeBase<EditorCommandAddDirectionalLight>
{
    HYP_OBJECT_BODY(EditorCommandAddDirectionalLight);

public:
    using NodeType = DirectionalLight;
};

DEFINE_EDITOR_COMMAND(AddDirectionalLight);

#pragma endregion EditorCommandAddDirectionalLight

#pragma region EditorCommandAddSpotLight

class EditorCommandAddSpotLight final : public EditorCommandAddNodeBase<EditorCommandAddSpotLight>
{
    HYP_OBJECT_BODY(EditorCommandAddSpotLight);

public:
    using NodeType = SpotLight;
};

DEFINE_EDITOR_COMMAND(AddSpotLight);

#pragma endregion EditorCommandAddSpotLight

#pragma region EditorCommandAddAreaRectLight

class EditorCommandAddAreaRectLight final : public EditorCommandAddNodeBase<EditorCommandAddAreaRectLight>
{
    HYP_OBJECT_BODY(EditorCommandAddAreaRectLight);

public:
    using NodeType = AreaRectLight;
};

DEFINE_EDITOR_COMMAND(AddAreaRectLight);

#pragma endregion EditorCommandAddAreaRectLight

#pragma region EditorCommandAddPlayerEntity

class EditorCommandAddPlayerEntity final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAddPlayerEntity);

public:
    virtual ~EditorCommandAddPlayerEntity() override = default;

    virtual String GetText() const override
    {
        return "Add Player Entity";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot add player entity!");

            return;
        }

        Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "No active scene; cannot add player entity!");

            return;
        }

        const Vec3f insertionPoint = subsystem->CalculateSceneInsertionPoint(5.0f, 0.5f);

        Handle<Entity> playerEntity = MakeHandle<Entity>();
        playerEntity->SetName(activeScene->GetUniqueNodeName("Player"));
        playerEntity->SetWorldTranslation(insertionPoint);
        playerEntity->SetIsDynamic(true);
        InitObject(playerEntity);

        Handle<CapsulePhysicsShape> capsuleShape = MakeHandle<CapsulePhysicsShape>();
        capsuleShape->SetName(NAME_FMT("{}CapsuleShape", playerEntity->GetName()));
        InitObject(capsuleShape);

        Handle<Camera> camera = MakeHandle<Camera>();
        camera->SetName(activeScene->GetUniqueNodeNameT<Camera>());
        camera->SetLocalTranslation(Vec3f(0.0f, 1.6f, 0.0f));
        camera->AddTag<EntityTag::PrimaryCamera>();

        Handle<FirstPersonCameraController> firstPersonCameraController = MakeHandle<FirstPersonCameraController>();
        camera->AddCameraController(firstPersonCameraController);

        InitObject(camera);

        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [playerEntity, capsuleShape, camera, previousFocusedNode, activeScene]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [playerEntity, capsuleShape, camera, activeScene](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->PutAssetUnique(capsuleShape);

                                activeScene->GetRoot()->AddChild(playerEntity);

                                if (!playerEntity->HasComponent<CharacterControllerComponent>())
                                {
                                    CharacterControllerComponent characterControllerComponent;
                                    characterControllerComponent.shape = capsuleShape;
                                    playerEntity->AddComponent<CharacterControllerComponent>(characterControllerComponent);
                                }
                                else // has component (can happen if going undo->redo)
                                {
                                    playerEntity->GetComponent<CharacterControllerComponent>().shape = capsuleShape;
                                }

                                playerEntity->AddChild(camera);

                                editorSubsystem->SetSelectedNodes({ playerEntity });
                                editorSubsystem->SetFocusedNode(playerEntity, true);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [playerEntity, capsuleShape, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->RemoveAsset(capsuleShape);

                                playerEntity->Remove();

                                if (editorSubsystem->GetFocusedNode() == playerEntity)
                                {
                                    editorSubsystem->SetFocusedNode(nullptr, true);

                                    Handle<Node> focusedNode = previousFocusedNode.Lock();

                                    if (focusedNode.IsValid())
                                    {
                                        editorSubsystem->SetFocusedNode(focusedNode, true);
                                    }
                                }
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(AddPlayerEntity);

#pragma endregion EditorCommandAddPlayerEntity

#pragma region EditorCommandImportContent

class EditorCommandImportContent final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandImportContent);

public:
    virtual ~EditorCommandImportContent() override = default;

    virtual String GetText() const override
    {
        return m_text.Length() ? m_text : EditorCommandBase::GetText();
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        ShowOpenFileDialog(
            "Select the file(s) to import into the project",
            EngineGlobals::GetDataDirectory(),
            { "obj", "fbx", "gltf", "glb", "mesh.xml", "skeleton.xml", "jpg", "jpeg", "png", "tga", "bmp", "wav" },
            /* allowMultiple */ true, /* allowDirectories */ false,
            [this, weakSubsystem = MakeWeakRef(subsystem)](TResult<Array<FilePath>>&& result) mutable
            {
                if (result.HasError())
                {
                    HYP_LOG(Editor, Error, "Failed to select files to import: {}", result.GetError().GetMessage());

                    return;
                }

                if (result.GetValue().Size() > 1)
                {
                    m_text = HYP_FORMAT("Import {} files", result.GetValue().Size());
                }
                else if (result.GetValue().Size() == 1)
                {
                    m_text = HYP_FORMAT("Import '{}'", result.GetValue()[0].Basename());
                }
                else
                {
                    HYP_LOG(Editor, Warning, "No files selected for import.");

                    return;
                }

                struct ImportProgressContext
                {
                    AtomicVar<uint32> numProcessed { 0 };
                    AtomicVar<uint32> numFailed { 0 };
                    uint32 numTotal = 0;
                    EditorTaskBase* task = nullptr;
                };

                auto context = MakeShared<ImportProgressContext>();
                context->numTotal = uint32(result.GetValue().Size());

                EditorTaskScope* editorTaskScope = new EditorTaskScope(
                    TickableEditorTask::StaticClass(),
                    [context]()
                    {
                        const uint32 numProcessed = context->numProcessed.Get(MemoryOrder::RELAXED);
                        const uint32 numFailed = context->numFailed.Get(MemoryOrder::RELAXED);

                        if (EditorTaskBase* task = context->task)
                        {
                            task->SetProgress(context->numTotal > 0
                                    ? MathUtil::Min(float(numProcessed) / float(context->numTotal), 1.0f)
                                    : 1.0f);

                            task->SetDescription(numFailed > 0
                                    ? HYP_FORMAT("Importing {}/{} assets ({} failed)", numProcessed, context->numTotal, numFailed)
                                    : HYP_FORMAT("Importing {}/{} assets", numProcessed, context->numTotal));
                        }
                    },
                    "Importing content...",
                    HYP_FORMAT("Importing 0/{} assets", context->numTotal),
                    /* isForegroundTask */ true);

                context->task = editorTaskScope->GetEditorTask();

                // Create identifier based on the common folder of the assets
                String identifier = "Unknown";

                if (result.GetValue().Any())
                {
                    identifier = result.GetValue()[0].BasePath().Basename();
                }

                // Queue up an asset batch
                AssetBatch* batch = AssetManager::GetInstance()->CreateBatch(identifier);

                for (const FilePath& file : result.GetValue())
                {
                    batch->Add(file.Basename(), file);
                }

                batch->GetCallbacks().OnItemComplete
                    .Bind([context](const AssetBatchCallbackData&)
                          {
                              context->numProcessed.Increment(1, MemoryOrder::RELEASE);
                          })
                    .Detach();

                batch->GetCallbacks().OnItemFailed
                    .Bind([context](const AssetBatchCallbackData&)
                          {
                              context->numFailed.Increment(1, MemoryOrder::RELEASE);
                              context->numProcessed.Increment(1, MemoryOrder::RELEASE);
                          })
                    .Detach();

                batch->OnComplete
                    .Bind([editorTaskScope, weakSubsystem = std::move(weakSubsystem)](AssetMap& results) mutable
                          {
                              HYP_LOG(Editor, Verbose, "{} assets loaded.", results.Size());

                              String postText;
                              int numFailed = 0;
                              uint32 numProcessed = 0;

                              Handle<EditorSubsystem> subsystem = weakSubsystem.Lock();

                              Set<uint32, EditorAllocator> changedBuckets;

                              for (auto& it : results)
                              {
                                  String& key = it.first;
                                  LoadedAsset& loadedAsset = it.second;

                                  editorTaskScope->GetEditorTask()->SetDescription(HYP_FORMAT("Processing {}/{}: {}", ++numProcessed, results.Size(), key) + postText);

                                  if (!loadedAsset.IsValid())
                                  {
                                      HYP_LOG(Editor, Error, "Failed to import asset '{}': {}", key, loadedAsset.GetError().GetMessage());

                                      postText = HYP_FORMAT(" ({} failed)", ++numFailed);

                                      editorTaskScope->GetEditorTask()->SetDescription(HYP_FORMAT("Processing {}/{}: {}", numProcessed, results.Size(), key) + postText);

                                      continue;
                                  }

                                  Handle<AssetObject> assetObject = loadedAsset.ExtractAs<AssetObject>();
                                  if (!assetObject.IsValid())
                                  {
                                      continue;
                                  }

                                  GetCurrentAssetRegistry()->PutAssetUnique(assetObject);

                                  const AssetPath& assetPath = assetObject->GetPath();

                                  if (assetPath.IsValid() && subsystem.IsValid())
                                  {
                                      changedBuckets.Insert(assetPath.GetBucket().GetIndex());
                                  }
                              }

                              for (uint32 bucketIndex : changedBuckets)
                              {
                                  subsystem->OnAssetsChanged(bucketIndex);
                              }

                              delete editorTaskScope;
                          })
                    .Detach();

                batch->LoadAsync();

                // Note: The batch will be destroyed automatically by AssetManager when complete
            });
    }

private:
    String m_text;
};

DEFINE_EDITOR_COMMAND(ImportContent);

#pragma endregion EditorCommandImportContent

#pragma region EditorCommandReparentNode

class EditorCommandReparentNode final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandReparentNode);

public:
    EditorCommandReparentNode() = default;

    virtual ~EditorCommandReparentNode() override = default;

    virtual String GetText() const override
    {
        return m_text.Length() ? m_text : EditorCommandBase::GetText();
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 2)
        {
            HYP_LOG(Editor, Error, "EditorCommandReparentNode: expected at least one node and a new parent");
            return;
        }

        const Handle<Scene> activeScene = subsystem->GetActiveScene();

        uint64 newParentAddress = 0;

        if (!StringUtil::Parse(GetArgument(NumArguments() - 1), &newParentAddress) || newParentAddress == NULL)
        {
            HYP_LOG(Editor, Error, "EditorCommandReparentNode: invalid new parent address");
            return;
        }

        Handle<Node> newParent = MakeStrongRef(reinterpret_cast<Node*>(newParentAddress));

        if (!newParent.IsValid() || newParent->GetScene() != activeScene.Get())
        {
            HYP_LOG(Editor, Error, "EditorCommandReparentNode: invalid new parent");
            return;
        }

        Array<Handle<Node>> candidates;

        for (int argumentIndex = 0; argumentIndex < NumArguments() - 1; argumentIndex++)
        {
            uint64 nodeAddress = 0;

            if (!StringUtil::Parse(GetArgument(argumentIndex), &nodeAddress) || nodeAddress == NULL)
            {
                HYP_LOG(Editor, Error, "EditorCommandReparentNode: invalid node address '{}'", GetArgument(argumentIndex));
                return;
            }

            Handle<Node> node = MakeStrongRef(reinterpret_cast<Node*>(nodeAddress));

            if (!node.IsValid() || node->GetScene() != activeScene.Get())
            {
                HYP_LOG(Editor, Error, "EditorCommandReparentNode: invalid node");
                return;
            }

            // Prevent cycles: reject if newParent is the dragged node itself or any of its descendants.
            if (newParent->IsOrHasParent(node))
            {
                HYP_LOG(Editor, Warning, "EditorCommandReparentNode: cannot reparent '{}' to its own descendant", node->GetName());
                continue;
            }

            candidates.PushBack(std::move(node));
        }

        // Each entry is a moved node paired with its previous parent.
        Array<Pair<Handle<Node>, Handle<Node>>> moves;

        for (const Handle<Node>& node : candidates)
        {
            // A node whose ancestor is also being moved travels along with that ancestor.
            bool hasAncestorInSet = false;

            for (Node* p = node->GetParent(); p && !hasAncestorInSet; p = p->GetParent())
            {
                for (const Handle<Node>& other : candidates)
                {
                    if (other.Get() == p)
                    {
                        hasAncestorInSet = true;
                        break;
                    }
                }
            }

            if (hasAncestorInSet)
            {
                continue;
            }

            Node* previousParent = node->GetParent();

            if (!previousParent)
            {
                HYP_LOG(Editor, Error, "EditorCommandReparentNode: '{}' has no parent, cannot reparent", node->GetName());
                continue;
            }

            if (previousParent == newParent.Get())
            {
                continue;
            }

            moves.PushBack({ node, MakeStrongRef(previousParent) });
        }

        if (moves.Empty())
        {
            return;
        }

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandReparentNode: no project loaded");
            return;
        }

        m_text = moves.Size() == 1
            ? HYP_FORMAT("Attach '{}' to '{}'", moves[0].first->GetName(), newParent->GetName())
            : HYP_FORMAT("Attach {} nodes to '{}'", moves.Size(), newParent->GetName());

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [moves, newParent]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [moves, newParent](EditorSubsystem*, EditorProject*)
                            {
                                for (const auto& move : moves)
                                {
                                    move.first->Remove();
                                    newParent->AddChild(move.first);
                                }
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [moves](EditorSubsystem*, EditorProject*)
                            {
                                for (int i = int(moves.Size()) - 1; i >= 0; --i)
                                {
                                    moves[i].first->Remove();
                                    moves[i].second->AddChild(moves[i].first);
                                }
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }

private:
    String m_text;
};

DEFINE_EDITOR_COMMAND(ReparentNode);

#pragma endregion EditorCommandReparentNode

#pragma region EditorCommandMoveNodeToScene

class EditorCommandMoveNodeToScene final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandMoveNodeToScene);

public:
    EditorCommandMoveNodeToScene() = default;

    virtual ~EditorCommandMoveNodeToScene() override = default;

    virtual String GetText() const override
    {
        return m_text.Length() ? m_text : EditorCommandBase::GetText();
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        uint64 nodeAddress = 0;
        uint64 targetSceneAddress = 0;

        if (!StringUtil::Parse(GetArgument(0), &nodeAddress) || !StringUtil::Parse(GetArgument(1), &targetSceneAddress)
            || nodeAddress == NULL || targetSceneAddress == NULL)
        {
            HYP_LOG(Editor, Error, "EditorCommandMoveNodeToScene: invalid node or scene address");
            return;
        }

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandMoveNodeToScene: no project loaded");
            return;
        }

        World* projectWorld = currentProject->GetWorld().Get();
        if (!projectWorld)
        {
            HYP_LOG(Editor, Error, "EditorCommandMoveNodeToScene: project has no world");
            return;
        }

        Handle<Node> node = MakeStrongRef(reinterpret_cast<Node*>(nodeAddress));
        Handle<Scene> targetScene = MakeStrongRef(reinterpret_cast<Scene*>(targetSceneAddress));

        if (!node.IsValid() || !targetScene.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandMoveNodeToScene: invalid node or scene");
            return;
        }

        bool targetSceneInWorld = false;

        for (const Handle<Scene>& scene : projectWorld->GetScenes())
        {
            if (scene == targetScene)
            {
                targetSceneInWorld = true;

                break;
            }
        }

        if (!targetSceneInWorld)
        {
            HYP_LOG(Editor, Error, "EditorCommandMoveNodeToScene: target scene does not belong to the project's world");
            return;
        }

        Scene* nodeScene = node->GetScene();

        if (!nodeScene)
        {
            HYP_LOG(Editor, Error, "EditorCommandMoveNodeToScene: node has no scene, cannot move");
            return;
        }

        if (nodeScene == targetScene.Get())
        {
            HYP_LOG(Editor, Warning, "EditorCommandMoveNodeToScene: node is already in the target scene");
            return;
        }

        Node* previousParent = node->GetParent();
        if (!previousParent)
        {
            HYP_LOG(Editor, Error, "EditorCommandMoveNodeToScene: node has no parent, cannot move scene root");
            return;
        }

        Handle<Node> targetRoot = targetScene->GetRoot();
        if (!targetRoot.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandMoveNodeToScene: target scene has no root node");
            return;
        }

        m_text = HYP_FORMAT("Move '{}' to scene '{}'", node->GetName(), targetScene->GetName());

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [node, targetRoot, previousParent = MakeStrongRef(previousParent)]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [node, targetRoot](EditorSubsystem*, EditorProject*)
                            {
                                node->Remove();
                                targetRoot->AddChild(node);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [node, previousParent](EditorSubsystem*, EditorProject*)
                            {
                                node->Remove();
                                previousParent->AddChild(node);
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }

private:
    String m_text;
};

DEFINE_EDITOR_COMMAND(MoveNodeToScene);

#pragma endregion EditorCommandMoveNodeToScene

#pragma region RenameNode

class EditorCommandRenameNode final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandRenameNode);

public:
    virtual ~EditorCommandRenameNode() override = default;

    virtual String GetText() const override
    {
        return m_text.Length() ? m_text : EditorCommandBase::GetText();
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        uint64 nodeAddress = 0;

        if (!StringUtil::Parse(GetArgument(0), &nodeAddress) || nodeAddress == NULL)
        {
            HYP_LOG(Editor, Error, "EditorCommandRenameNode: invalid node address");
            return;
        }

        if (NumArguments() < 2)
        {
            HYP_LOG(Editor, Error, "EditorCommandRenameNode: no new name provided");
            return;
        }

        ANSIString newNameString = GetArgument(1);

        for (int argumentIndex = 2; argumentIndex < NumArguments(); argumentIndex++)
        {
            newNameString += " ";
            newNameString += GetArgument(argumentIndex);
        }

        Handle<Node> node = MakeStrongRef(reinterpret_cast<Node*>(nodeAddress));

        if (!node.IsValid() || node->GetScene() != subsystem->GetActiveScene().Get())
        {
            HYP_LOG(Editor, Error, "EditorCommandRenameNode: invalid node");
            return;
        }

        const Name previousName = node->GetName();
        const Name newName = Name(newNameString);

        if (newName == previousName)
        {
            return;
        }

        m_text = HYP_FORMAT("Rename node \"{}\"", newName);

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandRenameNode: no project loaded");
            return;
        }

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [node, previousName, newName]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [node, newName](EditorSubsystem*, EditorProject*)
                            {
                                node->SetName(newName);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [node, previousName](EditorSubsystem*, EditorProject*)
                            {
                                node->SetName(previousName);
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }

private:
    String m_text;
};

DEFINE_EDITOR_COMMAND(RenameNode);

#pragma endregion RenameNode

#pragma region DeleteNode

class EditorCommandDeleteNode final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandDeleteNode);

public:
    virtual ~EditorCommandDeleteNode() override = default;

    virtual String GetText() const override
    {
        return "Delete Nodes";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandDeleteNode: no project loaded");
            return;
        }

        Array<Handle<Node>> nodesToDelete;

        if (NumArguments() >= 1 && !GetArgument(0).Empty())
        {
            // Parse UUID from string arg
            const UUID nodeUuid = UUID(GetArgument(0).Data());

            if (nodeUuid == UUID::Invalid())
            {
                HYP_LOG(Editor, Error, "EditorCommandDeleteNode: invalid UUID '{}'", GetArgument(0));
                return;
            }

            const Handle<Scene> activeScene = subsystem->GetActiveScene();
            Node* node = activeScene.IsValid() ? activeScene->FindNodeByUUID(nodeUuid) : nullptr;

            if (!node)
            {
                HYP_LOG(Editor, Warning, "EditorCommandDeleteNode: could not find node with UUID '{}'", GetArgument(0));
                return;
            }

            nodesToDelete.PushBack(MakeStrongRef(node));
        }
        else
        {
            // Delete all selected nodes; fall back to focused node if nothing selected
            nodesToDelete = subsystem->GetSelectedNodes();

            if (nodesToDelete.Empty())
            {
                Handle<Node> focusedNode = subsystem->GetFocusedNode();

                if (focusedNode.IsValid())
                {
                    nodesToDelete.PushBack(focusedNode);
                }
            }
        }

        // Filter: skip nodes whose parent is also in the deletion set (they'll be removed implicitly)
        Array<Handle<Node>> topLevelNodes;
        for (const Handle<Node>& node : nodesToDelete)
        {
            if (!node.IsValid())
            {
                continue;
            }

            bool hasAncestorInSet = false;
            for (Node* p = node->GetParent(); p; p = p->GetParent())
            {
                for (const Handle<Node>& other : nodesToDelete)
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

        if (topLevelNodes.Empty())
        {
            HYP_LOG(Editor, Warning, "EditorCommandDeleteNode: no nodes to delete (all are children of other selected nodes)");
            return;
        }

        // Build undo data: each node paired with its parent
        Array<Pair<Handle<Node>, WeakHandle<Node>>> nodesWithParents;
        for (const Handle<Node>& node : topLevelNodes)
        {
            Node* parentRaw = node->GetParent();
            if (!parentRaw)
            {
                HYP_LOG(Editor, Warning, "EditorCommandDeleteNode: node has no parent (cannot delete root)");
                continue;
            }

            nodesWithParents.PushBack({ node, MakeWeakRef(parentRaw) });
        }

        if (nodesWithParents.Empty())
        {
            return;
        }

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            nodesWithParents.Size() == 1
                ? HYP_FORMAT("Delete {}", nodesWithParents[0].first->GetName())
                : HYP_FORMAT("Delete {} nodes", nodesWithParents.Size()),
            Proc<EditorActionFunctions()>(
                [nodesWithParents]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [nodesWithParents](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                for (const auto& pair : nodesWithParents)
                                {
                                    pair.first->Remove();
                                }

                                // Clear focus if it was one of the deleted nodes
                                if (Handle<Node> focusedNode = editorSubsystem->GetFocusedNode(); !focusedNode.IsValid())
                                {
                                    // Focus was auto-cleared; nothing to restore
                                }
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [nodesWithParents](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                // Re-attach in reverse order so original order is preserved
                                for (int i = nodesWithParents.Size() - 1; i >= 0; --i)
                                {
                                    const auto& pair = nodesWithParents[i];

                                    Handle<Node> parent = pair.second.Lock();
                                    if (!parent.IsValid())
                                    {
                                        continue;
                                    }

                                    parent->AddChild(pair.first);
                                }

                                // Focus the first restored node
                                if (nodesWithParents.Any())
                                {
                                    editorSubsystem->SetFocusedNode(nodesWithParents[0].first, true);
                                }
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(DeleteNode);

#pragma endregion DeleteNode

} // namespace Hyperion
