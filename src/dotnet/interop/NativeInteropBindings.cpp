/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Engine.hpp>
#include <Types.hpp>

#include <core/threading/Mutex.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/memory/UniquePtr.hpp>
#include <core/utilities/Pair.hpp>
#include <core/logging/Logger.hpp>

#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/interop/ManagedMethod.hpp>
#include <dotnet/interop/ManagedGuid.hpp>
#include <dotnet/interop/ManagedObject.hpp>
#include <dotnet/DotNetSystem.hpp>

using namespace hyperion;
using namespace hyperion::dotnet;

namespace hyperion {
HYP_DECLARE_LOG_CHANNEL(DotNET);
} // namespace hyperion

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
        HYP_LOG(DotNET, LogLevel::ERROR, "Assembly engine version mismatch: Assembly version: {}.{}.{}, Engine version: {}.{}.{}",
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

HYP_EXPORT void NativeInterop_SetInvokeMethodFunction(ManagedGuid *assembly_guid, ClassHolder *class_holder, ClassHolder::InvokeMethodFunction invoke_method_fptr)
{
    AssertThrow(class_holder != nullptr);

    class_holder->SetInvokeMethodFunction(invoke_method_fptr);

    // @TODO: Store the assembly guid somewhere
}

HYP_EXPORT void ManagedClass_Create(ManagedGuid *assembly_guid, ClassHolder *class_holder, int32 type_hash, const char *type_name, ManagedClass *out_managed_class)
{
    AssertThrow(assembly_guid != nullptr);
    AssertThrow(class_holder != nullptr);

    Class *class_object = class_holder->GetOrCreateClassObject(type_hash, type_name);

    *out_managed_class = ManagedClass { type_hash, class_object, *assembly_guid };
}

HYP_EXPORT void ManagedClass_AddMethod(ManagedClass managed_class, const char *method_name, ManagedGuid guid, uint32 num_attributes, const char **attribute_names)
{
    if (!managed_class.class_object || !method_name) {
        return;
    }

    ManagedMethod method_object;
    method_object.guid = guid;

    if (num_attributes != 0) {
        method_object.attribute_names.Reserve(num_attributes);

        for (uint32 i = 0; i < num_attributes; i++) {
            method_object.attribute_names.PushBack(attribute_names[i]);
        }
    }

    if (managed_class.class_object->HasMethod(method_name)) {
        HYP_LOG(DotNET, LogLevel::ERROR, "Class '{}' already has a method named '{}'!", managed_class.class_object->GetName(), method_name);

        return;
    }

    managed_class.class_object->AddMethod(
        method_name,
        std::move(method_object)
    );
}

HYP_EXPORT void ManagedClass_SetNewObjectFunction(ManagedClass managed_class, Class::NewObjectFunction new_object_fptr)
{
    if (!managed_class.class_object) {
        return;
    }

    managed_class.class_object->SetNewObjectFunction(new_object_fptr);
}

HYP_EXPORT void ManagedClass_SetFreeObjectFunction(ManagedClass managed_class, Class::FreeObjectFunction free_object_fptr)
{
    if (!managed_class.class_object) {
        return;
    }

    managed_class.class_object->SetFreeObjectFunction(free_object_fptr);
}

HYP_EXPORT void NativeInterop_AddMethodToCache(ManagedGuid *assembly_guid, ManagedGuid *method_guid, void *method_info_ptr)
{
    AssertThrow(assembly_guid != nullptr);
    AssertThrow(method_guid != nullptr);
    AssertThrow(method_info_ptr != nullptr);

    DotNetSystem::GetInstance().AddMethodToCache(*assembly_guid, *method_guid, method_info_ptr);
}

HYP_EXPORT void NativeInterop_AddObjectToCache(ManagedGuid *assembly_guid, ManagedGuid *object_guid, void *object_ptr, ManagedObject *out_managed_object)
{
    AssertThrow(assembly_guid != nullptr);
    AssertThrow(object_guid != nullptr);
    AssertThrow(object_ptr != nullptr);
    AssertThrow(out_managed_object != nullptr);

    DotNetSystem::GetInstance().AddObjectToCache(*assembly_guid, *object_guid, object_ptr, out_managed_object);
}

} // extern "C"
