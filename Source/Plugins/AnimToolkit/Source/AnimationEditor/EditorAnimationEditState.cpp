/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <AnimationEditor/EditorAnimationEditState.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorAction.hpp>
#include <Editor/EditorActionStack.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Prefab.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Animation/AnimationComponent.hpp>
#include <Animation/CharacterModelComponent.hpp>

#include <Scene/Animation/Skeleton.hpp>
#include <Animation/HumanoidMapping.hpp>
#include <Animation/AnimationPose.hpp>

#include <Animation/AnimationSystem.hpp>

#include <Animation/AnimationGraphInstance.hpp>

#include <Rendering/DebugDrawer.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/SerializationUtils.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Ray.hpp>

#include <Core/Logging/Logger.hpp>

#include <cmath>

#include <EditorAnimationEditState.generated.inl>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

namespace {

constexpr float PreviewFrameRate = 30.0f;
constexpr uint32 RootTrailSamples = 64;

AnimationSystem* GetProjectAnimationSystem(EditorSubsystem& subsystem)
{
    const Handle<World>& world = subsystem.GetProjectWorld();

    return world.IsValid() ? world->GetSystem<AnimationSystem>() : nullptr;
}

const Handle<Skeleton>& GetEntitySkeleton(const Entity& entity)
{
    const MeshComponent* meshComponent = entity.GetEntityManager() ? entity.TryGetComponent<MeshComponent>() : nullptr;

    return meshComponent ? meshComponent->skeleton : Handle<Skeleton>::empty;
}

bool UsesSkeletonOrClip(
    const Skeleton& entitySkeleton,
    const Skeleton* skeleton,
    const Animation* animation)
{
    if (skeleton)
    {
        return &entitySkeleton == skeleton;
    }

    if (!animation)
    {
        return true;
    }

    return AnyOf(entitySkeleton.GetAnimations(), [animation](const Handle<Animation>& entityAnimation)
        {
            return entityAnimation.Get() == animation;
        });
}

Entity* FindSkinnedEntityInWorld(EditorSubsystem& subsystem, const Skeleton* skeleton, const Animation* animation)
{
    const Handle<World>& world = subsystem.GetProjectWorld();

    if (!world.IsValid())
    {
        return nullptr;
    }

    for (const Handle<Scene>& scene : world->GetScenes())
    {
        if (!scene.IsValid() || !scene->GetRoot().IsValid() || (scene->GetSceneFlags() & SceneFlags::EDITOR))
        {
            continue;
        }

        for (Node* descendant : scene->GetRoot()->GetDescendants())
        {
            Entity* entity = TryCast<Entity>(descendant);

            if (!entity)
            {
                continue;
            }

            const Handle<Skeleton>& entitySkeleton = GetEntitySkeleton(*entity);

            if (!entitySkeleton.IsValid())
            {
                continue;
            }

            if (UsesSkeletonOrClip(*entitySkeleton, skeleton, animation))
            {
                return entity;
            }
        }
    }

    return nullptr;
}

Handle<Skeleton> FindHumanoidSkeleton(Name skeletonName)
{
    for (const Handle<Skeleton>& skeleton : HumanoidSkeletonData::CollectHumanoidSkeletons())
    {
        if (skeleton->GetName() == skeletonName)
        {
            return skeleton;
        }
    }

    return Handle<Skeleton>::empty;
}

Handle<Entity> FindSkinnedEntityInPrefabs(const Skeleton* skeleton, const Animation* animation)
{
    Array<AssetDesc> prefabDescs;
    GetCurrentAssetRegistry()->GetBucketAssetDescs(AssetBuckets::Prefabs.GetIndex(), prefabDescs);

    for (const AssetDesc& prefabDesc : prefabDescs)
    {
        Handle<Prefab> prefab = GetCurrentAssetRegistry()->GetAsset<Prefab>(AssetBuckets::Prefabs, prefabDesc.name);

        if (!prefab.IsValid() || !prefab->GetRoot().IsValid())
        {
            continue;
        }

        for (Node* descendant : prefab->GetRoot()->GetDescendants())
        {
            Entity* entity = TryCast<Entity>(descendant);

            if (!entity || !GetEntitySkeleton(*entity).IsValid())
            {
                continue;
            }

            const Handle<Skeleton>& entitySkeleton = GetEntitySkeleton(*entity);

            if (UsesSkeletonOrClip(*entitySkeleton, skeleton, animation))
            {
                return MakeStrongRef(entity);
            }
        }
    }

    return Handle<Entity>::empty;
}

RenderableAttributeSet OverlayAttributes(Topology topology, EnumFlags<MaterialAttributeFlags> flags)
{
    RenderableAttributeSet attributes;

    MeshAttributes& meshAttributes = attributes.GetMeshAttributes();
    meshAttributes.inputLayout = StaticVertexInputLayout<VT_Simple>;
    meshAttributes.topology = topology;

    MaterialAttributes& materialAttributes = attributes.GetMaterialAttributes();
    materialAttributes.bucket = RenderBucket::Debug;
    materialAttributes.fillMode = topology == Topology::Lines ? FillMode::Fill : FillMode::Line;
    materialAttributes.blendFunction = BlendFunction::None();
    materialAttributes.flags = flags;

    return attributes;
}

bool IsJointSlot(const SkeletonBindData& bindData, uint32 slot)
{
    // the loaders add a synthetic, unnamed root above the real ones
    return bindData.boneNames[slot].IsValid() && *bindData.boneNames[slot].LookupString() != '\0';
}

String SaveGraphText(const AnimationGraph& graph)
{
    String text;

    const Result result = graph.SaveContents(text);
    Assert(!result.HasError(), "Failed to save animation graph '{}': {}", graph.GetName(), result.GetError().GetMessage());

    return text;
}

void LoadGraphText(AnimationGraph& graph, const String& text)
{
    const Result result = graph.LoadContents(text);
    Assert(!result.HasError(), "Failed to load animation graph '{}': {}", graph.GetName(), result.GetError().GetMessage());
}

} // namespace

#pragma region EditorBoneNode

EditorBoneNode::EditorBoneNode() = default;

EditorBoneNode::EditorBoneNode(EditorAnimationEditState* owner, Name boneName, uint32 boneIndex, Name parentBoneName, const Transform& bindTransform)
    : m_owner(owner),
      m_boneName(boneName),
      m_boneIndex(boneIndex),
      m_parentBoneName(parentBoneName),
      m_bindTransform(bindTransform)
{
}

EditorBoneNode::~EditorBoneNode() = default;

void EditorBoneNode::AddSocket()
{
    if (m_owner && m_owner->IsActive())
    {
        m_owner->AddSocket(*this);
    }
}

#pragma endregion EditorBoneNode

#pragma region EditorSocketNode

EditorSocketNode::EditorSocketNode() = default;

EditorSocketNode::EditorSocketNode(Name name)
    : Node(name)
{
}

EditorSocketNode::~EditorSocketNode() = default;

#pragma endregion EditorSocketNode

#pragma region EditorAnimationEditState

EditorAnimationEditState::EditorAnimationEditState() = default;

EditorAnimationEditState::~EditorAnimationEditState() = default;

bool EditorAnimationEditState::IsActive() const
{
    return m_skeleton.IsValid() && m_editScene.IsValid();
}

bool EditorAnimationEditState::CanEditAsset(const Handle<AssetObject>& asset) const
{
    return asset.IsValid() && (asset->IsA<Skeleton>() || asset->IsA<Animation>() || asset->IsA<AnimationGraph>());
}

bool EditorAnimationEditState::EnterAsset(const Handle<AssetObject>& asset)
{
    if (!asset.IsValid())
    {
        return false;
    }

    if (asset->IsA<Skeleton>())
    {
        return EnterSkeleton(Cast<Skeleton>(asset));
    }

    if (asset->IsA<AnimationGraph>())
    {
        return EnterGraph(Cast<AnimationGraph>(asset));
    }

    if (asset->IsA<Animation>())
    {
        return EnterAnimation(Cast<Animation>(asset));
    }

    return false;
}

Handle<AssetObject> EditorAnimationEditState::GetInspectedAsset() const
{
    if (!IsActive() || IsEditingGraph())
    {
        return Handle<AssetObject>::empty;
    }

    return m_skeleton;
}

bool EditorAnimationEditState::EnterSkeleton(const Handle<Skeleton>& skeleton)
{
    return EnterWith(skeleton, Handle<Animation>::empty, Handle<Entity>::empty);
}

bool EditorAnimationEditState::EnterAnimation(const Handle<Animation>& animation)
{
    if (!animation.IsValid())
    {
        return false;
    }

    if (Entity* entity = FindSkinnedEntityInWorld(*m_subsystem, nullptr, animation.Get()))
    {
        return EnterWith(GetEntitySkeleton(*entity), animation, MakeStrongRef(entity));
    }

    if (Handle<Entity> entity = FindSkinnedEntityInPrefabs(nullptr, animation.Get()))
    {
        return EnterWith(GetEntitySkeleton(*entity), animation, entity);
    }

    HYP_LOG(Editor, Warning, "Cannot edit animation '{}': no skeleton uses it", animation->GetName());

    return false;
}

bool EditorAnimationEditState::EnterEntity(const Handle<Entity>& entity)
{
    if (!entity.IsValid() || !GetEntitySkeleton(*entity).IsValid())
    {
        HYP_LOG(Editor, Warning, "Cannot edit animation: the entity has no skeleton");

        return false;
    }

    const Handle<Skeleton>& skeleton = GetEntitySkeleton(*entity);

    Handle<Animation> animation;

    if (const AnimationComponent* animationComponent = entity->TryGetComponent<AnimationComponent>())
    {
        animation = animationComponent->animation.IsValid()
            ? animationComponent->animation
            : skeleton->GetAnimation(animationComponent->playbackState.animationIndex);
    }

    return EnterWith(skeleton, animation, entity);
}

bool EditorAnimationEditState::EnterWith(
    const Handle<Skeleton>& skeleton,
    const Handle<Animation>& animation,
    const Handle<Entity>& sourceEntity,
    const Handle<AnimationGraph>& graph)
{
    AssertOnThread(g_simThread);

    if (!skeleton.IsValid() || !skeleton->GetRootBone().IsValid())
    {
        HYP_LOG(Editor, Warning, "Cannot edit animation: the skeleton has no bones");

        return false;
    }

    Assert(!IsActive(), "Exit animation edit mode before entering it again");

    Handle<Entity> previewSource = sourceEntity;

    if (!previewSource.IsValid())
    {
        previewSource = MakeStrongRef(FindSkinnedEntityInWorld(*m_subsystem, skeleton.Get(), nullptr));
    }

    if (!previewSource.IsValid())
    {
        previewSource = FindSkinnedEntityInPrefabs(skeleton.Get(), nullptr);
    }

    if (!previewSource.IsValid())
    {
        HYP_LOG(Editor, Warning, "Cannot edit skeleton '{}': no mesh uses it", skeleton->GetName());

        return false;
    }

    if (!BeginEnter())
    {
        return false;
    }

    m_skeleton = skeleton;
    m_animation = animation;
    m_sourceEntity = previewSource;

    m_time = 0.0f;
    m_isPlaying = animation.IsValid();
    m_workingNotifies.Clear();

    CreateScenes(NAME_FMT("AnimationEdit_{}", skeleton->GetName()));
    BuildWorkingRoot();

    HYP_LOG(Editor, Info, "Editing skeleton '{}'", m_skeleton->GetName());

    if (graph.IsValid())
    {
        SetUpGraph(graph);
    }

    FinishEnter();

    OnTimelineChanged();

    return true;
}

void EditorAnimationEditState::ClearTarget()
{
    m_skeleton.Reset();
    m_animation.Reset();
    m_sourceEntity.Reset();

    m_workingRoot.Reset();
    m_previewEntity.Reset();
    m_boneNodes.Clear();
    m_unboundSockets.Clear();

    m_workingNotifies.Clear();

    m_sourceGraph.Reset();
    m_graph.Reset();
    m_graphSnapshot = String::empty;
    m_graphOwner.Reset();
    m_isGraphReadOnly = false;

    m_rootTrail.Clear();
    m_rootTrailClip = nullptr;

    m_isPlaying = false;
}

void EditorAnimationEditState::BuildWorkingRoot()
{
    m_boneNodes.Clear();
    m_unboundSockets.Clear();
    m_previewEntity.Reset();

    m_workingRoot = MakeHandle<Node>(m_skeleton->GetName());
    InitObject(m_workingRoot);

    m_editScene->SetRoot(m_workingRoot);

    const Handle<Entity>& source = m_sourceEntity;
    Assert(source.IsValid());

    m_previewEntity = TryCast<Entity>(source->Clone());
    Assert(m_previewEntity.IsValid(), "Failed to clone preview entity '{}'", source->GetName());

    for (const Handle<Node>& child : Array<Handle<Node>>(m_previewEntity->GetChildren()))
    {
        child->Remove();
    }

    Prefab::UntagAsPrefabInstance(m_previewEntity.Get());

    m_previewEntity->SetName(NAME("Preview"));
    // keep the source's orientation and scale, which importers often put on a parent (e.g. Z-up to Y-up)
    const Mat4f sourceWorldMatrix = source->GetWorldMatrix();
    m_previewEntity->SetLocalTransform(Transform(Vec3f::Zero(), sourceWorldMatrix.ExtractScale(), sourceWorldMatrix.ExtractRotation()));
    InitObject(m_previewEntity);

    m_workingRoot->AddChild(m_previewEntity);

    if (MeshComponent* meshComponent = m_previewEntity->TryGetComponent<MeshComponent>())
    {
        meshComponent->skeleton = m_skeleton;
    }

    if (!m_previewEntity->HasComponent<AnimationComponent>())
    {
        m_previewEntity->AddComponent<AnimationComponent>(AnimationComponent {});
    }

    const SkeletonBindData& bindData = m_skeleton->GetBindData();

    m_boneNodes.Reserve(bindData.NumSlots());

    for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
    {
        const uint32 parentSlot = bindData.parentSlots[slot];
        const Name parentBoneName = parentSlot != SkeletonBindData::InvalidSlot ? bindData.boneNames[parentSlot] : Name::Invalid();

        Handle<EditorBoneNode> boneNode = MakeHandle<EditorBoneNode>(this, bindData.boneNames[slot], slot, parentBoneName, bindData.restLocals[slot]);
        boneNode->SetName(IsJointSlot(bindData, slot) ? bindData.boneNames[slot] : NAME("(root)"));
        boneNode->SetLocalTransform(bindData.restLocals[slot]);
        InitObject(boneNode);

        Assert(parentSlot == SkeletonBindData::InvalidSlot || parentSlot < slot);

        Node* parentNode = parentSlot != SkeletonBindData::InvalidSlot
            ? static_cast<Node*>(m_boneNodes[parentSlot].Get())
            : static_cast<Node*>(m_previewEntity.Get());

        parentNode->AddChild(boneNode);

        m_boneNodes.PushBack(std::move(boneNode));
    }

    for (const SkeletonSocket& socket : m_skeleton->GetSockets())
    {
        const uint32 slot = bindData.FindSlot(socket.boneName);

        if (slot == SkeletonBindData::InvalidSlot)
        {
            HYP_LOG(Editor, Error, "Socket '{}' is on bone '{}', which skeleton '{}' doesn't have; it is kept as is", socket.name, socket.boneName, m_skeleton->GetName());

            m_unboundSockets.PushBack(socket);

            continue;
        }

        Handle<EditorSocketNode> socketNode = MakeHandle<EditorSocketNode>(socket.name);
        socketNode->SetLocalTransform(socket.offset);
        InitObject(socketNode);

        m_boneNodes[slot]->AddChild(socketNode);
    }

    ApplyPreviewClip();
}

bool EditorAnimationEditState::Apply()
{
    AssertOnThread(g_simThread);

    if (!IsActive())
    {
        return false;
    }

    const Handle<EditorProject>& project = m_subsystem->GetCurrentProject();
    Assert(project.IsValid());

    struct ClipNotifiesEdit
    {
        Handle<Animation> clip;
        Array<AnimationNotify> previousNotifies;
        Array<AnimationNotify> newNotifies;
    };

    Array<ClipNotifiesEdit> clipEdits;

    for (const auto& it : m_workingNotifies)
    {
        if (it.first.IsValid() && it.first->GetNotifies() != it.second)
        {
            clipEdits.PushBack(ClipNotifiesEdit { it.first, it.first->GetNotifies(), it.second });
        }
    }

    Handle<Skeleton> skeleton = m_skeleton;

    Array<SkeletonSocket> previousSockets = skeleton->GetSockets();
    Array<SkeletonSocket> newSockets = CollectSockets();

    Handle<AnimationGraph> sourceGraph = m_sourceGraph;
    String previousGraphText;
    String newGraphText;

    if (sourceGraph.IsValid() && !m_isGraphReadOnly)
    {
        Assert(m_graph.IsValid());

        previousGraphText = SaveGraphText(*sourceGraph);
        newGraphText = SaveGraphText(*m_graph);
    }

    const bool graphChanged = newGraphText != previousGraphText;

    if (newSockets != previousSockets || clipEdits.Any() || graphChanged)
    {
        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            HYP_FORMAT("Edit Skeleton {}", skeleton->GetName()),
            Proc<EditorActionFunctions()>(
                [skeleton, previousSockets, newSockets, clipEdits, sourceGraph, graphChanged, previousGraphText, newGraphText]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [skeleton, newSockets, clipEdits, sourceGraph, graphChanged, newGraphText](EditorSubsystem*, EditorProject*)
                            {
                                skeleton->SetSockets(newSockets);
                                skeleton->MarkDirty();

                                for (const ClipNotifiesEdit& clipEdit : clipEdits)
                                {
                                    clipEdit.clip->SetNotifies(clipEdit.newNotifies);
                                    clipEdit.clip->MarkDirty();
                                }

                                if (graphChanged)
                                {
                                    LoadGraphText(*sourceGraph, newGraphText);
                                    sourceGraph->MarkDirty();
                                }
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [skeleton, previousSockets, clipEdits, sourceGraph, graphChanged, previousGraphText](EditorSubsystem*, EditorProject*)
                            {
                                skeleton->SetSockets(previousSockets);
                                skeleton->MarkDirty();

                                for (const ClipNotifiesEdit& clipEdit : clipEdits)
                                {
                                    clipEdit.clip->SetNotifies(clipEdit.previousNotifies);
                                    clipEdit.clip->MarkDirty();
                                }

                                if (graphChanged)
                                {
                                    LoadGraphText(*sourceGraph, previousGraphText);
                                    sourceGraph->MarkDirty();
                                }
                            })
                    };
                }));

        InitObject(action);
        project->GetActionStack()->PushAction(action);

        HYP_LOG(Editor, Info, "Applied edits to skeleton '{}' ({} sockets, {} clips with changed notifies)", skeleton->GetName(), newSockets.Size(), clipEdits.Size());
    }

    m_workingNotifies.Clear();

    ResetActionStack();
    SetDirty(false);

    OnTimelineChanged();

    return true;
}

void EditorAnimationEditState::Revert()
{
    AssertOnThread(g_simThread);

    if (!IsActive())
    {
        return;
    }

    m_subsystem->SetFocusedNode(Handle<Node>::Null());
    m_subsystem->ClearSelection();

    m_workingNotifies.Clear();

    BuildWorkingRoot();

    if (m_sourceGraph.IsValid())
    {
        SetUpGraph(m_sourceGraph);
    }

    ResetActionStack();
    SetDirty(false);

    OnTimelineChanged();
}

String EditorAnimationEditState::GetDisplayName() const
{
    if (m_graph.IsValid())
    {
        return String(*m_graph->GetName());
    }

    return m_skeleton.IsValid() ? String(*m_skeleton->GetName()) : String::empty;
}

String EditorAnimationEditState::GetKindName() const
{
    if (IsEditingGraph())
    {
        return "animation graph";
    }

    return "skeleton";
}

const Handle<Skeleton>& EditorAnimationEditState::GetSkeleton() const
{
    return m_skeleton;
}

const Handle<Animation>& EditorAnimationEditState::GetAnimation() const
{
    return m_animation;
}

void EditorAnimationEditState::SetAnimation(const Handle<Animation>& animation)
{
    if (!IsActive())
    {
        return;
    }

    m_animation = animation;
    m_time = MathUtil::Min(m_time, GetLength());

    ApplyPreviewClip();

    OnTimelineChanged();
}

bool EditorAnimationEditState::SetAnimationByName(Name name)
{
    if (!IsActive())
    {
        return false;
    }

    Handle<Animation> animation = HumanoidSkeletonData::ForSkeleton(*m_skeleton)->FindCompatibleClip(name);

    SetAnimation(animation);

    return animation.IsValid();
}

Array<Name> EditorAnimationEditState::GetClipNames() const
{
    return m_skeleton.IsValid() ? HumanoidSkeletonData::ForSkeleton(*m_skeleton)->GetCompatibleClipNames() : Array<Name>();
}

Array<Name> EditorAnimationEditState::GetHumanoidSkeletonNames() const
{
    Array<Name> skeletonNames;

    if (!m_skeleton.IsValid() || !HumanoidSkeletonData::IsHumanoid(*m_skeleton))
    {
        return skeletonNames;
    }

    for (const Handle<Skeleton>& skeleton : HumanoidSkeletonData::CollectHumanoidSkeletons())
    {
        if (!skeletonNames.Contains(skeleton->GetName()))
        {
            skeletonNames.PushBack(skeleton->GetName());
        }
    }

    return skeletonNames;
}

Name EditorAnimationEditState::GetClipChoiceName() const
{
    if (!m_animation.IsValid())
    {
        return Name::Invalid();
    }

    if (!m_skeleton.IsValid() || m_skeleton->GetAnimations().Contains(m_animation))
    {
        return m_animation->GetName();
    }

    return NAME_FMT("{} ({})", m_animation->GetName(), m_animation->GetSourceSkeleton().GetName());
}

bool EditorAnimationEditState::EnterAnimationOnSkeleton(const Handle<Animation>& animation, Name skeletonName)
{
    Handle<Skeleton> skeleton = FindHumanoidSkeleton(skeletonName);

    if (!skeleton.IsValid())
    {
        HYP_LOG(Editor, Warning, "Cannot preview on '{}': no humanoid skeleton has that name", skeletonName);

        return false;
    }

    return EnterWith(skeleton, animation, Handle<Entity>::empty);
}

const Handle<Entity>& EditorAnimationEditState::GetPreviewEntity() const
{
    return m_previewEntity;
}

float EditorAnimationEditState::GetTime() const
{
    return m_time;
}

void EditorAnimationEditState::SetTime(float time)
{
    m_time = MathUtil::Clamp(time, 0.0f, GetLength());

    ApplyPreviewClip();
}

float EditorAnimationEditState::GetLength() const
{
    return m_animation.IsValid() ? m_animation->GetLength() : 0.0f;
}

float EditorAnimationEditState::GetFrameRate() const
{
    return PreviewFrameRate;
}

void EditorAnimationEditState::StepFrames(int32 numFrames)
{
    m_isPlaying = false;

    SetTime(m_time + float(numFrames) / PreviewFrameRate);
}

bool EditorAnimationEditState::IsPlaying() const
{
    return m_isPlaying;
}

void EditorAnimationEditState::SetPlaying(bool playing)
{
    m_isPlaying = playing && (m_animation.IsValid() || m_graph.IsValid());

    if (m_isPlaying && !m_isLooping && m_time >= GetLength())
    {
        m_time = 0.0f;
    }
}

bool EditorAnimationEditState::IsLooping() const
{
    return m_isLooping;
}

void EditorAnimationEditState::SetLooping(bool looping)
{
    m_isLooping = looping;
}

float EditorAnimationEditState::GetSpeed() const
{
    return m_speed;
}

void EditorAnimationEditState::SetSpeed(float speed)
{
    m_speed = MathUtil::Clamp(speed, 0.0f, 10.0f);
}

const Array<AnimationNotify>& EditorAnimationEditState::GetWorkingNotifies() const
{
    static const Array<AnimationNotify> s_noNotifies;

    if (!m_animation.IsValid())
    {
        return s_noNotifies;
    }

    auto it = m_workingNotifies.Find(m_animation);

    return it != m_workingNotifies.End() ? it->second : m_animation->GetNotifies();
}

void EditorAnimationEditState::PushNotifyEdit(const String& actionName, Array<AnimationNotify> notifies)
{
    if (!IsActive() || !m_animation.IsValid() || !m_actionStack.IsValid())
    {
        return;
    }

    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        actionName,
        Proc<EditorActionFunctions()>(
            [this, clip = m_animation, previousNotifies = GetWorkingNotifies(), newNotifies = std::move(notifies)]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [this, clip, newNotifies](EditorSubsystem*, EditorProject*)
                        {
                            m_workingNotifies[clip] = newNotifies;

                            OnTimelineChanged();
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [this, clip, previousNotifies](EditorSubsystem*, EditorProject*)
                        {
                            m_workingNotifies[clip] = previousNotifies;

                            OnTimelineChanged();
                        })
                };
            }));

    InitObject(action);
    m_actionStack->PushAction(action);
}

uint32 EditorAnimationEditState::NumNotifies() const
{
    return uint32(GetWorkingNotifies().Size());
}

Name EditorAnimationEditState::GetNotifyName(uint32 index) const
{
    return index < NumNotifies() ? GetWorkingNotifies()[index].name : Name::Invalid();
}

float EditorAnimationEditState::GetNotifyTime(uint32 index) const
{
    return index < NumNotifies() ? GetWorkingNotifies()[index].time : 0.0f;
}

float EditorAnimationEditState::GetNotifyDuration(uint32 index) const
{
    return index < NumNotifies() ? GetWorkingNotifies()[index].duration : 0.0f;
}

void EditorAnimationEditState::AddNotify(float time)
{
    if (!m_animation.IsValid())
    {
        return;
    }

    Array<AnimationNotify> notifies = GetWorkingNotifies();
    notifies.PushBack(AnimationNotify { NAME("Notify"), MathUtil::Clamp(time, 0.0f, GetLength()), 0.0f });

    PushNotifyEdit("Add Notify", std::move(notifies));
}

void EditorAnimationEditState::RemoveNotify(uint32 index)
{
    if (index >= NumNotifies())
    {
        return;
    }

    Array<AnimationNotify> notifies = GetWorkingNotifies();
    notifies.EraseAt(index);

    PushNotifyEdit("Remove Notify", std::move(notifies));
}

void EditorAnimationEditState::SetNotifyTime(uint32 index, float time)
{
    if (index >= NumNotifies())
    {
        return;
    }

    Array<AnimationNotify> notifies = GetWorkingNotifies();
    notifies[index].time = MathUtil::Clamp(time, 0.0f, GetLength());

    PushNotifyEdit("Move Notify", std::move(notifies));
}

void EditorAnimationEditState::SetNotifyName(uint32 index, Name name)
{
    if (index >= NumNotifies())
    {
        return;
    }

    Array<AnimationNotify> notifies = GetWorkingNotifies();
    notifies[index].name = name;

    PushNotifyEdit("Rename Notify", std::move(notifies));
}

void EditorAnimationEditState::SetNotifyDuration(uint32 index, float duration)
{
    if (index >= NumNotifies())
    {
        return;
    }

    Array<AnimationNotify> notifies = GetWorkingNotifies();
    notifies[index].duration = MathUtil::Clamp(duration, 0.0f, GetLength() - notifies[index].time);

    PushNotifyEdit("Change Notify Duration", std::move(notifies));
}

uint32 EditorAnimationEditState::NumSockets() const
{
    return uint32(CollectSockets().Size());
}

void EditorAnimationEditState::AddSocket(EditorBoneNode& bone)
{
    if (!IsActive() || !m_actionStack.IsValid() || !IsJointSlot(m_skeleton->GetBindData(), bone.GetBoneIndex()))
    {
        return;
    }

    const Array<SkeletonSocket> sockets = CollectSockets();

    Name socketName = NAME_FMT("{}_Socket", bone.GetBoneName());

    for (uint32 suffix = 1; AnyOf(sockets, [&socketName](const SkeletonSocket& socket) { return socket.name == socketName; }); ++suffix)
    {
        socketName = NAME_FMT("{}_Socket{}", bone.GetBoneName(), suffix);
    }

    Handle<EditorSocketNode> socketNode = MakeHandle<EditorSocketNode>(socketName);
    InitObject(socketNode);

    Handle<Node> boneNode = MakeStrongRef(&bone);

    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        HYP_FORMAT("Add Socket {}", socketName),
        Proc<EditorActionFunctions()>(
            [boneNode, socketNode]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [boneNode, socketNode](EditorSubsystem* subsystem, EditorProject*)
                        {
                            boneNode->AddChild(socketNode);

                            subsystem->ClearSelection();
                            subsystem->AddToSelection(socketNode);
                            subsystem->SetFocusedNode(socketNode);
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [socketNode](EditorSubsystem* subsystem, EditorProject*)
                        {
                            subsystem->RemoveFromSelection(socketNode);

                            if (subsystem->GetFocusedNode() == socketNode)
                            {
                                subsystem->SetFocusedNode(Handle<Node>::Null());
                            }

                            socketNode->Remove();
                        })
                };
            }));

    InitObject(action);
    m_actionStack->PushAction(action);
}

Array<SkeletonSocket> EditorAnimationEditState::CollectSockets() const
{
    Array<SkeletonSocket> sockets = m_unboundSockets;

    for (const Handle<EditorBoneNode>& boneNode : m_boneNodes)
    {
        for (const Handle<Node>& child : boneNode->GetChildren())
        {
            if (child.IsValid() && child->IsA<EditorSocketNode>())
            {
                sockets.PushBack(SkeletonSocket { child->GetName(), boneNode->GetBoneName(), child->GetLocalTransform() });
            }
        }
    }

    return sockets;
}

bool EditorAnimationEditState::IsProtectedNode(const Node* node) const
{
    if (!IsActive() || !node)
    {
        return false;
    }

    return node == m_workingRoot.Get() || node == m_previewEntity.Get() || node->IsA<EditorBoneNode>();
}

float EditorAnimationEditState::GetJointRadius() const
{
    if (!m_previewEntity.IsValid())
    {
        return 0.01f;
    }

    const BoundingBox bounds = m_previewEntity->GetWorldBounds();

    return bounds.IsValid() ? MathUtil::Max(bounds.GetRadius() * 0.025f, 0.004f) : 0.01f;
}

EditorBoneNode* EditorAnimationEditState::PickJoint(const Ray& ray, float& outDistance) const
{
    if (!IsActive() || !m_skeleton.IsValid())
    {
        return nullptr;
    }

    const SkeletonBindData& bindData = m_skeleton->GetBindData();
    const float pickRadius = GetJointRadius() * 1.5f;

    EditorBoneNode* closest = nullptr;
    outDistance = MathUtil::MaxSafeValue<float>();

    for (const Handle<EditorBoneNode>& boneNode : m_boneNodes)
    {
        if (!IsJointSlot(bindData, boneNode->GetBoneIndex()))
        {
            continue;
        }

        const Vec3f toJoint = boneNode->GetWorldTranslation() - ray.position;
        const float along = toJoint.Dot(ray.direction);

        if (along < 0.0f || along >= outDistance)
        {
            continue;
        }

        if (toJoint.LengthSquared() - along * along <= pickRadius * pickRadius)
        {
            closest = boneNode.Get();
            outDistance = along;
        }
    }

    return closest;
}

void EditorAnimationEditState::ApplyPreviewClip()
{
    if (!m_previewEntity.IsValid() || !m_previewEntity->GetEntityManager())
    {
        return;
    }

    if (AnimationComponent* animationComponent = m_previewEntity->TryGetComponent<AnimationComponent>())
    {
        animationComponent->animation = m_animation;
        animationComponent->graph = m_graph;
        animationComponent->playbackState.status = AnimationPlaybackStatus::Stopped;
    }

    AnimationSystem* animationSystem = GetProjectAnimationSystem(*m_subsystem);

    if (!animationSystem)
    {
        return;
    }

    if (m_graph.IsValid())
    {
        // the graph runs on its own clock; this only keeps the preview alive
        animationSystem->AdvanceGraphPreview(m_previewEntity.Get(), 0.0f);
    }
    else if (m_animation.IsValid())
    {
        animationSystem->SetPreviewTime(m_previewEntity.Get(), m_time);
    }
    else
    {
        animationSystem->StopPreview(m_previewEntity.Get());
    }
}

void EditorAnimationEditState::Update(float delta)
{
    if (!IsActive())
    {
        return;
    }

    const float length = GetLength();

    if (m_isPlaying && length > 0.0f)
    {
        m_time += delta * m_speed;

        if (m_time > length)
        {
            if (m_isLooping)
            {
                m_time = std::fmod(m_time, length);
            }
            else
            {
                m_time = length;
                m_isPlaying = false;
            }
        }

        ApplyPreviewClip();
    }

    if (m_graph.IsValid() && m_previewEntity.IsValid())
    {
        if (AnimationSystem* animationSystem = GetProjectAnimationSystem(*m_subsystem))
        {
            animationSystem->AdvanceGraphPreview(m_previewEntity.Get(), m_isPlaying ? delta * m_speed : 0.0f);
        }
    }

    UpdateBoneMirrors();
    UpdateRootTrail();
}

void EditorAnimationEditState::UpdateBoneMirrors()
{
    if (!m_previewEntity.IsValid() || m_boneNodes.Empty())
    {
        return;
    }

    const SkeletonBindData& bindData = m_skeleton->GetBindData();

    AnimationSystem* animationSystem = GetProjectAnimationSystem(*m_subsystem);

    Span<const Transform> pose = animationSystem && (m_animation.IsValid() || m_graph.IsValid())
        ? animationSystem->GetRenderPose(m_previewEntity.Get())
        : Span<const Transform>();

    if (pose.Size() != bindData.NumSlots())
    {
        pose = bindData.restLocals.ToSpan();
    }

    for (uint32 slot = 0; slot < m_boneNodes.Size() && slot < pose.Size(); ++slot)
    {
        if (!(m_boneNodes[slot]->GetLocalTransform() == pose[slot]))
        {
            m_boneNodes[slot]->SetLocalTransform(pose[slot]);
        }
    }
}

void EditorAnimationEditState::UpdateRootTrail()
{
    if (m_rootTrailClip == m_animation.Get())
    {
        return;
    }

    m_rootTrailClip = m_animation.Get();
    m_rootTrail.Clear();

    if (!m_animation.IsValid())
    {
        return;
    }

    const SkeletonBindData& bindData = m_skeleton->GetBindData();

    uint32 rootSlot = SkeletonBindData::InvalidSlot;

    for (uint32 slot = 0; slot < bindData.NumSlots() && rootSlot == SkeletonBindData::InvalidSlot; ++slot)
    {
        rootSlot = IsJointSlot(bindData, slot) ? slot : rootSlot;
    }

    if (rootSlot == SkeletonBindData::InvalidSlot)
    {
        return;
    }

    const ClipBinding binding = PoseOps::BindClip(*m_animation, *m_skeleton);

    Array<Mat4f> modelMatrices;
    Array<Mat4f> skinningMatrices;
    modelMatrices.Resize(bindData.NumSlots());
    skinningMatrices.Resize(bindData.NumSlots());

    const float length = m_animation->GetLength();

    for (uint32 sample = 0; sample < RootTrailSamples; ++sample)
    {
        Array<Transform> pose = Array<Transform>(PoseOps::GetRestPose(bindData));

        ApplyAnimParams params {};
        params.time = length * float(sample) / float(RootTrailSamples - 1);
        params.blend = 1.0f;

        PoseOps::ApplyBlended(*m_animation, binding, bindData, params, pose.ToSpan());
        PoseOps::ComputeMatrices(bindData, pose.ToSpan(), modelMatrices.ToSpan(), skinningMatrices.ToSpan());

        m_rootTrail.PushBack(modelMatrices[rootSlot].ExtractTranslation());
    }
}

void EditorAnimationEditState::DebugDraw(DebugDrawCommandList& debugDrawCommandList, Span<const Handle<Node>> selectedNodes) const
{
    if (!IsActive() || !m_previewEntity.IsValid())
    {
        return;
    }

    static const RenderableAttributeSet s_lineAttributes = OverlayAttributes(Topology::Lines, MaterialAttributeFlags::None);
    static const RenderableAttributeSet s_jointAttributes = OverlayAttributes(Topology::Triangles, MaterialAttributeFlags::None);

    static const Color s_boneColor = Color(0.35f, 0.85f, 1.0f, 1.0f);
    static const Color s_selectedColor = Color(1.0f, 0.8f, 0.2f, 1.0f);
    static const Color s_socketColor = Color(1.0f, 0.45f, 0.2f, 1.0f);
    static const Color s_trailColor = Color(0.6f, 1.0f, 0.4f, 1.0f);

    const SkeletonBindData& bindData = m_skeleton->GetBindData();
    const float jointRadius = GetJointRadius();

    const auto isSelected = [&selectedNodes](const Node* node)
    {
        return AnyOf(selectedNodes, [node](const Handle<Node>& selectedNode)
            {
                return selectedNode.Get() == node;
            });
    };

    for (const Handle<EditorBoneNode>& boneNode : m_boneNodes)
    {
        const uint32 slot = boneNode->GetBoneIndex();

        if (!IsJointSlot(bindData, slot))
        {
            continue;
        }

        const Vec3f joint = boneNode->GetWorldTranslation();
        const uint32 parentSlot = bindData.parentSlots[slot];

        if (parentSlot != SkeletonBindData::InvalidSlot && IsJointSlot(bindData, parentSlot))
        {
            debugDrawCommandList.line(m_boneNodes[parentSlot]->GetWorldTranslation(), joint, s_boneColor, s_lineAttributes);
        }

        const bool selected = isSelected(boneNode.Get());

        debugDrawCommandList.sphere(joint, selected ? jointRadius * 1.6f : jointRadius, selected ? s_selectedColor : s_boneColor, s_jointAttributes);

        for (const Handle<Node>& child : boneNode->GetChildren())
        {
            if (!child.IsValid() || !child->IsA<EditorSocketNode>())
            {
                continue;
            }

            const Vec3f socketPosition = child->GetWorldTranslation();
            const Color socketColor = isSelected(child.Get()) ? s_selectedColor : s_socketColor;

            debugDrawCommandList.line(joint, socketPosition, socketColor, s_lineAttributes);
            debugDrawCommandList.sphere(socketPosition, jointRadius * 0.8f, socketColor, s_jointAttributes);
        }
    }

    const Mat4f entityMatrix = m_previewEntity->GetWorldMatrix();

    for (uint32 sample = 1; sample < m_rootTrail.Size(); ++sample)
    {
        debugDrawCommandList.line((entityMatrix * Mat4f::Translation(m_rootTrail[sample - 1])).ExtractTranslation(), (entityMatrix * Mat4f::Translation(m_rootTrail[sample])).ExtractTranslation(), s_trailColor, s_lineAttributes);
    }
}

static bool ReferencesGraph(const Entity& entity, const AnimationGraph* graph)
{
    if (!entity.GetEntityManager())
    {
        return false;
    }

    if (const AnimationComponent* animationComponent = entity.TryGetComponent<AnimationComponent>(); animationComponent && animationComponent->graph.Get() == graph)
    {
        return true;
    }

    if (const CharacterModelComponent* characterModel = entity.TryGetComponent<CharacterModelComponent>())
    {
        if (characterModel->graph.IsValid())
        {
            return characterModel->graph.Get() == graph;
        }

        return GetDefaultCharacterGraph().Get() == graph;
    }

    return false;
}

static Entity* FindSkinnedSelfOrDescendant(Node& node)
{
    if (Entity* entity = TryCast<Entity>(&node); entity && GetEntitySkeleton(*entity).IsValid())
    {
        return entity;
    }

    for (Node* descendant : node.GetDescendants())
    {
        if (Entity* entity = TryCast<Entity>(descendant); entity && GetEntitySkeleton(*entity).IsValid())
        {
            return entity;
        }
    }

    return nullptr;
}

static Entity* FindGraphOwnerInSubtree(Node& node, const AnimationGraph* graph)
{
    if (Entity* entity = TryCast<Entity>(&node); entity && ReferencesGraph(*entity, graph))
    {
        return entity;
    }

    for (Node* descendant : node.GetDescendants())
    {
        if (Entity* entity = TryCast<Entity>(descendant); entity && ReferencesGraph(*entity, graph))
        {
            return entity;
        }
    }

    return nullptr;
}

static Entity* FindGraphOwner(EditorSubsystem& subsystem, const AnimationGraph* graph, Node* start)
{
    if (start)
    {
        for (Node* node = start; node; node = node->GetParent())
        {
            if (Entity* entity = TryCast<Entity>(node); entity && ReferencesGraph(*entity, graph))
            {
                return entity;
            }
        }

        if (Entity* owner = FindGraphOwnerInSubtree(*start, graph))
        {
            return owner;
        }
    }

    const Handle<World>& world = subsystem.GetProjectWorld();

    if (!world.IsValid())
    {
        return nullptr;
    }

    for (const Handle<Scene>& scene : world->GetScenes())
    {
        if (!scene.IsValid() || !scene->GetRoot().IsValid() || (scene->GetSceneFlags() & SceneFlags::EDITOR))
        {
            continue;
        }

        if (Entity* owner = FindGraphOwnerInSubtree(*scene->GetRoot(), graph); owner && FindSkinnedSelfOrDescendant(*owner))
        {
            return owner;
        }
    }

    return nullptr;
}

// whether target feeds into nodeIndex, directly or through other nodes
static bool FeedsInto(const AnimationGraph& graph, uint32 target, uint32 nodeIndex, uint32 depth = 0)
{
    const AnimationGraphNode* node = graph.GetNode(nodeIndex);

    if (!node || depth > graph.NumNodes())
    {
        return false;
    }

    for (uint32 slot = 0; slot < node->NumInputs(); ++slot)
    {
        const uint32 input = node->GetInput(slot);

        if (input == target || FeedsInto(graph, target, input, depth + 1))
        {
            return true;
        }
    }

    return false;
}

static Handle<AnimationGraph> GetReferencedGraph(const Entity& entity)
{
    if (!entity.GetEntityManager())
    {
        return Handle<AnimationGraph>::empty;
    }

    if (const AnimationComponent* animationComponent = entity.TryGetComponent<AnimationComponent>(); animationComponent && animationComponent->graph.IsValid())
    {
        return animationComponent->graph;
    }

    if (const CharacterModelComponent* characterModel = entity.TryGetComponent<CharacterModelComponent>())
    {
        if (characterModel->graph.IsValid())
        {
            return characterModel->graph;
        }

        return GetDefaultCharacterGraph();
    }

    return Handle<AnimationGraph>::empty;
}

Handle<AnimationGraph> EditorAnimationEditState::FindEntityGraph(const Handle<Entity>& entity) const
{
    if (!entity.IsValid())
    {
        return Handle<AnimationGraph>::empty;
    }

    for (Node* node = entity.Get(); node; node = node->GetParent())
    {
        if (const Entity* ancestor = TryCast<Entity>(node))
        {
            if (Handle<AnimationGraph> graph = GetReferencedGraph(*ancestor); graph.IsValid())
            {
                return graph;
            }
        }
    }

    for (Node* descendant : entity->GetDescendants())
    {
        if (const Entity* child = TryCast<Entity>(descendant))
        {
            if (Handle<AnimationGraph> graph = GetReferencedGraph(*child); graph.IsValid())
            {
                return graph;
            }
        }
    }

    return Handle<AnimationGraph>::empty;
}

Handle<Entity> EditorAnimationEditState::FindRunningGraphEntity(const Handle<Entity>& entity) const
{
    if (!entity.IsValid())
    {
        return Handle<Entity>::empty;
    }

    auto runsGraph = [](Node* node) -> Entity*
    {
        Entity* candidate = TryCast<Entity>(node);
        const AnimationComponent* animationComponent = candidate && candidate->GetEntityManager() ? candidate->TryGetComponent<AnimationComponent>() : nullptr;

        if (animationComponent && animationComponent->graphInstance.IsValid())
        {
            return candidate;
        }

        return nullptr;
    };

    if (Entity* running = runsGraph(entity.Get()))
    {
        return MakeStrongRef(running);
    }

    for (Node* descendant : entity->GetDescendants())
    {
        if (Entity* running = runsGraph(descendant))
        {
            return MakeStrongRef(running);
        }
    }

    for (Node* node = entity->GetParent(); node; node = node->GetParent())
    {
        if (Entity* running = runsGraph(node))
        {
            return MakeStrongRef(running);
        }
    }

    return Handle<Entity>::empty;
}

bool EditorAnimationEditState::EnterEntityGraph(const Handle<Entity>& entity)
{
    AssertOnThread(g_simThread);

    return EnterGraphFrom(FindEntityGraph(entity), entity.Get());
}

bool EditorAnimationEditState::EnterGraph(const Handle<AnimationGraph>& graph)
{
    AssertOnThread(g_simThread);

    return EnterGraphFrom(graph, m_subsystem->GetFocusedNode().Get());
}

bool EditorAnimationEditState::EnterGraphFrom(const Handle<AnimationGraph>& graph, Node* start)
{
    if (!graph.IsValid())
    {
        return false;
    }

    Handle<Entity> owner = MakeStrongRef(FindGraphOwner(*m_subsystem, graph.Get(), start));
    Handle<Entity> source;

    if (owner.IsValid())
    {
        source = MakeStrongRef(FindSkinnedSelfOrDescendant(*owner));
    }

    if (!source.IsValid() && graph->GetTargetSkeleton().IsValid())
    {
        source = MakeStrongRef(FindSkinnedEntityInWorld(*m_subsystem, graph->GetTargetSkeleton().Get(), nullptr));

        if (!source.IsValid())
        {
            source = FindSkinnedEntityInPrefabs(graph->GetTargetSkeleton().Get(), nullptr);
        }
    }

    for (const Handle<Animation>& clip : graph->CollectClips())
    {
        if (source.IsValid())
        {
            break;
        }

        source = MakeStrongRef(FindSkinnedEntityInWorld(*m_subsystem, nullptr, clip.Get()));

        if (!source.IsValid())
        {
            source = FindSkinnedEntityInPrefabs(nullptr, clip.Get());
        }
    }

    if (!source.IsValid())
    {
        source = MakeStrongRef(FindSkinnedEntityInWorld(*m_subsystem, nullptr, nullptr));
    }

    if (!source.IsValid())
    {
        source = FindSkinnedEntityInPrefabs(nullptr, nullptr);
    }

    if (!source.IsValid())
    {
        HYP_LOG(Editor, Warning, "Cannot edit animation graph '{}': no skinned entity in the world or in any prefab to preview it on", graph->GetName());

        return false;
    }

    if (!EnterWith(GetEntitySkeleton(*source), Handle<Animation>::empty, source, graph))
    {
        return false;
    }

    m_graphOwner = owner;
    m_isGraphReadOnly = graph->GetAssetRegistry() != GetCurrentAssetRegistry();

    if (!m_isGraphReadOnly && owner.IsValid() && !m_graph->GetTargetSkeleton().IsValid())
    {
        m_graph->SetTargetSkeleton(m_skeleton);
        m_graphSnapshot = SaveGraphText(*m_graph);
    }

    OnGraphChanged();

    return true;
}

bool EditorAnimationEditState::IsGraphReadOnly() const
{
    return IsEditingGraph() && m_isGraphReadOnly;
}

static Handle<Skeleton> FindSkeletonAsset(Name skeletonName)
{
    for (const Handle<AssetRegistry>& registry : { GetCurrentAssetRegistry(), GetEngineAssetRegistry() })
    {
        if (!registry.IsValid())
        {
            continue;
        }

        if (Handle<Skeleton> skeleton = registry->GetAsset<Skeleton>(AssetBuckets::Skeletons, skeletonName); skeleton.IsValid())
        {
            return skeleton;
        }
    }

    return Handle<Skeleton>::empty;
}

static bool IsPreviewCompatible(const Skeleton& entitySkeleton, const Skeleton& targetSkeleton)
{
    return &entitySkeleton == &targetSkeleton || (HumanoidSkeletonData::IsHumanoid(entitySkeleton) && HumanoidSkeletonData::IsHumanoid(targetSkeleton));
}

Array<Name> EditorAnimationEditState::GetSkeletonAssetNames() const
{
    Array<Name> names;

    for (const Handle<AssetRegistry>& registry : { GetCurrentAssetRegistry(), GetEngineAssetRegistry() })
    {
        if (!registry.IsValid())
        {
            continue;
        }

        Array<AssetDesc> descs;
        registry->GetBucketAssetDescs(AssetBuckets::Skeletons.GetIndex(), descs);

        for (const AssetDesc& desc : descs)
        {
            if (desc.name.IsValid() && !names.Contains(desc.name))
            {
                names.PushBack(desc.name);
            }
        }
    }

    return names;
}

Name EditorAnimationEditState::GetFocusedSkeletonName() const
{
    const Handle<Node> focused = m_subsystem->GetFocusedNode();

    if (!focused.IsValid())
    {
        return Name::Invalid();
    }

    for (Node* node = focused.Get(); node; node = node->GetParent())
    {
        if (const Entity* entity = TryCast<Entity>(node); entity && GetEntitySkeleton(*entity).IsValid())
        {
            return GetEntitySkeleton(*entity)->GetName();
        }
    }

    if (Entity* skinned = FindSkinnedSelfOrDescendant(*focused))
    {
        return GetEntitySkeleton(*skinned)->GetName();
    }

    return Name::Invalid();
}

Handle<AnimationGraph> EditorAnimationEditState::CreateGraphAsset(Name name, Name skeletonName)
{
    AssertOnThread(g_simThread);

    const Handle<EditorProject>& project = m_subsystem->GetCurrentProject();

    if (!project.IsValid() || !GetCurrentAssetRegistry().IsValid())
    {
        return Handle<AnimationGraph>::empty;
    }

    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(name.IsValid() ? name : NAME("NewAnimationGraph"));
    InitObject(graph);

    graph->SetTargetSkeleton(FindSkeletonAsset(skeletonName));

    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        "New AnimationGraph",
        Proc<EditorActionFunctions()>(
            [graph]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [graph](EditorSubsystem*, EditorProject*)
                        {
                            GetCurrentAssetRegistry()->PutAssetUnique(AssetBuckets::Animations, graph);
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [graph](EditorSubsystem*, EditorProject*)
                        {
                            GetCurrentAssetRegistry()->RemoveAsset(graph);
                        })
                };
            }));

    InitObject(action);
    project->GetActionStack()->PushAction(action);

    return graph;
}

Name EditorAnimationEditState::GetGraphTargetSkeletonName() const
{
    if (!IsEditingGraph() || !m_graph->GetTargetSkeleton().IsValid())
    {
        return Name::Invalid();
    }

    return m_graph->GetTargetSkeleton()->GetName();
}

void EditorAnimationEditState::SetGraphTargetSkeleton(Name skeletonName)
{
    AssertOnThread(g_simThread);

    if (!IsEditingGraph())
    {
        return;
    }

    const Handle<Skeleton> skeleton = FindSkeletonAsset(skeletonName);

    m_graph->SetTargetSkeleton(skeleton);

    CommitGraphEdit("Set Graph Skeleton");

    if (!skeleton.IsValid() || (m_skeleton.IsValid() && IsPreviewCompatible(*m_skeleton, *skeleton)))
    {
        return;
    }

    Handle<Entity> source = MakeStrongRef(FindSkinnedEntityInWorld(*m_subsystem, skeleton.Get(), nullptr));

    if (!source.IsValid())
    {
        source = FindSkinnedEntityInPrefabs(skeleton.Get(), nullptr);
    }

    if (source.IsValid())
    {
        SetGraphPreviewSource(source);
    }
}

bool EditorAnimationEditState::CopyGraphToProject()
{
    AssertOnThread(g_simThread);

    if (!IsEditingGraph() || !m_isGraphReadOnly)
    {
        return false;
    }

    const Handle<AssetRegistry> registry = GetCurrentAssetRegistry();

    if (!registry.IsValid())
    {
        return false;
    }

    Handle<AnimationGraph> copy = MakeHandle<AnimationGraph>(m_sourceGraph->GetName());
    InitObject(copy);
    LoadGraphText(*copy, SaveGraphText(*m_graph));

    registry->PutAssetUnique(AssetBuckets::Animations, copy);
    copy->MarkDirty();

    if (m_graphOwner.IsValid() && m_graphOwner->GetEntityManager())
    {
        const Handle<Entity> owner = m_graphOwner;
        const Handle<EditorProject>& project = m_subsystem->GetCurrentProject();

        auto setOwnerGraph = [owner](const Handle<AnimationGraph>& graph)
        {
            if (!owner.IsValid() || !owner->GetEntityManager())
            {
                return;
            }

            if (CharacterModelComponent* characterModel = owner->TryGetComponent<CharacterModelComponent>())
            {
                characterModel->graph = graph;
            }
            else if (AnimationComponent* animationComponent = owner->TryGetComponent<AnimationComponent>())
            {
                animationComponent->graph = graph;
            }
        };

        Handle<AnimationGraph> previousGraph;

        if (const CharacterModelComponent* characterModel = owner->TryGetComponent<CharacterModelComponent>())
        {
            previousGraph = characterModel->graph;
        }
        else if (const AnimationComponent* animationComponent = owner->TryGetComponent<AnimationComponent>())
        {
            previousGraph = animationComponent->graph;
        }

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            HYP_FORMAT("Use Graph {}", copy->GetName()),
            Proc<EditorActionFunctions()>(
                [setOwnerGraph, copy, previousGraph]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [setOwnerGraph, copy](EditorSubsystem*, EditorProject*)
                            {
                                setOwnerGraph(copy);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [setOwnerGraph, previousGraph](EditorSubsystem*, EditorProject*)
                            {
                                setOwnerGraph(previousGraph);
                            })
                    };
                }));

        InitObject(action);

        if (project.IsValid())
        {
            project->GetActionStack()->PushAction(action);
        }
    }

    m_sourceGraph = copy;
    m_isGraphReadOnly = false;

    HYP_LOG(Editor, Info, "Copied animation graph '{}' into the project", copy->GetName());

    OnGraphChanged();

    return true;
}

Array<Handle<Entity>> EditorAnimationEditState::GetGraphPreviewCandidates() const
{
    Array<Handle<Entity>> candidates;

    const Handle<World>& world = m_subsystem->GetProjectWorld();

    if (!IsEditingGraph() || !world.IsValid())
    {
        return candidates;
    }

    const Skeleton* target = m_graph->GetTargetSkeleton().Get();

    for (const Handle<Scene>& scene : world->GetScenes())
    {
        if (!scene.IsValid() || !scene->GetRoot().IsValid() || (scene->GetSceneFlags() & SceneFlags::EDITOR))
        {
            continue;
        }

        for (Node* descendant : scene->GetRoot()->GetDescendants())
        {
            Entity* entity = TryCast<Entity>(descendant);

            if (!entity || !GetEntitySkeleton(*entity).IsValid())
            {
                continue;
            }

            if (target && !IsPreviewCompatible(*GetEntitySkeleton(*entity), *target) && entity != m_sourceEntity.Get())
            {
                continue;
            }

            candidates.PushBack(MakeStrongRef(entity));
        }
    }

    return candidates;
}

const Handle<Entity>& EditorAnimationEditState::GetPreviewSource() const
{
    return m_sourceEntity;
}

bool EditorAnimationEditState::SetGraphPreviewSource(const Handle<Entity>& entity)
{
    AssertOnThread(g_simThread);

    if (!IsEditingGraph() || !entity.IsValid() || entity == m_sourceEntity)
    {
        return false;
    }

    const Handle<Skeleton>& skeleton = GetEntitySkeleton(*entity);

    if (!skeleton.IsValid() || !skeleton->GetRootBone().IsValid())
    {
        return false;
    }

    m_subsystem->SetFocusedNode(Handle<Node>::Null());
    m_subsystem->ClearSelection();

    m_skeleton = skeleton;
    m_sourceEntity = entity;

    BuildWorkingRoot();

    OnTimelineChanged();
    OnGraphChanged();

    return true;
}

static bool HasOverlappingNodes(const AnimationGraph& graph)
{
    for (uint32 first = 0; first < graph.NumNodes(); ++first)
    {
        for (uint32 second = first + 1; second < graph.NumNodes(); ++second)
        {
            const AnimationGraphNode* a = graph.GetNode(first);
            const AnimationGraphNode* b = graph.GetNode(second);

            if (a && b && MathUtil::Abs(a->editorPosition.x - b->editorPosition.x) < 1.0f && MathUtil::Abs(a->editorPosition.y - b->editorPosition.y) < 1.0f)
            {
                return true;
            }
        }
    }

    return false;
}

static void ArrangeNodes(AnimationGraph& graph)
{
    static constexpr float ColumnSpacing = 240.0f;
    static constexpr float RowSpacing = 120.0f;

    const uint32 numNodes = graph.NumNodes();

    Array<uint32> depths;
    depths.Resize(numNodes);

    for (uint32& depth : depths)
    {
        depth = ~0u;
    }

    Array<uint32> pending;

    if (graph.GetNode(graph.GetRootNode()))
    {
        depths[graph.GetRootNode()] = 0;
        pending.PushBack(graph.GetRootNode());
    }

    for (size_t cursor = 0; cursor < pending.Size(); ++cursor)
    {
        const uint32 nodeIndex = pending[cursor];
        const AnimationGraphNode* node = graph.GetNode(nodeIndex);
        const uint32 inputDepth = depths[nodeIndex] + 1;

        if (!node || inputDepth > numNodes)
        {
            continue;
        }

        for (uint32 slot = 0; slot < node->NumInputs(); ++slot)
        {
            const uint32 input = node->GetInput(slot);

            if (input < numNodes && (depths[input] == ~0u || depths[input] < inputDepth))
            {
                depths[input] = inputDepth;
                pending.PushBack(input);
            }
        }
    }

    uint32 maxDepth = 0;

    for (uint32 depth : depths)
    {
        if (depth != ~0u)
        {
            maxDepth = MathUtil::Max(maxDepth, depth);
        }
    }

    Array<uint32> rows;
    rows.Resize(maxDepth + 2);

    for (uint32 nodeIndex = 0; nodeIndex < numNodes; ++nodeIndex)
    {
        Handle<AnimationGraphNode> node = graph.GetNodeObject(nodeIndex);

        if (!node.IsValid())
        {
            continue;
        }

        uint32 depth = depths[nodeIndex];

        if (depth == ~0u)
        {
            depth = maxDepth + 1;
        }

        node->editorPosition = Vec2f(-float(depth) * ColumnSpacing, float(rows[depth]++) * RowSpacing);
    }

    graph.MarkChanged();
}

void EditorAnimationEditState::ArrangeGraphNodes()
{
    if (!m_graph.IsValid())
    {
        return;
    }

    ArrangeNodes(*m_graph);

    CommitGraphEdit("Arrange Graph");
}

void EditorAnimationEditState::SetUpGraph(const Handle<AnimationGraph>& sourceGraph)
{
    const String text = SaveGraphText(*sourceGraph);

    m_sourceGraph = sourceGraph;

    m_graph = MakeHandle<AnimationGraph>(sourceGraph->GetName());
    InitObject(m_graph);
    LoadGraphText(*m_graph, text);

    m_graphSnapshot = text;

    if (HasOverlappingNodes(*m_graph))
    {
        ArrangeNodes(*m_graph);

        m_graphSnapshot = SaveGraphText(*m_graph);
    }

    m_animation.Reset();
    m_time = 0.0f;
    m_isPlaying = true;

    ApplyPreviewClip();

    OnTimelineChanged();
    OnGraphChanged();
}

void EditorAnimationEditState::RestoreGraph(const String& text)
{
    Assert(m_graph.IsValid());

    LoadGraphText(*m_graph, text);
    m_graphSnapshot = text;

    OnGraphChanged();
}

void EditorAnimationEditState::CommitGraphEdit(const String& actionName)
{
    if (!IsActive() || !m_graph.IsValid() || !m_actionStack.IsValid())
    {
        return;
    }

    const String newText = SaveGraphText(*m_graph);

    if (newText == m_graphSnapshot)
    {
        return;
    }

    m_graph->MarkChanged();

    const String previousText = m_graphSnapshot;
    m_graphSnapshot = newText;

    // the edit is already on the working graph, so the push mustn't reload it (that would swap out the node objects)
    auto isApplied = MakeShared<bool>(true);

    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        actionName,
        Proc<EditorActionFunctions()>(
            [this, previousText, newText, isApplied]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [this, newText, isApplied](EditorSubsystem*, EditorProject*)
                        {
                            if (*isApplied)
                            {
                                *isApplied = false;

                                return;
                            }

                            RestoreGraph(newText);
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [this, previousText](EditorSubsystem*, EditorProject*)
                        {
                            RestoreGraph(previousText);
                        })
                };
            }));

    InitObject(action);
    m_actionStack->PushAction(action);

    OnGraphChanged();
}

const Handle<AnimationGraph>& EditorAnimationEditState::GetGraph() const
{
    return m_graph;
}

bool EditorAnimationEditState::IsEditingGraph() const
{
    return IsActive() && m_graph.IsValid();
}

Array<Name> EditorAnimationEditState::GetGraphNodeClasses() const
{
    return AnimationGraph::GetNodeClassNames();
}

AnimationGraphNode* EditorAnimationEditState::GetGraphNode(uint32 nodeIndex) const
{
    return m_graph.IsValid() ? m_graph->GetMutableNode(nodeIndex) : nullptr;
}

uint32 EditorAnimationEditState::AddGraphNode(Name className, Vec2f position)
{
    if (!m_graph.IsValid())
    {
        return InvalidAnimationGraphNode;
    }

    Handle<AnimationGraphNode> node = AnimationGraph::CreateNode(className);

    if (!node.IsValid())
    {
        return InvalidAnimationGraphNode;
    }

    node->editorPosition = position;

    const uint32 nodeIndex = m_graph->AddNode(node);

    if (m_graph->GetNode(m_graph->GetRootNode()) == nullptr)
    {
        m_graph->SetRootNode(nodeIndex);
    }

    CommitGraphEdit("Add Graph Node");

    return nodeIndex;
}

uint32 EditorAnimationEditState::AddGraphClipNode(const Handle<Animation>& clip, Vec2f position)
{
    if (!m_graph.IsValid())
    {
        return InvalidAnimationGraphNode;
    }

    Handle<AnimationClipNode> clipNode = MakeHandle<AnimationClipNode>();
    clipNode->clip = clip;
    clipNode->name = clip.IsValid() ? clip->GetName() : Name::Invalid();
    clipNode->editorPosition = position;

    const uint32 nodeIndex = m_graph->AddNode(clipNode);

    if (m_graph->GetNode(m_graph->GetRootNode()) == nullptr)
    {
        m_graph->SetRootNode(nodeIndex);
    }

    CommitGraphEdit("Add Clip Node");

    return nodeIndex;
}

void EditorAnimationEditState::RemoveGraphNode(uint32 nodeIndex)
{
    if (!GetGraphNode(nodeIndex))
    {
        return;
    }

    m_graph->RemoveNode(nodeIndex);

    CommitGraphEdit("Remove Graph Node");
}

bool EditorAnimationEditState::LinkGraphInput(uint32 fromNode, uint32 toNode, uint32 slot)
{
    AnimationGraphNode* node = GetGraphNode(toNode);

    // a connection back into one of its own inputs would make a cycle the runtime has to drop
    if (!node || !GetGraphNode(fromNode) || fromNode == toNode || FeedsInto(*m_graph, toNode, fromNode))
    {
        return false;
    }

    if (slot < node->NumInputs())
    {
        node->SetInput(slot, fromNode);

        return true;
    }

    return node->AddInput(fromNode);
}

void EditorAnimationEditState::ConnectGraphNodes(uint32 fromNode, uint32 toNode, uint32 slot)
{
    if (LinkGraphInput(fromNode, toNode, slot))
    {
        CommitGraphEdit("Connect Graph Nodes");
    }
}

void EditorAnimationEditState::DisconnectGraphInput(uint32 nodeIndex, uint32 slot)
{
    AnimationGraphNode* node = GetGraphNode(nodeIndex);

    if (!node || slot >= node->NumInputs())
    {
        return;
    }

    UnlinkGraphInput(*node, slot);

    CommitGraphEdit("Disconnect Graph Input");
}

void EditorAnimationEditState::UnlinkGraphInput(AnimationGraphNode& node, uint32 slot)
{
    // a state machine's inputs are its states, which outlive their links; other nodes drop the input (blend samples included)
    if (node.IsA<AnimationStateMachineNode>())
    {
        node.SetInput(slot, InvalidAnimationGraphNode);
    }
    else
    {
        node.RemoveInput(slot);
    }
}

void EditorAnimationEditState::RewireGraphInput(uint32 nodeIndex, uint32 slot, uint32 toNode, uint32 toSlot)
{
    AnimationGraphNode* node = GetGraphNode(nodeIndex);

    if (!node || slot >= node->NumInputs() || (nodeIndex == toNode && slot == toSlot))
    {
        return;
    }

    const uint32 fromNode = node->GetInput(slot);

    if (fromNode == InvalidAnimationGraphNode)
    {
        return;
    }

    // link first so a refused link leaves the old one untouched; removing the old slot afterwards can't shift the new one below it
    if (!LinkGraphInput(fromNode, toNode, toSlot))
    {
        return;
    }

    UnlinkGraphInput(*node, slot);

    CommitGraphEdit("Rewire Graph Input");
}

void EditorAnimationEditState::SetGraphRootNode(uint32 nodeIndex)
{
    if (!GetGraphNode(nodeIndex))
    {
        return;
    }

    m_graph->SetRootNode(nodeIndex);

    CommitGraphEdit("Set Graph Output");
}

void EditorAnimationEditState::MoveGraphNode(uint32 nodeIndex, Vec2f position)
{
    if (AnimationGraphNode* node = GetGraphNode(nodeIndex))
    {
        node->editorPosition = position;

        CommitGraphEdit("Move Graph Node");
    }
}

void EditorAnimationEditState::SetBlendSamplePosition(uint32 nodeIndex, uint32 sample, Vec2f position)
{
    AnimationGraphNode* node = GetGraphNode(nodeIndex);

    Array<AnimationBlendSample>* samples = nullptr;

    if (AnimationBlend1DNode* blend1D = TryCast<AnimationBlend1DNode>(node))
    {
        samples = &blend1D->samples;
        position.y = 0.0f;
    }
    else if (AnimationBlend2DNode* blend2D = TryCast<AnimationBlend2DNode>(node))
    {
        samples = &blend2D->samples;
    }

    if (!samples || sample >= samples->Size())
    {
        return;
    }

    (*samples)[sample].position = position;

    CommitGraphEdit("Move Blend Sample");
}

void EditorAnimationEditState::RenameGraphState(uint32 nodeIndex, uint32 stateIndex, Name name)
{
    AnimationStateMachineNode* stateMachine = TryCast<AnimationStateMachineNode>(GetGraphNode(nodeIndex));

    if (!stateMachine || stateIndex >= stateMachine->states.Size())
    {
        return;
    }

    stateMachine->states[stateIndex].name = name;

    CommitGraphEdit("Rename State");
}

uint32 EditorAnimationEditState::AddGraphTransition(uint32 nodeIndex, uint32 from, bool fromAnyState, uint32 to, float duration)
{
    AnimationStateMachineNode* stateMachine = TryCast<AnimationStateMachineNode>(GetGraphNode(nodeIndex));

    if (!stateMachine || to >= stateMachine->states.Size() || (!fromAnyState && from >= stateMachine->states.Size()))
    {
        return InvalidAnimationGraphNode;
    }

    AnimationStateTransition transition;
    transition.from = fromAnyState ? 0 : from;
    transition.fromAnyState = fromAnyState;
    transition.to = to;
    transition.duration = MathUtil::Max(duration, 0.0f);

    stateMachine->transitions.PushBack(transition);

    CommitGraphEdit("Add Transition");

    return uint32(stateMachine->transitions.Size() - 1);
}

void EditorAnimationEditState::AddGraphTransitionCondition(uint32 nodeIndex, uint32 transitionIndex, Name parameter, uint32 op, float value)
{
    AnimationStateMachineNode* stateMachine = TryCast<AnimationStateMachineNode>(GetGraphNode(nodeIndex));

    if (!stateMachine || transitionIndex >= stateMachine->transitions.Size() || op > uint32(AnimationConditionOp::StateNormalizedTimeAtLeast))
    {
        return;
    }

    stateMachine->transitions[transitionIndex].conditions.PushBack(AnimationTransitionCondition { parameter, AnimationConditionOp(op), value });

    CommitGraphEdit("Add Transition Condition");
}

uint32 EditorAnimationEditState::AddGraphState(uint32 nodeIndex, Vec2f position)
{
    AnimationStateMachineNode* stateMachine = TryCast<AnimationStateMachineNode>(GetGraphNode(nodeIndex));

    if (!stateMachine)
    {
        return InvalidAnimationGraphNode;
    }

    stateMachine->AddInput(InvalidAnimationGraphNode);
    stateMachine->states.Back().editorPosition = position;

    CommitGraphEdit("Add State");

    return uint32(stateMachine->states.Size() - 1);
}

void EditorAnimationEditState::RemoveGraphState(uint32 nodeIndex, uint32 stateIndex)
{
    AnimationStateMachineNode* stateMachine = TryCast<AnimationStateMachineNode>(GetGraphNode(nodeIndex));

    if (!stateMachine || stateIndex >= stateMachine->states.Size())
    {
        return;
    }

    stateMachine->RemoveInput(stateIndex);

    CommitGraphEdit("Remove State");
}

void EditorAnimationEditState::MoveGraphState(uint32 nodeIndex, uint32 stateIndex, Vec2f position)
{
    AnimationStateMachineNode* stateMachine = TryCast<AnimationStateMachineNode>(GetGraphNode(nodeIndex));

    if (!stateMachine || stateIndex >= stateMachine->states.Size())
    {
        return;
    }

    stateMachine->states[stateIndex].editorPosition = position;

    CommitGraphEdit("Move State");
}

void EditorAnimationEditState::SetGraphEntryState(uint32 nodeIndex, uint32 stateIndex)
{
    AnimationStateMachineNode* stateMachine = TryCast<AnimationStateMachineNode>(GetGraphNode(nodeIndex));

    if (!stateMachine || stateIndex >= stateMachine->states.Size() || stateMachine->entryState == stateIndex)
    {
        return;
    }

    stateMachine->entryState = stateIndex;

    CommitGraphEdit("Set Entry State");
}

void EditorAnimationEditState::RemoveGraphTransition(uint32 nodeIndex, uint32 transitionIndex)
{
    AnimationStateMachineNode* stateMachine = TryCast<AnimationStateMachineNode>(GetGraphNode(nodeIndex));

    if (!stateMachine || transitionIndex >= stateMachine->transitions.Size())
    {
        return;
    }

    stateMachine->transitions.EraseAt(transitionIndex);

    CommitGraphEdit("Remove Transition");
}

void EditorAnimationEditState::SetGraphTransition(uint32 nodeIndex, uint32 transitionIndex, float duration, uint32 curve, int32 priority, bool interruptible, uint32 mode)
{
    AnimationStateMachineNode* stateMachine = TryCast<AnimationStateMachineNode>(GetGraphNode(nodeIndex));

    if (!stateMachine || transitionIndex >= stateMachine->transitions.Size() || curve > uint32(AnimationBlendCurve::EaseOut) || mode > uint32(AnimationTransitionMode::Inertialize))
    {
        return;
    }

    AnimationStateTransition& transition = stateMachine->transitions[transitionIndex];
    transition.duration = MathUtil::Max(duration, 0.0f);
    transition.curve = AnimationBlendCurve(curve);
    transition.priority = priority;
    transition.interruptible = interruptible;
    transition.mode = AnimationTransitionMode(mode);

    CommitGraphEdit("Edit Transition");
}

void EditorAnimationEditState::SetGraphTransitionCondition(uint32 nodeIndex, uint32 transitionIndex, uint32 conditionIndex, Name parameter, uint32 op, float value)
{
    AnimationStateMachineNode* stateMachine = TryCast<AnimationStateMachineNode>(GetGraphNode(nodeIndex));

    if (!stateMachine || conditionIndex >= stateMachine->NumConditions(transitionIndex) || op > uint32(AnimationConditionOp::StateNormalizedTimeAtLeast))
    {
        return;
    }

    stateMachine->transitions[transitionIndex].conditions[conditionIndex] = AnimationTransitionCondition { parameter, AnimationConditionOp(op), value };

    CommitGraphEdit("Edit Transition Condition");
}

void EditorAnimationEditState::RemoveGraphTransitionCondition(uint32 nodeIndex, uint32 transitionIndex, uint32 conditionIndex)
{
    AnimationStateMachineNode* stateMachine = TryCast<AnimationStateMachineNode>(GetGraphNode(nodeIndex));

    if (!stateMachine || conditionIndex >= stateMachine->NumConditions(transitionIndex))
    {
        return;
    }

    stateMachine->transitions[transitionIndex].conditions.EraseAt(conditionIndex);

    CommitGraphEdit("Remove Transition Condition");
}

Vec2f EditorAnimationEditState::GetBlendSamplePosition(uint32 nodeIndex, uint32 sample) const
{
    const AnimationGraphNode* node = GetGraphNode(nodeIndex);

    if (const AnimationBlend1DNode* blend1D = TryCast<AnimationBlend1DNode>(node); blend1D && sample < blend1D->samples.Size())
    {
        return blend1D->samples[sample].position;
    }

    if (const AnimationBlend2DNode* blend2D = TryCast<AnimationBlend2DNode>(node); blend2D && sample < blend2D->samples.Size())
    {
        return blend2D->samples[sample].position;
    }

    return Vec2f::Zero();
}

Name EditorAnimationEditState::GetBlendParameter(uint32 nodeIndex) const
{
    const AnimationGraphNode* node = GetGraphNode(nodeIndex);

    if (const AnimationBlend1DNode* blend1D = TryCast<AnimationBlend1DNode>(node))
    {
        return blend1D->parameter;
    }

    if (const AnimationBlend2DNode* blend2D = TryCast<AnimationBlend2DNode>(node))
    {
        return blend2D->parameter;
    }

    return Name::Invalid();
}

void EditorAnimationEditState::AddGraphParameter(Name parameter, uint32 type)
{
    if (!m_graph.IsValid() || !parameter.IsValid() || m_graph->GetParameterNames().Contains(parameter))
    {
        return;
    }

    m_graph->AddParameter(parameter, AnimationGraphParameterType(MathUtil::Min(type, uint32(AnimationGraphParameterType::Vector2))));

    CommitGraphEdit("Add Graph Parameter");
}

void EditorAnimationEditState::RemoveGraphParameter(Name parameter)
{
    if (!m_graph.IsValid())
    {
        return;
    }

    m_graph->RemoveParameter(parameter);

    CommitGraphEdit("Remove Graph Parameter");
}

uint32 EditorAnimationEditState::AddGraphClipNodeByName(Name clipName, Vec2f position)
{
    for (const Handle<AssetRegistry>& registry : { GetCurrentAssetRegistry(), GetEngineAssetRegistry() })
    {
        if (!registry.IsValid())
        {
            continue;
        }

        if (Handle<Animation> clip = registry->GetAsset<Animation>(AssetBuckets::Animations, clipName); clip.IsValid())
        {
            return AddGraphClipNode(clip, position);
        }
    }

    return InvalidAnimationGraphNode;
}

bool EditorAnimationEditState::RenameGraphParameter(Name from, Name to)
{
    if (!m_graph.IsValid() || !to.IsValid() || from == to)
    {
        return false;
    }

    const Array<Name> names = m_graph->GetParameterNames();

    if (!names.Contains(from) || names.Contains(to))
    {
        return false;
    }

    m_graph->RenameParameter(from, to);

    CommitGraphEdit("Rename Graph Parameter");

    return true;
}

void EditorAnimationEditState::SetGraphParameterDefault(Name parameter, Vec2f value)
{
    if (!m_graph.IsValid())
    {
        return;
    }

    m_graph->SetParameterDefault(parameter, value);

    CommitGraphEdit("Set Graph Parameter Default");

    SetPreviewParameter(parameter, value);
}

void EditorAnimationEditState::SetGraphParameterType(Name parameter, uint32 type)
{
    if (!m_graph.IsValid())
    {
        return;
    }

    m_graph->SetParameterType(parameter, AnimationGraphParameterType(MathUtil::Min(type, uint32(AnimationGraphParameterType::Vector2))));

    CommitGraphEdit("Set Graph Parameter Type");
}

void EditorAnimationEditState::RenameGraphNode(uint32 nodeIndex, Name name)
{
    if (AnimationGraphNode* node = GetGraphNode(nodeIndex))
    {
        node->name = name;

        CommitGraphEdit("Rename Graph Node");
    }
}

uint32 EditorAnimationEditState::DuplicateGraphNode(uint32 nodeIndex, Vec2f offset)
{
    if (!m_graph.IsValid() || !GetGraphNode(nodeIndex))
    {
        return InvalidAnimationGraphNode;
    }

    BoxedValue copy;

    if (!CloneWithoutTransientMembers(BoxedValue(m_graph->GetNodeObject(nodeIndex)), copy) || !copy.Is<Handle<AnimationGraphNode>>())
    {
        return InvalidAnimationGraphNode;
    }

    Handle<AnimationGraphNode> node = copy.Get<Handle<AnimationGraphNode>>();
    node->editorPosition += offset;

    const uint32 duplicateIndex = m_graph->AddNode(node);

    CommitGraphEdit("Duplicate Graph Node");

    return duplicateIndex;
}

AnimationGraphInstance* EditorAnimationEditState::GetPreviewGraphInstance() const
{
    const AnimationComponent* animationComponent = m_previewEntity.IsValid() && m_previewEntity->GetEntityManager()
        ? m_previewEntity->TryGetComponent<AnimationComponent>()
        : nullptr;

    return animationComponent ? animationComponent->graphInstance.Get() : nullptr;
}

void EditorAnimationEditState::SetPreviewParameter(Name parameter, Vec2f value)
{
    if (AnimationGraphInstance* instance = GetPreviewGraphInstance())
    {
        instance->SetVector2(parameter, value);
    }
}

void EditorAnimationEditState::TriggerPreviewParameter(Name parameter)
{
    if (AnimationGraphInstance* instance = GetPreviewGraphInstance())
    {
        instance->SetTrigger(parameter);
    }
}

#pragma endregion EditorAnimationEditState

} // namespace Hyperion
