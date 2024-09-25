/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/core/RefCountedPtrBindings.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT void RefCountedPtr_IncRef(ManagedRefCountedPtr managed_ref_counted_ptr)
{
    auto *ref_count_data = reinterpret_cast<typename memory::RefCountedPtrBase<std::atomic<uint>>::RefCountDataType *>(managed_ref_counted_ptr.address);

    if (!ref_count_data) {
        return;
    }

    ref_count_data->IncRefCount_Strong();
}

HYP_EXPORT void RefCountedPtr_DecRef(ManagedRefCountedPtr managed_ref_counted_ptr)
{
    auto *ref_count_data = reinterpret_cast<typename memory::RefCountedPtrBase<std::atomic<uint>>::RefCountDataType *>(managed_ref_counted_ptr.address);

    if (!ref_count_data) {
        return;
    }

    ref_count_data->DecRefCount_Strong();
}

HYP_EXPORT void WeakRefCountedPtr_IncRef(ManagedWeakRefCountedPtr managed_weak_ref_counted_ptr)
{
    auto *ref_count_data = reinterpret_cast<typename memory::WeakRefCountedPtrBase<std::atomic<uint>>::RefCountDataType *>(managed_weak_ref_counted_ptr.address);

    if (!ref_count_data) {
        return;
    }

    ref_count_data->IncRefCount_Weak();
}

HYP_EXPORT void WeakRefCountedPtr_DecRef(ManagedWeakRefCountedPtr managed_weak_ref_counted_ptr)
{
    auto *ref_count_data = reinterpret_cast<typename memory::WeakRefCountedPtrBase<std::atomic<uint>>::RefCountDataType *>(managed_weak_ref_counted_ptr.address);

    if (!ref_count_data) {
        return;
    }

    ref_count_data->DecRefCount_Weak();
}

} // extern "C"