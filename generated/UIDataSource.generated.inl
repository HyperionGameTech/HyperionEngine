#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region UIDataSource Reflection Data

HYP_BEGIN_CLASS(UIDataSource, 192, 0, NAME("UIDataSourceBase"))
    HypMethod(NAME(HYP_STR(Size)), &UIDataSource::Size),
    HypMethod(NAME(HYP_STR(Clear)), &UIDataSource::Clear)
HYP_END_CLASS

#pragma endregion UIDataSource Reflection Data

} // namespace hyperion

#include <scripting/ScriptObjectResource.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/Method.hpp>

namespace hyperion {

#pragma region UIElementFactoryBase Reflection Data

HYP_BEGIN_CLASS(UIElementFactoryBase, 190, 0, NAME("HypObjectBase"), HypClassAttribute("abstract", true))
    HypMethod(NAME(HYP_STR(GetElementTypeId)), &UIElementFactoryBase::GetElementTypeId, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(CreateUIObject)), &UIElementFactoryBase::CreateUIObject, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(UpdateUIObject)), &UIElementFactoryBase::UpdateUIObject, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } })
HYP_END_CLASS

#pragma endregion UIElementFactoryBase Reflection Data

#pragma region UIElementFactoryBase Scriptable Methods

TypeId UIElementFactoryBase::GetElementTypeId() const
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("GetElementTypeId");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<TypeId>(method_ptr);
        }
    }

    return GetElementTypeId_Impl();
}
Handle<UIObject> UIElementFactoryBase::CreateUIObject(UIObject * parent, const HypData & value, const HypData & context) const
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("CreateUIObject");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<Handle<UIObject>>(method_ptr, parent, value, context);
        }
    }

    return CreateUIObject_Impl(parent, value, context);
}
void UIElementFactoryBase::UpdateUIObject(UIObject * uiObject, const HypData & value, const HypData & context) const
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("UpdateUIObject");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            managed_object->InvokeMethod<void>(method_ptr, uiObject, value, context);
            return;
        }
    }

    UpdateUIObject_Impl(uiObject, value, context);
}
#pragma endregion UIElementFactoryBase Scriptable Methods
} // namespace hyperion


namespace hyperion {

#pragma region UIDataSourceBase Reflection Data

HYP_BEGIN_CLASS(UIDataSourceBase, 191, 1, NAME("HypObjectBase"), HypClassAttribute("abstract", true))
    HypMethod(NAME(HYP_STR(Size)), &UIDataSourceBase::Size),
    HypMethod(NAME(HYP_STR(Clear)), &UIDataSourceBase::Clear)
HYP_END_CLASS

#pragma endregion UIDataSourceBase Reflection Data

} // namespace hyperion

