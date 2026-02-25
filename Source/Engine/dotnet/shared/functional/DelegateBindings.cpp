/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>

#include <scripting/ScriptableDelegate.hpp>

using namespace Hyperion;

#ifdef HYP_DOTNET
using namespace Hyperion::dotnet;
#endif

extern "C"
{
#ifdef HYP_DOTNET
    HYP_EXPORT DelegateHandler* ScriptableDelegate_Bind(IScriptableDelegate* pDelegate, ManagedClass* pClass, ObjectReference* pObjectReference)
    {
        Assert(pDelegate != nullptr);
        Assert(pObjectReference != nullptr);
        Assert(pClass != nullptr);

        return new DelegateHandler(pDelegate->BindMethod("DynamicInvoke", MakeUnique<ManagedObject>(pClass->RefCountedPtrFromThis(), *pObjectReference, ObjectFlags::CREATED_FROM_MANAGED)));
    }
#endif

    HYP_EXPORT int ScriptableDelegate_RemoveAllDetached(IScriptableDelegate* pDelegate)
    {
        Assert(pDelegate != nullptr);

        return pDelegate->RemoveAllDetached();
    }

    HYP_EXPORT int8 ScriptableDelegate_Remove(IScriptableDelegate* pDelegate, DelegateHandler* pHandle)
    {
        Assert(pDelegate != nullptr);

        if (!pHandle)
        {
            return 0;
        }

        return pDelegate->Remove(std::move(*pHandle));
    }

    HYP_EXPORT void DelegateHandler_Detach(DelegateHandler* pHandle)
    {
        Assert(pHandle != nullptr);

        pHandle->Detach();
    }

    HYP_EXPORT void DelegateHandler_Remove(DelegateHandler* pHandle)
    {
        Assert(pHandle != nullptr);

        pHandle->Reset();
    }

    HYP_EXPORT void DelegateHandler_Destroy(DelegateHandler* pHandle)
    {
        Assert(pHandle != nullptr);

        delete pHandle;
    }

} // extern "C"
