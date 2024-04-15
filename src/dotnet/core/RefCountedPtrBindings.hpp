/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_RUNTIME_DOTNET_REF_COUNTED_PTR_BINDINGS_HPP
#define HYPERION_V2_RUNTIME_DOTNET_REF_COUNTED_PTR_BINDINGS_HPP

#include <core/lib/RefCountedPtr.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
struct ManagedRefCountedPtr
{
    uintptr_t   address;
};

static_assert(sizeof(ManagedRefCountedPtr) == 8, "sizeof(ManagedRefCountedPtr) must be 8 bytes to match C#");
static_assert(sizeof(ManagedRefCountedPtr) == sizeof(RefCountedPtr<void>), "ManagedRefCountedPtr size mismatch");

struct ManagedWeakRefCountedPtr
{
    uintptr_t   address;
};

static_assert(sizeof(ManagedWeakRefCountedPtr) == 8, "sizeof(ManagedWeakRefCountedPtr) must be 8 bytes to match C#");
static_assert(sizeof(ManagedWeakRefCountedPtr) == sizeof(Weak<void>), "ManagedWeakRefCountedPtr size mismatch");

extern HYP_API void RefCountedPtr_IncRef(ManagedRefCountedPtr);
extern HYP_API void RefCountedPtr_DecRef(ManagedRefCountedPtr);

extern HYP_API void WeakRefCountedPtr_IncRef(ManagedWeakRefCountedPtr);
extern HYP_API void WeakRefCountedPtr_DecRef(ManagedWeakRefCountedPtr);
} // extern "C"

template <class T>
static inline RC<T> GetRefCountedPtrFromManaged(ManagedRefCountedPtr managed_ref_counted_ptr)
{
    auto *ref_count_data = reinterpret_cast<typename detail::RefCountedPtrBase<>::RefCountDataType *>(managed_ref_counted_ptr.address);

    if (!ref_count_data) {
        return nullptr;
    }

    RC<T> ref_counted_ptr;
    ref_counted_ptr.SetRefCountData(ref_count_data, true /* inc_ref */);
    return ref_counted_ptr;
}

template <class T>
static inline Weak<T> GetWeakRefCountedPtrFromManaged(ManagedWeakRefCountedPtr managed_weak_ref_counted_ptr)
{
    auto *ref_count_data = reinterpret_cast<typename detail::WeakRefCountedPtrBase<>::RefCountDataType *>(managed_weak_ref_counted_ptr.address);

    if (!ref_count_data) {
        return nullptr;
    }

    Weak<T> weak_ref_counted_ptr;
    weak_ref_counted_ptr.SetRefCountData(ref_count_data, true /* inc_ref */);
    return weak_ref_counted_ptr;
}

#endif