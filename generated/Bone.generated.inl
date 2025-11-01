#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region Bone Reflection Data

HYP_BEGIN_CLASS(Bone, 145, 0, NAME("Node"))
    HypField(NAME(HYP_STR(Skeleton)), &Bone::m_skeleton, offsetof(Bone, m_skeleton), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(BoneMatrix)), &Bone::m_boneMatrix, offsetof(Bone, m_boneMatrix)),
    HypField(NAME(HYP_STR(Keyframe)), &Bone::m_keyframe, offsetof(Bone, m_keyframe), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(BindingTransform)), &Bone::m_bindingTransform, offsetof(Bone, m_bindingTransform)),
    HypField(NAME(HYP_STR(PoseTransform)), &Bone::m_poseTransform, offsetof(Bone, m_poseTransform)),
    HypField(NAME(HYP_STR(WorldBoneTranslation)), &Bone::m_worldBoneTranslation, offsetof(Bone, m_worldBoneTranslation)),
    HypField(NAME(HYP_STR(WorldBoneRotation)), &Bone::m_worldBoneRotation, offsetof(Bone, m_worldBoneRotation)),
    HypField(NAME(HYP_STR(InvBindingRotation)), &Bone::m_invBindingRotation, offsetof(Bone, m_invBindingRotation))
HYP_END_CLASS

#pragma endregion Bone Reflection Data

} // namespace hyperion

