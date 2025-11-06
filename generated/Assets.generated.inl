#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region AssetChangeType Reflection Data

HYP_BEGIN_ENUM(AssetChangeType, 266, 0, {})
    StaticField(NAME(HYP_STR(CHANGED)), AssetChangeType::CHANGED),
    StaticField(NAME(HYP_STR(CREATED)), AssetChangeType::CREATED),
    StaticField(NAME(HYP_STR(DELETED)), AssetChangeType::DELETED),
    StaticField(NAME(HYP_STR(RENAMED)), AssetChangeType::RENAMED),
    StaticField(NAME(HYP_STR(MAX)), AssetChangeType::MAX)
HYP_END_ENUM

#pragma endregion AssetChangeType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AssetLoaderDefinition Reflection Data

HYP_BEGIN_STRUCT(AssetLoaderDefinition, 267, 0, {})
HYP_END_STRUCT

#pragma endregion AssetLoaderDefinition Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AssetManager Reflection Data

HYP_BEGIN_CLASS(AssetManager, 55, 0, NAME("ObjectBase"))
    Method(NAME(HYP_STR(GetInstance)), &AssetManager::GetInstance),
    Method(NAME(HYP_STR(GetBasePath)), &AssetManager::GetBasePath),
    Method(NAME(HYP_STR(SetBasePath)), &AssetManager::SetBasePath),
    Method(NAME(HYP_STR(GetBaseAssetCollector)), &AssetManager::GetBaseAssetCollector),
    Method(NAME(HYP_STR(AddAssetCollector)), &AssetManager::AddAssetCollector),
    Method(NAME(HYP_STR(RemoveAssetCollector)), &AssetManager::RemoveAssetCollector)
HYP_END_CLASS

#pragma endregion AssetManager Reflection Data

} // namespace hyperion

#include <scripting/ScriptObjectResource.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/ManagedMethod.hpp>

namespace hyperion {

#pragma region AssetCollector Reflection Data

HYP_BEGIN_CLASS(AssetCollector, 56, 0, NAME("ObjectBase"))
    Method(NAME(HYP_STR(GetBasePath)), &AssetCollector::GetBasePath, Span<const ClassAttribute> { {ClassAttribute("property", "BasePath"), ClassAttribute("serialize", true) } }),
    Method(NAME(HYP_STR(SetBasePath)), &AssetCollector::SetBasePath, Span<const ClassAttribute> { {ClassAttribute("property", "BasePath"), ClassAttribute("serialize", true) } }),
    Method(NAME(HYP_STR(NotifyAssetChanged)), &AssetCollector::NotifyAssetChanged),
    Method(NAME(HYP_STR(IsWatching)), &AssetCollector::IsWatching, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(StartWatching)), &AssetCollector::StartWatching, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(StopWatching)), &AssetCollector::StopWatching, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(OnAssetChanged)), &AssetCollector::OnAssetChanged, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } })
HYP_END_CLASS

#pragma endregion AssetCollector Reflection Data

#pragma region AssetCollector Scriptable Methods

bool AssetCollector::IsWatching() const
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("IsWatching");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
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
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
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
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
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
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
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

