#include <dotnet/core/RefCountedPtrBindings.hpp>

#include <core/lib/RefCountedPtr.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {

HYP_EXPORT void RefCountedPtr_IncRef(ManagedRefCountedPtr managed_ref_counted_ptr)
{
    auto *ref_count_data = reinterpret_cast<typename detail::RefCountedPtrBase<>::RefCountDataType *>(managed_ref_counted_ptr.address);

    if (!ref_count_data) {
        return;
    }

    detail::RefCountedPtrBase<> ref_counted_ptr;
    ref_counted_ptr.SetRefCountData(ref_count_data, true /* inc_ref */);
    ref_counted_ptr.Release(); // Release the object to prevent the ref count from being decremented on destruction
}

HYP_EXPORT void RefCountedPtr_DecRef(ManagedRefCountedPtr managed_ref_counted_ptr)
{
    auto *ref_count_data = reinterpret_cast<typename detail::RefCountedPtrBase<>::RefCountDataType *>(managed_ref_counted_ptr.address);

    if (!ref_count_data) {
        return;
    }

    detail::RefCountedPtrBase<> ref_counted_ptr;
    ref_counted_ptr.SetRefCountData(ref_count_data, false /* inc_ref */);
    ref_counted_ptr.Reset(); // Reset the ref count data to decrement the ref count
}

HYP_EXPORT void WeakRefCountedPtr_IncRef(ManagedWeakRefCountedPtr managed_weak_ref_counted_ptr)
{
    auto *ref_count_data = reinterpret_cast<typename detail::WeakRefCountedPtrBase<>::RefCountDataType *>(managed_weak_ref_counted_ptr.address);

    if (!ref_count_data) {
        return;
    }

    detail::WeakRefCountedPtrBase<> weak_ref_counted_ptr;
    weak_ref_counted_ptr.SetRefCountData(ref_count_data, true /* inc_ref */);
    weak_ref_counted_ptr.Release(); // Release the object to prevent the ref count from being decremented on destruction
}

HYP_EXPORT void WeakRefCountedPtr_DecRef(ManagedWeakRefCountedPtr managed_weak_ref_counted_ptr)
{
    auto *ref_count_data = reinterpret_cast<typename detail::WeakRefCountedPtrBase<>::RefCountDataType *>(managed_weak_ref_counted_ptr.address);

    if (!ref_count_data) {
        return;
    }

    detail::WeakRefCountedPtrBase<> weak_ref_counted_ptr;
    weak_ref_counted_ptr.SetRefCountData(ref_count_data, false /* inc_ref */);
    weak_ref_counted_ptr.Reset(); // Reset the weak ref count data to decrement the ref count
}

} // extern "C"