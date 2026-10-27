/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Array.hpp>

#include <Core/Math/Mat4f.hpp>
#include <Core/Math/Transform.hpp>
#include <Core/Math/Quat4f.hpp>
#include <Core/Math/Vector3.hpp>

#include <Core/Memory/SharedPtr.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Utilities/Span.hpp>

#include <Animation/AnimationMemory.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class Animation;
class Skeleton;
struct SkeletonBindData;
struct ApplyAnimParams;

struct RetargetModelTransform
{
    Quat4f rotation = Quat4f::Identity();
    Vec3f position = Vec3f::Zero();
    float scale = 1.0f;
};

/// plays a clip recorded on one humanoid rig on another.
///     - each mapped bone takes the source bone's turn away from its bind pose,
///       in model space corrected for which way each rig faces.
struct RetargetBinding
{
    Handle<Skeleton> sourceSkeleton;
    uint32 sourceSkeletonRevision = 0;
    Array<uint32, AnimAllocator> sourceSlotByTarget;
    uint32 targetHips = ~0u;
    float hipsScale = 1.0f;
    Quat4f sourceToTarget = Quat4f::Identity();
    bool mirrored = false;
    Vec3f mirrorNormal;
    Array<Quat4f, AnimAllocator> inverseSourceBindRotations;
    Array<Quat4f, AnimAllocator> targetBindRotations;
    Vec3f sourceHipsBindPosition;
    Vec3f targetHipsBindPosition;
    Array<uint32, AnimAllocator> sourceHipsChain;
    Array<uint32, AnimAllocator> sourceHipsChainTracks;
};

struct ClipBinding
{
    Array<uint32, AnimAllocator> trackSlots;
    uint32 animationRevision = 0;
    
    /// the slot that carries the clip's root motion
    uint32 rootSlot = ~0u;
    uint32 rootTrack = ~0u;
    RetargetModelTransform rootParentModel;

    /// set for a clip from another humanoid rig, whose trackSlots then index that rig's slots.
    SharedPtr<RetargetBinding> retarget;
};

namespace PoseOps {

ANIMATION_API ClipBinding BindClip(
    const Animation& animation,
    const SkeletonBindData& bindData);

/// binds by bone name, or retargets through both humanoid mappings when the clip was recorded on another rig
ANIMATION_API ClipBinding BindClip(
    const Animation& animation,
    const Skeleton& skeleton);

/// \return false once the clip or the rig it's retargeted from has changed since it was bound
ANIMATION_API bool IsBindingCurrent(
    const ClipBinding& binding,
    const Animation& animation);

/// \return how many of the skeleton's bones the binding moves
ANIMATION_API uint32 CountBoundSlots(const ClipBinding& binding);

/// \return the skeleton's up in model space
ANIMATION_API Vec3f GetModelUp(const Skeleton& skeleton);

/// \return direction the skeleton faces in model space
ANIMATION_API Vec3f GetModelForward(const Skeleton& skeleton);

/// \return how far a root motion clip moves the character across the ground between two times, in model space
ANIMATION_API Vec3f GetRootMotion(
    const Animation& animation,
    const ClipBinding& binding,
    const Vec3f& up,
    float fromTime,
    float toTime,
    bool wrapped);

/// takes that movement back out of a sampled pose, so the clip plays in place
ANIMATION_API void RemoveRootMotion(
    const Animation& animation,
    const ClipBinding& binding,
    const Vec3f& up,
    float time,
    Span<Transform> pose);

/// raises each foot of a humanoid by its offset along up with two-bone IK, lowering the pelvis for the lower one
ANIMATION_API void ApplyFootIK(
    const Skeleton& skeleton,
    const Vec3f& up,
    float leftOffset,
    float rightOffset,
    Span<Transform> pose);

ANIMATION_API Span<const Transform> GetRestPose(const SkeletonBindData& bindData);

ANIMATION_API void ApplyBlended(
    const Animation& animation,
    const ClipBinding& binding,
    const SkeletonBindData& bindData,
    const ApplyAnimParams& params,
    Span<Transform> pose);

ANIMATION_API void ApplyLayered(
    const Animation& animation,
    const ClipBinding& binding,
    const ClipBinding* layerBinding,
    const ClipBinding* secondLayerBinding,
    const SkeletonBindData& bindData,
    const ApplyAnimParams& params,
    Span<Transform> pose);

ANIMATION_API void ApplyTwist(const SkeletonBindData& bindData, Name rootBoneName, Name endBoneName, float angle, Span<Transform> pose);

ANIMATION_API void ComputeMatrices(
    const SkeletonBindData& bindData,
    Span<const Transform> pose,
    Span<Mat4f> outModelMatrices,
    Span<Mat4f> outSkinningMatrices);

ANIMATION_API void SampleClip(
    const Animation& animation,
    const ClipBinding& binding,
    const SkeletonBindData& bindData,
    float time,
    Span<Transform> pose);

// boneWeights, when not empty, scale weight per slot (a BoneMask)
ANIMATION_API void BlendPoses(
    Span<const Transform> from,
    Span<const Transform> to,
    float weight,
    Span<const float> boneWeights,
    Span<Transform> outPose);

// adds additive's difference from the rest pose onto pose
ANIMATION_API void ApplyAdditive(
    const SkeletonBindData& bindData,
    Span<const Transform> additive,
    float weight,
    Span<Transform> pose);

// easing toward the clip by delta / remaining covers the gap linearly over the fade
ANIMATION_API float AdvanceCrossFade(float delta, float& remaining, float defaultBlend);

} // namespace PoseOps

} // namespace Hyperion
