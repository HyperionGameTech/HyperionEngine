/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypObject.hpp>

#include <core/ObjectPool.hpp>

#include <dotnet/Object.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;

extern "C" {

struct HypObjectInitializer
{
    const HypClass  *hyp_class;
    void            *native_address;
};

#pragma region HypObject

HYP_EXPORT void HypObject_Verify(const HypClass *hyp_class, void *native_address)
{
    if (!hyp_class || !native_address) {
        return;
    }

    if (hyp_class->UseHandles()) {
        const TypeID type_id = hyp_class->GetTypeID();

        ObjectContainerBase &container = ObjectPool::GetContainer(type_id);
        
        const uint32 index = container.GetObjectIndex(native_address);

        if (index == ~0u) {
            HYP_FAIL("Address %p is not valid for object container for TypeID %u", native_address, type_id.Value());
        }
    } else if (hyp_class->UseRefCountedPtr()) {
        AssertThrow(native_address != nullptr);

        EnableRefCountedPtrFromThisBase<> *ptr_casted = static_cast<EnableRefCountedPtrFromThisBase<> *>(native_address);
        
        auto *ref_count_data = ptr_casted->weak.GetRefCountData_Internal();
        AssertThrow(ref_count_data != nullptr);
    } else {
        HYP_FAIL("Unhandled HypClass allocation method");
    }
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

        // RC<void> rc;
        // rc.SetRefCountData_Internal(static_cast<typename RC<void>::RefCountedPtrBase::RefCountDataType *>(native_address), /* inc_ref */ true);
        // (void)rc.Release();
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

        ref_count_data->DecRefCount_Strong();

        // RC<void> rc;
        // rc.SetRefCountData_Internal(static_cast<typename RC<void>::RefCountedPtrBase::RefCountDataType *>(native_address), /* inc_ref */ false);
        // rc.Reset();
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