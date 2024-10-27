/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/memory/RefCountedPtr.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT void RefCountedPtr_Get(uintptr_t ctrl_block, HypData *out_hyp_data)
{
    AssertThrow(out_hyp_data != nullptr);

    auto *ref_count_data = reinterpret_cast<typename memory::RefCountedPtrBase<std::atomic<uint>>::RefCountDataType *>(ctrl_block);
    AssertThrow(ref_count_data != nullptr);

    RC<void> rc;
    rc.SetRefCountData_Internal(ref_count_data, /* inc_ref */ true);

    *out_hyp_data = HypData(std::move(rc));
}

HYP_EXPORT uint32 RefCountedPtr_IncRef(uintptr_t ctrl_block)
{
    auto *ref_count_data = reinterpret_cast<typename memory::RefCountedPtrBase<std::atomic<uint>>::RefCountDataType *>(ctrl_block);
    AssertThrow(ref_count_data != nullptr);

    return ref_count_data->IncRefCount_Strong();
}

HYP_EXPORT uint32 RefCountedPtr_DecRef(uintptr_t ctrl_block)
{
    auto *ref_count_data = reinterpret_cast<typename memory::RefCountedPtrBase<std::atomic<uint>>::RefCountDataType *>(ctrl_block);
    AssertThrow(ref_count_data != nullptr);

    return ref_count_data->DecRefCount_Strong();
}

HYP_EXPORT uint32 WeakRefCountedPtr_IncRef(uintptr_t ctrl_block)
{
    auto *ref_count_data = reinterpret_cast<typename memory::WeakRefCountedPtrBase<std::atomic<uint>>::RefCountDataType *>(ctrl_block);
    AssertThrow(ref_count_data != nullptr);

    return ref_count_data->IncRefCount_Weak();
}

HYP_EXPORT uint32 WeakRefCountedPtr_DecRef(uintptr_t ctrl_block)
{
    auto *ref_count_data = reinterpret_cast<typename memory::WeakRefCountedPtrBase<std::atomic<uint>>::RefCountDataType *>(ctrl_block);
    AssertThrow(ref_count_data != nullptr);

    return ref_count_data->DecRefCount_Weak();
}

HYP_EXPORT uint32 WeakRefCountedPtr_Lock(uintptr_t ctrl_block)
{
    auto *ref_count_data = reinterpret_cast<typename memory::WeakRefCountedPtrBase<std::atomic<uint>>::RefCountDataType *>(ctrl_block);
    AssertThrow(ref_count_data != nullptr);

    if (ref_count_data->value == nullptr) {
        return 0;
    }

    return ref_count_data->IncRefCount_Strong();
}

} // extern "C"