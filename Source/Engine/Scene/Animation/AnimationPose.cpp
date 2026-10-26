/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Animation/AnimationPose.hpp>
#include <Scene/Animation/BoneMask.hpp>
#include <Scene/Animation/Animation.hpp>
#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Animation/Keyframe.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Quat4f.hpp>

namespace Hyperion {

namespace {

Transform BlendTransforms(const Transform& from, const Transform& to, float blend)
{
    return Keyframe(0.0f, from).Blend(Keyframe(0.0f, to), blend).transform;
}

// Bone::ClearPose() resets a bone and every bone beneath it, stopping at nodes that aren't bones
void ClearSubtree(const SkeletonBindData& bindData, uint32 slot, Span<Transform> pose)
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

        // slots are in depth-first order, so the subtree ends at the first slot outside it
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

    return binding;
}

Array<Transform> MakeRestPose(const SkeletonBindData& bindData)
{
    return Array<Transform>(bindData.restLocals.ToSpan());
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

    const float layerWeight = MathUtil::Clamp(params.layerWeight, 0.0f, 1.0f);
    const float blend = MathUtil::Clamp(params.blend, 0.0f, 1.0f);
    const float secondLayerWeight = MathUtil::Clamp(params.secondLayerWeight, 0.0f, 1.0f);

    const Animation* layerAnimation = layerBinding ? params.layerAnimation : nullptr;
    const Animation* secondLayerAnimation = secondLayerBinding && secondLayerWeight > 0.0f ? params.secondLayerAnimation : nullptr;

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

void SampleClip(const Animation& animation, const ClipBinding& binding, const SkeletonBindData& bindData, float time, Span<Transform> pose)
{
    HYP_SCOPE;

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

        // @FIXME NASTY! AWFUL TERNARY !!!!
        outPose[slot] = slotWeight <= 0.0f ? from[slot]
            : slotWeight >= 1.0f           ? to[slot]
                                           : BlendTransforms(from[slot], to[slot], slotWeight);
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
