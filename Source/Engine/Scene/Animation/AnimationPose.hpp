/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Array.hpp>

#include <Core/Math/Mat4f.hpp>
#include <Core/Math/Transform.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Utilities/Span.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class Animation;
struct SkeletonBindData;
struct ApplyAnimParams;

// the bind data slot each of an animation's tracks drives, or SkeletonBindData::InvalidSlot
struct ClipBinding
{
    Array<uint32> trackSlots;
    uint32 animationRevision = 0;
};

namespace PoseOps {

ENGINE_API ClipBinding BindClip(const Animation& animation, const SkeletonBindData& bindData);

ENGINE_API Array<Transform> MakeRestPose(const SkeletonBindData& bindData);

ENGINE_API void ApplyBlended(
    const Animation& animation,
    const ClipBinding& binding,
    const SkeletonBindData& bindData,
    const ApplyAnimParams& params,
    Span<Transform> pose);

ENGINE_API void ApplyLayered(
    const Animation& animation,
    const ClipBinding& binding,
    const ClipBinding* layerBinding,
    const ClipBinding* secondLayerBinding,
    const SkeletonBindData& bindData,
    const ApplyAnimParams& params,
    Span<Transform> pose);

ENGINE_API void ApplyTwist(const SkeletonBindData& bindData, Name rootBoneName, Name endBoneName, float angle, Span<Transform> pose);

ENGINE_API void ComputeMatrices(
    const SkeletonBindData& bindData,
    Span<const Transform> pose,
    Span<Mat4f> outModelMatrices,
    Span<Mat4f> outSkinningMatrices);

ENGINE_API void SampleClip(const Animation& animation, const ClipBinding& binding, const SkeletonBindData& bindData, float time, Span<Transform> pose);

// boneWeights, when not empty, scale weight per slot (a BoneMask)
ENGINE_API void BlendPoses(Span<const Transform> from, Span<const Transform> to, float weight, Span<const float> boneWeights, Span<Transform> outPose);

// adds additive's difference from the rest pose onto pose
ENGINE_API void ApplyAdditive(const SkeletonBindData& bindData, Span<const Transform> additive, float weight, Span<Transform> pose);

// easing toward the clip by delta / remaining covers the gap linearly over the fade
ENGINE_API float AdvanceCrossFade(float delta, float& remaining, float defaultBlend);

} // namespace PoseOps

} // namespace Hyperion
