#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>
#include <scripting/ScriptObjectResource.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/ManagedMethod.hpp>

namespace hyperion {

#pragma region EditorActionBase Reflection Data

HYP_BEGIN_CLASS(EditorActionBase, 34, 1, NAME("HypObjectBase"), ClassAttribute("abstract", true))
    Method(NAME(HYP_STR(GetName)), &EditorActionBase::GetName, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(Execute)), &EditorActionBase::Execute, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } }),
    Method(NAME(HYP_STR(Revert)), &EditorActionBase::Revert, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } })
HYP_END_CLASS

#pragma endregion EditorActionBase Reflection Data

#pragma region EditorActionBase Scriptable Methods

Name EditorActionBase::GetName() const
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
void EditorActionBase::Execute(EditorSubsystem * editorSubsystem, EditorProject * project)
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("Execute");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            managed_object->InvokeMethod<void>(method_ptr, editorSubsystem, project);
            return;
        }
    }

    Execute_Impl(editorSubsystem, project);
}
void EditorActionBase::Revert(EditorSubsystem * editorSubsystem, EditorProject * project)
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("Revert");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            managed_object->InvokeMethod<void>(method_ptr, editorSubsystem, project);
            return;
        }
    }

    Revert_Impl(editorSubsystem, project);
}
#pragma endregion EditorActionBase Scriptable Methods
} // namespace hyperion


namespace hyperion {

#pragma region FunctionalEditorAction Reflection Data

HYP_BEGIN_CLASS(FunctionalEditorAction, 35, 0, NAME("EditorActionBase"))
    Method(NAME(HYP_STR(GetName)), &FunctionalEditorAction::GetName),
    Method(NAME(HYP_STR(Execute)), &FunctionalEditorAction::Execute),
    Method(NAME(HYP_STR(Revert)), &FunctionalEditorAction::Revert)
HYP_END_CLASS

#pragma endregion FunctionalEditorAction Reflection Data

} // namespace hyperion

