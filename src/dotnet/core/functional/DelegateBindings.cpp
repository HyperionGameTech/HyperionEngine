/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/interop/ManagedObject.hpp>

#include <core/functional/ScriptableDelegate.hpp>

#include <Types.hpp>

namespace hyperion::dotnet {

extern "C" {

HYP_EXPORT DelegateHandler *ScriptableDelegate_Bind(IScriptableDelegate *delegate, dotnet::Class *class_object_ptr, ObjectReference *object_reference)
{
    AssertThrow(delegate != nullptr);
    AssertThrow(object_reference != nullptr);
    AssertThrow(class_object_ptr != nullptr);
    
    return new DelegateHandler(delegate->BindManaged("DynamicInvoke", MakeUnique<dotnet::Object>(class_object_ptr->RefCountedPtrFromThis(), *object_reference, ObjectFlags::CREATED_FROM_MANAGED)));
}

HYP_EXPORT int ScriptableDelegate_RemoveAllDetached(IScriptableDelegate *delegate)
{
    AssertThrow(delegate != nullptr);
    
    return delegate->RemoveAllDetached();
}

HYP_EXPORT int8 ScriptableDelegate_Remove(IScriptableDelegate *delegate, DelegateHandler *delegate_handler)
{
    AssertThrow(delegate != nullptr);

    if (!delegate_handler) {
        return 0;
    }

    return delegate->Remove(std::move(*delegate_handler));
}

HYP_EXPORT void DelegateHandler_Detach(DelegateHandler *delegate_handler)
{
    AssertThrow(delegate_handler != nullptr);

    delegate_handler->Detach();
}

HYP_EXPORT void DelegateHandler_Remove(DelegateHandler *delegate_handler)
{
    AssertThrow(delegate_handler != nullptr);

    delegate_handler->Reset();
}

HYP_EXPORT void DelegateHandler_Destroy(DelegateHandler *delegate_handler)
{
    AssertThrow(delegate_handler != nullptr);

    delete delegate_handler;
}

} // extern "C"

} // namespace hyperion::dotnet