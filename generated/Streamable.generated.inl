#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>
#include <scripting/ScriptObjectResource.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/ManagedMethod.hpp>

namespace hyperion {

#pragma region StreamableBase Reflection Data

HYP_BEGIN_CLASS(StreamableBase, 179, 2, NAME("ObjectBase"), ClassAttribute("abstract", true))
    Method(NAME(HYP_STR(GetKey)), &StreamableBase::GetKey),
    Method(NAME(HYP_STR(GetBoundingBox)), &StreamableBase::GetBoundingBox, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(OnStreamStart)), &StreamableBase::OnStreamStart, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(OnLoaded)), &StreamableBase::OnLoaded, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(OnRemoved)), &StreamableBase::OnRemoved, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(GetBoundingBox_Impl)), &StreamableBase::GetBoundingBox_Impl),
    Method(NAME(HYP_STR(OnStreamStart_Impl)), &StreamableBase::OnStreamStart_Impl),
    Method(NAME(HYP_STR(OnLoaded_Impl)), &StreamableBase::OnLoaded_Impl),
    Method(NAME(HYP_STR(OnRemoved_Impl)), &StreamableBase::OnRemoved_Impl)
HYP_END_CLASS

#pragma endregion StreamableBase Reflection Data

#pragma region StreamableBase Scriptable Methods

BoundingBox StreamableBase::GetBoundingBox() const
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("GetBoundingBox");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<BoundingBox>(method_ptr);
        }
    }

    return GetBoundingBox_Impl();
}
void StreamableBase::OnStreamStart()
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("OnStreamStart");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            managed_object->InvokeMethod<void>(method_ptr);
            return;
        }
    }

    OnStreamStart_Impl();
}
void StreamableBase::OnLoaded()
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("OnLoaded");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            managed_object->InvokeMethod<void>(method_ptr);
            return;
        }
    }

    OnLoaded_Impl();
}
void StreamableBase::OnRemoved()
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("OnRemoved");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            managed_object->InvokeMethod<void>(method_ptr);
            return;
        }
    }

    OnRemoved_Impl();
}
#pragma endregion StreamableBase Scriptable Methods
} // namespace hyperion


namespace hyperion {

#pragma region StreamableKey Reflection Data

HYP_BEGIN_STRUCT(StreamableKey, 399, 0, {})
    Field(NAME(HYP_STR(Uuid)), &StreamableKey::uuid, offsetof(StreamableKey, uuid), Span<const ClassAttribute> { {ClassAttribute("property", "Uuid"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(AssetPath)), &StreamableKey::assetPath, offsetof(StreamableKey, assetPath), Span<const ClassAttribute> { {ClassAttribute("property", "AssetPath"), ClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion StreamableKey Reflection Data

} // namespace hyperion

