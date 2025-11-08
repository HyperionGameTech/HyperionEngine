#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region AssetPackageFlags Reflection Data

HYP_BEGIN_ENUM(AssetPackageFlags, 221, 0, {})
    StaticField(NAME(HYP_STR(APF_NONE)), AssetPackageFlags::APF_NONE),
    StaticField(NAME(HYP_STR(APF_TRANSIENT)), AssetPackageFlags::APF_TRANSIENT),
    StaticField(NAME(HYP_STR(APF_HIDDEN)), AssetPackageFlags::APF_HIDDEN)
HYP_END_ENUM

#pragma endregion AssetPackageFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AssetPackage Reflection Data

HYP_BEGIN_CLASS(AssetPackage, 26, 0, NAME("ObjectBase"))
    Method(NAME(HYP_STR(GetUUID)), &AssetPackage::GetUUID),
    Method(NAME(HYP_STR(SetUUID)), &AssetPackage::SetUUID),
    Method(NAME(HYP_STR(GetName)), &AssetPackage::GetName),
    Method(NAME(HYP_STR(Rename)), &AssetPackage::Rename),
    Method(NAME(HYP_STR(GetFriendlyName)), &AssetPackage::GetFriendlyName),
    Method(NAME(HYP_STR(SetFriendlyName)), &AssetPackage::SetFriendlyName),
    Method(NAME(HYP_STR(GetFlags)), &AssetPackage::GetFlags),
    Method(NAME(HYP_STR(IsTransient)), &AssetPackage::IsTransient),
    Method(NAME(HYP_STR(IsHidden)), &AssetPackage::IsHidden),
    Method(NAME(HYP_STR(IsSubpackageOf)), &AssetPackage::IsSubpackageOf),
    Method(NAME(HYP_STR(BuildPackagePath)), &AssetPackage::BuildPackagePath),
    Method(NAME(HYP_STR(BuildAssetPath)), &AssetPackage::BuildAssetPath),
    Method(NAME(HYP_STR(HasAssetWithName)), &AssetPackage::HasAssetWithName),
    Method(NAME(HYP_STR(GetUniqueAssetName)), &AssetPackage::GetUniqueAssetName),
    Method(NAME(HYP_STR(Save)), &AssetPackage::Save),
    Method(NAME(HYP_STR(GetDependencies)), &AssetPackage::GetDependencies),
    Method(NAME(HYP_STR(GetRelativeDependencies)), &AssetPackage::GetRelativeDependencies, Span<const ClassAttribute> { {ClassAttribute("property", "Dependencies"), ClassAttribute("serialize", true) } }),
    Method(NAME(HYP_STR(SetRelativeDependencies)), &AssetPackage::SetRelativeDependencies, Span<const ClassAttribute> { {ClassAttribute("property", "Dependencies"), ClassAttribute("serialize", true) } }),
    Method(NAME(HYP_STR(AddDependency)), &AssetPackage::AddDependency),
    Method(NAME(HYP_STR(IsDirty)), &AssetPackage::IsDirty, Span<const ClassAttribute> { {ClassAttribute("property", "IsDirty"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(MarkDirty)), &AssetPackage::MarkDirty),
    Field(NAME(HYP_STR(Uuid)), &AssetPackage::m_uuid, offsetof(AssetPackage, m_uuid)),
    Field(NAME(HYP_STR(Name)), &AssetPackage::m_name, offsetof(AssetPackage, m_name)),
    Field(NAME(HYP_STR(FriendlyName)), &AssetPackage::m_friendlyName, offsetof(AssetPackage, m_friendlyName)),
    Field(NAME(HYP_STR(Flags)), &AssetPackage::m_flags, offsetof(AssetPackage, m_flags)),
    Field(NAME(HYP_STR(Dependencies)), &AssetPackage::m_dependencies, offsetof(AssetPackage, m_dependencies), Span<const ClassAttribute> { {ClassAttribute("transient", true) } })
HYP_END_CLASS

#pragma endregion AssetPackage Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AssetRegistry Reflection Data

HYP_BEGIN_CLASS(AssetRegistry, 27, 0, NAME("ObjectBase"))
    Method(NAME(HYP_STR(GetRootPath)), &AssetRegistry::GetRootPath),
    Method(NAME(HYP_STR(SetRootPath)), &AssetRegistry::SetRootPath),
    Method(NAME(HYP_STR(RemovePackage)), &AssetRegistry::RemovePackage),
    Method(NAME(HYP_STR(GetPackageFromPath)), &AssetRegistry::GetPackageFromPath),
    Method(NAME(HYP_STR(GetSubpackage)), &AssetRegistry::GetSubpackage),
    Method(NAME(HYP_STR(LoadSubpackages)), &AssetRegistry::LoadSubpackages),
    Method(NAME(HYP_STR(GetUniqueAssetName)), &AssetRegistry::GetUniqueAssetName),
    Field(NAME(HYP_STR(RootPath)), &AssetRegistry::m_rootPath, offsetof(AssetRegistry, m_rootPath), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } })
HYP_END_CLASS

#pragma endregion AssetRegistry Reflection Data

} // namespace hyperion

