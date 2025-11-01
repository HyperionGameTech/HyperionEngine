#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region BoneDesc Reflection Data

HYP_BEGIN_STRUCT(BoneDesc, 224, 0, {})
    HypField(NAME(HYP_STR(Name)), &BoneDesc::name, offsetof(BoneDesc, name)),
    HypField(NAME(HYP_STR(BindingTransform)), &BoneDesc::bindingTransform, offsetof(BoneDesc, bindingTransform)),
    HypField(NAME(HYP_STR(ParentName)), &BoneDesc::parentName, offsetof(BoneDesc, parentName))
HYP_END_STRUCT

#pragma endregion BoneDesc Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region SkeletonAsset Reflection Data

HYP_BEGIN_CLASS(SkeletonAsset, 19, 0, NAME("AssetObject"))
    HypField(NAME(HYP_STR(SkeletonDesc)), &SkeletonAsset::m_skeletonDesc, offsetof(SkeletonAsset, m_skeletonDesc), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } })
HYP_END_CLASS

#pragma endregion SkeletonAsset Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region SkeletonData Reflection Data

HYP_BEGIN_STRUCT(SkeletonData, 225, 0, {})
    HypField(NAME(HYP_STR(Animations)), &SkeletonData::animations, offsetof(SkeletonData, animations))
HYP_END_STRUCT

#pragma endregion SkeletonData Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region SkeletonDesc Reflection Data

HYP_BEGIN_STRUCT(SkeletonDesc, 226, 0, {})
    HypField(NAME(HYP_STR(Bones)), &SkeletonDesc::bones, offsetof(SkeletonDesc, bones)),
    HypField(NAME(HYP_STR(AnimationNames)), &SkeletonDesc::animationNames, offsetof(SkeletonDesc, animationNames))
HYP_END_STRUCT

#pragma endregion SkeletonDesc Reflection Data

} // namespace hyperion

