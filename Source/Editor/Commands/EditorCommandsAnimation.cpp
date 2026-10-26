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

} // namespace Hyperion
