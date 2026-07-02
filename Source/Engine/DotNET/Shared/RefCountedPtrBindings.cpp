/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/Memory/SharedPtr.hpp>

#include <Core/Reflection/Class.hpp>
#include <Core/Debug/Debug.hpp>

using namespace Hyperion;

extern "C"
{
    HYP_EXPORT void SharedPtr_Get(UIntPtr address, ValueStorage<BoxedValue>* pOutBoxed)
    {
        Assert(pOutBoxed != nullptr);

        auto* base = reinterpret_cast<typename memory::SharedPtrBase<AtomicVar<uint32>>*>(address);
        AssertDebug(base != nullptr);

        SharedPtr<void> rc;
        rc.SetBlock_Internal(base->GetBlock_Internal(), /* incRef */ true);

        pOutBoxed->Construct(std::move(rc));
    }

    HYP_EXPORT void SharedPtr_IncRef(UIntPtr address)
    {
        auto* base = reinterpret_cast<typename memory::SharedPtrBase<AtomicVar<uint32>>*>(address);
        AssertDebug(base != nullptr);

        memory::detail::IncStrong(base->GetBlock_Internal());
    }

    HYP_EXPORT void SharedPtr_DecRef(UIntPtr address)
    {
        auto* base = reinterpret_cast<typename memory::SharedPtrBase<AtomicVar<uint32>>*>(address);
        AssertDebug(base != nullptr);

        memory::detail::ReleaseStrong(base->GetBlock_Internal());
    }

    HYP_EXPORT void WeakPtr_IncRef(UIntPtr address)
    {
        auto* base = reinterpret_cast<typename memory::WeakPtrBase<AtomicVar<uint32>>*>(address);
        AssertDebug(base != nullptr);

        memory::detail::IncWeak(base->GetBlock_Internal());
    }

    HYP_EXPORT void WeakPtr_DecRef(UIntPtr address)
    {
        auto* base = reinterpret_cast<typename memory::WeakPtrBase<AtomicVar<uint32>>*>(address);
        AssertDebug(base != nullptr);

        memory::detail::ReleaseWeak(base->GetBlock_Internal());
    }

    HYP_EXPORT uint32 WeakPtr_Lock(UIntPtr address)
    {
        auto* base = reinterpret_cast<typename memory::WeakPtrBase<AtomicVar<uint32>>*>(address);
        AssertDebug(base != nullptr);

        return memory::detail::IncStrong(base->GetBlock_Internal());
    }

} // extern "C"
