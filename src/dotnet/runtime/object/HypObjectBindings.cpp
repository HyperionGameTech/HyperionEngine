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

HYP_EXPORT void HypObject_Initialize(const HypClass *hyp_class, dotnet::Class *class_object_ptr, dotnet::ObjectReference *object_reference, void **out_instance_ptr)
{
    AssertThrow(hyp_class != nullptr);
    AssertThrow(class_object_ptr != nullptr);
    AssertThrow(object_reference != nullptr);
    AssertThrow(out_instance_ptr != nullptr);

    *out_instance_ptr = nullptr;

    {
        // Suppress default managed object creation
        HypObjectInitializerFlagsGuard flags_guard(HypObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION);

        if (hyp_class->UseHandles()) {
            //ObjectContainerBase &container = ObjectPool::GetContainer(hyp_class->GetTypeID());
            
            ObjectContainerBase &container = ObjectPool::GetObjectContainerHolder().GetObjectContainer(hyp_class->GetTypeID());
            
            const uint32 index = container.NextIndex();
            container.ConstructAtIndex(index);

            *out_instance_ptr = container.GetObjectPointer(index);
        } else if (hyp_class->UseRefCountedPtr()) {
            HypData value;
            
            // Set allow_abstract to true so we can use classes marked as "Abstract"
            // allowing the managed class to override methods of an abstract class
            hyp_class->CreateInstance(value, /* allow_abstract */ true);

            RC<void> rc = std::move(value.Get<RC<void>>());
            AssertThrow(rc != nullptr);

            *out_instance_ptr = rc.Release();
        } else {
            HYP_FAIL("Unsupported allocation method for HypClass %s", *hyp_class->GetName());
        }
    }

    IHypObjectInitializer *initializer = hyp_class->GetObjectInitializer(*out_instance_ptr);
    AssertThrow(initializer != nullptr);

    // make it WEAK_REFERENCE since we don't want to release the object on destructor call,
    // as it is managed by the .NET runtime

    // @FIXME: Don't use hyp_class->GetManagedClass(), instead, take Class* passed into the function
    // this will allow C# derived classes to be used
    UniquePtr<dotnet::Object> managed_object = MakeUnique<dotnet::Object>(class_object_ptr, *object_reference, ObjectFlags::WEAK_REFERENCE);
    initializer->SetManagedObject(managed_object.Release());
}

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
}

HYP_EXPORT void *HypObject_IncRef(const HypClass *hyp_class, void *native_address, int8 is_weak)
{
    AssertThrow(hyp_class != nullptr);
    AssertThrow(native_address != nullptr);

    const TypeID type_id = hyp_class->GetTypeID();

    if (hyp_class->UseHandles()) {
        ObjectContainerBase &container = ObjectPool::GetContainer(type_id);

        IHypObject *hyp_object_ptr = static_cast<IHypObject *>(native_address);

        if (is_weak) {
            container.IncRefWeak(hyp_object_ptr->GetID().Value() - 1);
        } else {
            container.IncRefStrong(hyp_object_ptr->GetID().Value() - 1);
        }

        return nullptr;
    } else if (hyp_class->UseRefCountedPtr()) {
        EnableRefCountedPtrFromThisBase<> *ptr_casted = static_cast<EnableRefCountedPtrFromThisBase<> *>(native_address);
        
        auto *ref_count_data = ptr_casted->weak.GetRefCountData_Internal();
        AssertThrow(ref_count_data != nullptr);

        if (is_weak) {
            ref_count_data->IncRefCount_Weak();
        } else {
            ref_count_data->IncRefCount_Strong();
        }

        return ref_count_data;
    } else {
        HYP_FAIL("Unhandled HypClass allocation method");
    }
}

HYP_EXPORT void HypObject_DecRef(const HypClass *hyp_class, void *native_address, void *control_block_ptr, int8 is_weak)
{
    AssertThrow(hyp_class != nullptr);
    AssertThrow(native_address != nullptr);

    const TypeID type_id = hyp_class->GetTypeID();

    if (hyp_class->UseHandles()) {
        ObjectContainerBase &container = ObjectPool::GetContainer(type_id);

        IHypObject *hyp_object_ptr = static_cast<IHypObject *>(native_address);
        
        if (is_weak) {
            container.DecRefWeak(hyp_object_ptr->GetID().Value() - 1);
        } else {
            container.DecRefStrong(hyp_object_ptr->GetID().Value() - 1);
        }
    } else if (hyp_class->UseRefCountedPtr()) {
        AssertThrow(control_block_ptr != nullptr);

        typename RC<void>::RefCountedPtrBase::RefCountDataType *ref_count_data = static_cast<typename RC<void>::RefCountedPtrBase::RefCountDataType *>(control_block_ptr);

        if (is_weak) {
            Weak<void> weak;
            weak.SetRefCountData_Internal(ref_count_data, /* inc_ref */ false);
        } else {
            RC<void> rc;
            rc.SetRefCountData_Internal(ref_count_data, /* inc_ref */ false);
        }
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