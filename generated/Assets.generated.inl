#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region AssetChangeType Reflection Data

HYP_BEGIN_ENUM(AssetChangeType, 265, 0, {})
    HypConstant(NAME(HYP_STR(CHANGED)), AssetChangeType::CHANGED),
    HypConstant(NAME(HYP_STR(CREATED)), AssetChangeType::CREATED),
    HypConstant(NAME(HYP_STR(DELETED)), AssetChangeType::DELETED),
    HypConstant(NAME(HYP_STR(RENAMED)), AssetChangeType::RENAMED),
    HypConstant(NAME(HYP_STR(MAX)), AssetChangeType::MAX)
HYP_END_ENUM

#pragma endregion AssetChangeType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AssetLoaderDefinition Reflection Data

HYP_BEGIN_STRUCT(AssetLoaderDefinition, 266, 0, {})
HYP_END_STRUCT

#pragma endregion AssetLoaderDefinition Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AssetManager Reflection Data

HYP_BEGIN_CLASS(AssetManager, 55, 0, NAME("HypObjectBase"))
    HypMethod(NAME(HYP_STR(GetInstance)), &AssetManager::GetInstance),
    HypMethod(NAME(HYP_STR(GetBasePath)), &AssetManager::GetBasePath),
    HypMethod(NAME(HYP_STR(SetBasePath)), &AssetManager::SetBasePath),
    HypMethod(NAME(HYP_STR(GetBaseAssetCollector)), &AssetManager::GetBaseAssetCollector),
    HypMethod(NAME(HYP_STR(AddAssetCollector)), &AssetManager::AddAssetCollector),
    HypMethod(NAME(HYP_STR(RemoveAssetCollector)), &AssetManager::RemoveAssetCollector)
HYP_END_CLASS

#pragma endregion AssetManager Reflection Data

} // namespace hyperion

#include <scripting/ScriptObjectResource.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/Method.hpp>

namespace hyperion {

#pragma region AssetCollector Reflection Data

HYP_BEGIN_CLASS(AssetCollector, 56, 0, NAME("HypObjectBase"))
    HypMethod(NAME(HYP_STR(GetBasePath)), &AssetCollector::GetBasePath, Span<const HypClassAttribute> { {HypClassAttribute("property", "BasePath"), HypClassAttribute("serialize", true) } }),
    HypMethod(NAME(HYP_STR(SetBasePath)), &AssetCollector::SetBasePath, Span<const HypClassAttribute> { {HypClassAttribute("property", "BasePath"), HypClassAttribute("serialize", true) } }),
    HypMethod(NAME(HYP_STR(NotifyAssetChanged)), &AssetCollector::NotifyAssetChanged),
    HypMethod(NAME(HYP_STR(IsWatching)), &AssetCollector::IsWatching, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(StartWatching)), &AssetCollector::StartWatching, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(StopWatching)), &AssetCollector::StopWatching, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(OnAssetChanged)), &AssetCollector::OnAssetChanged, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } })
HYP_END_CLASS

#pragma endregion AssetCollector Reflection Data

#pragma region AssetCollector Scriptable Methods

bool AssetCollector::IsWatching() const
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("IsWatching");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<bool>(method_ptr);
        }
    }

    return IsWatching_Impl();
}
void AssetCollector::StartWatching()
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("StartWatching");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            managed_object->InvokeMethod<void>(method_ptr);
            return;
        }
    }

    StartWatching_Impl();
}
void AssetCollector::StopWatching()
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("StopWatching");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            managed_object->InvokeMethod<void>(method_ptr);
            return;
        }
    }

    StopWatching_Impl();
}
void AssetCollector::OnAssetChanged(const FilePath & path, AssetChangeType changeType)
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("OnAssetChanged");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            managed_object->InvokeMethod<void>(method_ptr, path, changeType);
            return;
        }
    }

    OnAssetChanged_Impl(path, changeType);
}
#pragma endregion AssetCollector Scriptable Methods
} // namespace hyperion

