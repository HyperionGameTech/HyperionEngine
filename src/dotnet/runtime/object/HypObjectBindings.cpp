/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypObject.hpp>

#include <core/ObjectPool.hpp>

#include <dotnet/Object.hpp>
#include <dotnet/interop/ManagedObject.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C" {

struct HypObjectInitializer
{
    const HypClass  *hyp_class;
    void            *native_address;
};

#pragma region HypObject

HYP_EXPORT void HypObject_Verify(const HypClass *hyp_class, void *native_address, dotnet::ObjectReference *object_reference)
{
    if (!hyp_class || !native_address) {
        return;
    }

    AssertThrow(hyp_class->GetObjectInitializer(native_address) != nullptr);

    // Object reference is only available when creating a HypObject, initiated from the C# side,
    // in which case GetManagedObject() should not return null
    if (object_reference != nullptr) {
        AssertThrow(hyp_class->GetObjectInitializer(native_address)->GetManagedObject() != nullptr);
        AssertThrow(hyp_class->GetObjectInitializer(native_address)->GetManagedObject()->GetObjectReference() == *object_reference);
    }

    // AssertThrow(hyp_class->GetObjectInitializer(native_address)->GetManagedObject()->GetObjectReference

    // if (hyp_class->UseHandles()) {
    //     const TypeID type_id = hyp_class->GetTypeID();

    //     ObjectContainerBase &container = ObjectPool::GetContainer(type_id);
        
    //     const uint32 index = container.GetObjectIndex(native_address);

    //     if (index == ~0u) {
    //         HYP_FAIL("Address %p is not valid for object container for TypeID %u", native_address, type_id.Value());
    //     }
    // } else if (hyp_class->UseRefCountedPtr()) {
    //     AssertThrow(native_address != nullptr);

    //     EnableRefCountedPtrFromThisBase<> *ptr_casted = static_cast<EnableRefCountedPtrFromThisBase<> *>(native_address);
        
    //     auto *ref_count_data = ptr_casted->weak.GetRefCountData_Internal();
    //     AssertThrow(ref_count_data != nullptr);
    // } else {
    //     HYP_FAIL("Unhandled HypClass allocation method");
    // }
}

HYP_EXPORT void HypObject_IncRef(const HypClass *hyp_class, void *native_address)
{
    if (!hyp_class || !native_address) {
        return;
    }

    const TypeID type_id = hyp_class->GetTypeID();

    if (hyp_class->UseHandles()) {
        ObjectContainerBase &container = ObjectPool::GetContainer(type_id);
        
        const uint32 index = container.GetObjectIndex(native_address);
        if (index == ~0u) {
            HYP_FAIL("Address %p is not valid for object container for TypeID %u", native_address, type_id.Value());
        }

        container.IncRefStrong(index);
    } else if (hyp_class->UseRefCountedPtr()) {
        EnableRefCountedPtrFromThisBase<> *ptr_casted = static_cast<EnableRefCountedPtrFromThisBase<> *>(native_address);
        
        auto *ref_count_data = ptr_casted->weak.GetRefCountData_Internal();
        AssertThrow(ref_count_data != nullptr);

        ref_count_data->IncRefCount_Strong();
    } else {
        HYP_FAIL("Unhandled HypClass allocation method");
    }
}

HYP_EXPORT void HypObject_DecRef(const HypClass *hyp_class, void *native_address)
{
    if (!native_address || !hyp_class) {
        return;
    }

    const TypeID type_id = hyp_class->GetTypeID();

    if (hyp_class->UseHandles()) {
        ObjectContainerBase &container = ObjectPool::GetContainer(type_id);
        
        const uint32 index = container.GetObjectIndex(native_address);
        if (index == ~0u) {
            HYP_FAIL("Address %p is not valid for object container for TypeID %u", native_address, type_id.Value());
        }
        
        container.DecRefStrong(index);
    } else if (hyp_class->UseRefCountedPtr()) {
        EnableRefCountedPtrFromThisBase<> *ptr_casted = static_cast<EnableRefCountedPtrFromThisBase<> *>(native_address);
        
        auto *ref_count_data = ptr_casted->weak.GetRefCountData_Internal();
        AssertThrow(ref_count_data != nullptr);

        RC<void> rc;
        rc.SetRefCountData_Internal(ref_count_data, /* inc_ref */ false);
        rc.Reset();
    } else {
        HYP_FAIL("Unhandled HypClass allocation method");
    }
}

HYP_EXPORT HypProperty *HypObject_GetProperty(const HypClass *hyp_class, const Name *name)
{
    if (!hyp_class || !name) {
        return nullptr;
    }

    return hyp_class->GetProperty(*name);
}

HYP_EXPORT HypMethod *HypObject_GetMethod(const HypClass *hyp_class, const Name *name)
{
    if (!hyp_class || !name) {
        return nullptr;
    }

    return hyp_class->GetMethod(*name);
}

#pragma endregion HypObject

} // extern "C"