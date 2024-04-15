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

    AssertThrowMsg(m_class_ptr->GetParent() != nullptr, "Class parent not set!");
    AssertThrowMsg(m_class_ptr->GetParent()->GetInvokeMethodFunction() != nullptr, "Invoke method function pointer not set on class parent!");

    AssertThrowMsg(m_class_ptr->GetNewObjectFunction() != nullptr, "New object function pointer not set!");
    AssertThrowMsg(m_class_ptr->GetFreeObjectFunction() != nullptr, "Free object function pointer not set!");
}

Object::~Object()
{
    m_class_ptr->GetFreeObjectFunction()(m_managed_object);
}

void *Object::InvokeMethod(const ManagedMethod *method_ptr, void **args_vptr, void *return_value_vptr)
{
    return m_class_ptr->GetParent()->GetInvokeMethodFunction()(method_ptr->guid, m_managed_object.guid, args_vptr, return_value_vptr);
}

const ManagedMethod *Object::GetMethod(const String &method_name) const
{
    auto it = m_class_ptr->GetMethods().Find(method_name);

    if (it == m_class_ptr->GetMethods().End()) {
        return nullptr;
    }

    return &it->second;
}

} // namespace hyperion::dotnet