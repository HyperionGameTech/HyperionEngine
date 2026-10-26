/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <AnimationEditor/AnimationEditorExtension.hpp>
#include <AnimationEditor/EditorAnimationEditState.hpp>
#include <Editor/EditorSubsystem.hpp>

#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Animation/AnimationComponent.hpp>

#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Animation/Bone.hpp>
#include <Scene/Animation/Animation.hpp>

#include <Animation/AnimationSystem.hpp>

#include <Rendering/DebugDrawer.hpp>

#include <Framework/CVarManager.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Ray.hpp>

#include <cmath>

#include <AnimationEditorExtension.generated.inl>

namespace Hyperion {

static CVar<bool> s_cvShowBones { "Editor.ShowBones", false };

static RenderableAttributeSet BoneOverlayAttributes(Topology topology)
{
    RenderableAttributeSet attributes;

    MeshAttributes& meshAttributes = attributes.GetMeshAttributes();
    meshAttributes.inputLayout = StaticVertexInputLayout<VT_Simple>;
    meshAttributes.topology = topology;

    MaterialAttributes& materialAttributes = attributes.GetMaterialAttributes();
    materialAttributes.bucket = RenderBucket::Debug;
    materialAttributes.fillMode = topology == Topology::Lines ? FillMode::Fill : FillMode::Line;
    materialAttributes.blendFunction = BlendFunction::None();
    materialAttributes.flags = MaterialAttributeFlags::None;

    return attributes;
}

static Animation* ResolvePreviewAnimation(Entity& entity)
{
    if (!entity.GetEntityManager())
    {
        return nullptr;
    }

    const AnimationComponent* animationComponent = entity.TryGetComponent<AnimationComponent>();
    const MeshComponent* meshComponent = entity.TryGetComponent<MeshComponent>();

    if (!animationComponent || !meshComponent || !meshComponent->skeleton.IsValid())
    {
        return nullptr;
    }

    return animationComponent->animation.IsValid()
        ? animationComponent->animation.Get()
        : meshComponent->skeleton->GetAnimation(animationComponent->playbackState.animationIndex).Get();
}

static AnimationSystem* GetAnimationSystem(Entity& entity)
{
    Scene* scene = entity.GetScene();
    World* world = scene ? scene->GetWorld() : nullptr;

    return world ? world->GetSystem<AnimationSystem>() : nullptr;
}

AnimationEditorExtension::AnimationEditorExtension() = default;

AnimationEditorExtension::~AnimationEditorExtension() = default;

Handle<EditorAnimationEditState> AnimationEditorExtension::GetEditState()
{
    if (!m_editState.IsValid() && GetEditorSubsystem() != nullptr)
    {
        m_editState = MakeHandle<EditorAnimationEditState>();
        InitObject(m_editState);

        m_editState->Initialize(GetEditorSubsystem());
    }

    return m_editState;
}

bool AnimationEditorExtension::IsEditingAnimation() const
{
    return m_editState.IsValid() && m_editState->IsActive();
}

bool AnimationEditorExtension::CanEditAnimation(const Handle<Node>& node) const
{
    const EditorSubsystem* editorSubsystem = GetEditorSubsystem();
    const Entity* entity = TryCast<Entity>(node.Get());

    if (!editorSubsystem || !entity || !entity->GetEntityManager() || editorSubsystem->IsSimulating() || editorSubsystem->IsInIsolatedEditMode())
    {
        return false;
    }

    const MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();

    return meshComponent && meshComponent->skeleton.IsValid();
}

bool AnimationEditorExtension::IsShowBonesEnabled() const
{
    return s_cvShowBones.Get();
}

void AnimationEditorExtension::SetShowBonesEnabled(bool enabled)
{
    s_cvShowBones.Set(enabled);
}

void AnimationEditorExtension::StartAnimationPreview(const Handle<Entity>& entity)
{
    StopAnimationPreview();

    if (!entity || !GetEditorSubsystem() || GetEditorSubsystem()->IsSimulating() || !ResolvePreviewAnimation(*entity))
    {
        return;
    }

    AnimationSystem* animationSystem = GetAnimationSystem(*entity);

    if (!animationSystem)
    {
        return;
    }

    m_animationPreviewEntity = MakeWeakRef(entity);
    m_animationPreviewTime = entity->GetComponent<AnimationComponent>().playbackState.currentTime;
    m_isAnimationPreviewPlaying = false;

    animationSystem->SetPreviewTime(entity.Get(), m_animationPreviewTime);
}

void AnimationEditorExtension::StopAnimationPreview()
{
    if (Handle<Entity> entity = m_animationPreviewEntity.Lock())
    {
        if (AnimationSystem* animationSystem = GetAnimationSystem(*entity))
        {
            animationSystem->StopPreview(entity.Get());
        }
    }

    m_animationPreviewEntity.Reset();
    m_isAnimationPreviewPlaying = false;
}

bool AnimationEditorExtension::IsAnimationPreviewActive(const Handle<Entity>& entity) const
{
    return entity.IsValid() && m_animationPreviewEntity.GetUnsafe() == entity.Get();
}

bool AnimationEditorExtension::IsAnimationPreviewPlaying() const
{
    return m_isAnimationPreviewPlaying;
}

void AnimationEditorExtension::SetAnimationPreviewPlaying(bool playing)
{
    m_isAnimationPreviewPlaying = playing && m_animationPreviewEntity.IsValid();
}

float AnimationEditorExtension::GetAnimationPreviewTime() const
{
    return m_animationPreviewTime;
}

void AnimationEditorExtension::SetAnimationPreviewTime(float time)
{
    m_animationPreviewTime = MathUtil::Clamp(time, 0.0f, GetAnimationPreviewLength());
}

float AnimationEditorExtension::GetAnimationPreviewLength() const
{
    Handle<Entity> entity = m_animationPreviewEntity.Lock();

    return GetAnimationClipLength(entity);
}

float AnimationEditorExtension::GetAnimationClipLength(const Handle<Entity>& entity) const
{
    const Animation* animation = entity ? ResolvePreviewAnimation(*entity) : nullptr;

    return animation ? animation->GetLength() : 0.0f;
}

void AnimationEditorExtension::OnAttached()
{
    GetEditState();
}

void AnimationEditorExtension::OnDetached()
{
    StopAnimationPreview();

    if (m_editState.IsValid())
    {
        m_editState->Shutdown();
        m_editState.Reset();
    }
}

void AnimationEditorExtension::Update(float delta)
{
    UpdateAnimationPreview(delta);

    if (m_editState.IsValid())
    {
        m_editState->Update(delta);
    }
}

void AnimationEditorExtension::DebugDraw(DebugDrawCommandList& debugDrawCommandList)
{
    DebugDrawSelectedBones(debugDrawCommandList);

    if (IsEditingAnimation())
    {
        m_editState->DebugDraw(debugDrawCommandList, GetEditorSubsystem()->GetSelectedNodes().ToSpan());
    }
}

bool AnimationEditorExtension::TestPickRay(const Ray& ray, RayTestResults& outResults)
{
    if (!IsEditingAnimation())
    {
        return false;
    }

    float jointDistance = 0.0f;

    EditorBoneNode* joint = m_editState->PickJoint(ray, jointDistance);

    if (!joint)
    {
        return false;
    }

    RayHit jointHit {};
    jointHit.node = joint;
    jointHit.hitpoint = ray.position + ray.direction * jointDistance;
    // joints sit inside the mesh, so they rank ahead of every surface hit
    jointHit.distance = -1.0f / MathUtil::Max(jointDistance, 0.001f);

    return outResults.AddHit(jointHit);
}

bool AnimationEditorExtension::SelectsPickedNode(const Node* node) const
{
    return node->IsA<EditorBoneNode>() || node->IsA<EditorSocketNode>();
}

void AnimationEditorExtension::UpdateAnimationPreview(float delta)
{
    Handle<Entity> entity = m_animationPreviewEntity.Lock();

    if (!entity)
    {
        m_animationPreviewEntity.Reset();

        return;
    }

    const Array<Handle<Node>> selectedNodes = GetEditorSubsystem()->GetSelectedNodes();

    bool isSelected = false;

    for (const Node* node = entity.Get(); node != nullptr && !isSelected; node = node->GetParent())
    {
        isSelected = AnyOf(selectedNodes, [node](const Handle<Node>& selectedNode)
            {
                return selectedNode.Get() == node;
            });
    }

    AnimationSystem* animationSystem = GetAnimationSystem(*entity);

    if (!isSelected || GetEditorSubsystem()->IsSimulating() || !animationSystem)
    {
        StopAnimationPreview();

        return;
    }

    const float length = GetAnimationPreviewLength();

    if (m_isAnimationPreviewPlaying && length > 0.0f)
    {
        m_animationPreviewTime = std::fmod(m_animationPreviewTime + delta, length);
    }

    animationSystem->SetPreviewTime(entity.Get(), MathUtil::Min(m_animationPreviewTime, length));
}

void AnimationEditorExtension::DebugDrawSelectedBones(DebugDrawCommandList& debugDrawCommandList)
{
    if (!s_cvShowBones.Get())
    {
        return;
    }

    static const RenderableAttributeSet s_jointAttributes = BoneOverlayAttributes(Topology::Triangles);
    static const RenderableAttributeSet s_boneAttributes = BoneOverlayAttributes(Topology::Lines);

    static const Color s_boneColor = Color(0.35f, 0.85f, 1.0f, 1.0f);

    Array<const Skeleton*> drawnSkeletons;

    const auto drawSkeleton = [&](const Entity& entity, const Skeleton& skeleton)
    {
        if (drawnSkeletons.Contains(&skeleton) || !skeleton.GetRootBone())
        {
            return;
        }

        drawnSkeletons.PushBack(&skeleton);

        const SkeletonBindData& bindData = skeleton.GetBindData();
        const Span<const Mat4f> poseMatrices = skeleton.GetPoseModelMatrices();

        Array<const Node*> slotNodes;
        slotNodes.Reserve(bindData.NumSlots());
        slotNodes.PushBack(skeleton.GetRootBone().Get());

        for (Node* descendant : skeleton.GetRootBone()->GetDescendants())
        {
            slotNodes.PushBack(descendant);
        }

        if (slotNodes.Size() != bindData.NumSlots())
        {
            return;
        }

        const Mat4f entityMatrix = entity.GetWorldMatrix();

        const auto jointPosition = [&](uint32 slot)
        {
            const Mat4f& modelMatrix = poseMatrices.Size() == bindData.NumSlots() ? poseMatrices[slot] : slotNodes[slot]->GetWorldMatrix();

            return (entityMatrix * modelMatrix).ExtractTranslation();
        };

        float jointRadius = 0.01f;

        for (uint32 slot = 1; slot < bindData.NumSlots(); ++slot)
        {
            if (!slotNodes[slot]->IsA<Bone>())
            {
                continue;
            }

            const Vec3f joint = jointPosition(slot);
            const uint32 parentSlot = bindData.parentSlots[slot];

            // the loaders add a synthetic root above the real ones; it has no joint of its own
            if (parentSlot != SkeletonBindData::InvalidSlot && parentSlot != 0 && slotNodes[parentSlot]->IsA<Bone>())
            {
                const Vec3f parentJoint = jointPosition(parentSlot);
                jointRadius = MathUtil::Max((joint - parentJoint).Length() * 0.08f, 0.002f);

                debugDrawCommandList.line(parentJoint, joint, s_boneColor, s_boneAttributes);
            }

            debugDrawCommandList.sphere(joint, jointRadius, s_boneColor, s_jointAttributes);
        }
    };

    for (const Handle<Node>& selectedNode : GetEditorSubsystem()->GetSelectedNodes())
    {
        if (!selectedNode.IsValid())
        {
            continue;
        }

        const auto visit = [&](Node* node)
        {
            Entity* entity = TryCast<Entity>(node);

            if (!entity || !entity->GetEntityManager())
            {
                return;
            }

            if (const MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>(); meshComponent && meshComponent->GetRenderSkeleton().IsValid())
            {
                drawSkeleton(*entity, *meshComponent->GetRenderSkeleton());
            }
        };

        visit(selectedNode.Get());

        for (Node* descendant : selectedNode->GetDescendants())
        {
            visit(descendant);
        }
    }
}

} // namespace Hyperion
