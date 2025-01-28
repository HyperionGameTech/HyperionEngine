/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Object.hpp>
#include <dotnet/interop/ManagedObject.hpp>

#include <core/functional/ScriptableDelegate.hpp>

#include <Types.hpp>

namespace hyperion::dotnet {

extern "C" {

HYP_EXPORT DelegateHandler *ScriptableDelegate_Bind(IScriptableDelegate *delegate, dotnet::Class *class_object_ptr, ObjectReference *object_reference)
{
    AssertThrow(delegate != nullptr);
    AssertThrow(object_reference != nullptr);
    
    return new DelegateHandler(delegate->BindManaged(dotnet::Object(class_object_ptr, *object_reference, ObjectFlags::CREATED_FROM_MANAGED)));
}

HYP_EXPORT void DelegateHandler_Destroy(DelegateHandler *delegate_handler)
{
    AssertThrow(delegate_handler != nullptr);

    delete delegate_handler;
}

} // extern "C"

} // namespace hyperion::dotnet