#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region AssetObject Reflection Data

HYP_BEGIN_CLASS(AssetObject, 45, 9, NAME("HypObjectBase"), ClassAttribute("abstract", true))
    Method(NAME(HYP_STR(GetUUID)), &AssetObject::GetUUID),
    Method(NAME(HYP_STR(GetName)), &AssetObject::GetName),
    Method(NAME(HYP_STR(Rename)), &AssetObject::Rename),
    Method(NAME(HYP_STR(GetFriendlyName)), &AssetObject::GetFriendlyName, Span<const ClassAttribute> { {ClassAttribute("property", "FriendlyName") } }),
    Method(NAME(HYP_STR(SetFriendlyName)), &AssetObject::SetFriendlyName, Span<const ClassAttribute> { {ClassAttribute("property", "FriendlyName") } }),
    Method(NAME(HYP_STR(GetOriginalFilepath)), &AssetObject::GetOriginalFilepath),
    Method(NAME(HYP_STR(SetOriginalFilepath)), &AssetObject::SetOriginalFilepath),
    Method(NAME(HYP_STR(GetPackage)), &AssetObject::GetPackage),
    Method(NAME(HYP_STR(GetPath)), &AssetObject::GetPath),
    Method(NAME(HYP_STR(IsRegistered)), &AssetObject::IsRegistered),
    Method(NAME(HYP_STR(GetAssetFlags)), &AssetObject::GetAssetFlags),
    Method(NAME(HYP_STR(SetAssetFlags)), &AssetObject::SetAssetFlags),
    Method(NAME(HYP_STR(IsPersistentlyLoaded)), &AssetObject::IsPersistentlyLoaded),
    Method(NAME(HYP_STR(SetIsPersistentlyLoaded)), &AssetObject::SetIsPersistentlyLoaded),
    Method(NAME(HYP_STR(IsTransient)), &AssetObject::IsTransient),
    Method(NAME(HYP_STR(SetIsTransient)), &AssetObject::SetIsTransient),
    Method(NAME(HYP_STR(SetIsTransientByProxy)), &AssetObject::SetIsTransientByProxy),
    Method(NAME(HYP_STR(IsLoaded)), &AssetObject::IsLoaded),
    Method(NAME(HYP_STR(Save)), &AssetObject::Save),
    Field(NAME(HYP_STR(Uuid)), &AssetObject::m_uuid, offsetof(AssetObject, m_uuid)),
    Field(NAME(HYP_STR(Name)), &AssetObject::m_name, offsetof(AssetObject, m_name)),
    Field(NAME(HYP_STR(FriendlyName)), &AssetObject::m_friendlyName, offsetof(AssetObject, m_friendlyName), Span<const ClassAttribute> { {ClassAttribute("property", "FriendlyName") } }),
    Field(NAME(HYP_STR(Flags)), &AssetObject::m_flags, offsetof(AssetObject, m_flags)),
    Field(NAME(HYP_STR(OriginalFilepath)), &AssetObject::m_originalFilepath, offsetof(AssetObject, m_originalFilepath)),
    Field(NAME(HYP_STR(Package)), &AssetObject::m_package, offsetof(AssetObject, m_package), Span<const ClassAttribute> { {ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(Resource)), &AssetObject::m_resource, offsetof(AssetObject, m_resource), Span<const ClassAttribute> { {ClassAttribute("noscriptbindings", true), ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(AssetPath)), &AssetObject::m_assetPath, offsetof(AssetObject, m_assetPath), Span<const ClassAttribute> { {ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(ManifestPath)), &AssetObject::m_manifestPath, offsetof(AssetObject, m_manifestPath), Span<const ClassAttribute> { {ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(Pool)), &AssetObject::m_pool, offsetof(AssetObject, m_pool), Span<const ClassAttribute> { {ClassAttribute("noscriptbindings", true), ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(PersistentResource)), &AssetObject::m_persistentResource, offsetof(AssetObject, m_persistentResource), Span<const ClassAttribute> { {ClassAttribute("noscriptbindings", true), ClassAttribute("transient", true) } })
HYP_END_CLASS

#pragma endregion AssetObject Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AssetObjectFlags Reflection Data

HYP_BEGIN_ENUM(AssetObjectFlags, 262, 0, {})
    StaticField(NAME(HYP_STR(AOF_NONE)), AssetObjectFlags::AOF_NONE),
    StaticField(NAME(HYP_STR(AOF_PERSISTENT)), AssetObjectFlags::AOF_PERSISTENT),
    StaticField(NAME(HYP_STR(AOF_TRANSIENT)), AssetObjectFlags::AOF_TRANSIENT),
    StaticField(NAME(HYP_STR(AOF_TRANSIENT_BY_PROXY)), AssetObjectFlags::AOF_TRANSIENT_BY_PROXY)
HYP_END_ENUM

#pragma endregion AssetObjectFlags Reflection Data

} // namespace hyperion

