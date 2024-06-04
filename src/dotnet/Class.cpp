/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/Object.hpp>

namespace hyperion::dotnet {

void Class::EnsureLoaded() const
{
    if (!m_parent || !m_parent->CheckAssemblyLoaded()) {
        HYP_THROW("Cannot use managed class: assembly has been unloaded");
    }
}

UniquePtr<Object> Class::NewObject()
{
    EnsureLoaded();

    AssertThrowMsg(m_new_object_fptr != nullptr, "New object function pointer not set for managed class %s", m_name.Data());

    ManagedObject managed_object = m_new_object_fptr();

    return UniquePtr<Object>::Construct(this, managed_object);
}

void *Class::InvokeStaticMethod(const ManagedMethod *method_ptr, void **args_vptr, void *return_value_vptr)
{
    EnsureLoaded();
    
    AssertThrowMsg(m_parent->GetInvokeMethodFunction() != nullptr, "Invoke method function pointer not set");

    return m_parent->GetInvokeMethodFunction()(method_ptr->guid, {}, args_vptr, return_value_vptr);
}

} // namespace hyperion::dotnet