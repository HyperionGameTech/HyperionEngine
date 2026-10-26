/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Editor/Commands/EditorCommandsCommon.hpp>

#include <Editor/Animation/EditorAnimationEditState.hpp>
#include <Editor/EditorModelImportReview.hpp>

#include <Scene/EntityManager.hpp>

#include <Scene/Components/AnimationComponent.hpp>

#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Animation/Animation.hpp>
#include <Scene/Animation/AnimationGraphInstance.hpp>

namespace Hyperion {

#pragma region PreviewAnimation

class EditorCommandPreviewAnimation final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandPreviewAnimation);

public:
    virtual ~EditorCommandPreviewAnimation() override = default;

    virtual String GetText() const override
    {
        return "Preview Animation";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 2)
        {
            HYP_LOG(Editor, Warning, "EditorCommandPreviewAnimation requires an entity UUID and a time");
            return;
        }

        Entity* targetEntity = TryCast<Entity>(ResolveNodeUuidArgument(subsystem, GetArgument(0)));

        if (!targetEntity || !targetEntity->TryGetComponent<AnimationComponent>())
        {
            HYP_LOG(Editor, Warning, "EditorCommandPreviewAnimation: '{}' is not an entity with an AnimationComponent", GetArgument(0));
            return;
        }

        Handle<Entity> entity = MakeStrongRef(targetEntity);

        float time = 0.0f;
        StringUtil::Parse(GetArgument(1), &time);

        subsystem->ClearSelection();
        subsystem->AddToSelection(entity);
        subsystem->SetFocusedNode(entity);

        if (!subsystem->IsAnimationPreviewActive(entity))
        {
            subsystem->StartAnimationPreview(entity);
        }

        subsystem->SetAnimationPreviewPlaying(false);
        subsystem->SetAnimationPreviewTime(time);
    }
};

DEFINE_EDITOR_COMMAND(PreviewAnimation);

#pragma endregion PreviewAnimation

#pragma region AnimationEdit

/// opens animation edit mode on an entity (by UUID), a Skeleton asset or an Animation asset
class EditorCommandEditAnimation final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandEditAnimation);

public:
    virtual ~EditorCommandEditAnimation() override = default;

    virtual String GetText() const override
    {
        return "Edit Animation";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 1 || GetArgument(0).Empty())
        {
            HYP_LOG(Editor, Warning, "EditorCommandEditAnimation requires an entity UUID or a Skeleton or Animation asset name");
            return;
        }

        const Handle<EditorAnimationEditState>& animationEditState = subsystem->GetAnimationEditState();

        const Name assetName = Name(ANSIString(GetArgument(0)));

        if (Handle<Skeleton> skeleton = GetCurrentAssetRegistry()->GetAsset<Skeleton>(AssetBuckets::Skeletons, assetName))
        {
            animationEditState->EnterSkeleton(skeleton);
            return;
        }

        if (Handle<AnimationGraph> graph = GetCurrentAssetRegistry()->GetAsset<AnimationGraph>(AssetBuckets::Animations, assetName))
        {
            animationEditState->EnterGraph(graph);
            return;
        }

        if (Handle<Animation> animation = GetCurrentAssetRegistry()->GetAsset<Animation>(AssetBuckets::Animations, assetName))
        {
            animationEditState->EnterAnimation(animation);
            return;
        }

        if (Entity* entity = TryCast<Entity>(ResolveNodeUuidArgument(subsystem, GetArgument(0))))
        {
            animationEditState->EnterEntity(MakeStrongRef(entity));
            return;
        }

        HYP_LOG(Editor, Warning, "EditorCommandEditAnimation: '{}' is not an entity, Skeleton or Animation", GetArgument(0));
    }
};

DEFINE_EDITOR_COMMAND(EditAnimation);

class EditorCommandAnimationEditSetTime final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAnimationEditSetTime);

public:
    virtual ~EditorCommandAnimationEditSetTime() override = default;

    virtual String GetText() const override
    {
        return "Set Animation Edit Time";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        float time = 0.0f;

        if (NumArguments() < 1 || !StringUtil::Parse(GetArgument(0), &time))
        {
            HYP_LOG(Editor, Warning, "EditorCommandAnimationEditSetTime requires a time in seconds");
            return;
        }

        const Handle<EditorAnimationEditState>& animationEditState = subsystem->GetAnimationEditState();

        animationEditState->SetPlaying(false);
        animationEditState->SetTime(time);
    }
};

DEFINE_EDITOR_COMMAND(AnimationEditSetTime);

/// picks the clip shown in animation edit mode; no argument shows the bind pose
class EditorCommandAnimationEditSetClip final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAnimationEditSetClip);

public:
    virtual ~EditorCommandAnimationEditSetClip() override = default;

    virtual String GetText() const override
    {
        return "Set Animation Edit Clip";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        const Name clipName = NumArguments() >= 1 && !GetArgument(0).Empty() ? Name(ANSIString(GetArgument(0))) : Name::Invalid();

        if (!subsystem->GetAnimationEditState()->SetAnimationByName(clipName) && clipName.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandAnimationEditSetClip: the skeleton has no clip '{}'", GetArgument(0));
        }
    }
};

DEFINE_EDITOR_COMMAND(AnimationEditSetClip);

class EditorCommandAnimationEditAddNotify final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAnimationEditAddNotify);

public:
    virtual ~EditorCommandAnimationEditAddNotify() override = default;

    virtual String GetText() const override
    {
        return "Add Animation Notify";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        float time = 0.0f;

        if (NumArguments() < 1 || !StringUtil::Parse(GetArgument(0), &time))
        {
            HYP_LOG(Editor, Warning, "EditorCommandAnimationEditAddNotify requires a time in seconds");
            return;
        }

        const Handle<EditorAnimationEditState>& animationEditState = subsystem->GetAnimationEditState();

        animationEditState->AddNotify(time);

        if (NumArguments() >= 2 && animationEditState->NumNotifies() != 0)
        {
            animationEditState->SetNotifyName(animationEditState->NumNotifies() - 1, Name(ANSIString(GetArgument(1))));
        }
    }
};

DEFINE_EDITOR_COMMAND(AnimationEditAddNotify);

class EditorCommandAnimationEditApply final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAnimationEditApply);

public:
    virtual ~EditorCommandAnimationEditApply() override = default;

    virtual String GetText() const override
    {
        return "Apply Animation Edit";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        subsystem->GetAnimationEditState()->Apply();
    }
};

DEFINE_EDITOR_COMMAND(AnimationEditApply);

class EditorCommandExitAnimationEdit final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandExitAnimationEdit);

public:
    virtual ~EditorCommandExitAnimationEdit() override = default;

    virtual String GetText() const override
    {
        return "Exit Animation Edit";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        subsystem->GetAnimationEditState()->Exit(/* apply */ NumArguments() >= 1 && GetArgument(0) == "apply");
    }
};

DEFINE_EDITOR_COMMAND(ExitAnimationEdit);

/// selects what a viewport click at the normalized position would pick first, joints included
class EditorCommandSelectAtViewport final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSelectAtViewport);

public:
    virtual ~EditorCommandSelectAtViewport() override = default;

    virtual String GetText() const override
    {
        return "Select At Viewport";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        float screenX = 0.5f;
        float screenY = 0.5f;

        if (NumArguments() < 2 || !StringUtil::Parse(GetArgument(0), &screenX) || !StringUtil::Parse(GetArgument(1), &screenY))
        {
            HYP_LOG(Editor, Warning, "EditorCommandSelectAtViewport requires normalized x and y viewport coordinates");
            return;
        }

        Node* pickedNode = subsystem->PickNodeAtViewport(Vec2f(screenX, screenY));

        subsystem->ClearSelection();

        if (pickedNode)
        {
            subsystem->AddToSelection(MakeStrongRef(pickedNode));
            subsystem->SetFocusedNode(MakeStrongRef(pickedNode));
        }
    }
};

DEFINE_EDITOR_COMMAND(SelectAtViewport);

#pragma endregion AnimationEdit

#pragma region ModelImport

class EditorCommandReimportAsset final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandReimportAsset);

public:
    virtual ~EditorCommandReimportAsset() override = default;

    virtual String GetText() const override
    {
        return "Reimport Asset";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        uint32 bucketIndex = 0;

        if (NumArguments() < 2 || !StringUtil::Parse(GetArgument(0), &bucketIndex) || bucketIndex == AssetBuckets::None.GetIndex() || bucketIndex >= MaxAssetBuckets)
        {
            HYP_LOG(Editor, Warning, "EditorCommandReimportAsset requires a bucket index and an asset name");
            return;
        }

        Handle<AssetObject> asset = GetCurrentAssetRegistry()->GetAsset(*AssetBuckets::AllBuckets[bucketIndex], Name(ANSIString(GetArgument(1))));

        if (!subsystem->ReimportAsset(asset))
        {
            HYP_LOG(Editor, Warning, "EditorCommandReimportAsset: '{}' could not be reimported", GetArgument(1));
        }
    }
};

DEFINE_EDITOR_COMMAND(ReimportAsset);

/// commits or cancels the oldest model import waiting for its settings; commit takes an optional rig mode
/// (create, existing <skeleton>, none) followed by takes to leave out
class EditorCommandResolveModelImport final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandResolveModelImport);

public:
    virtual ~EditorCommandResolveModelImport() override = default;

    virtual String GetText() const override
    {
        return "Resolve Model Import";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        Handle<EditorModelImportReview> review = subsystem->GetPendingModelImportReview();

        if (!review.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandResolveModelImport: no model import is waiting");
            return;
        }

        if (NumArguments() < 1 || GetArgument(0) != "commit")
        {
            review->Cancel();
            return;
        }

        int argumentIndex = 1;

        if (NumArguments() > argumentIndex && GetArgument(argumentIndex) == "existing" && NumArguments() > argumentIndex + 1)
        {
            review->SetRigMode(uint32(ImportRigMode::UseExisting));
            review->SetTargetSkeleton(Name(ANSIString(GetArgument(argumentIndex + 1))));

            argumentIndex += 2;
        }
        else if (NumArguments() > argumentIndex && (GetArgument(argumentIndex) == "create" || GetArgument(argumentIndex) == "none"))
        {
            review->SetRigMode(uint32(GetArgument(argumentIndex) == "none" ? ImportRigMode::None : ImportRigMode::Create));

            ++argumentIndex;
        }

        for (; argumentIndex < NumArguments(); ++argumentIndex)
        {
            review->SetTakeIncluded(Name(ANSIString(GetArgument(argumentIndex))), false);
        }

        review->Commit(subsystem);
    }
};

DEFINE_EDITOR_COMMAND(ResolveModelImport);

#pragma endregion ModelImport

#pragma region AnimationGraph

/// logs each node of an entity's running animation graph (or the animation edit preview's) with its weight and time
class EditorCommandDumpAnimationGraph final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandDumpAnimationGraph);

public:
    virtual ~EditorCommandDumpAnimationGraph() override = default;

    virtual String GetText() const override
    {
        return "Dump Animation Graph";
    }

    virtual bool AllowedWhileSimulating() const override
    {
        return true;
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        Entity* entity = NumArguments() >= 1 ? TryCast<Entity>(ResolveNodeUuidArgument(subsystem, GetArgument(0))) : nullptr;
        const AnimationComponent* animationComponent = entity ? entity->TryGetComponent<AnimationComponent>() : nullptr;

        const AnimationGraphInstance* runningInstance = animationComponent ? animationComponent->graphInstance.Get() : nullptr;

        if (!entity)
        {
            runningInstance = subsystem->GetAnimationEditState()->GetPreviewGraphInstance();
        }

        if (!runningInstance || !runningInstance->GetGraph().IsValid())
        {
            HYP_LOG(Editor, Info, "DumpAnimationGraph: no running animation graph");
            return;
        }

        const AnimationGraphInstance& instance = *runningInstance;
        const AnimationGraph& graph = *instance.GetGraph();

        for (uint32 nodeIndex = 0; nodeIndex < graph.NumNodes(); ++nodeIndex)
        {
            const AnimationGraphNode* node = graph.GetNode(nodeIndex);

            HYP_LOG(Editor, Info, "DumpAnimationGraph: node {} '{}' ({}) weight {} time {}",
                nodeIndex,
                node ? node->name : Name::Invalid(),
                node ? node->InstanceClass()->GetName() : Name::Invalid(),
                instance.GetNodeWeight(nodeIndex),
                instance.GetNodeTime(nodeIndex));

            if (const AnimationClipNode* clipNode = TryCast<AnimationClipNode>(node))
            {
                HYP_LOG(Editor, Info, "DumpAnimationGraph:   clip {} length {}",
                    clipNode->clip.IsValid() ? clipNode->clip->GetName() : Name::Invalid(),
                    clipNode->clip.IsValid() ? clipNode->clip->GetLength() : 0.0f);
            }

            const AnimationStateMachineNode* stateMachine = TryCast<AnimationStateMachineNode>(node);
            const AnimationGraphNodeState* nodeState = instance.GetNodeState(nodeIndex);

            if (stateMachine && nodeState)
            {
                HYP_LOG(Editor, Info, "DumpAnimationGraph:   state {} for {}s", stateMachine->DescribeState(*nodeState), nodeState->stateTime);
            }
        }
    }
};

DEFINE_EDITOR_COMMAND(DumpAnimationGraph);

/// edits the graph open in animation edit mode:
/// add <class> <x> <y> | clip <animation> <x> <y> | remove <node> | connect <from> <to> <slot> | disconnect <node> <slot>
/// | output <node> | move <node> <x> <y> | sample <node> <sample> <x> <y> | parameter <name> <type> | unparameter <name>
/// | preview <name> <x> [y]
/// | fire <trigger> | state <node> <state> <name> | transition <node> <from|any> <to> <duration> | condition <node> <transition> <parameter|-> <op index> <value>
class EditorCommandEditAnimationGraph final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandEditAnimationGraph);

public:
    virtual ~EditorCommandEditAnimationGraph() override = default;

    virtual String GetText() const override
    {
        return "Edit Animation Graph";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        const Handle<EditorAnimationEditState>& animationEditState = subsystem->GetAnimationEditState();

        if (!animationEditState->IsEditingGraph() || NumArguments() < 2)
        {
            HYP_LOG(Editor, Warning, "EditorCommandEditAnimationGraph: no graph is being edited, or no operation was given");
            return;
        }

        const String& operation = GetArgument(0);

        const auto number = [this](int argumentIndex) -> float
        {
            float value = 0.0f;

            if (argumentIndex < NumArguments())
            {
                StringUtil::Parse(GetArgument(argumentIndex), &value);
            }

            return value;
        };

        const auto index = [&number](int argumentIndex)
        {
            return uint32(number(argumentIndex));
        };

        const Name nameArgument = Name(ANSIString(GetArgument(1)));

        if (operation == "add")
        {
            animationEditState->AddGraphNode(nameArgument, Vec2f(number(2), number(3)));
        }
        else if (operation == "clip")
        {
            animationEditState->AddGraphClipNode(GetCurrentAssetRegistry()->GetAsset<Animation>(AssetBuckets::Animations, nameArgument), Vec2f(number(2), number(3)));
        }
        else if (operation == "remove")
        {
            animationEditState->RemoveGraphNode(index(1));
        }
        else if (operation == "connect")
        {
            animationEditState->ConnectGraphNodes(index(1), index(2), index(3));
        }
        else if (operation == "disconnect")
        {
            animationEditState->DisconnectGraphInput(index(1), index(2));
        }
        else if (operation == "output")
        {
            animationEditState->SetGraphRootNode(index(1));
        }
        else if (operation == "move")
        {
            animationEditState->MoveGraphNode(index(1), Vec2f(number(2), number(3)));
        }
        else if (operation == "sample")
        {
            animationEditState->SetBlendSamplePosition(index(1), index(2), Vec2f(number(3), number(4)));
        }
        else if (operation == "parameter")
        {
            animationEditState->AddGraphParameter(nameArgument, index(2));
        }
        else if (operation == "unparameter")
        {
            animationEditState->RemoveGraphParameter(nameArgument);
        }
        else if (operation == "state")
        {
            animationEditState->RenameGraphState(index(1), index(2), NumArguments() > 3 ? Name(ANSIString(GetArgument(3))) : Name::Invalid());
        }
        else if (operation == "transition")
        {
            const bool fromAnyState = NumArguments() > 2 && GetArgument(2) == "any";

            animationEditState->AddGraphTransition(index(1), fromAnyState ? 0 : index(2), fromAnyState, index(3), number(4));
        }
        else if (operation == "condition")
        {
            const bool hasParameter = NumArguments() > 3 && GetArgument(3) != "-";

            animationEditState->AddGraphTransitionCondition(index(1), index(2), hasParameter ? Name(ANSIString(GetArgument(3))) : Name::Invalid(), index(4), number(5));
        }
        else if (operation == "fire")
        {
            animationEditState->TriggerPreviewParameter(nameArgument);
        }
        else if (operation == "preview")
        {
            animationEditState->SetPreviewParameter(nameArgument, Vec2f(number(2), number(3)));
        }
        else
        {
            HYP_LOG(Editor, Warning, "EditorCommandEditAnimationGraph: unknown operation '{}'", operation);
        }
    }
};

DEFINE_EDITOR_COMMAND(EditAnimationGraph);

#pragma endregion AnimationGraph

} // namespace Hyperion
