/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Animation/Bone.hpp>
#include <Scene/Animation/Animation.hpp>

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

static Skeleton::BoundTrackCounter g_boundTrackCounter = nullptr;

void Skeleton::SetBoundTrackCounter(BoundTrackCounter counter)
{
    g_boundTrackCounter = counter;
}

uint32 Skeleton::CountBoundTracks(const Handle<Animation>& animation) const
{
    if (!animation)
    {
        return 0;
    }

    if (g_boundTrackCounter != nullptr)
    {
        return g_boundTrackCounter(*animation, *this);
    }

    const SkeletonBindData& bindData = GetBindData();

    uint32 count = 0;

    for (const Handle<AnimationTrack>& track : animation->GetTracks())
    {
        if (track.IsValid() && bindData.FindSlot(track->GetBoneName()) != SkeletonBindData::InvalidSlot)
        {
            ++count;
        }
    }

    return count;
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

void Skeleton::SetLegacyHumanoid(const BoxedValue& legacyHumanoid)
{
    // a skeleton written with no legacy mapping has a null in its place
    m_legacyHumanoid = legacyHumanoid.IsValid() && !legacyHumanoid.Is<Handle<ObjectBase>>() ? legacyHumanoid : BoxedValue();
}

BoxedValue Skeleton::TakeLegacyHumanoid() const
{
    BoxedValue legacyHumanoid = std::move(m_legacyHumanoid);
    m_legacyHumanoid = BoxedValue();

    return legacyHumanoid;
}

void Skeleton::OnExtensionChanged(AssetExtension& extension)
{
    ++m_revision;
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
    instance->ShareExtensionsFrom(*this);
    instance->m_instanceSource = this;
    instance->m_instanceSourceRevision = m_revision;

    if (m_rootBone)
    {
        instance->SetRootBone(Cast<Bone>(CloneSkeletonNode(*m_rootBone)));
    }

    InitObject(instance);

    return instance;
}

void ComputeSkeletonPoseMatrices(const SkeletonBindData& bindData, Span<const Transform> pose, Span<Mat4f> outModelMatrices, Span<Mat4f> outSkinningMatrices)
{
    HYP_SCOPE;

    const uint32 numSlots = bindData.NumSlots();

    for (uint32 slot = 0; slot < numSlots; ++slot)
    {
        const uint32 parentSlot = bindData.parentSlots[slot];

        outModelMatrices[slot] = parentSlot != SkeletonBindData::InvalidSlot
            ? outModelMatrices[parentSlot] * pose[slot].GetMatrix()
            : pose[slot].GetMatrix();

        outSkinningMatrices[slot] = outModelMatrices[slot] * bindData.inverseBindMatrices[slot];
    }
}

bool Skeleton::IsInstanceCurrent() const
{
    return m_instanceSource == nullptr || m_instanceSource->GetRevision() == m_instanceSourceRevision;
}

void Skeleton::SetPose(Span<const Transform> localPose)
{
    const SkeletonBindData& bindData = GetBindData();

    if (localPose.Size() != bindData.NumSlots())
    {
        if (localPose.Size() != 0)
        {
            HYP_LOG(Animation, Warning, "Skeleton '{}' was given a pose of {} bones, but has {}; showing the bind pose", GetName(), localPose.Size(), bindData.NumSlots());
        }

        m_pose.Clear();
        m_poseModelMatrices.Clear();
        m_poseSkinningMatrices.Clear();
    }
    else
    {
        m_pose = Array<Transform>(localPose);
        m_poseModelMatrices.Resize(bindData.NumSlots());
        m_poseSkinningMatrices.Resize(bindData.NumSlots());

        ComputeSkeletonPoseMatrices(bindData, localPose, m_poseModelMatrices.ToSpan(), m_poseSkinningMatrices.ToSpan());
    }

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
