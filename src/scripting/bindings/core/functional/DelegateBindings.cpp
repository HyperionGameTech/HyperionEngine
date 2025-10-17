/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>

#include <core/functional/ScriptableDelegate.hpp>

#include <core/Types.hpp>

namespace hyperion::dotnet {

extern "C"
{

    HYP_EXPORT DelegateHandler* ScriptableDelegate_Bind(IScriptableDelegate* delegate, dotnet::ManagedClass* pClass, ObjectReference* objectReference)
    {
        Assert(delegate != nullptr);
        Assert(objectReference != nullptr);
        Assert(pClass != nullptr);

        return new DelegateHandler(delegate->BindManaged("DynamicInvoke", MakeUnique<dotnet::ManagedObject>(pClass->RefCountedPtrFromThis(), *objectReference, ObjectFlags::CREATED_FROM_MANAGED)));
    }

    HYP_EXPORT int ScriptableDelegate_RemoveAllDetached(IScriptableDelegate* delegate)
    {
        Assert(delegate != nullptr);

        return delegate->RemoveAllDetached();
    }

    HYP_EXPORT int8 ScriptableDelegate_Remove(IScriptableDelegate* delegate, DelegateHandler* delegateHandler)
    {
        Assert(delegate != nullptr);

        if (!delegateHandler)
        {
            return 0;
        }

        return delegate->Remove(std::move(*delegateHandler));
    }

    HYP_EXPORT void DelegateHandler_Detach(DelegateHandler* delegateHandler)
    {
        Assert(delegateHandler != nullptr);

        delegateHandler->Detach();
    }

    HYP_EXPORT void DelegateHandler_Remove(DelegateHandler* delegateHandler)
    {
        Assert(delegateHandler != nullptr);

        delegateHandler->Reset();
    }

    HYP_EXPORT void DelegateHandler_Destroy(DelegateHandler* delegateHandler)
    {
        Assert(delegateHandler != nullptr);

        delete delegateHandler;
    }

} // extern "C"

} // namespace hyperion::dotnet