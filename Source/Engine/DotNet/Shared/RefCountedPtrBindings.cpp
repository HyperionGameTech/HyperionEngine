/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/Memory/RefCountedPtr.hpp>

#include <Core/Reflection/Class.hpp>
#include <Core/Debug/Debug.hpp>

using namespace Hyperion;

extern "C"
{
    HYP_EXPORT void RefCountedPtr_Get(UIntPtr address, ValueStorage<BoxedValue>* pOutBoxed)
    {
        Assert(pOutBoxed != nullptr);

        auto* base = reinterpret_cast<typename memory::RefCountedPtrBase<AtomicVar<uint32>>*>(address);
        AssertDebug(base != nullptr);

        RC<void> rc;
        rc.SetBlock_Internal(base->GetBlock_Internal(), /* incRef */ true);

        pOutBoxed->Construct(std::move(rc));
    }

    HYP_EXPORT void RefCountedPtr_IncRef(UIntPtr address)
    {
        auto* base = reinterpret_cast<typename memory::RefCountedPtrBase<AtomicVar<uint32>>*>(address);
        AssertDebug(base != nullptr);

        memory::detail::IncStrong(base->GetBlock_Internal());
    }

    HYP_EXPORT void RefCountedPtr_DecRef(UIntPtr address)
    {
        auto* base = reinterpret_cast<typename memory::RefCountedPtrBase<AtomicVar<uint32>>*>(address);
        AssertDebug(base != nullptr);

        memory::detail::ReleaseStrong(base->GetBlock_Internal());
    }

    HYP_EXPORT void WeakRefCountedPtr_IncRef(UIntPtr address)
    {
        auto* base = reinterpret_cast<typename memory::WeakRefCountedPtrBase<AtomicVar<uint32>>*>(address);
        AssertDebug(base != nullptr);

        memory::detail::IncWeak(base->GetBlock_Internal());
    }

    HYP_EXPORT void WeakRefCountedPtr_DecRef(UIntPtr address)
    {
        auto* base = reinterpret_cast<typename memory::WeakRefCountedPtrBase<AtomicVar<uint32>>*>(address);
        AssertDebug(base != nullptr);

        memory::detail::ReleaseWeak(base->GetBlock_Internal());
    }

    HYP_EXPORT uint32 WeakRefCountedPtr_Lock(UIntPtr address)
    {
        auto* base = reinterpret_cast<typename memory::WeakRefCountedPtrBase<AtomicVar<uint32>>*>(address);
        AssertDebug(base != nullptr);

        return memory::detail::IncStrong(base->GetBlock_Internal());
    }

} // extern "C"
