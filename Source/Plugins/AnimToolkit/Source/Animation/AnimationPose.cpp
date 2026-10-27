/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Animation/AnimationPose.hpp>
#include <Animation/BoneMask.hpp>
#include <Scene/Animation/Animation.hpp>
#include <Scene/Animation/Skeleton.hpp>
#include <Animation/HumanoidMapping.hpp>
#include <Scene/Animation/Keyframe.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Quat4f.hpp>

#include <cmath>

namespace Hyperion {

namespace {

struct RetargetScratch
{
    Array<Transform, AnimAllocator> sourcePose;
    Array<RetargetModelTransform, AnimAllocator> sourceModel;
    Array<RetargetModelTransform, AnimAllocator> targetModel;
};

thread_local RetargetScratch* t_retargetScratch;

const RetargetModelTransform g_emptyRMT {};

Transform BlendTransforms(
    const Transform& from,
    const Transform& to,
    float blend)
{
    return Keyframe(0.0f, from).Blend(Keyframe(0.0f, to), blend).transform;
}

// Bone::ClearPose() resets a bone and every bone beneath it, stopping at nodes that aren't bones
void ClearSubtree(
    const SkeletonBindData& bindData,
    uint32 slot,
    Span<Transform> pose)
{
    pose[slot] = bindData.restLocals[slot];

    for (uint32 candidate = slot + 1; candidate < bindData.NumSlots(); ++candidate)
    {
        uint32 ancestor = bindData.parentSlots[candidate];
        bool reachesBoneChain = bindData.boneNames[candidate].IsValid();

        while (ancestor != SkeletonBindData::InvalidSlot && ancestor != slot)
        {
            reachesBoneChain = reachesBoneChain && bindData.boneNames[ancestor].IsValid();
            ancestor = bindData.parentSlots[ancestor];
        }

        if (ancestor != slot)
        {
            break;
        }

        if (reachesBoneChain)
        {
            pose[candidate] = bindData.restLocals[candidate];
        }
    }
}

Array<uint32> TracksBySlot(const ClipBinding& binding, uint32 numSlots)
{
    Array<uint32> tracksBySlot;
    tracksBySlot.Resize(numSlots);

    for (uint32& trackIndex : tracksBySlot)
    {
        trackIndex = SkeletonBindData::InvalidSlot;
    }

    for (uint32 trackIndex = 0; trackIndex < uint32(binding.trackSlots.Size()); ++trackIndex)
    {
        const uint32 slot = binding.trackSlots[trackIndex];

        if (slot != SkeletonBindData::InvalidSlot && tracksBySlot[slot] == SkeletonBindData::InvalidSlot)
        {
            tracksBySlot[slot] = trackIndex;
        }
    }

    return tracksBySlot;
}

RetargetModelTransform ComposeModelTransform(
    const RetargetModelTransform& parent,
    const Transform& local)
{
    RetargetModelTransform model;
    model.rotation = parent.rotation * local.GetRotation();
    model.position = parent.position + parent.rotation.RotateVector(local.GetTranslation() * parent.scale);
    model.scale = parent.scale * local.GetScale().x;

    return model;
}

void ComputeModelTransforms(
    const SkeletonBindData& bindData,
    Span<const Transform> pose,
    Array<RetargetModelTransform, AnimAllocator>& outModel)
{
    outModel.Resize(bindData.NumSlots());

    for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
    {
        const uint32 parentSlot = bindData.parentSlots[slot];

        outModel[slot] = ComposeModelTransform(
            parentSlot != SkeletonBindData::InvalidSlot ? outModel[parentSlot] : RetargetModelTransform {},
            pose[slot]);
    }
}

Vec3f GetHumanoidBindPosition(
    const SkeletonBindData& bindData,
    const HumanoidMapping& mapping,
    const Array<RetargetModelTransform, AnimAllocator>& bindModel,
    HumanoidBone bone)
{
    const uint32 slot = bindData.FindSlot(mapping.GetBoneName(bone));
    Assert(slot != SkeletonBindData::InvalidSlot, "Humanoid bone {} isn't a bone of the skeleton", uint32(bone));

    return bindModel[slot].position;
}

struct CharacterAxes
{
    Vec3f up;
    Vec3f right;
    Vec3f forward;

    bool mirrored = false;
};

Vec3f GetHumanoidFacing(
    const SkeletonBindData& bindData,
    const HumanoidMapping& mapping,
    const Array<RetargetModelTransform, AnimAllocator>& model,
    const Vec3f& up);

CharacterAxes GetCharacterAxes(
    const SkeletonBindData& bindData,
    const HumanoidMapping& mapping,
    const Array<RetargetModelTransform, AnimAllocator>& bindModel)
{
    const Vec3f hips = GetHumanoidBindPosition(bindData, mapping, bindModel, HumanoidBone::Hips);
    const Vec3f head = GetHumanoidBindPosition(bindData, mapping, bindModel, HumanoidBone::Head);

    CharacterAxes axes;
    axes.up = (head - hips).Normalized();

    const Vec3f right = GetHumanoidBindPosition(bindData, mapping, bindModel, HumanoidBone::RightUpperArm)
        - GetHumanoidBindPosition(bindData, mapping, bindModel, HumanoidBone::LeftUpperArm);

    axes.right = (right - axes.up * right.Dot(axes.up)).Normalized();
    axes.forward = GetHumanoidFacing(bindData, mapping, bindModel, axes.up);
    axes.mirrored = axes.right.Cross(axes.up).Dot(axes.forward) < 0.0f;

    return axes;
}

Quat4f MirrorRotation(const Quat4f& rotation, const Vec3f& normal)
{
    const Vec3f axis(rotation.x, rotation.y, rotation.z);
    const Vec3f mirroredAxis = axis - normal * (2.0f * axis.Dot(normal));

    return Quat4f(-mirroredAxis.x, -mirroredAxis.y, -mirroredAxis.z, rotation.w);
}

Vec3f MirrorPoint(const Vec3f& point, const Vec3f& normal)
{
    return point - normal * (2.0f * point.Dot(normal));
}

Vec3f GetHumanoidFacing(
    const SkeletonBindData& bindData,
    const HumanoidMapping& mapping,
    const Array<RetargetModelTransform, AnimAllocator>& model,
    const Vec3f& up)
{
    const auto slotOf = [&](HumanoidBone bone)
    {
        return bindData.FindSlot(mapping.GetBoneName(bone));
    };

    const auto flatten = [&up](const Vec3f& direction)
    {
        return direction - up * direction.Dot(up);
    };

    const uint32 leftFoot = slotOf(HumanoidBone::LeftFoot);
    const uint32 rightFoot = slotOf(HumanoidBone::RightFoot);
    const uint32 leftHip = slotOf(HumanoidBone::LeftUpperLeg);
    const uint32 rightHip = slotOf(HumanoidBone::RightUpperLeg);

    Assert(leftFoot != SkeletonBindData::InvalidSlot && rightFoot != SkeletonBindData::InvalidSlot
        && leftHip != SkeletonBindData::InvalidSlot && rightHip != SkeletonBindData::InvalidSlot,
        "Humanoid legs aren't bones of the skeleton");

    // toes are optional
    const uint32 leftToes = slotOf(HumanoidBone::LeftToes);
    const uint32 rightToes = slotOf(HumanoidBone::RightToes);

    if (leftToes != SkeletonBindData::InvalidSlot && rightToes != SkeletonBindData::InvalidSlot)
    {
        const Vec3f toes = flatten((model[leftToes].position - model[leftFoot].position) + (model[rightToes].position - model[rightFoot].position));

        if (toes.LengthSquared() > 1e-10f)
        {
            return toes.Normalized();
        }
    }

    const Vec3f facing = flatten(model[rightHip].position - model[leftHip].position).Cross(up);

    return facing.LengthSquared() > 1e-10f ? facing.Normalized() : Vec3f::UnitZ();
}

// the shortest rotation taking unit vector from onto unit vector to
Quat4f RotationBetween(const Vec3f& from, const Vec3f& to)
{
    const float cosAngle = MathUtil::Clamp(from.Dot(to), -1.0f, 1.0f);

    if (cosAngle > 1.0f - 1e-6f)
    {
        return Quat4f::Identity();
    }

    Vec3f axis = from.Cross(to);
    if (axis.LengthSquared() < 1e-12f)
    {
        axis = MathUtil::Abs(from.x) < 0.9f ? Vec3f::UnitX().Cross(from) : Vec3f::UnitY().Cross(from);
    }

    const Quat4f rotation(axis.Normalized(), MathUtil::Arccos(cosAngle));
    if ((rotation.RotateVector(from) - to).LengthSquared() > (rotation.Inverse().RotateVector(from) - to).LengthSquared())
    {
        return rotation.Inverse();
    }

    return rotation;
}

Quat4f GetSourceToTarget(const CharacterAxes& source, const CharacterAxes& target)
{
    const Quat4f alignUp = RotationBetween(source.up, target.up);
    const Quat4f alignForward = RotationBetween(alignUp.RotateVector(source.forward), target.forward);
    const Quat4f upThenForward = alignForward * alignUp;
    const Quat4f forwardThenUp = alignUp * alignForward;

    if ((upThenForward.RotateVector(source.forward) - target.forward).LengthSquared() > (forwardThenUp.RotateVector(source.forward) - target.forward).LengthSquared())
    {
        return forwardThenUp;
    }

    return upThenForward;
}

float GetHipsHeight(
    const SkeletonBindData& bindData,
    const HumanoidMapping& mapping,
    const Array<RetargetModelTransform, AnimAllocator>& bindModel)
{
    const Vec3f hips = GetHumanoidBindPosition(bindData, mapping, bindModel, HumanoidBone::Hips);
    const Vec3f head = GetHumanoidBindPosition(bindData, mapping, bindModel, HumanoidBone::Head);

    const Vec3f feet = (GetHumanoidBindPosition(bindData, mapping, bindModel, HumanoidBone::LeftFoot)
        + GetHumanoidBindPosition(bindData, mapping, bindModel, HumanoidBone::RightFoot)) * 0.5f;

    return (hips - feet).Dot((head - hips).Normalized());
}

void SampleRetargeted(
    const Animation& animation,
    const ClipBinding& binding,
    const SkeletonBindData& bindData,
    float time,
    Span<Transform> outPose)
{
    const RetargetBinding& retarget = *binding.retarget;
    const SkeletonBindData& sourceBindData = retarget.sourceSkeleton->GetBindData();

    if (HYP_UNLIKELY(!t_retargetScratch))
    {
        /// note: leaked intentionally
        t_retargetScratch = new RetargetScratch;
    }

    RetargetScratch& scratch = *t_retargetScratch;
    scratch.sourcePose = sourceBindData.restLocals;

    for (uint32 trackIndex = 0; trackIndex < uint32(binding.trackSlots.Size()); ++trackIndex)
    {
        const uint32 slot = binding.trackSlots[trackIndex];

        if (slot != SkeletonBindData::InvalidSlot)
        {
            scratch.sourcePose[slot] = animation.GetTrack(trackIndex)->GetKeyframe(time).transform;
        }
    }

    ComputeModelTransforms(sourceBindData, scratch.sourcePose.ToSpan(), scratch.sourceModel);

    scratch.targetModel.Resize(bindData.NumSlots());

    const Quat4f targetToSource = retarget.sourceToTarget.Inverse();

    for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
    {
        const uint32 parentSlot = bindData.parentSlots[slot];

        const RetargetModelTransform& parentModel = parentSlot != SkeletonBindData::InvalidSlot
            ? scratch.targetModel[parentSlot]
            : g_emptyRMT;

        Transform local = bindData.restLocals[slot];

        const uint32 sourceSlot = retarget.sourceSlotByTarget[slot];

        if (sourceSlot != SkeletonBindData::InvalidSlot)
        {
            Quat4f turnFromBind = scratch.sourceModel[sourceSlot].rotation * retarget.inverseSourceBindRotations[sourceSlot];

            if (retarget.mirrored)
            {
                turnFromBind = MirrorRotation(turnFromBind, retarget.mirrorNormal);
            }

            const Quat4f modelRotation = retarget.sourceToTarget * turnFromBind * targetToSource * retarget.targetBindRotations[slot];

            local.SetRotation(parentModel.rotation.Inverse() * modelRotation);

            if (slot == retarget.targetHips)
            {
                Vec3f sourceOffset = scratch.sourceModel[sourceSlot].position - retarget.sourceHipsBindPosition;

                if (retarget.mirrored)
                {
                    sourceOffset = MirrorPoint(sourceOffset, retarget.mirrorNormal);
                }

                const Vec3f offset = retarget.sourceToTarget.RotateVector(sourceOffset) * retarget.hipsScale;

                local.SetTranslation(parentModel.rotation.Inverse().RotateVector(
                        retarget.targetHipsBindPosition + offset - parentModel.position) / parentModel.scale);
            }
        }

        outPose[slot] = local;
        scratch.targetModel[slot] = ComposeModelTransform(parentModel, local);
    }
}

// where the root motion slot is at time, in the target's model space up to a constant offset
Vec3f SampleRootPosition(const Animation& animation, const ClipBinding& binding, float time)
{
    if (binding.retarget)
    {
        const RetargetBinding& retarget = *binding.retarget;
        const SkeletonBindData& sourceBindData = retarget.sourceSkeleton->GetBindData();

        RetargetModelTransform sourceModel;

        for (size_t index = 0; index < retarget.sourceHipsChain.Size(); ++index)
        {
            const uint32 trackIndex = retarget.sourceHipsChainTracks[index];

            sourceModel = ComposeModelTransform(sourceModel, trackIndex != SkeletonBindData::InvalidSlot
                    ? animation.GetTrack(trackIndex)->GetKeyframe(time).transform
                    : sourceBindData.restLocals[retarget.sourceHipsChain[index]]);
        }

        const Vec3f sourcePosition = retarget.mirrored
            ? MirrorPoint(sourceModel.position, retarget.mirrorNormal)
            : sourceModel.position;

        return retarget.sourceToTarget.RotateVector(sourcePosition) * retarget.hipsScale;
    }

    const RetargetModelTransform& parentModel = binding.rootParentModel;

    return parentModel.rotation.RotateVector(
        animation.GetTrack(binding.rootTrack)->GetKeyframe(time).transform.GetTranslation() * parentModel.scale);
}

// the clip's track nearest the skeleton's root, which carries its root motion
uint32 FindRootTrack(const ClipBinding& binding)
{
    uint32 rootTrack = SkeletonBindData::InvalidSlot;

    for (uint32 trackIndex = 0; trackIndex < uint32(binding.trackSlots.Size()); ++trackIndex)
    {
        const uint32 slot = binding.trackSlots[trackIndex];

        if (slot != SkeletonBindData::InvalidSlot && (rootTrack == SkeletonBindData::InvalidSlot || slot < binding.trackSlots[rootTrack]))
        {
            rootTrack = trackIndex;
        }
    }

    return rootTrack;
}

// a slot's parent in model space, from the bind pose (the bones above a clip's root don't animate)
RetargetModelTransform GetParentModelTransform(const SkeletonBindData& bindData, uint32 slot)
{
    Array<uint32> chain;

    for (uint32 parent = bindData.parentSlots[slot]; parent != SkeletonBindData::InvalidSlot; parent = bindData.parentSlots[parent])
    {
        chain.PushBack(parent);
    }

    RetargetModelTransform model;

    for (auto it = chain.End(); it != chain.Begin();)
    {
        --it;
        model = ComposeModelTransform(model, bindData.restLocals[*it]);
    }

    return model;
}

// bends the middle joint so the chain's end can reach target, then swings the chain at it; the end keeps its model-space rotation
void SolveTwoBoneIK(
    const SkeletonBindData& bindData,
    const Array<RetargetModelTransform, AnimAllocator>& model,
    uint32 upper,
    uint32 lower,
    uint32 end,
    const Vec3f& target,
    const Vec3f& kneeForward,
    Span<Transform> pose)
{
    const Vec3f a = model[upper].position;
    const Vec3f b = model[lower].position;
    const Vec3f c = model[end].position;

    const float upperLength = (b - a).Length();
    const float lowerLength = (c - b).Length();

    if (upperLength < 1e-6f || lowerLength < 1e-6f)
    {
        return;
    }

    const float reach = MathUtil::Clamp((target - a).Length(),
        MathUtil::Abs(upperLength - lowerLength) * 1.001f + 1e-6f,
        (upperLength + lowerLength) * 0.999f);

    Vec3f bendAxis = (b - a).Cross(c - b);

    // a straight leg has no bend plane of its own, so it bends in the one facing forward, with the knee going forward
    const bool isStraight = bendAxis.LengthSquared() < 1e-12f;

    if (isStraight)
    {
        bendAxis = (c - a).Cross(kneeForward);

        if (bendAxis.LengthSquared() < 1e-12f)
        {
            return;
        }
    }

    bendAxis.Normalize();

    const float currentAngle = MathUtil::Arccos(MathUtil::Clamp((a - b).Normalized().Dot((c - b).Normalized()), -1.0f, 1.0f));
    const float targetAngle = MathUtil::Arccos(MathUtil::Clamp((upperLength * upperLength + lowerLength * lowerLength - reach * reach) / (2.0f * upperLength * lowerLength), -1.0f, 1.0f));

    const Quat4f turn(bendAxis, targetAngle - currentAngle);

    const auto swingFor = [&](const Quat4f& kneeTurn)
    {
        return RotationBetween((b + kneeTurn.RotateVector(c - b) - a).Normalized(), (target - a).Normalized());
    };

    const auto reachError = [&](const Quat4f& kneeTurn)
    {
        return MathUtil::Abs((b + kneeTurn.RotateVector(c - b) - a).Length() - reach);
    };

    const auto kneeForwardness = [&](const Quat4f& kneeTurn)
    {
        return swingFor(kneeTurn).RotateVector(b - a).Dot(kneeForward);
    };

    // the turn's sign follows the engine's handedness, so keep whichever opens the joint to the wanted reach
    Quat4f kneeTurn = turn;

    if (isStraight)
    {
        if (kneeForwardness(turn.Inverse()) > kneeForwardness(turn))
        {
            kneeTurn = turn.Inverse();
        }
    }
    else if (reachError(turn.Inverse()) < reachError(turn))
    {
        kneeTurn = turn.Inverse();
    }

    const Quat4f swing = swingFor(kneeTurn);

    // everything below the upper bone swings; the lower bone and below also bend
    const auto newModelRotation = [&](uint32 slot)
    {
        Quat4f rotation = model[slot].rotation;

        for (uint32 current = slot; current != SkeletonBindData::InvalidSlot; current = bindData.parentSlots[current])
        {
            if (current == lower)
            {
                rotation = kneeTurn * rotation;
                break;
            }

            if (current == upper)
            {
                break;
            }
        }

        return swing * rotation;
    };

    const auto parentModelRotation = [&](uint32 slot)
    {
        const uint32 parentSlot = bindData.parentSlots[slot];

        if (parentSlot == SkeletonBindData::InvalidSlot)
        {
            return Quat4f::Identity();
        }

        // above the upper bone nothing moves
        for (uint32 current = parentSlot; current != SkeletonBindData::InvalidSlot; current = bindData.parentSlots[current])
        {
            if (current == upper)
            {
                return newModelRotation(parentSlot);
            }
        }

        return model[parentSlot].rotation;
    };

    const Quat4f upperRotation = newModelRotation(upper);
    const Quat4f lowerRotation = newModelRotation(lower);

    pose[upper].SetRotation(parentModelRotation(upper).Inverse() * upperRotation);
    pose[lower].SetRotation(parentModelRotation(lower).Inverse() * lowerRotation);
    pose[end].SetRotation(parentModelRotation(end).Inverse() * model[end].rotation);
}

} // namespace

namespace PoseOps {

ClipBinding BindClip(const Animation& animation, const SkeletonBindData& bindData)
{
    ClipBinding binding;
    binding.animationRevision = animation.GetRevision();
    binding.trackSlots.Reserve(animation.NumTracks());

    for (const Handle<AnimationTrack>& track : animation.GetTracks())
    {
        binding.trackSlots.PushBack(track.IsValid() ? bindData.FindSlot(track->GetBoneName()) : SkeletonBindData::InvalidSlot);
    }

    binding.rootTrack = FindRootTrack(binding);

    if (binding.rootTrack != SkeletonBindData::InvalidSlot)
    {
        binding.rootSlot = binding.trackSlots[binding.rootTrack];
        binding.rootParentModel = GetParentModelTransform(bindData, binding.rootSlot);
    }

    return binding;
}

ClipBinding BindClip(const Animation& animation, const Skeleton& skeleton)
{
    const SkeletonBindData& bindData = skeleton.GetBindData();

    ClipBinding binding = BindClip(animation, bindData);

    if (!HumanoidSkeletonData::IsHumanoid(skeleton))
    {
        return binding;
    }

    const HumanoidMapping& targetMapping = HumanoidSkeletonData::GetSkeletonMapping(skeleton);

    // a clip that already drives this rig's hips by name was made for it
    const uint32 targetHips = bindData.FindSlot(targetMapping.GetBoneName(HumanoidBone::Hips));

    if (binding.trackSlots.Contains(targetHips))
    {
        return binding;
    }

    Handle<Skeleton> sourceSkeleton = animation.ResolveSourceSkeleton();

    if (!sourceSkeleton.IsValid() || sourceSkeleton.Get() == &skeleton || !HumanoidSkeletonData::IsHumanoid(*sourceSkeleton))
    {
        return binding;
    }

    const SkeletonBindData& sourceBindData = sourceSkeleton->GetBindData();
    const HumanoidMapping& sourceMapping = HumanoidSkeletonData::GetSkeletonMapping(*sourceSkeleton);

    SharedPtr<RetargetBinding> retarget = MakeSharedWithAllocator<RetargetBinding, AnimAllocator>();
    retarget->sourceSkeleton = sourceSkeleton;
    retarget->sourceSkeletonRevision = sourceSkeleton->GetRevision();
    retarget->targetHips = targetHips;

    retarget->sourceSlotByTarget.Resize(bindData.NumSlots());

    for (uint32& sourceSlot : retarget->sourceSlotByTarget)
    {
        sourceSlot = SkeletonBindData::InvalidSlot;
    }

    for (uint32 bone = 0; bone < NumHumanoidBones; ++bone)
    {
        const uint32 targetSlot = bindData.FindSlot(targetMapping.GetBoneName(HumanoidBone(bone)));
        const uint32 sourceSlot = sourceBindData.FindSlot(sourceMapping.GetBoneName(HumanoidBone(bone)));

        if (targetSlot != SkeletonBindData::InvalidSlot && sourceSlot != SkeletonBindData::InvalidSlot)
        {
            retarget->sourceSlotByTarget[targetSlot] = sourceSlot;
        }
    }

    Array<RetargetModelTransform, AnimAllocator> sourceBindModel;
    Array<RetargetModelTransform, AnimAllocator> targetBindModel;

    ComputeModelTransforms(sourceBindData, sourceBindData.restLocals.ToSpan(), sourceBindModel);
    ComputeModelTransforms(bindData, bindData.restLocals.ToSpan(), targetBindModel);

    const CharacterAxes sourceAxes = GetCharacterAxes(sourceBindData, sourceMapping, sourceBindModel);
    const CharacterAxes targetAxes = GetCharacterAxes(bindData, targetMapping, targetBindModel);

    retarget->sourceToTarget = GetSourceToTarget(sourceAxes, targetAxes);

    // a rotation can't turn a mirrored rig into an unmirrored one, so its motion is mirrored across its own sides first
    retarget->mirrored = sourceAxes.mirrored != targetAxes.mirrored;
    retarget->mirrorNormal = sourceAxes.right;

    retarget->inverseSourceBindRotations.Resize(sourceBindData.NumSlots());

    for (uint32 slot = 0; slot < sourceBindData.NumSlots(); ++slot)
    {
        retarget->inverseSourceBindRotations[slot] = sourceBindModel[slot].rotation.Inverse();
    }

    retarget->targetBindRotations.Resize(bindData.NumSlots());

    for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
    {
        retarget->targetBindRotations[slot] = targetBindModel[slot].rotation;
    }

    retarget->sourceHipsBindPosition = GetHumanoidBindPosition(sourceBindData, sourceMapping, sourceBindModel, HumanoidBone::Hips);
    retarget->targetHipsBindPosition = GetHumanoidBindPosition(bindData, targetMapping, targetBindModel, HumanoidBone::Hips);

    const float sourceHipsHeight = GetHipsHeight(sourceBindData, sourceMapping, sourceBindModel);

    if (sourceHipsHeight > MathUtil::epsilonF)
    {
        retarget->hipsScale = GetHipsHeight(bindData, targetMapping, targetBindModel) / sourceHipsHeight;
    }

    binding.trackSlots = BindClip(animation, sourceBindData).trackSlots;

    // the source hips and the bones above them, root first, place the hips for root motion
    for (uint32 slot = sourceBindData.FindSlot(sourceMapping.GetBoneName(HumanoidBone::Hips)); slot != SkeletonBindData::InvalidSlot; slot = sourceBindData.parentSlots[slot])
    {
        retarget->sourceHipsChain.PushFront(slot);
        retarget->sourceHipsChainTracks.PushFront(SkeletonBindData::InvalidSlot);
    }

    for (uint32 trackIndex = 0; trackIndex < uint32(binding.trackSlots.Size()); ++trackIndex)
    {
        for (size_t index = 0; index < retarget->sourceHipsChain.Size(); ++index)
        {
            if (retarget->sourceHipsChain[index] == binding.trackSlots[trackIndex] && retarget->sourceHipsChainTracks[index] == SkeletonBindData::InvalidSlot)
            {
                retarget->sourceHipsChainTracks[index] = trackIndex;
            }
        }
    }

    binding.rootTrack = SkeletonBindData::InvalidSlot;
    binding.rootSlot = targetHips;
    binding.rootParentModel = GetParentModelTransform(bindData, targetHips);
    binding.retarget = std::move(retarget);

    return binding;
}

bool IsBindingCurrent(const ClipBinding& binding, const Animation& animation)
{
    if (binding.animationRevision != animation.GetRevision())
    {
        return false;
    }

    return !binding.retarget || binding.retarget->sourceSkeleton->GetRevision() == binding.retarget->sourceSkeletonRevision;
}

uint32 CountBoundSlots(const ClipBinding& binding)
{
    const Array<uint32, AnimAllocator>& slots = binding.retarget ? binding.retarget->sourceSlotByTarget : binding.trackSlots;

    uint32 count = 0;

    for (uint32 slot : slots)
    {
        if (slot != SkeletonBindData::InvalidSlot)
        {
            ++count;
        }
    }

    return count;
}

Vec3f GetModelUp(const Skeleton& skeleton)
{
    if (!HumanoidSkeletonData::IsHumanoid(skeleton))
    {
        return Vec3f::UnitY();
    }

    const SkeletonBindData& bindData = skeleton.GetBindData();

    Array<RetargetModelTransform, AnimAllocator> bindModel;
    ComputeModelTransforms(bindData, bindData.restLocals.ToSpan(), bindModel);

    return GetCharacterAxes(bindData, HumanoidSkeletonData::GetSkeletonMapping(skeleton), bindModel).up;
}

Vec3f GetModelForward(const Skeleton& skeleton)
{
    if (!HumanoidSkeletonData::IsHumanoid(skeleton))
    {
        return Vec3f::UnitZ();
    }

    const SkeletonBindData& bindData = skeleton.GetBindData();

    Array<RetargetModelTransform, AnimAllocator> bindModel;
    ComputeModelTransforms(bindData, bindData.restLocals.ToSpan(), bindModel);

    const HumanoidMapping& mapping = HumanoidSkeletonData::GetSkeletonMapping(skeleton);

    return GetHumanoidFacing(bindData, mapping, bindModel, GetCharacterAxes(bindData, mapping, bindModel).up);
}

Vec3f GetRootMotion(const Animation& animation, const ClipBinding& binding, const Vec3f& up, float fromTime, float toTime, bool wrapped)
{
    if (binding.rootSlot == SkeletonBindData::InvalidSlot)
    {
        return Vec3f::Zero();
    }

    const auto positionAt = [&](float time)
    {
        return SampleRootPosition(animation, binding, time);
    };

    Vec3f modelDelta = positionAt(toTime) - positionAt(fromTime);

    if (wrapped)
    {
        const float length = animation.GetLength();

        // forward through the end and on from the start, or backward through the start and on from the end
        modelDelta = toTime <= fromTime
            ? (positionAt(length) - positionAt(fromTime)) + (positionAt(toTime) - positionAt(0.0f))
            : (positionAt(0.0f) - positionAt(fromTime)) + (positionAt(toTime) - positionAt(length));
    }

    return modelDelta - up * modelDelta.Dot(up);
}

void RemoveRootMotion(const Animation& animation, const ClipBinding& binding, const Vec3f& up, float time, Span<Transform> pose)
{
    if (binding.rootSlot == SkeletonBindData::InvalidSlot)
    {
        return;
    }

    const RetargetModelTransform& parentModel = binding.rootParentModel;

    // the root keeps its height and where it started, so the clip plays in place
    const Vec3f modelDelta = SampleRootPosition(animation, binding, time) - SampleRootPosition(animation, binding, 0.0f);
    const Vec3f horizontal = modelDelta - up * modelDelta.Dot(up);

    pose[binding.rootSlot].SetTranslation(pose[binding.rootSlot].GetTranslation() - parentModel.rotation.Inverse().RotateVector(horizontal) / parentModel.scale);
}

void ApplyFootIK(const Skeleton& skeleton, const Vec3f& up, float leftOffset, float rightOffset, Span<Transform> pose)
{
    if (leftOffset == 0.0f && rightOffset == 0.0f)
    {
        return;
    }

    Assert(HumanoidSkeletonData::IsHumanoid(skeleton), "Foot IK on skeleton '{}', which isn't humanoid", skeleton.GetName());

    const HumanoidMapping& mapping = HumanoidSkeletonData::GetSkeletonMapping(skeleton);
    const SkeletonBindData& bindData = skeleton.GetBindData();

    const auto slotOf = [&](HumanoidBone bone)
    {
        return bindData.FindSlot(mapping.GetBoneName(bone));
    };

    const uint32 hips = slotOf(HumanoidBone::Hips);
    const uint32 legs[2][3] = {
        { slotOf(HumanoidBone::LeftUpperLeg), slotOf(HumanoidBone::LeftLowerLeg), slotOf(HumanoidBone::LeftFoot) },
        { slotOf(HumanoidBone::RightUpperLeg), slotOf(HumanoidBone::RightLowerLeg), slotOf(HumanoidBone::RightFoot) }
    };

    Array<RetargetModelTransform, AnimAllocator> model;
    ComputeModelTransforms(bindData, pose, model);

    const Vec3f targets[2] = {
        model[legs[0][2]].position + up * leftOffset,
        model[legs[1][2]].position + up * rightOffset
    };

    // the pelvis comes down as far as the lower foot needs, so that leg isn't stretched past its length
    const float pelvisDrop = MathUtil::Min(0.0f, MathUtil::Min(leftOffset, rightOffset));

    if (pelvisDrop < 0.0f)
    {
        const uint32 parentSlot = bindData.parentSlots[hips];
        const RetargetModelTransform parentModel = parentSlot != SkeletonBindData::InvalidSlot ? model[parentSlot] : RetargetModelTransform {};

        pose[hips].SetTranslation(pose[hips].GetTranslation() + parentModel.rotation.Inverse().RotateVector(up * pelvisDrop) / parentModel.scale);

        ComputeModelTransforms(bindData, pose, model);
    }

    const Vec3f kneeForward = GetHumanoidFacing(bindData, mapping, model, up);

    for (uint32 side = 0; side < 2; ++side)
    {
        SolveTwoBoneIK(
            bindData,
            model,
            legs[side][0],
            legs[side][1], 
            legs[side][2],
            targets[side],
            kneeForward,
            pose);
    }
}

Span<const Transform> GetRestPose(const SkeletonBindData& bindData)
{
    return bindData.restLocals;
}

void ApplyBlended(
    const Animation& animation,
    const ClipBinding& binding,
    const SkeletonBindData& bindData,
    const ApplyAnimParams& params,
    Span<Transform> pose)
{
    HYP_SCOPE;

    const float blend = MathUtil::Clamp(params.blend, 0.0f, 1.0f);

    if (binding.retarget)
    {
        Array<Transform> retargetedPose;
        retargetedPose.Resize(bindData.NumSlots());

        SampleRetargeted(animation, binding, bindData, params.time, retargetedPose.ToSpan());

        for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
        {
            pose[slot] = BlendTransforms(pose[slot], retargetedPose[slot], blend);
        }

        return;
    }

    for (uint32 trackIndex = 0; trackIndex < uint32(binding.trackSlots.Size()); ++trackIndex)
    {
        const uint32 slot = binding.trackSlots[trackIndex];

        if (slot == SkeletonBindData::InvalidSlot)
        {
            continue;
        }

        if (params.blend <= MathUtil::epsilonF)
        {
            ClearSubtree(bindData, slot, pose);
        }

        const Keyframe frame = animation.GetTrack(trackIndex)->GetKeyframe(params.time);

        pose[slot] = BlendTransforms(pose[slot], frame.transform, blend);
    }
}

void ApplyLayered(
    const Animation& animation,
    const ClipBinding& binding,
    const ClipBinding* layerBinding,
    const ClipBinding* secondLayerBinding,
    const SkeletonBindData& bindData,
    const ApplyAnimParams& params,
    Span<Transform> pose)
{
    HYP_SCOPE;

    // layers only mix tracks of one rig, so a retargeted clip plays on its own
    if (binding.retarget)
    {
        ApplyBlended(animation, binding, bindData, params, pose);

        return;
    }

    const float layerWeight = MathUtil::Clamp(params.layerWeight, 0.0f, 1.0f);
    const float blend = MathUtil::Clamp(params.blend, 0.0f, 1.0f);
    const float secondLayerWeight = MathUtil::Clamp(params.secondLayerWeight, 0.0f, 1.0f);

    const Animation* layerAnimation = layerBinding && !layerBinding->retarget ? params.layerAnimation : nullptr;
    const Animation* secondLayerAnimation = secondLayerBinding && !secondLayerBinding->retarget && secondLayerWeight > 0.0f ? params.secondLayerAnimation : nullptr;

    const uint32 numSlots = bindData.NumSlots();

    const Array<uint32> layerTracksBySlot = layerAnimation ? TracksBySlot(*layerBinding, numSlots) : Array<uint32>();
    const Array<uint32> secondLayerTracksBySlot = secondLayerAnimation ? TracksBySlot(*secondLayerBinding, numSlots) : Array<uint32>();

    Array<float> excludedBoneWeights;
    Span<const float> layerBoneWeights = params.layerBoneWeights;

    if (layerAnimation && layerBoneWeights.Size() != numSlots && params.layerExcludedBone.IsValid())
    {
        excludedBoneWeights = BoneMask::BuildExcludingSubtree(bindData, params.layerExcludedBone);
        layerBoneWeights = excludedBoneWeights.ToSpan();
    }

    const auto layerWeightAt = [&](uint32 slot)
    {
        return layerBoneWeights.Size() == numSlots ? layerWeight * MathUtil::Clamp(layerBoneWeights[slot], 0.0f, 1.0f) : layerWeight;
    };

    for (uint32 trackIndex = 0; trackIndex < uint32(binding.trackSlots.Size()); ++trackIndex)
    {
        const uint32 slot = binding.trackSlots[trackIndex];

        if (slot == SkeletonBindData::InvalidSlot)
        {
            continue;
        }

        Keyframe frame = animation.GetTrack(trackIndex)->GetKeyframe(params.time);

        if (layerAnimation && layerTracksBySlot[slot] != SkeletonBindData::InvalidSlot && layerWeightAt(slot) > 0.0f)
        {
            frame = frame.Blend(layerAnimation->GetTrack(layerTracksBySlot[slot])->GetKeyframe(params.layerTime), layerWeightAt(slot));
        }

        if (secondLayerAnimation && secondLayerTracksBySlot[slot] != SkeletonBindData::InvalidSlot)
        {
            frame = frame.Blend(secondLayerAnimation->GetTrack(secondLayerTracksBySlot[slot])->GetKeyframe(params.secondLayerTime), secondLayerWeight);
        }

        pose[slot] = BlendTransforms(pose[slot], frame.transform, blend);
    }

    if (!layerAnimation)
    {
        return;
    }

    const Array<uint32> baseTracksBySlot = TracksBySlot(binding, numSlots);

    // bones only the layer animates
    for (uint32 layerTrackIndex = 0; layerTrackIndex < uint32(layerBinding->trackSlots.Size()); ++layerTrackIndex)
    {
        const uint32 slot = layerBinding->trackSlots[layerTrackIndex];

        if (slot == SkeletonBindData::InvalidSlot || baseTracksBySlot[slot] != SkeletonBindData::InvalidSlot || layerWeightAt(slot) <= 0.0f)
        {
            continue;
        }

        const Keyframe layerFrame = layerAnimation->GetTrack(layerTrackIndex)->GetKeyframe(params.layerTime);

        pose[slot] = BlendTransforms(pose[slot], layerFrame.transform, blend * layerWeightAt(slot));
    }
}

void ApplyTwist(const SkeletonBindData& bindData, Name rootBoneName, Name endBoneName, float angle, Span<Transform> pose)
{
    const uint32 endSlot = bindData.FindSlot(endBoneName);

    if (endSlot == SkeletonBindData::InvalidSlot)
    {
        return;
    }

    Array<uint32> chain;

    for (uint32 slot = endSlot; slot != SkeletonBindData::InvalidSlot; slot = bindData.parentSlots[slot])
    {
        if (!bindData.boneNames[slot].IsValid())
        {
            return;
        }

        chain.PushBack(slot);

        if (bindData.boneNames[slot] == rootBoneName)
        {
            break;
        }
    }

    if (chain.Empty() || bindData.boneNames[chain.Back()] != rootBoneName)
    {
        return;
    }

    // an even share per bone; turning each bone in its own frame lets its children inherit the earlier shares
    const Quat4f shareRotation(Vec3f::UnitY(), angle / float(chain.Size()));

    for (const uint32 slot : chain)
    {
        pose[slot].SetRotation(pose[slot].GetRotation() * shareRotation);
    }
}

void ComputeMatrices(
    const SkeletonBindData& bindData,
    Span<const Transform> pose,
    Span<Mat4f> outModelMatrices,
    Span<Mat4f> outSkinningMatrices)
{
    ComputeSkeletonPoseMatrices(bindData, pose, outModelMatrices, outSkinningMatrices);
}

void SampleClip(
    const Animation& animation,
    const ClipBinding& binding,
    const SkeletonBindData& bindData,
    float time,
    Span<Transform> pose)
{
    HYP_SCOPE;

    if (binding.retarget)
    {
        SampleRetargeted(animation, binding, bindData, time, pose);

        return;
    }

    for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
    {
        pose[slot] = bindData.restLocals[slot];
    }

    for (uint32 trackIndex = 0; trackIndex < uint32(binding.trackSlots.Size()); ++trackIndex)
    {
        const uint32 slot = binding.trackSlots[trackIndex];

        if (slot != SkeletonBindData::InvalidSlot)
        {
            pose[slot] = animation.GetTrack(trackIndex)->GetKeyframe(time).transform;
        }
    }
}

void BlendPoses(Span<const Transform> from, Span<const Transform> to, float weight, Span<const float> boneWeights, Span<Transform> outPose)
{
    HYP_SCOPE;

    for (size_t slot = 0; slot < outPose.Size(); ++slot)
    {
        const float slotWeight = MathUtil::Clamp(boneWeights.Size() == outPose.Size() ? weight * boneWeights[slot] : weight, 0.0f, 1.0f);

        if (slotWeight <= 0.0f)
        {
            outPose[slot] = from[slot];
        }
        else if (slotWeight >= 1.0f)
        {
            outPose[slot] = to[slot];
        }
        else
        {
            outPose[slot] = BlendTransforms(from[slot], to[slot], slotWeight);
        }
    }
}

void ApplyAdditive(const SkeletonBindData& bindData, Span<const Transform> additive, float weight, Span<Transform> pose)
{
    HYP_SCOPE;

    if (weight <= 0.0f)
    {
        return;
    }

    for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
    {
        const Transform& rest = bindData.restLocals[slot];

        const Vec3f deltaTranslation = additive[slot].GetTranslation() - rest.GetTranslation();
        const Quat4f deltaRotation = rest.GetRotation().Inverse() * additive[slot].GetRotation();

        Transform& target = pose[slot];
        target.SetTranslation(target.GetTranslation() + deltaTranslation * weight);
        target.SetRotation(target.GetRotation() * Quat4f::Identity().Slerp(deltaRotation, weight));
    }
}

float AdvanceCrossFade(float delta, float& remaining, float defaultBlend)
{
    if (remaining <= 0.0f)
    {
        return defaultBlend;
    }

    const float blend = MathUtil::Min(delta / remaining, 1.0f);
    remaining = MathUtil::Max(remaining - delta, 0.0f);

    return blend;
}

} // namespace PoseOps

} // namespace Hyperion
