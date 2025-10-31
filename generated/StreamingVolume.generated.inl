#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region StreamingVolumeShape Reflection Data

HYP_BEGIN_ENUM(StreamingVolumeShape, 277, 0, {})
    HypConstant(NAME(HYP_STR(SPHERE)), StreamingVolumeShape::SPHERE),
    HypConstant(NAME(HYP_STR(BOX)), StreamingVolumeShape::BOX),
    HypConstant(NAME(HYP_STR(MAX)), StreamingVolumeShape::MAX),
    HypConstant(NAME(HYP_STR(INVALID)), StreamingVolumeShape::INVALID)
HYP_END_ENUM

#pragma endregion StreamingVolumeShape Reflection Data

} // namespace hyperion

#include <scripting/ScriptObjectResource.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/Method.hpp>

namespace hyperion {

#pragma region StreamingVolumeBase Reflection Data

HYP_BEGIN_CLASS(StreamingVolumeBase, 68, 1, NAME("HypObjectBase"), HypClassAttribute("abstract", true))
    HypMethod(NAME(HYP_STR(GetShape)), &StreamingVolumeBase::GetShape, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(GetBoundingBox)), &StreamingVolumeBase::GetBoundingBox, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(GetBoundingSphere)), &StreamingVolumeBase::GetBoundingSphere, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(ContainsPoint)), &StreamingVolumeBase::ContainsPoint, Span<const HypClassAttribute> { {HypClassAttribute("scriptable", true) } }),
    HypMethod(NAME(HYP_STR(GetShape_Impl)), &StreamingVolumeBase::GetShape_Impl),
    HypMethod(NAME(HYP_STR(GetBoundingBox_Impl)), &StreamingVolumeBase::GetBoundingBox_Impl),
    HypMethod(NAME(HYP_STR(GetBoundingSphere_Impl)), &StreamingVolumeBase::GetBoundingSphere_Impl),
    HypMethod(NAME(HYP_STR(ContainsPoint_Impl)), &StreamingVolumeBase::ContainsPoint_Impl),
    HypMethod(NAME(HYP_STR(NotifyUpdate)), &StreamingVolumeBase::NotifyUpdate)
HYP_END_CLASS

#pragma endregion StreamingVolumeBase Reflection Data

#pragma region StreamingVolumeBase Scriptable Methods

StreamingVolumeShape StreamingVolumeBase::GetShape() const
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("GetShape");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<StreamingVolumeShape>(method_ptr);
        }
    }

    return GetShape_Impl();
}
bool StreamingVolumeBase::GetBoundingBox(BoundingBox & outAabb) const
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("GetBoundingBox");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<bool>(method_ptr, outAabb);
        }
    }

    return GetBoundingBox_Impl(outAabb);
}
bool StreamingVolumeBase::GetBoundingSphere(BoundingSphere & outSphere) const
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("GetBoundingSphere");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<bool>(method_ptr, outSphere);
        }
    }

    return GetBoundingSphere_Impl(outSphere);
}
bool StreamingVolumeBase::ContainsPoint(const Vec3f & point) const
{
    if (ScriptObjectResource* managed_object_resource = GetScriptObjectResource(); managed_object_resource && managed_object_resource->GetManagedClass()) {
        constexpr HashCode hash_code = HashCode::GetHashCode("ContainsPoint");
        if (dotnet::Method *method_ptr = managed_object_resource->GetManagedClass()->GetMethodByHash(hash_code)) {
            TResourceHandle<ScriptObjectResource> resource_handle(*managed_object_resource);
            dotnet::ManagedObject *managed_object = managed_object_resource->GetManagedObject();

            return managed_object->InvokeMethod<bool>(method_ptr, point);
        }
    }

    return ContainsPoint_Impl(point);
}
#pragma endregion StreamingVolumeBase Scriptable Methods
} // namespace hyperion

