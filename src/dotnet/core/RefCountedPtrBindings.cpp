/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/core/RefCountedPtrBindings.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT void RefCountedPtr_IncRef(ManagedRefCountedPtr managed_ref_counted_ptr)
{
    auto *ref_count_data = reinterpret_cast<typename memory::detail::RefCountedPtrBase<std::atomic<uint>>::RefCountDataType *>(managed_ref_counted_ptr.address);

    if (!ref_count_data) {
        return;
    }

    memory::detail::RefCountedPtrBase<std::atomic<uint>> ref_counted_ptr;
    ref_counted_ptr.SetRefCountData_Internal(ref_count_data, true /* inc_ref */);
    ref_counted_ptr.Release(); // Release the object to prevent the ref count from being decremented on destruction
}

HYP_EXPORT void RefCountedPtr_DecRef(ManagedRefCountedPtr managed_ref_counted_ptr)
{
    auto *ref_count_data = reinterpret_cast<typename memory::detail::RefCountedPtrBase<std::atomic<uint>>::RefCountDataType *>(managed_ref_counted_ptr.address);

    if (!ref_count_data) {
        return;
    }

    memory::detail::RefCountedPtrBase<std::atomic<uint>> ref_counted_ptr;
    ref_counted_ptr.SetRefCountData_Internal(ref_count_data, false /* inc_ref */);
    ref_counted_ptr.Reset(); // Reset the ref count data to decrement the ref count
}

HYP_EXPORT void WeakRefCountedPtr_IncRef(ManagedWeakRefCountedPtr managed_weak_ref_counted_ptr)
{
    auto *ref_count_data = reinterpret_cast<typename memory::detail::WeakRefCountedPtrBase<std::atomic<uint>>::RefCountDataType *>(managed_weak_ref_counted_ptr.address);

    if (!ref_count_data) {
        return;
    }

    memory::detail::WeakRefCountedPtrBase<std::atomic<uint>> weak_ref_counted_ptr;
    weak_ref_counted_ptr.SetRefCountData_Internal(ref_count_data, true /* inc_ref */);
    weak_ref_counted_ptr.Release(); // Release the object to prevent the ref count from being decremented on destruction
}

HYP_EXPORT void WeakRefCountedPtr_DecRef(ManagedWeakRefCountedPtr managed_weak_ref_counted_ptr)
{
    auto *ref_count_data = reinterpret_cast<typename memory::detail::WeakRefCountedPtrBase<std::atomic<uint>>::RefCountDataType *>(managed_weak_ref_counted_ptr.address);

    if (!ref_count_data) {
        return;
    }

    memory::detail::WeakRefCountedPtrBase<std::atomic<uint>> weak_ref_counted_ptr;
    weak_ref_counted_ptr.SetRefCountData_Internal(ref_count_data, false /* inc_ref */);
    weak_ref_counted_ptr.Reset(); // Reset the weak ref count data to decrement the ref count
}

} // extern "C"