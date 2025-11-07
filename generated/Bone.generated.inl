#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Bone Reflection Data

HYP_BEGIN_CLASS(Bone, 145, 0, NAME("Node"))
    Field(NAME(HYP_STR(Skeleton)), &Bone::m_skeleton, offsetof(Bone, m_skeleton), Span<const ClassAttribute> { {ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(BoneMatrix)), &Bone::m_boneMatrix, offsetof(Bone, m_boneMatrix)),
    Field(NAME(HYP_STR(Keyframe)), &Bone::m_keyframe, offsetof(Bone, m_keyframe), Span<const ClassAttribute> { {ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(BindingTransform)), &Bone::m_bindingTransform, offsetof(Bone, m_bindingTransform)),
    Field(NAME(HYP_STR(PoseTransform)), &Bone::m_poseTransform, offsetof(Bone, m_poseTransform)),
    Field(NAME(HYP_STR(WorldBoneTranslation)), &Bone::m_worldBoneTranslation, offsetof(Bone, m_worldBoneTranslation)),
    Field(NAME(HYP_STR(WorldBoneRotation)), &Bone::m_worldBoneRotation, offsetof(Bone, m_worldBoneRotation)),
    Field(NAME(HYP_STR(InvBindingRotation)), &Bone::m_invBindingRotation, offsetof(Bone, m_invBindingRotation))
HYP_END_CLASS

#pragma endregion Bone Reflection Data

} // namespace hyperion

