/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>

namespace hyperion::dotnet {

Object::Object(Class *class_ptr, ManagedObject managed_object)
    : m_class_ptr(class_ptr),
      m_managed_object(managed_object)
{
    AssertThrowMsg(m_class_ptr != nullptr, "Class pointer not set!");

    AssertThrowMsg(m_class_ptr->GetClassHolder() != nullptr, "Class holder not set!");
    AssertThrowMsg(m_class_ptr->GetClassHolder()->GetInvokeMethodFunction() != nullptr, "Invoke method function pointer not set on class parent!");

    AssertThrowMsg(m_class_ptr->GetNewObjectFunction() != nullptr, "New object function pointer not set!");
    AssertThrowMsg(m_class_ptr->GetFreeObjectFunction() != nullptr, "Free object function pointer not set!");
}

Object::~Object()
{
    m_class_ptr->EnsureLoaded();

    m_class_ptr->GetFreeObjectFunction()(m_managed_object);
}

void *Object::InvokeMethod(const ManagedMethod *method_ptr, void **args_vptr, void *return_value_vptr)
{
    m_class_ptr->EnsureLoaded();

    return m_class_ptr->GetClassHolder()->GetInvokeMethodFunction()(method_ptr->guid, m_managed_object.guid, args_vptr, return_value_vptr);
}

const ManagedMethod *Object::GetMethod(UTF8StringView method_name) const
{
    m_class_ptr->EnsureLoaded();

    auto it = m_class_ptr->GetMethods().FindAs(method_name);

    if (it == m_class_ptr->GetMethods().End()) {
        return nullptr;
    }

    return &it->second;
}

} // namespace hyperion::dotnet