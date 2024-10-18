/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypObject.hpp>
#include <core/object/HypData.hpp>

#include <core/Name.hpp>

#include <dotnet/Object.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C" {

#pragma region HypClass

HYP_EXPORT void *HypClass_CreateInstance(const HypClass *hyp_class)
{
    AssertThrow(hyp_class != nullptr);

    if (hyp_class->UseHandles()) {
        ObjectContainerBase &container = ObjectPool::GetContainer(hyp_class->GetTypeID());
        
        const uint32 index = container.NextIndex();
        container.ConstructAtIndex(index);

        return container.GetObjectPointer(index);
    } else if (hyp_class->UseRefCountedPtr()) {
        HypData value;
        hyp_class->CreateInstance(value);

        RC<void> &rc = value.Get<RC<void>>();
        AssertThrow(rc != nullptr);

        return rc.Release();
    } else {
        HYP_FAIL("Unsupported allocation method for HypClass %s", *hyp_class->GetName());
    }


    // HypData value;
    // hyp_class->CreateInstance(value);

    // if (hyp_class->UseHandles()) {
    //     return value.Get<AnyHandle>().Release().GetPointer();
    // } else if (hyp_class->UseRefCountedPtr()) {
    //     return value.Get<RC<void>>().Release();
    // } else {
    //     HYP_FAIL("Unsupported allocation method for HypClass %s", *hyp_class->GetName());
    // }
}

HYP_EXPORT void *HypClass_InitInstance(const HypClass *hyp_class, dotnet::ObjectReference *object_reference)
{
    AssertThrow(hyp_class != nullptr);
    AssertThrow(object_reference != nullptr);

    void *created_object_ptr = nullptr;

    {
        // Suppress default managed object creation
        HypObjectInitializerFlagsGuard flags_guard(HypObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION);

        if (hyp_class->UseHandles()) {
            ObjectContainerBase &container = ObjectPool::GetContainer(hyp_class->GetTypeID());
            
            const uint32 index = container.NextIndex();
            container.ConstructAtIndex(index);

            created_object_ptr = container.GetObjectPointer(index);
        } else if (hyp_class->UseRefCountedPtr()) {
            HypData value;
            hyp_class->CreateInstance(value);

            RC<void> &rc = value.Get<RC<void>>();
            AssertThrow(rc != nullptr);

            created_object_ptr = rc.Release();
        } else {
            HYP_FAIL("Unsupported allocation method for HypClass %s", *hyp_class->GetName());
        }
    }

    IHypObjectInitializer *initializer = hyp_class->GetObjectInitializer(created_object_ptr);
    AssertThrow(initializer != nullptr);

    // make it WEAK_REFERENCE since we don't want to release the object on destructor call,
    // as it is managed by the .NET runtime
    UniquePtr<dotnet::Object> managed_object = MakeUnique<dotnet::Object>(hyp_class->GetManagedClass(), *object_reference, ObjectFlags::WEAK_REFERENCE);

    SetHypObjectInitializerManagedObject(initializer, created_object_ptr, std::move(managed_object));

    return created_object_ptr;

    // HypData value;

    // // make it WEAK_REFERENCE since we don't want to release the object on destructor call,
    // // as it is managed by the .NET runtime
    // hyp_class->CreateInstance(value, MakeUnique<dotnet::Object>(hyp_class->GetManagedClass(), *object_reference, ObjectFlags::WEAK_REFERENCE));

    // if (hyp_class->UseHandles()) {
    //     return value.Get<AnyHandle>().Release().GetPointer();
    // } else if (hyp_class->UseRefCountedPtr()) {
    //     return value.Get<RC<void>>().Release();
    // } else {
    //     HYP_FAIL("Unsupported allocation method for HypClass %s", *hyp_class->GetName());
    // }
}

HYP_EXPORT const HypClass *HypClass_GetClassByName(const char *name)
{
    if (!name) {
        return nullptr;
    }

    const WeakName weak_name(name);

    return HypClassRegistry::GetInstance().GetClass(weak_name);
}

HYP_EXPORT void HypClass_GetName(const HypClass *hyp_class, Name *out_name)
{
    if (!hyp_class || !out_name) {
        return;
    }

    *out_name = hyp_class->GetName();
}

HYP_EXPORT void HypClass_GetTypeID(const HypClass *hyp_class, TypeID *out_type_id)
{
    if (!hyp_class || !out_type_id) {
        return;
    }

    *out_type_id = hyp_class->GetTypeID();
}

HYP_EXPORT uint32 HypClass_GetSize(const HypClass *hyp_class)
{
    if (!hyp_class) {
        return 0;
    }

    return uint32(hyp_class->GetSize());
}

HYP_EXPORT uint32 HypClass_GetFlags(const HypClass *hyp_class)
{
    if (!hyp_class) {
        return 0;
    }

    return uint32(hyp_class->GetFlags());
}

HYP_EXPORT uint32 HypClass_GetProperties(const HypClass *hyp_class, const void **out_properties)
{
    if (!hyp_class || !out_properties) {
        return 0;
    }

    if (hyp_class->GetProperties().Empty()) {
        return 0;
    }

    *out_properties = hyp_class->GetProperties().Begin();

    return (uint32)hyp_class->GetProperties().Size();
}

HYP_EXPORT HypProperty *HypClass_GetProperty(const HypClass *hyp_class, const Name *name)
{
    if (!hyp_class || !name) {
        return nullptr;
    }

    return hyp_class->GetProperty(*name);
}

HYP_EXPORT uint32 HypClass_GetMethods(const HypClass *hyp_class, const void **out_methods)
{
    if (!hyp_class || !out_methods) {
        return 0;
    }

    if (hyp_class->GetMethods().Empty()) {
        return 0;
    }

    *out_methods = hyp_class->GetMethods().Begin();

    return (uint32)hyp_class->GetMethods().Size();
}

HYP_EXPORT HypMethod *HypClass_GetMethod(const HypClass *hyp_class, const Name *name)
{
    if (!hyp_class || !name) {
        return nullptr;
    }

    return hyp_class->GetMethod(*name);
}

HYP_EXPORT HypField *HypClass_GetField(const HypClass *hyp_class, const Name *name)
{
    if (!hyp_class || !name) {
        return nullptr;
    }

    return hyp_class->GetField(*name);
}

HYP_EXPORT uint32 HypClass_GetFields(const HypClass *hyp_class, const void **out_fields)
{
    if (!hyp_class || !out_fields) {
        return 0;
    }

    if (hyp_class->GetFields().Empty()) {
        return 0;
    }

    *out_fields = hyp_class->GetFields().Begin();

    return (uint32)hyp_class->GetFields().Size();
}

#pragma endregion HypClass

} // extern "C"