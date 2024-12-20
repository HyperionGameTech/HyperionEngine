/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>

namespace hyperion::dotnet {

Object::Object(Class *class_ptr, ObjectReference object_reference, EnumFlags<ObjectFlags> object_flags)
    : m_class_ptr(class_ptr),
      m_object_reference(object_reference),
      m_object_flags(object_flags)
{
    AssertThrowMsg(m_class_ptr != nullptr, "Class pointer not set!");

    // AssertThrowMsg(m_class_ptr->GetClassHolder() != nullptr, "Class holder not set!");
    // AssertThrowMsg(m_class_ptr->GetClassHolder()->GetInvokeMethodFunction() != nullptr, "Invoke method function pointer not set on class parent!");

    // AssertThrowMsg(m_class_ptr->GetNewObjectFunction() != nullptr, "New object function pointer not set!");
    // AssertThrowMsg(m_class_ptr->GetFreeObjectFunction() != nullptr, "Free object function pointer not set!");
}

Object::~Object()
{
    if (!(m_object_flags & ObjectFlags::WEAK_REFERENCE)) {
        m_class_ptr->EnsureLoaded();

        m_class_ptr->GetFreeObjectFunction()(m_object_reference);
    }
}

void *Object::InvokeMethod_Internal(const Method *method_ptr, void **args_vptr, void *return_value_vptr)
{
    AssertThrow(method_ptr != nullptr);

    m_class_ptr->EnsureLoaded();

    return m_class_ptr->GetClassHolder()->GetInvokeMethodFunction()(method_ptr->GetGuid(), m_object_reference.guid, args_vptr, return_value_vptr);
}

const Method *Object::GetMethod(UTF8StringView method_name) const
{
    m_class_ptr->EnsureLoaded();

    auto it = m_class_ptr->GetMethods().FindAs(method_name);

    if (it == m_class_ptr->GetMethods().End()) {
        return nullptr;
    }

    return &it->second;
}

const Property *Object::GetProperty(UTF8StringView property_name) const
{
    m_class_ptr->EnsureLoaded();

    auto it = m_class_ptr->GetProperties().FindAs(property_name);

    if (it == m_class_ptr->GetProperties().End()) {
        return nullptr;
    }

    return &it->second;
}

} // namespace hyperion::dotnet