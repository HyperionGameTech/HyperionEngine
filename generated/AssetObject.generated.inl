#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region AssetObject Reflection Data

HYP_BEGIN_CLASS(AssetObject, 45, 9, NAME("HypObjectBase"), HypClassAttribute("abstract", true))
    HypMethod(NAME(HYP_STR(GetUUID)), &AssetObject::GetUUID),
    HypMethod(NAME(HYP_STR(GetName)), &AssetObject::GetName),
    HypMethod(NAME(HYP_STR(Rename)), &AssetObject::Rename),
    HypMethod(NAME(HYP_STR(GetFriendlyName)), &AssetObject::GetFriendlyName, Span<const HypClassAttribute> { {HypClassAttribute("property", "FriendlyName") } }),
    HypMethod(NAME(HYP_STR(SetFriendlyName)), &AssetObject::SetFriendlyName, Span<const HypClassAttribute> { {HypClassAttribute("property", "FriendlyName") } }),
    HypMethod(NAME(HYP_STR(GetOriginalFilepath)), &AssetObject::GetOriginalFilepath),
    HypMethod(NAME(HYP_STR(SetOriginalFilepath)), &AssetObject::SetOriginalFilepath),
    HypMethod(NAME(HYP_STR(GetPackage)), &AssetObject::GetPackage),
    HypMethod(NAME(HYP_STR(GetPath)), &AssetObject::GetPath),
    HypMethod(NAME(HYP_STR(IsRegistered)), &AssetObject::IsRegistered),
    HypMethod(NAME(HYP_STR(GetAssetFlags)), &AssetObject::GetAssetFlags),
    HypMethod(NAME(HYP_STR(SetAssetFlags)), &AssetObject::SetAssetFlags),
    HypMethod(NAME(HYP_STR(IsPersistentlyLoaded)), &AssetObject::IsPersistentlyLoaded),
    HypMethod(NAME(HYP_STR(SetIsPersistentlyLoaded)), &AssetObject::SetIsPersistentlyLoaded),
    HypMethod(NAME(HYP_STR(IsTransient)), &AssetObject::IsTransient),
    HypMethod(NAME(HYP_STR(SetIsTransient)), &AssetObject::SetIsTransient),
    HypMethod(NAME(HYP_STR(SetIsTransientByProxy)), &AssetObject::SetIsTransientByProxy),
    HypMethod(NAME(HYP_STR(IsLoaded)), &AssetObject::IsLoaded),
    HypMethod(NAME(HYP_STR(Save)), &AssetObject::Save),
    HypField(NAME(HYP_STR(Uuid)), &AssetObject::m_uuid, offsetof(AssetObject, m_uuid)),
    HypField(NAME(HYP_STR(Name)), &AssetObject::m_name, offsetof(AssetObject, m_name)),
    HypField(NAME(HYP_STR(FriendlyName)), &AssetObject::m_friendlyName, offsetof(AssetObject, m_friendlyName), Span<const HypClassAttribute> { {HypClassAttribute("property", "FriendlyName") } }),
    HypField(NAME(HYP_STR(Flags)), &AssetObject::m_flags, offsetof(AssetObject, m_flags)),
    HypField(NAME(HYP_STR(OriginalFilepath)), &AssetObject::m_originalFilepath, offsetof(AssetObject, m_originalFilepath)),
    HypField(NAME(HYP_STR(Package)), &AssetObject::m_package, offsetof(AssetObject, m_package), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(Resource)), &AssetObject::m_resource, offsetof(AssetObject, m_resource), Span<const HypClassAttribute> { {HypClassAttribute("noscriptbindings", true), HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(AssetPath)), &AssetObject::m_assetPath, offsetof(AssetObject, m_assetPath), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(ManifestPath)), &AssetObject::m_manifestPath, offsetof(AssetObject, m_manifestPath), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(Pool)), &AssetObject::m_pool, offsetof(AssetObject, m_pool), Span<const HypClassAttribute> { {HypClassAttribute("noscriptbindings", true), HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(PersistentResource)), &AssetObject::m_persistentResource, offsetof(AssetObject, m_persistentResource), Span<const HypClassAttribute> { {HypClassAttribute("noscriptbindings", true), HypClassAttribute("transient", true) } })
HYP_END_CLASS

#pragma endregion AssetObject Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AssetObjectFlags Reflection Data

HYP_BEGIN_ENUM(AssetObjectFlags, 261, 0, {})
    HypConstant(NAME(HYP_STR(AOF_NONE)), AssetObjectFlags::AOF_NONE),
    HypConstant(NAME(HYP_STR(AOF_PERSISTENT)), AssetObjectFlags::AOF_PERSISTENT),
    HypConstant(NAME(HYP_STR(AOF_TRANSIENT)), AssetObjectFlags::AOF_TRANSIENT),
    HypConstant(NAME(HYP_STR(AOF_TRANSIENT_BY_PROXY)), AssetObjectFlags::AOF_TRANSIENT_BY_PROXY)
HYP_END_ENUM

#pragma endregion AssetObjectFlags Reflection Data

} // namespace hyperion

