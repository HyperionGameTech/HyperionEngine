#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region BoneDesc Reflection Data

HYP_BEGIN_STRUCT(BoneDesc, 257, 0, {})
    Field(NAME(HYP_STR(Name)), &BoneDesc::name, offsetof(BoneDesc, name)),
    Field(NAME(HYP_STR(BindingTransform)), &BoneDesc::bindingTransform, offsetof(BoneDesc, bindingTransform)),
    Field(NAME(HYP_STR(ParentName)), &BoneDesc::parentName, offsetof(BoneDesc, parentName))
HYP_END_STRUCT

#pragma endregion BoneDesc Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region SkeletonAsset Reflection Data

HYP_BEGIN_CLASS(SkeletonAsset, 46, 0, NAME("AssetObject"))
    Field(NAME(HYP_STR(SkeletonDesc)), &SkeletonAsset::m_skeletonDesc, offsetof(SkeletonAsset, m_skeletonDesc), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } })
HYP_END_CLASS

#pragma endregion SkeletonAsset Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region SkeletonData Reflection Data

HYP_BEGIN_STRUCT(SkeletonData, 258, 0, {})
    Field(NAME(HYP_STR(Animations)), &SkeletonData::animations, offsetof(SkeletonData, animations))
HYP_END_STRUCT

#pragma endregion SkeletonData Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region SkeletonDesc Reflection Data

HYP_BEGIN_STRUCT(SkeletonDesc, 259, 0, {})
    Field(NAME(HYP_STR(Bones)), &SkeletonDesc::bones, offsetof(SkeletonDesc, bones)),
    Field(NAME(HYP_STR(AnimationNames)), &SkeletonDesc::animationNames, offsetof(SkeletonDesc, animationNames))
HYP_END_STRUCT

#pragma endregion SkeletonDesc Reflection Data

} // namespace hyperion

