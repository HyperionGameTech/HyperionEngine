/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RUNTIME_DOTNET_REF_COUNTED_PTR_BINDINGS_HPP
#define HYPERION_RUNTIME_DOTNET_REF_COUNTED_PTR_BINDINGS_HPP

#include <core/memory/RefCountedPtr.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

using namespace hyperion;

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

/*! \brief Creates a ManagedRefCountedPtr from a RefCountedPtr. Increments the reference count.
 *
 * \param ref_counted_ptr The RefCountedPtr to create a ManagedRefCountedPtr from.
 * \return The ManagedRefCountedPtr.
 */
template <class T>
static inline ManagedRefCountedPtr CreateManagedRefCountedPtr(const RC<T> &ref_counted_ptr)
{
    ManagedRefCountedPtr managed_ref_counted_ptr;
    managed_ref_counted_ptr.address = reinterpret_cast<uintptr_t>(static_cast<const void *>(ref_counted_ptr.GetRefCountData_Internal()));
    RefCountedPtr_IncRef(managed_ref_counted_ptr);

    return managed_ref_counted_ptr;
}

template <class T>
static inline RC<T> GetRefCountedPtrFromManaged(ManagedRefCountedPtr managed_ref_counted_ptr)
{
    auto *ref_count_data = reinterpret_cast<typename memory::detail::RefCountedPtrBase<std::atomic<uint>>::RefCountDataType *>(managed_ref_counted_ptr.address);

    if (!ref_count_data) {
        return nullptr;
    }

    RC<T> ref_counted_ptr;
    ref_counted_ptr.SetRefCountData_Internal(ref_count_data, true /* inc_ref */);

    return ref_counted_ptr;
}

template <class T>
static inline Weak<T> GetWeakRefCountedPtrFromManaged(ManagedWeakRefCountedPtr managed_weak_ref_counted_ptr)
{
    auto *ref_count_data = reinterpret_cast<typename memory::detail::WeakRefCountedPtrBase<std::atomic<uint>>::RefCountDataType *>(managed_weak_ref_counted_ptr.address);

    if (!ref_count_data) {
        return nullptr;
    }

    Weak<T> weak_ref_counted_ptr;
    weak_ref_counted_ptr.SetRefCountData_Internal(ref_count_data, true /* inc_ref */);

    return weak_ref_counted_ptr;
}

#endif