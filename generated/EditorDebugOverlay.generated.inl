#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region TextureEditorDebugOverlay Reflection Data

HYP_BEGIN_CLASS(TextureEditorDebugOverlay, 212, 0, NAME("EditorDebugOverlayBase"))
HYP_END_CLASS

#pragma endregion TextureEditorDebugOverlay Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region TextEditorDebugOverlay Reflection Data

HYP_BEGIN_CLASS(TextEditorDebugOverlay, 213, 0, NAME("EditorDebugOverlayBase"))
HYP_END_CLASS

#pragma endregion TextEditorDebugOverlay Reflection Data

} // namespace hyperion

#include <scripting/ScriptObjectResource.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/ManagedMethod.hpp>

namespace hyperion {

#pragma region EditorDebugOverlayBase Reflection Data

HYP_BEGIN_CLASS(EditorDebugOverlayBase, 209, 4, NAME("ObjectBase"), ClassAttribute("abstract", true))
    Method(NAME(HYP_STR(GetUIObject)), &EditorDebugOverlayBase::GetUIObject),
    Method(NAME(HYP_STR(GetPlacement)), &EditorDebugOverlayBase::GetPlacement, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(Update)), &EditorDebugOverlayBase::Update, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(CreateUIObject)), &EditorDebugOverlayBase::CreateUIObject, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(GetName)), &EditorDebugOverlayBase::GetName, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(IsEnabled)), &EditorDebugOverlayBase::IsEnabled, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(GetPlacement_Impl)), &EditorDebugOverlayBase::GetPlacement_Impl),
    Method(NAME(HYP_STR(Update_Impl)), &EditorDebugOverlayBase::Update_Impl),
    Method(NAME(HYP_STR(GetName_Impl)), &EditorDebugOverlayBase::GetName_Impl),
    Method(NAME(HYP_STR(IsEnabled_Impl)), &EditorDebugOverlayBase::IsEnabled_Impl)
HYP_END_CLASS

#pragma endregion EditorDebugOverlayBase Reflection Data

#pragma region EditorDebugOverlayBase Scriptable Methods

int EditorDebugOverlayBase::GetPlacement() const
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("GetPlacement");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<int>(method_ptr);
        }
    }

    return GetPlacement_Impl();
}
void EditorDebugOverlayBase::Update(float delta)
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("Update");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            managed_object->InvokeMethod<void>(method_ptr, delta);
            return;
        }
    }

    Update_Impl(delta);
}
Handle<UIObject> EditorDebugOverlayBase::CreateUIObject(UIObject * spawnParent)
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("CreateUIObject");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<Handle<UIObject>>(method_ptr, spawnParent);
        }
    }

    return CreateUIObject_Impl(spawnParent);
}
Name EditorDebugOverlayBase::GetName() const
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("GetName");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<Name>(method_ptr);
        }
    }

    return GetName_Impl();
}
bool EditorDebugOverlayBase::IsEnabled() const
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("IsEnabled");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<bool>(method_ptr);
        }
    }

    return IsEnabled_Impl();
}
#pragma endregion EditorDebugOverlayBase Scriptable Methods
} // namespace hyperion

