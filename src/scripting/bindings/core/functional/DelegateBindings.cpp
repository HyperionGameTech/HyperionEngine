/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/functional/ScriptableDelegate.hpp>

#include <core/Types.hpp>

#ifdef HYP_DOTNET
#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/interop/ManagedObject.hpp>
#endif

namespace hyperion::dotnet {

extern "C"
{
#ifdef HYP_DOTNET
    HYP_EXPORT DelegateHandler* ScriptableDelegate_Bind(IScriptableDelegate* delegate, dotnet::Class* classObjectPtr, ObjectReference* objectReference)
    {
        Assert(delegate != nullptr);
        Assert(objectReference != nullptr);
        Assert(classObjectPtr != nullptr);

        return new DelegateHandler(delegate->BindManaged("DynamicInvoke", MakeUnique<dotnet::Object>(classObjectPtr->RefCountedPtrFromThis(), *objectReference, ObjectFlags::CREATED_FROM_MANAGED)));
    }
#endif

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
