/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/memory/RefCountedPtr.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>

#include <core/Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT const void* RefCountedPtr_GetNullCtrlBlock()
    {
        return nullptr; //&memory::RefCountedPtrBase<AtomicVar<uint32>>::emptyRefCountData;
    }

    HYP_EXPORT void RefCountedPtr_Get(uintptr_t ctrlBlock, uintptr_t address, ValueStorage<HypData>* outHypData)
    {
        Assert(outHypData != nullptr);

        auto* refCountData = reinterpret_cast<typename memory::RefCountedPtrBase<AtomicVar<uint32>>::RefCountDataType*>(ctrlBlock);
        AssertDebug(refCountData != nullptr);

        RC<void> rc;
        rc.SetRefCountData_Internal(reinterpret_cast<void*>(address), refCountData, /* incRef */ true);

        outHypData->Construct(std::move(rc));
    }

    HYP_EXPORT void RefCountedPtr_IncRef(uintptr_t ctrlBlock, uintptr_t address)
    {
        auto* refCountData = reinterpret_cast<typename memory::RefCountedPtrBase<AtomicVar<uint32>>::RefCountDataType*>(ctrlBlock);
        AssertDebug(refCountData != nullptr);

        refCountData->IncRefCount_Strong(reinterpret_cast<void*>(address));
    }

    HYP_EXPORT void RefCountedPtr_DecRef(uintptr_t ctrlBlock, uintptr_t address)
    {
        auto* refCountData = reinterpret_cast<typename memory::RefCountedPtrBase<AtomicVar<uint32>>::RefCountDataType*>(ctrlBlock);
        AssertDebug(refCountData != nullptr);

        refCountData->DecRefCount_Strong(reinterpret_cast<void*>(address));
    }

    HYP_EXPORT void WeakRefCountedPtr_IncRef(uintptr_t ctrlBlock, uintptr_t address)
    {
        auto* refCountData = reinterpret_cast<typename memory::WeakRefCountedPtrBase<AtomicVar<uint32>>::RefCountDataType*>(ctrlBlock);
        AssertDebug(refCountData != nullptr);

        refCountData->IncRefCount_Weak(reinterpret_cast<void*>(address));
    }

    HYP_EXPORT void WeakRefCountedPtr_DecRef(uintptr_t ctrlBlock, uintptr_t address)
    {
        auto* refCountData = reinterpret_cast<typename memory::WeakRefCountedPtrBase<AtomicVar<uint32>>::RefCountDataType*>(ctrlBlock);
        AssertDebug(refCountData != nullptr);

        refCountData->IncRefCount_Weak(reinterpret_cast<void*>(address));
    }

    HYP_EXPORT uint32 WeakRefCountedPtr_Lock(uintptr_t ctrlBlock, uintptr_t address)
    {
        auto* refCountData = reinterpret_cast<typename memory::WeakRefCountedPtrBase<AtomicVar<uint32>>::RefCountDataType*>(ctrlBlock);
        AssertDebug(refCountData != nullptr);

        return refCountData->IncRefCount_Strong(reinterpret_cast<void*>(address));
    }

} // extern "C"