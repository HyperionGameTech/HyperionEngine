#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region Skeleton Reflection Data

HYP_BEGIN_CLASS(Skeleton, 54, 0, NAME("AssetObject"))
    HypMethod(NAME(HYP_STR(GetRootBone)), &Skeleton::GetRootBone, Span<const HypClassAttribute> { {HypClassAttribute("property", "RootBone") } }),
    HypMethod(NAME(HYP_STR(SetRootBone)), &Skeleton::SetRootBone, Span<const HypClassAttribute> { {HypClassAttribute("property", "RootBone") } }),
    HypMethod(NAME(HYP_STR(GetSkeletonAsset)), &Skeleton::GetSkeletonAsset, Span<const HypClassAttribute> { {HypClassAttribute("property", "SkeletonAsset") } }),
    HypMethod(NAME(HYP_STR(SetSkeletonAsset)), &Skeleton::SetSkeletonAsset, Span<const HypClassAttribute> { {HypClassAttribute("property", "SkeletonAsset") } }),
    HypField(NAME(HYP_STR(RootBone)), &Skeleton::m_rootBone, offsetof(Skeleton, m_rootBone), Span<const HypClassAttribute> { {HypClassAttribute("property", "RootBone") } })
HYP_END_CLASS

#pragma endregion Skeleton Reflection Data

} // namespace hyperion

