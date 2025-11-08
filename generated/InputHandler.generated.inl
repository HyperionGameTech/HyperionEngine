#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>
#include <scripting/ScriptObjectResource.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/ManagedMethod.hpp>

namespace hyperion {

#pragma region InputHandlerBase Reflection Data

HYP_BEGIN_CLASS(InputHandlerBase, 55, 3, NAME("ObjectBase"), ClassAttribute("abstract", true))
    Method(NAME(HYP_STR(OnKeyDown)), &InputHandlerBase::OnKeyDown, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(OnKeyUp)), &InputHandlerBase::OnKeyUp, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(OnMouseDown)), &InputHandlerBase::OnMouseDown, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(OnMouseUp)), &InputHandlerBase::OnMouseUp, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(OnMouseMove)), &InputHandlerBase::OnMouseMove, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(OnMouseDrag)), &InputHandlerBase::OnMouseDrag, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(OnMouseLeave)), &InputHandlerBase::OnMouseLeave, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(OnClick)), &InputHandlerBase::OnClick, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(OnKeyDown_Impl)), &InputHandlerBase::OnKeyDown_Impl),
    Method(NAME(HYP_STR(OnKeyUp_Impl)), &InputHandlerBase::OnKeyUp_Impl),
    Method(NAME(HYP_STR(OnMouseDown_Impl)), &InputHandlerBase::OnMouseDown_Impl),
    Method(NAME(HYP_STR(OnMouseUp_Impl)), &InputHandlerBase::OnMouseUp_Impl),
    Method(NAME(HYP_STR(OnMouseMove_Impl)), &InputHandlerBase::OnMouseMove_Impl),
    Method(NAME(HYP_STR(OnMouseDrag_Impl)), &InputHandlerBase::OnMouseDrag_Impl),
    Method(NAME(HYP_STR(OnMouseLeave_Impl)), &InputHandlerBase::OnMouseLeave_Impl),
    Method(NAME(HYP_STR(OnClick_Impl)), &InputHandlerBase::OnClick_Impl)
HYP_END_CLASS

#pragma endregion InputHandlerBase Reflection Data

#pragma region InputHandlerBase Scriptable Methods

bool InputHandlerBase::OnKeyDown(const KeyboardEvent & evt)
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("OnKeyDown");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<bool>(method_ptr, evt);
        }
    }

    return OnKeyDown_Impl(evt);
}
bool InputHandlerBase::OnKeyUp(const KeyboardEvent & evt)
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("OnKeyUp");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<bool>(method_ptr, evt);
        }
    }

    return OnKeyUp_Impl(evt);
}
bool InputHandlerBase::OnMouseDown(const MouseEvent & evt)
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("OnMouseDown");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<bool>(method_ptr, evt);
        }
    }

    return OnMouseDown_Impl(evt);
}
bool InputHandlerBase::OnMouseUp(const MouseEvent & evt)
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("OnMouseUp");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<bool>(method_ptr, evt);
        }
    }

    return OnMouseUp_Impl(evt);
}
bool InputHandlerBase::OnMouseMove(const MouseEvent & evt)
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("OnMouseMove");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<bool>(method_ptr, evt);
        }
    }

    return OnMouseMove_Impl(evt);
}
bool InputHandlerBase::OnMouseDrag(const MouseEvent & evt)
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("OnMouseDrag");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<bool>(method_ptr, evt);
        }
    }

    return OnMouseDrag_Impl(evt);
}
bool InputHandlerBase::OnMouseLeave(const MouseEvent & evt)
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("OnMouseLeave");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<bool>(method_ptr, evt);
        }
    }

    return OnMouseLeave_Impl(evt);
}
bool InputHandlerBase::OnClick(const MouseEvent & evt)
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("OnClick");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<bool>(method_ptr, evt);
        }
    }

    return OnClick_Impl(evt);
}
#pragma endregion InputHandlerBase Scriptable Methods
} // namespace hyperion


namespace hyperion {

#pragma region NullInputHandler Reflection Data

HYP_BEGIN_CLASS(NullInputHandler, 57, 0, NAME("InputHandlerBase"))
HYP_END_CLASS

#pragma endregion NullInputHandler Reflection Data

} // namespace hyperion

