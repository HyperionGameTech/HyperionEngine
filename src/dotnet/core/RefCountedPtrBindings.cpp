/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/memory/RefCountedPtr.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT const void* RefCountedPtr_GetNullCtrlBlock()
    {
        return nullptr; //&memory::RefCountedPtrBase<AtomicVar<uint32>>::empty_ref_count_data;
    }

    HYP_EXPORT void RefCountedPtr_Get(uintptr_t ctrl_block, uintptr_t address, ValueStorage<HypData>* out_hyp_data)
    {
        AssertThrow(out_hyp_data != nullptr);

        auto* ref_count_data = reinterpret_cast<typename memory::RefCountedPtrBase<AtomicVar<uint32>>::RefCountDataType*>(ctrl_block);
        AssertDebug(ref_count_data != nullptr);
        AssertDebug(ref_count_data->value != nullptr);

        RC<void> rc;
        rc.SetRefCountData_Internal(reinterpret_cast<void*>(address), ref_count_data, /* inc_ref */ true);

        out_hyp_data->Construct(std::move(rc));
    }

    HYP_EXPORT void RefCountedPtr_IncRef(uintptr_t ctrl_block, uintptr_t address)
    {
        auto* ref_count_data = reinterpret_cast<typename memory::RefCountedPtrBase<AtomicVar<uint32>>::RefCountDataType*>(ctrl_block);
        AssertDebug(ref_count_data != nullptr);
        AssertDebug(ref_count_data->value != nullptr);

        ref_count_data->IncRefCount_Strong(reinterpret_cast<void*>(address));
    }

    HYP_EXPORT void RefCountedPtr_DecRef(uintptr_t ctrl_block, uintptr_t address)
    {
        auto* ref_count_data = reinterpret_cast<typename memory::RefCountedPtrBase<AtomicVar<uint32>>::RefCountDataType*>(ctrl_block);
        AssertDebug(ref_count_data != nullptr);
        AssertDebug(ref_count_data->value != nullptr);

        ref_count_data->DecRefCount_Strong(reinterpret_cast<void*>(address));
    }

    HYP_EXPORT void WeakRefCountedPtr_IncRef(uintptr_t ctrl_block, uintptr_t address)
    {
        auto* ref_count_data = reinterpret_cast<typename memory::WeakRefCountedPtrBase<AtomicVar<uint32>>::RefCountDataType*>(ctrl_block);
        AssertDebug(ref_count_data != nullptr);
        AssertDebug(ref_count_data->value != nullptr);

        ref_count_data->IncRefCount_Weak(reinterpret_cast<void*>(address));
    }

    HYP_EXPORT void WeakRefCountedPtr_DecRef(uintptr_t ctrl_block, uintptr_t address)
    {
        auto* ref_count_data = reinterpret_cast<typename memory::WeakRefCountedPtrBase<AtomicVar<uint32>>::RefCountDataType*>(ctrl_block);
        AssertDebug(ref_count_data != nullptr);
        AssertDebug(ref_count_data->value != nullptr);

        ref_count_data->IncRefCount_Weak(reinterpret_cast<void*>(address));
    }

    HYP_EXPORT uint32 WeakRefCountedPtr_Lock(uintptr_t ctrl_block, uintptr_t address)
    {
        auto* ref_count_data = reinterpret_cast<typename memory::WeakRefCountedPtrBase<AtomicVar<uint32>>::RefCountDataType*>(ctrl_block);
        AssertDebug(ref_count_data != nullptr);
        AssertDebug(ref_count_data->value != nullptr);

        return ref_count_data->IncRefCount_Strong(reinterpret_cast<void*>(address));
    }

} // extern "C"