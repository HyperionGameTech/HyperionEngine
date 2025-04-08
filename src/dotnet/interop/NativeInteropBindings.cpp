/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Types.hpp>

#include <core/threading/Mutex.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/utilities/Pair.hpp>

#include <core/logging/Logger.hpp>

#include <core/object/HypClass.hpp>

#include <dotnet/Class.hpp>
#include <dotnet/Object.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/Method.hpp>

#include <dotnet/interop/ManagedGuid.hpp>
#include <dotnet/interop/ManagedObject.hpp>
#include <dotnet/interop/ManagedAttribute.hpp>

#include <dotnet/DotNetSystem.hpp>

using namespace hyperion;
using namespace hyperion::dotnet;

namespace hyperion {
HYP_DECLARE_LOG_CHANNEL(DotNET);
} // namespace hyperion

static AttributeSet InternManagedAttributeHolder(ManagedAttributeHolder *managed_attribute_holder_ptr)
{
    if (!managed_attribute_holder_ptr) {
        return { };
    }

    Array<Attribute> attributes;
    attributes.Reserve(managed_attribute_holder_ptr->managed_attributes_size);

    for (uint32 i = 0; i < managed_attribute_holder_ptr->managed_attributes_size; i++) {
        AssertThrow(managed_attribute_holder_ptr->managed_attributes_ptr[i].class_ptr != nullptr);

        attributes.PushBack(Attribute {
            MakeUnique<Object>(
                managed_attribute_holder_ptr->managed_attributes_ptr[i].class_ptr->RefCountedPtrFromThis(),
                managed_attribute_holder_ptr->managed_attributes_ptr[i].object_reference,
                ObjectFlags::CREATED_FROM_MANAGED
            )
        });
    }

    return AttributeSet(std::move(attributes));
}

extern "C" {
HYP_EXPORT bool NativeInterop_VerifyEngineVersion(uint32 assembly_engine_version, bool major, bool minor, bool patch)
{
    static constexpr uint32 major_mask = (0xffu << 16u);
    static constexpr uint32 minor_mask = (0xffu << 8u);
    static constexpr uint32 patch_mask = 0xffu;

    const uint32 mask = (major ? major_mask : 0u)
        | (minor ? minor_mask : 0u)
        | (patch ? patch_mask : 0u);

    const uint32 engine_version_major_minor = engine_version & mask;

    if ((assembly_engine_version & mask) != engine_version_major_minor) {
        HYP_LOG(DotNET, Error, "Assembly engine version mismatch: Assembly version: {}.{}.{}, Engine version: {}.{}.{}",
            (assembly_engine_version >> 16u) & 0xffu,
            (assembly_engine_version >> 8u) & 0xffu,
            assembly_engine_version & 0xffu,
            (engine_version >> 16u) & 0xffu,
            (engine_version >> 8u) & 0xffu,
            engine_version & 0xffu);

        return false;
    }

    return true;
}

HYP_EXPORT void NativeInterop_SetInvokeGetterFunction(ManagedGuid *assembly_guid, Assembly *assembly_ptr, InvokeGetterFunction invoke_getter_fptr)
{
    AssertThrow(assembly_ptr != nullptr);

    assembly_ptr->SetInvokeGetterFunction(invoke_getter_fptr);
}

HYP_EXPORT void NativeInterop_SetInvokeSetterFunction(ManagedGuid *assembly_guid, Assembly *assembly_ptr, InvokeSetterFunction invoke_setter_fptr)
{
    AssertThrow(assembly_ptr != nullptr);

    assembly_ptr->SetInvokeSetterFunction(invoke_setter_fptr);
}

HYP_EXPORT void NativeInterop_SetAddObjectToCacheFunction(AddObjectToCacheFunction add_object_to_cache_fptr)
{
    DotNetSystem::GetInstance().GetGlobalFunctions().add_object_to_cache_function = add_object_to_cache_fptr;
}

HYP_EXPORT void NativeInterop_SetSetObjectReferenceTypeFunction(SetObjectReferenceTypeFunction set_object_reference_type_function)
{
    DotNetSystem::GetInstance().GetGlobalFunctions().set_object_reference_type_function = set_object_reference_type_function;
}

HYP_EXPORT void NativeInterop_SetTriggerGCFunction(TriggerGCFunction trigger_gc_function)
{
    DotNetSystem::GetInstance().GetGlobalFunctions().trigger_gc_function = trigger_gc_function;
}

HYP_EXPORT void NativeInterop_AddObjectToCache(void *ptr, Class **out_class_object_ptr, ObjectReference *out_object_reference, int8 weak)
{
    AssertThrow(ptr != nullptr);
    AssertThrow(out_class_object_ptr != nullptr);
    AssertThrow(out_object_reference != nullptr);
    
    DotNetSystem::GetInstance().GetGlobalFunctions().add_object_to_cache_function(ptr, out_class_object_ptr, out_object_reference, weak);
}

HYP_EXPORT void ManagedClass_Create(ManagedGuid *assembly_guid, Assembly *assembly_ptr, const HypClass *hyp_class, int32 type_hash, const char *type_name, uint32 type_size, TypeID type_id, Class *parent_class, uint32 flags, ManagedClass *out_managed_class)
{
    AssertThrow(assembly_guid != nullptr);
    AssertThrow(assembly_ptr != nullptr);

    HYP_LOG(DotNET, Info, "Registering .NET managed class {}", type_name);

    RC<Class> class_object = assembly_ptr->NewClass(hyp_class, type_hash, type_name, type_size, type_id, parent_class, flags);

    ManagedClass &managed_class = *out_managed_class;
    managed_class = { };
    managed_class.type_hash = type_hash;
    managed_class.class_object = class_object.Get();
    managed_class.assembly_guid = *assembly_guid;
    managed_class.flags = flags;
}

HYP_EXPORT int8 ManagedClass_FindByTypeHash(Assembly *assembly_ptr, int32 type_hash, Class **out_managed_class_object_ptr)
{
    AssertThrow(assembly_ptr != nullptr);

    AssertThrow(out_managed_class_object_ptr != nullptr);

    RC<Class> class_object = assembly_ptr->FindClassByTypeHash(type_hash);

    if (!class_object) {
        *out_managed_class_object_ptr = nullptr;

        return 0;
    }

    *out_managed_class_object_ptr = class_object;

    return 1;
}

HYP_EXPORT void ManagedClass_SetAttributes(ManagedClass *managed_class, ManagedAttributeHolder *managed_attribute_holder_ptr)
{
    AssertThrow(managed_class != nullptr);

    if (!managed_class->class_object || !managed_attribute_holder_ptr) {
        return;
    }

    AttributeSet attributes = InternManagedAttributeHolder(managed_attribute_holder_ptr);

    managed_class->class_object->SetAttributes(std::move(attributes));
}

HYP_EXPORT void ManagedClass_AddMethod(ManagedClass *managed_class, const char *method_name, ManagedGuid guid, InvokeMethodFunction invoke_fptr, ManagedAttributeHolder *managed_attribute_holder_ptr)
{
    AssertThrow(managed_class != nullptr);
    AssertThrow(invoke_fptr != nullptr);

    if (!managed_class->class_object || !method_name) {
        return;
    }

    AttributeSet attributes = InternManagedAttributeHolder(managed_attribute_holder_ptr);

    if (managed_class->class_object->HasMethod(method_name)) {
        HYP_LOG(DotNET, Error, "Class '{}' already has a method named '{}'!", managed_class->class_object->GetName(), method_name);

        return;
    }

    managed_class->class_object->AddMethod(
        method_name,
        Method(guid, invoke_fptr, std::move(attributes))
    );
}

HYP_EXPORT void ManagedClass_AddProperty(ManagedClass *managed_class, const char *property_name, ManagedGuid guid, ManagedAttributeHolder *managed_attribute_holder_ptr)
{
    AssertThrow(managed_class != nullptr);

    if (!managed_class->class_object || !property_name) {
        return;
    }

    AttributeSet attributes = InternManagedAttributeHolder(managed_attribute_holder_ptr);

    if (managed_class->class_object->HasProperty(property_name)) {
        HYP_LOG(DotNET, Error, "Class '{}' already has a property named '{}'!", managed_class->class_object->GetName(), property_name);

        return;
    }

    managed_class->class_object->AddProperty(
        property_name,
        Property(guid, std::move(attributes))
    );
}

HYP_EXPORT void ManagedClass_SetNewObjectFunction(ManagedClass *managed_class, Class::NewObjectFunction new_object_fptr)
{
    AssertThrow(managed_class != nullptr);
    AssertThrow(managed_class->class_object != nullptr);

    managed_class->class_object->SetNewObjectFunction(new_object_fptr);
}

HYP_EXPORT void ManagedClass_SetFreeObjectFunction(ManagedClass *managed_class, Class::FreeObjectFunction free_object_fptr)
{
    AssertThrow(managed_class != nullptr);
    AssertThrow(managed_class->class_object != nullptr);

    managed_class->class_object->SetFreeObjectFunction(free_object_fptr);
}

HYP_EXPORT void ManagedClass_SetMarshalObjectFunction(ManagedClass *managed_class, Class::MarshalObjectFunction marshal_object_fptr)
{
    AssertThrow(managed_class != nullptr);
    AssertThrow(managed_class->class_object != nullptr);

    managed_class->class_object->SetMarshalObjectFunction(marshal_object_fptr);
}

} // extern "C"
