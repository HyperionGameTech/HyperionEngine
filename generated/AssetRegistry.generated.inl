#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region AssetPackageFlags Reflection Data

HYP_BEGIN_ENUM(AssetPackageFlags, 259, 0, {})
    HypConstant(NAME(HYP_STR(APF_NONE)), AssetPackageFlags::APF_NONE),
    HypConstant(NAME(HYP_STR(APF_TRANSIENT)), AssetPackageFlags::APF_TRANSIENT),
    HypConstant(NAME(HYP_STR(APF_HIDDEN)), AssetPackageFlags::APF_HIDDEN)
HYP_END_ENUM

#pragma endregion AssetPackageFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AssetPackage Reflection Data

HYP_BEGIN_CLASS(AssetPackage, 43, 0, NAME("HypObjectBase"))
    HypMethod(NAME(HYP_STR(GetUUID)), &AssetPackage::GetUUID),
    HypMethod(NAME(HYP_STR(SetUUID)), &AssetPackage::SetUUID),
    HypMethod(NAME(HYP_STR(GetName)), &AssetPackage::GetName),
    HypMethod(NAME(HYP_STR(Rename)), &AssetPackage::Rename),
    HypMethod(NAME(HYP_STR(GetFriendlyName)), &AssetPackage::GetFriendlyName),
    HypMethod(NAME(HYP_STR(SetFriendlyName)), &AssetPackage::SetFriendlyName),
    HypMethod(NAME(HYP_STR(GetFlags)), &AssetPackage::GetFlags),
    HypMethod(NAME(HYP_STR(IsTransient)), &AssetPackage::IsTransient),
    HypMethod(NAME(HYP_STR(IsHidden)), &AssetPackage::IsHidden),
    HypMethod(NAME(HYP_STR(IsSubpackageOf)), &AssetPackage::IsSubpackageOf),
    HypMethod(NAME(HYP_STR(BuildPackagePath)), &AssetPackage::BuildPackagePath),
    HypMethod(NAME(HYP_STR(BuildAssetPath)), &AssetPackage::BuildAssetPath),
    HypMethod(NAME(HYP_STR(HasAssetWithName)), &AssetPackage::HasAssetWithName),
    HypMethod(NAME(HYP_STR(GetUniqueAssetName)), &AssetPackage::GetUniqueAssetName),
    HypMethod(NAME(HYP_STR(Save)), &AssetPackage::Save),
    HypMethod(NAME(HYP_STR(GetDependencies)), &AssetPackage::GetDependencies),
    HypMethod(NAME(HYP_STR(GetRelativeDependencies)), &AssetPackage::GetRelativeDependencies, Span<const HypClassAttribute> { {HypClassAttribute("property", "Dependencies"), HypClassAttribute("serialize", true) } }),
    HypMethod(NAME(HYP_STR(SetRelativeDependencies)), &AssetPackage::SetRelativeDependencies, Span<const HypClassAttribute> { {HypClassAttribute("property", "Dependencies"), HypClassAttribute("serialize", true) } }),
    HypMethod(NAME(HYP_STR(AddDependency)), &AssetPackage::AddDependency),
    HypMethod(NAME(HYP_STR(IsDirty)), &AssetPackage::IsDirty, Span<const HypClassAttribute> { {HypClassAttribute("property", "IsDirty"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(MarkDirty)), &AssetPackage::MarkDirty),
    HypField(NAME(HYP_STR(Uuid)), &AssetPackage::m_uuid, offsetof(AssetPackage, m_uuid)),
    HypField(NAME(HYP_STR(Name)), &AssetPackage::m_name, offsetof(AssetPackage, m_name)),
    HypField(NAME(HYP_STR(FriendlyName)), &AssetPackage::m_friendlyName, offsetof(AssetPackage, m_friendlyName)),
    HypField(NAME(HYP_STR(Flags)), &AssetPackage::m_flags, offsetof(AssetPackage, m_flags)),
    HypField(NAME(HYP_STR(Dependencies)), &AssetPackage::m_dependencies, offsetof(AssetPackage, m_dependencies), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } })
HYP_END_CLASS

#pragma endregion AssetPackage Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AssetRegistry Reflection Data

HYP_BEGIN_CLASS(AssetRegistry, 44, 0, NAME("HypObjectBase"))
    HypMethod(NAME(HYP_STR(GetRootPath)), &AssetRegistry::GetRootPath),
    HypMethod(NAME(HYP_STR(SetRootPath)), &AssetRegistry::SetRootPath),
    HypMethod(NAME(HYP_STR(RemovePackage)), &AssetRegistry::RemovePackage),
    HypMethod(NAME(HYP_STR(GetPackageFromPath)), &AssetRegistry::GetPackageFromPath),
    HypMethod(NAME(HYP_STR(GetSubpackage)), &AssetRegistry::GetSubpackage),
    HypMethod(NAME(HYP_STR(LoadSubpackages)), &AssetRegistry::LoadSubpackages),
    HypMethod(NAME(HYP_STR(GetUniqueAssetName)), &AssetRegistry::GetUniqueAssetName),
    HypField(NAME(HYP_STR(RootPath)), &AssetRegistry::m_rootPath, offsetof(AssetRegistry, m_rootPath), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true) } })
HYP_END_CLASS

#pragma endregion AssetRegistry Reflection Data

} // namespace hyperion

