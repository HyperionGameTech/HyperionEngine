#include <Editor/Commands/EditorCommandsCommon.hpp>

namespace Hyperion {

#pragma region TeleportTo

#pragma region Collision

Node* ResolveNodeUuidArgument(EditorSubsystem* subsystem, const String& nodeUuidArgument)
{
    if (nodeUuidArgument.Empty())
    {
        return nullptr;
    }

    const UUID nodeUuid = UUID(nodeUuidArgument.Data());

    if (nodeUuid == UUID::Invalid())
    {
        HYP_LOG(Editor, Warning, "Editor command: invalid node UUID '{}'", nodeUuidArgument);

        return nullptr;
    }

    const Handle<Scene> activeScene = subsystem->GetActiveScene();

    return activeScene.IsValid() ? activeScene->FindNodeByUUID(nodeUuid) : nullptr;
}

class EditorCommandGenerateConvexCollision final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandGenerateConvexCollision);

public:
    virtual ~EditorCommandGenerateConvexCollision() override = default;

    virtual String GetText() const override
    {
        return "Generate Convex Collision";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        Node* node = ResolveNodeUuidArgument(subsystem, NumArguments() >= 1 ? GetArgument(0) : String());

        subsystem->GenerateConvexCollision(node);
    }
};

DEFINE_EDITOR_COMMAND(GenerateConvexCollision);

class EditorCommandFitCollisionToMesh final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandFitCollisionToMesh);

public:
    virtual ~EditorCommandFitCollisionToMesh() override = default;

    virtual String GetText() const override
    {
        return "Fit Collision To Mesh";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        Node* node = ResolveNodeUuidArgument(subsystem, NumArguments() >= 1 ? GetArgument(0) : String());

        subsystem->FitPhysicsShapeToMesh(node);
    }
};

DEFINE_EDITOR_COMMAND(FitCollisionToMesh);

#pragma endregion Collision

#pragma region Volume

class EditorCommandFitVolumeToSelection final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandFitVolumeToSelection);

public:
    virtual ~EditorCommandFitVolumeToSelection() override = default;

    virtual String GetText() const override
    {
        return "Fit Volume to Selection";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        Node* volume = ResolveNodeUuidArgument(subsystem, NumArguments() >= 1 ? GetArgument(0) : String());

        subsystem->FitVolumeToSelection(volume);
    }
};

DEFINE_EDITOR_COMMAND(FitVolumeToSelection);

#pragma endregion Volume

class EditorCommandTeleportTo final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandTeleportTo);

public:
    virtual ~EditorCommandTeleportTo() override = default;

    virtual String GetText() const override
    {
        return "Teleport To";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        Handle<Node> node;

        if (NumArguments() >= 1 && !GetArgument(0).Empty())
        {
            const UUID nodeUuid = UUID(GetArgument(0).Data());

            if (nodeUuid == UUID::Invalid())
            {
                HYP_LOG(Editor, Warning, "EditorCommandTeleportTo: invalid UUID '{}'", GetArgument(0));
                return;
            }

            const Handle<Scene> activeScene = subsystem->GetActiveScene();
            Node* foundNode = activeScene.IsValid() ? activeScene->FindNodeByUUID(nodeUuid) : nullptr;

            if (!foundNode)
            {
                HYP_LOG(Editor, Warning, "EditorCommandTeleportTo: could not find node with UUID '{}'", GetArgument(0));
                return;
            }

            node = MakeStrongRef(foundNode);
        }
        else
        {
            node = subsystem->GetFocusedNode();
        }

        if (!node.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandTeleportTo: no node specified or focused");
            return;
        }

        EditorViewport* activeViewport = subsystem->GetActiveViewport();
        if (!activeViewport)
        {
            HYP_LOG(Editor, Warning, "EditorCommandTeleportTo: no active viewport");
            return;
        }

        const Handle<Camera>& camera = activeViewport->GetCamera();
        if (!camera.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandTeleportTo: no camera in active viewport");
            return;
        }

        camera->SetWorldTranslation(node->GetWorldTranslation());
    }
};

DEFINE_EDITOR_COMMAND(TeleportTo);

#pragma endregion TeleportTo

#pragma region MoveToCamera

class EditorCommandMoveToCamera final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandMoveToCamera);

public:
    virtual ~EditorCommandMoveToCamera() override = default;

    virtual String GetText() const override
    {
        return "Move to Camera";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        Handle<Node> node;

        if (NumArguments() >= 1 && !GetArgument(0).Empty())
        {
            const UUID nodeUuid = UUID(GetArgument(0).Data());

            if (nodeUuid == UUID::Invalid())
            {
                HYP_LOG(Editor, Warning, "EditorCommandMoveToCamera: invalid UUID '{}'", GetArgument(0));
                return;
            }

            const Handle<Scene> activeScene = subsystem->GetActiveScene();
            Node* foundNode = activeScene.IsValid() ? activeScene->FindNodeByUUID(nodeUuid) : nullptr;

            if (!foundNode)
            {
                HYP_LOG(Editor, Warning, "EditorCommandMoveToCamera: could not find node with UUID '{}'", GetArgument(0));
                return;
            }

            node = MakeStrongRef(foundNode);
        }
        else
        {
            node = subsystem->GetFocusedNode();
        }

        if (!node.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandMoveToCamera: no node specified or focused");
            return;
        }

        EditorViewport* activeViewport = subsystem->GetActiveViewport();
        if (!activeViewport)
        {
            HYP_LOG(Editor, Warning, "EditorCommandMoveToCamera: no active viewport");
            return;
        }

        const Handle<Camera>& camera = activeViewport->GetCamera();
        if (!camera.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandMoveToCamera: no camera in active viewport");
            return;
        }

        // volumes are positioned by their local bounds rather than the node origin
        if (VolumeBase* volume = DynamicCast<VolumeBase>(node.Get()))
        {
            const BoundingBox& localBounds = volume->GetLocalBounds();

            if (localBounds.IsValid() && localBounds.IsFinite())
            {
                const Vec3f center = localBounds.GetCenter();

                if (!MathUtil::ApproxEqual(center, Vec3f::Zero()))
                {
                    volume->SetLocalBounds(localBounds + (-center));
                }
            }
        }

        node->SetWorldTranslation(camera->GetWorldTranslation());
    }
};

DEFINE_EDITOR_COMMAND(MoveToCamera);

#pragma endregion MoveToCamera

#pragma region MoveEditorCamera

class EditorCommandMoveEditorCamera final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandMoveEditorCamera);

public:
    virtual ~EditorCommandMoveEditorCamera() override = default;

    virtual String GetText() const override
    {
        return "Move Editor Camera";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 3)
        {
            HYP_LOG(Editor, Warning, "EditorCommandMoveEditorCamera: expected <x> <y> <z> [<directionX> <directionY> <directionZ>]");

            return;
        }

        Vec3f translation;

        for (int i = 0; i < 3; i++)
        {
            if (!StringUtil::Parse(GetArgument(i), &translation[i]))
            {
                HYP_LOG(Editor, Warning, "EditorCommandMoveEditorCamera: invalid translation component '{}'", GetArgument(i));

                return;
            }
        }

        Vec3f direction;
        bool hasDirection = false;

        if (NumArguments() >= 6)
        {
            for (int i = 0; i < 3; i++)
            {
                if (!StringUtil::Parse(GetArgument(3 + i), &direction[i]))
                {
                    HYP_LOG(Editor, Warning, "EditorCommandMoveEditorCamera: invalid direction component '{}'", GetArgument(3 + i));

                    return;
                }
            }

            if (direction.LengthSquared() <= MathUtil::epsilonF)
            {
                HYP_LOG(Editor, Warning, "EditorCommandMoveEditorCamera: direction cannot be zero length");

                return;
            }

            direction.Normalize();

            hasDirection = true;
        }

        Camera* camera = nullptr;

        if (EditorViewport* activeViewport = subsystem->GetActiveViewport())
        {
            camera = activeViewport->GetCamera();
        }

        if (!camera && g_editorState.IsValid())
        {
            camera = g_editorState->GetEditorCamera();
        }

        if (!camera)
        {
            HYP_LOG(Editor, Warning, "EditorCommandMoveEditorCamera: no editor camera to move");

            return;
        }

        camera->SetWorldTranslation(translation);

        if (hasDirection)
        {
            camera->SetDirection(direction);
        }
    }
};

DEFINE_EDITOR_COMMAND(MoveEditorCamera);

#pragma endregion MoveEditorCamera

#pragma region Copy

class EditorCommandCopy final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandCopy);

public:
    virtual ~EditorCommandCopy() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        Array<Handle<Node>> nodes;

        if (NumArguments() >= 1 && !GetArgument(0).Empty())
        {
            const UUID nodeUuid = UUID(GetArgument(0).Data());

            if (nodeUuid == UUID::Invalid())
            {
                HYP_LOG(Editor, Warning, "EditorCommandCopy: invalid UUID '{}'", GetArgument(0));
                return;
            }

            const Handle<Scene> activeScene = subsystem->GetActiveScene();
            Node* node = activeScene.IsValid() ? activeScene->FindNodeByUUID(nodeUuid) : nullptr;

            if (node)
            {
                nodes.PushBack(MakeStrongRef(node));
            }
            else
            {
                HYP_LOG(Editor, Warning, "EditorCommandCopy: could not find node with UUID '{}'", GetArgument(0));
                return;
            }
        }
        else
        {
            // Copy all selected nodes; fall back to the focused node if nothing is selected
            nodes = subsystem->GetSelectedNodes();

            if (nodes.Empty())
            {
                Handle<Node> focusedNode = subsystem->GetFocusedNode();

                if (focusedNode.IsValid())
                {
                    nodes.PushBack(focusedNode);
                }
            }
        }

        if (nodes.Empty())
        {
            HYP_LOG(Editor, Warning, "No nodes to copy");
            return;
        }

        g_editorState->SetClipboardNodes(nodes);

        HYP_LOG(Editor, Verbose, "Copied {} node(s) to clipboard", nodes.Size());
    }
};

DEFINE_EDITOR_COMMAND(Copy);

#pragma endregion Copy

#pragma region Paste

class EditorCommandPaste final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandPaste);

public:
    virtual ~EditorCommandPaste() override = default;

    virtual String GetText() const override
    {
        return "Paste Nodes";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        EditorProject* currentProject = subsystem->GetCurrentProject();

        if (!currentProject)
        {
            HYP_LOG(Editor, Warning, "No current project");

            return;
        }

        Array<Handle<Node>> clipboardNodes = g_editorState->GetClipboardNodes();

        if (clipboardNodes.Empty())
        {
            HYP_LOG(Editor, Warning, "No nodes in clipboard");

            return;
        }

        Handle<Scene> activeScene = subsystem->GetActiveScene();

        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "No active scene");

            return;
        }

        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();
        const Handle<Node>& sceneRoot = activeScene->GetRoot();

        // Clone each clipboard node, pairing with its individual parent
        Array<Pair<Handle<Node>, WeakHandle<Node>>> newNodesWithParents;
        for (const Handle<Node>& clipboardNode : clipboardNodes)
        {
            if (!clipboardNode.IsValid())
            {
                continue;
            }

            Handle<Node> newNode = clipboardNode->Clone();

            if (!newNode.IsValid())
            {
                HYP_LOG(Editor, Error, "Failed to clone clipboard node '{}'", clipboardNode->GetName());

                continue;
            }

            const Name newName = activeScene->GetUniqueNodeName(clipboardNode->GetName().LookupString());
            
            newNode->SetName(newName);

            // Attach to the original node's parent, falling back to the scene root
            WeakHandle<Node> parentNode = MakeWeakRef(clipboardNode->GetParent());
            if (!parentNode.IsValid())
            {
                parentNode = MakeWeakRef(sceneRoot);
            }

            newNodesWithParents.PushBack({ newNode, parentNode });
        }

        if (newNodesWithParents.Empty())
        {
            HYP_LOG(Editor, Error, "Failed to create any pasted nodes");

            return;
        }

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            (newNodesWithParents.Size() == 1)
                ? HYP_FORMAT("Paste {}", newNodesWithParents[0].first->GetName())
                : HYP_FORMAT("Paste {} nodes", newNodesWithParents.Size()),
            Proc<EditorActionFunctions()>(
                [newNodesWithParents, previousFocusedNode, activeScene]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [newNodesWithParents, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                const Handle<Node>& sceneRoot = activeScene->GetRoot();

                                Array<Handle<Node>> addedNodes;

                                for (const auto& pair : newNodesWithParents)
                                {
                                    const Handle<Node>& newNode = pair.first;
                                    WeakHandle<Node> parentWeak = pair.second;

                                    Handle<Node> parentStrong = parentWeak.Lock();
                                    if (!parentStrong.IsValid())
                                    {
                                        parentStrong = MakeStrongRef(sceneRoot);
                                    }

                                    if (!parentStrong.IsValid())
                                    {
                                        HYP_LOG(Editor, Error, "Cannot paste node; no parent to attach to");

                                        continue;
                                    }

                                    parentStrong->AddChild(newNode);
                                    addedNodes.PushBack(newNode);
                                }

                                // Focus the first pasted node and select all pasted nodes
                                if (addedNodes.Any())
                                {
                                    editorSubsystem->SetSelectedNodes(addedNodes);
                                    editorSubsystem->SetFocusedNode(addedNodes[0], true);
                                }
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [newNodesWithParents, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                for (const auto& pair : newNodesWithParents)
                                {
                                    pair.first->Remove();
                                }

                                // Restore previous focused node if it was cleared
                                if (editorSubsystem->GetFocusedNode() == Handle<Node>::Null())
                                {
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

        HYP_LOG(Editor, Verbose, "Pasted {} node(s) to scene", newNodesWithParents.Size());
    }
};

DEFINE_EDITOR_COMMAND(Paste);

#pragma endregion Paste

#pragma region SelectAll

class EditorCommandSelectAll final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSelectAll);

public:
    virtual ~EditorCommandSelectAll() override = default;

    virtual String GetText() const override
    {
        return "Select All";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        const Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandSelectAll: no active scene");
            return;
        }

        const Handle<Node>& root = activeScene->GetRoot();
        if (!root.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandSelectAll: scene has no root node");
            return;
        }

        Array<Node*> descendants = root->GetDescendantsArray();

        subsystem->SetSelectedNodes({ root });

        for (Node* descendant : descendants)
        {
            subsystem->AddToSelection(MakeStrongRef(descendant));
        }
    }
};

DEFINE_EDITOR_COMMAND(SelectAll);

#pragma endregion SelectAll

#pragma region SelectAllInViewport

class EditorCommandSelectAllInViewport final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSelectAllInViewport);

public:
    virtual ~EditorCommandSelectAllInViewport() override = default;

    virtual String GetText() const override
    {
        return "Select All in Viewport";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        const Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandSelectAllInViewport: no active scene");
            return;
        }

        const Handle<Node>& root = activeScene->GetRoot();
        if (!root.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandSelectAllInViewport: scene has no root node");
            return;
        }

        EditorViewport* activeViewport = subsystem->GetActiveViewport();
        if (activeViewport == nullptr || !activeViewport->GetCamera().IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandSelectAllInViewport: no active viewport");
            return;
        }

        const Frustum& frustum = activeViewport->GetCamera()->GetFrustum();

        Array<Handle<Node>> visibleNodes;

        for (Node* descendant : root->GetDescendantsArray())
        {
            if (descendant == nullptr || descendant->IsRoot())
            {
                continue;
            }

            Handle<Node> nodeStrong = MakeStrongRef(descendant);

            const BoundingBox worldBounds = nodeStrong->GetWorldBounds();

            if (!worldBounds.IsValid() || !worldBounds.IsFinite())
            {
                continue;
            }

            if (frustum.ContainsAABB(worldBounds))
            {
                visibleNodes.PushBack(nodeStrong);
            }
        }

        if (visibleNodes.Empty())
        {
            return;
        }

        subsystem->SetSelectedNodes(visibleNodes);
        subsystem->SetFocusedNode(visibleNodes[0], true);
    }
};

DEFINE_EDITOR_COMMAND(SelectAllInViewport);

#pragma endregion SelectAllInViewport

#pragma region SelectNone

class EditorCommandSelectNone final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSelectNone);

public:
    virtual ~EditorCommandSelectNone() override = default;

    virtual String GetText() const override
    {
        return "Select None";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (subsystem->GetFocusedNode().IsValid())
        {
            subsystem->SetFocusedNode(Handle<Node>::Null(), true);
        }

        subsystem->ClearSelection();
    }
};

DEFINE_EDITOR_COMMAND(SelectNone);

#pragma endregion SelectNone

} // namespace Hyperion
