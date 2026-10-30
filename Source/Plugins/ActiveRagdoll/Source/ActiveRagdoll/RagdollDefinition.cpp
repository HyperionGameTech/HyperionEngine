/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <ActiveRagdoll/RagdollDefinition.hpp>

#include <Scene/Animation/Skeleton.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <RagdollDefinition.generated.inl>

namespace Hyperion {

RagdollDefinition::RagdollDefinition() = default;

RagdollDefinition::RagdollDefinition(Name name)
{
    SetName(name);
}

RagdollDefinition::~RagdollDefinition() = default;

static uint32 FindMappedSlot(const HumanoidMapping& mapping, const SkeletonBindData& bindData, Span<const HumanoidBone> bones, Span<const RagdollResolvedBody> taken)
{
    for (HumanoidBone bone : bones)
    {
        const Name boneName = mapping.GetBoneName(bone);

        if (!boneName.IsValid())
        {
            continue;
        }

        const uint32 slot = bindData.FindSlot(boneName);

        if (slot == SkeletonBindData::InvalidSlot)
        {
            continue;
        }

        bool isTaken = false;

        for (const RagdollResolvedBody& body : taken)
        {
            if (body.slot == slot)
            {
                isTaken = true;
                break;
            }
        }

        if (!isTaken)
        {
            return slot;
        }
    }

    return SkeletonBindData::InvalidSlot;
}

bool RagdollDefinition::Resolve(const Skeleton& skeleton, Array<RagdollResolvedBody>& outBodies) const
{
    outBodies.Clear();

    if (!HumanoidSkeletonData::IsHumanoid(skeleton))
    {
        HYP_LOG(Physics, Warning, "Ragdoll '{}': skeleton '{}' has no humanoid mapping", GetName(), skeleton.GetName());

        return false;
    }

    const HumanoidMapping& mapping = HumanoidSkeletonData::GetSkeletonMapping(skeleton);
    const SkeletonBindData& bindData = skeleton.GetBindData();

    float massSum = 0.0f;

    for (const RagdollBodyDefinition& definition : m_bodies)
    {
        const uint32 slot = FindMappedSlot(mapping, bindData, definition.bones.ToSpan(), outBodies.ToSpan());

        if (slot == SkeletonBindData::InvalidSlot)
        {
            continue;
        }

        RagdollResolvedBody body;
        body.definition = &definition;
        body.slot = slot;
        body.endSlot = FindMappedSlot(mapping, bindData, definition.ends.ToSpan(), Span<const RagdollResolvedBody>());
        body.endAtTip = definition.endAtTip;
        body.massFraction = definition.massFraction;

        if (body.endSlot == SkeletonBindData::InvalidSlot || body.endSlot == slot)
        {
            body.endSlot = slot;
            body.endAtTip = true;
        }

        massSum += body.massFraction;

        outBodies.PushBack(body);
    }

    if (outBodies.Size() < 2 || massSum <= 0.0f)
    {
        HYP_LOG(Physics, Warning, "Ragdoll '{}': skeleton '{}' only maps {} of its bodies", GetName(), skeleton.GetName(), outBodies.Size());

        outBodies.Clear();

        return false;
    }

    // bones the skeleton doesn't map leave their mass to the others
    for (RagdollResolvedBody& body : outBodies)
    {
        body.massFraction /= massSum;
    }

    return true;
}

Handle<RagdollDefinition> RagdollDefinition::GetDefaultHumanoid()
{
    const Handle<AssetRegistry> engineRegistry = GetEngineAssetRegistry();

    if (!engineRegistry.IsValid())
    {
        return Handle<RagdollDefinition>::empty;
    }

    return engineRegistry->GetAsset<RagdollDefinition>(AssetBuckets::PhysicsShapes, NAME("HumanoidRagdoll"));
}

} // namespace Hyperion
