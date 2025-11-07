#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Skeleton Reflection Data

HYP_BEGIN_CLASS(Skeleton, 25, 0, NAME("AssetObject"))
    Method(NAME(HYP_STR(GetRootBone)), &Skeleton::GetRootBone, Span<const ClassAttribute> { {ClassAttribute("property", "RootBone") } }),
    Method(NAME(HYP_STR(SetRootBone)), &Skeleton::SetRootBone, Span<const ClassAttribute> { {ClassAttribute("property", "RootBone") } }),
    Method(NAME(HYP_STR(GetSkeletonAsset)), &Skeleton::GetSkeletonAsset, Span<const ClassAttribute> { {ClassAttribute("property", "SkeletonAsset") } }),
    Method(NAME(HYP_STR(SetSkeletonAsset)), &Skeleton::SetSkeletonAsset, Span<const ClassAttribute> { {ClassAttribute("property", "SkeletonAsset") } }),
    Field(NAME(HYP_STR(RootBone)), &Skeleton::m_rootBone, offsetof(Skeleton, m_rootBone), Span<const ClassAttribute> { {ClassAttribute("property", "RootBone") } })
HYP_END_CLASS

#pragma endregion Skeleton Reflection Data

} // namespace hyperion

