/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Animation/Bone.hpp>
#include <Scene/Animation/Animation.hpp>
#include <Scene/Animation/AnimationPose.hpp>

#include <Rendering/RenderProxy.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Framework/EngineDriver.hpp>

#include <Skeleton.generated.inl>

namespace Hyperion {

static const Name s_nameSkeletonDefault = NAME("<unnamed skeleton>");

Skeleton::Skeleton()
    : m_renderProxyVersion(0)
{
}

Skeleton::Skeleton(const Handle<Bone>& rootBone)
    : m_rootBone(rootBone),
      m_renderProxyVersion(0)
{
    if (m_rootBone)
    {
        m_rootBone->SetSkeleton(this);
    }
}

Skeleton::~Skeleton()
{
    if (m_rootBone)
    {
        m_rootBone->SetSkeleton(nullptr);
    }

    SetReady(false);
}

Bone* Skeleton::FindBone(StringHash name) const
{
    if (!m_rootBone)
    {
        return nullptr;
    }

    if (m_rootBone->GetBoneName() == name)
    {
        return Cast<Bone>(m_rootBone.Get());
    }

    // @TODO Profile this

    for (Node* node : m_rootBone->GetDescendants())
    {
        if (!node)
        {
            continue;
        }

        Bone* bone = TryCast<Bone>(node);

        if (!bone)
        {
            continue;
        }

        if (bone->GetBoneName() == name)
        {
            return bone;
        }
    }

    return nullptr;
}

uint32 Skeleton::FindBoneIndex(StringHash name) const
{
    HYP_SCOPE;

    if (!m_rootBone)
    {
        return uint32(-1);
    }

    uint32 index = 0;

    if (m_rootBone->GetBoneName() == name)
    {
        return index;
    }

    for (Node* node : m_rootBone->GetDescendants())
    {
        ++index;

        if (!node)
        {
            continue;
        }

        Bone* bone = TryCast<Bone>(node);

        if (!bone)
        {
            continue;
        }

        if (bone->GetBoneName() == name)
        {
            return index;
        }
    }

    return uint32(-1);
}

const Handle<Bone>& Skeleton::GetRootBone() const
{
    return m_rootBone;
}

void Skeleton::SetRootBone(const Handle<Bone>& bone)
{
    HYP_SCOPE;

    if (m_rootBone)
    {
        m_rootBone->SetSkeleton(nullptr);

        m_rootBone.Reset();
    }

    if (!bone)
    {
        return;
    }

    m_rootBone = bone;
    m_rootBone->SetSkeleton(this);

    // The inverse bind matrices are Transient, so a skeleton loaded from disk needs them rebuilt here
    m_rootBone->SetToBindingPose();
    m_rootBone->StoreBindingPose();

    m_bindDataBuilt = false;
    ++m_revision;
}

const Handle<Animation>& Skeleton::FindAnimation(Name name) const
{
    for (const Handle<Animation>& animation : m_animations)
    {
        if (animation.IsValid() && animation->GetName() == name)
        {
            return animation;
        }
    }

    return Handle<Animation>::empty;
}

Array<Name> Skeleton::GetAnimationNames() const
{
    Array<Name> names;

    for (const Handle<Animation>& animation : m_animations)
    {
        if (animation.IsValid())
        {
            names.PushBack(animation->GetName());
        }
    }

    return names;
}

Array<Name> Skeleton::GetBoneNames() const
{
    Array<Name> names;

    for (const Name boneName : GetBindData().boneNames)
    {
        // the loaders' synthetic root bone has no name of its own
        if (boneName.IsValid() && *boneName.LookupString() != '\0')
        {
            names.PushBack(boneName);
        }
    }

    return names;
}

const SkeletonSocket* Skeleton::FindSocket(Name name) const
{
    for (const SkeletonSocket& socket : m_sockets)
    {
        if (socket.name == name)
        {
            return &socket;
        }
    }

    return nullptr;
}

Array<Name> Skeleton::GetSocketNames() const
{
    Array<Name> names;

    for (const SkeletonSocket& socket : m_sockets)
    {
        names.PushBack(socket.name);
    }

    return names;
}

uint32 Skeleton::CountBoundTracks(const Handle<Animation>& animation) const
{
    if (!animation)
    {
        return 0;
    }

    return PoseOps::CountBoundSlots(PoseOps::BindClip(*animation, *this));
}

bool SkeletonBindData::IsSlotWithin(uint32 slot, Name ancestorBoneName) const
{
    for (uint32 current = slot; current != InvalidSlot; current = parentSlots[current])
    {
        if (!boneNames[current].IsValid())
        {
            return false;
        }

        if (boneNames[current] == ancestorBoneName)
        {
            return true;
        }
    }

    return false;
}

const HumanoidMapping& Skeleton::GetHumanoidMapping() const
{
    if (!m_isHumanoidResolved)
    {
        m_isHumanoidResolved = true;

        if (m_humanoid.bones.Empty())
        {
            m_humanoid = HumanoidMapping::AutoMap(GetBindData());
        }
    }

    return m_humanoid;
}

void Skeleton::SetHumanoidMapping(const HumanoidMapping& humanoid)
{
    m_humanoid = humanoid;
    m_isHumanoidResolved = true;

    // clips bound to this skeleton choose between name binding and retargeting by the mapping
    ++m_revision;
}

Name Skeleton::GetHumanoidBoneName(uint32 bone) const
{
    return bone < NumHumanoidBones ? GetHumanoidMapping().GetBoneName(HumanoidBone(bone)) : Name::Invalid();
}

void Skeleton::SetHumanoidBoneName(uint32 bone, Name boneName)
{
    if (bone >= NumHumanoidBones)
    {
        return;
    }

    HumanoidMapping humanoid = GetHumanoidMapping();
    humanoid.SetBoneName(HumanoidBone(bone), boneName);

    SetHumanoidMapping(humanoid);
}

void Skeleton::AutoMapHumanoid()
{
    const HumanoidMapping autoMapped = HumanoidMapping::AutoMap(GetBindData());

    HumanoidMapping humanoid = m_humanoid;

    for (const HumanoidBoneBinding& binding : autoMapped.bones)
    {
        if (!humanoid.GetBoneName(binding.bone).IsValid())
        {
            humanoid.SetBoneName(binding.bone, binding.boneName);
        }
    }

    SetHumanoidMapping(humanoid);
}

Array<Handle<Skeleton>> Skeleton::CollectHumanoidSkeletons()
{
    Array<Handle<Skeleton>> skeletons;

    for (const Handle<AssetRegistry>& registry : { GetCurrentAssetRegistry(), GetEngineAssetRegistry() })
    {
        if (!registry.IsValid())
        {
            continue;
        }

        Array<AssetDesc> skeletonDescs;
        registry->GetBucketAssetDescs(AssetBuckets::Skeletons.GetIndex(), skeletonDescs);

        for (const AssetDesc& skeletonDesc : skeletonDescs)
        {
            Handle<Skeleton> skeleton = registry->GetAsset<Skeleton>(AssetBuckets::Skeletons, skeletonDesc.name);

            if (skeleton.IsValid() && skeleton->IsHumanoid() && !skeletons.Contains(skeleton))
            {
                skeletons.PushBack(std::move(skeleton));
            }
        }
    }

    return skeletons;
}

Array<Name> Skeleton::GetCompatibleClipNames() const
{
    Array<Name> clipNames = GetAnimationNames();

    if (!IsHumanoid())
    {
        return clipNames;
    }

    for (const Handle<Skeleton>& skeleton : CollectHumanoidSkeletons())
    {
        if (skeleton.Get() == this || skeleton.Get() == m_instanceSource)
        {
            continue;
        }

        for (const Handle<Animation>& animation : skeleton->GetAnimations())
        {
            if (animation.IsValid())
            {
                clipNames.PushBack(NAME_FMT("{} ({})", animation->GetName(), skeleton->GetName()));
            }
        }
    }

    return clipNames;
}

Handle<Animation> Skeleton::FindCompatibleClip(Name name) const
{
    if (const Handle<Animation>& animation = FindAnimation(name); animation.IsValid())
    {
        return animation;
    }

    if (!IsHumanoid())
    {
        return Handle<Animation>::empty;
    }

    for (const Handle<Skeleton>& skeleton : CollectHumanoidSkeletons())
    {
        for (const Handle<Animation>& animation : skeleton->GetAnimations())
        {
            if (animation.IsValid() && NAME_FMT("{} ({})", animation->GetName(), skeleton->GetName()) == name)
            {
                return animation;
            }
        }
    }

    return Handle<Animation>::empty;
}

void Skeleton::OnLoaded()
{
    for (const Handle<Animation>& animation : m_animations)
    {
        if (animation.IsValid() && !animation->GetSourceSkeleton().IsValid())
        {
            animation->StampSourceSkeleton(*this);
        }
    }
}

const SkeletonBindData& Skeleton::GetBindData() const
{
    if (m_bindDataBuilt)
    {
        return m_bindData;
    }

    m_bindData = SkeletonBindData {};
    m_bindDataBuilt = true;

    if (!m_rootBone)
    {
        return m_bindData;
    }

    Map<const Node*, uint32> slotsByNode;

    const auto addSlot = [&](const Node& node)
    {
        const uint32 slot = m_bindData.NumSlots();
        slotsByNode[&node] = slot;

        const auto parentIt = slotsByNode.Find(node.GetParent());
        const Bone* bone = TryCast<Bone>(&node);

        m_bindData.boneNames.PushBack(bone ? bone->GetBoneName() : Name::Invalid());
        m_bindData.parentSlots.PushBack(slot > 0 && parentIt != slotsByNode.End() ? parentIt->second : SkeletonBindData::InvalidSlot);
        m_bindData.restLocals.PushBack(bone ? bone->GetBindingTransform() : node.GetLocalTransform());
        m_bindData.inverseBindMatrices.PushBack(bone ? bone->m_inverseBindMatrix : Mat4f::Identity());

        if (bone && bone->GetBoneName().IsValid() && !m_bindData.slotsByBoneName.Contains(bone->GetBoneName()))
        {
            m_bindData.slotsByBoneName[bone->GetBoneName()] = slot;
        }
    };

    addSlot(*m_rootBone);

    for (Node* descendant : m_rootBone->GetDescendants())
    {
        addSlot(*descendant);
    }

    return m_bindData;
}

static Handle<Node> CloneSkeletonNode(const Node& source)
{
    Handle<Node> clone;

    if (const Bone* sourceBone = TryCast<Bone>(&source))
    {
        Handle<Bone> bone = MakeHandle<Bone>(sourceBone->GetBoneName());
        bone->SetBindingTransform(sourceBone->GetBindingTransform());

        clone = bone;
    }
    else
    {
        clone = MakeHandle<Node>();
    }

    clone->SetName(source.GetName());
    clone->SetLocalTransform(source.GetLocalTransform());

    for (const Handle<Node>& child : source.GetChildren())
    {
        if (child.IsValid())
        {
            clone->AddChild(CloneSkeletonNode(*child));
        }
    }

    return clone;
}

Handle<Skeleton> Skeleton::CreateInstance() const
{
    HYP_SCOPE;

    Handle<Skeleton> instance = MakeHandle<Skeleton>();
    instance->SetName(GetName());
    instance->m_animations = m_animations;
    instance->m_sockets = m_sockets;
    instance->m_humanoid = GetHumanoidMapping();
    instance->m_isHumanoidResolved = true;
    instance->m_instanceSource = this;

    if (m_rootBone)
    {
        instance->SetRootBone(Cast<Bone>(CloneSkeletonNode(*m_rootBone)));
    }

    InitObject(instance);

    return instance;
}

void Skeleton::SetPoseMatrices(Span<const Mat4f> modelMatrices, Span<const Mat4f> skinningMatrices)
{
    m_poseModelMatrices = Array<Mat4f>(modelMatrices);
    m_poseSkinningMatrices = Array<Mat4f>(skinningMatrices);

    SetNeedsRenderProxyUpdate();
}

void Skeleton::UpdateRenderProxy(RenderProxySkeleton* proxy)
{
    proxy->skeleton = this;

    if (m_poseSkinningMatrices.Any())
    {
        if (m_poseSkinningMatrices.Size() > MaxBonesPerSkeleton)
        {
            HYP_LOG_ONCE(Animation, Warning, "Skeleton has more bones than supported by the shader ({}). Some bones will be ignored in skinning.", MaxBonesPerSkeleton);
        }

        const size_t numBones = MathUtil::Min(m_poseSkinningMatrices.Size(), size_t(MaxBonesPerSkeleton));

        for (size_t boneIndex = 0; boneIndex < numBones; ++boneIndex)
        {
            proxy->bufferData.bones[boneIndex] = m_poseSkinningMatrices[boneIndex];
        }

        return;
    }

    if (m_rootBone != nullptr)
    {
        SkeletonShaderData& bufferData = proxy->bufferData;
        bufferData.bones[0] = m_rootBone->GetBoneMatrix();

        uint32 descendantIndex = 1;

        for (Node* descendant : m_rootBone->GetDescendants())
        {
            if (descendantIndex >= MaxBonesPerSkeleton)
            {
                HYP_LOG_ONCE(Animation, Warning, "Skeleton has more bones than supported by the shader ({}). Some bones will be ignored in skinning.", MaxBonesPerSkeleton);
                break;
            }

            if (!descendant)
            {
                ++descendantIndex;
                continue;
            }

            Bone* bone = TryCast<Bone>(descendant);

            if (!bone)
            {
                ++descendantIndex;
                continue;
            }

            bufferData.bones[descendantIndex] = bone->GetBoneMatrix();

            ++descendantIndex;
        }
    }
}

} // namespace Hyperion
