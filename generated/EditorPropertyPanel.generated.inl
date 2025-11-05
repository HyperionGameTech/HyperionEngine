#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>
#include <scripting/ScriptObjectResource.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/ManagedMethod.hpp>

namespace hyperion {

#pragma region EditorPropertyPanelBase Reflection Data

HYP_BEGIN_CLASS(EditorPropertyPanelBase, 18, 1, NAME("UIPanel"), ClassAttribute("abstract", true))
    Method(NAME(HYP_STR(Build)), &EditorPropertyPanelBase::Build, Span<const ClassAttribute> { {ClassAttribute("scriptable", true) } })
HYP_END_CLASS

#pragma endregion EditorPropertyPanelBase Reflection Data

#pragma region EditorPropertyPanelBase Scriptable Methods

void EditorPropertyPanelBase::Build(const HypData & hypData, const Property * property)
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("Build");
        if (dotnet::ManagedMethod *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            managed_object->InvokeMethod<void>(method_ptr, hypData, property);
            return;
        }
    }

    Build_Impl(hypData, property);
}
#pragma endregion EditorPropertyPanelBase Scriptable Methods
} // namespace hyperion

