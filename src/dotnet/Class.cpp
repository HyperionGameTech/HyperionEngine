/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/Object.hpp>

namespace hyperion::dotnet {

UniquePtr<Object> Class::NewObject()
{
    AssertThrowMsg(m_new_object_fptr != nullptr, "New object function pointer not set for managed class %s", m_name.Data());

    ManagedObject managed_object = m_new_object_fptr();

    return UniquePtr<Object>::Construct(this, managed_object);
}

void *Class::InvokeStaticMethod(const ManagedMethod *method_ptr, void **args_vptr, void *return_value_vptr)
{
    AssertThrowMsg(m_parent != nullptr, "Parent not set!");
    AssertThrowMsg(m_parent->GetInvokeMethodFunction() != nullptr, "Invoke method function pointer not set");

    return m_parent->GetInvokeMethodFunction()(method_ptr->guid, {}, args_vptr, return_value_vptr);
}

} // namespace hyperion::dotnet