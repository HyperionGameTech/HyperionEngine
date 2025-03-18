/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/Object.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion::dotnet {

Class::~Class()
{
    HYP_LOG(DotNET, Debug, "Class {} destroyed", m_name);
}

void Class::EnsureLoaded() const
{
    if (!m_class_holder || !m_class_holder->CheckAssemblyLoaded()) {
        HYP_THROW("Cannot use managed class: assembly has been unloaded");
    }

    AssertThrowMsg(m_class_holder->GetInvokeMethodFunction() != nullptr, "Invoke method function pointer not set!");

    AssertThrowMsg(GetNewObjectFunction() != nullptr, "Free object function pointer not set!");
    AssertThrowMsg(GetFreeObjectFunction() != nullptr, "Free object function pointer not set!");
}

Object Class::NewObject()
{
    EnsureLoaded();

    AssertThrowMsg(m_new_object_fptr != nullptr, "New object function pointer not set for managed class %s", m_name.Data());

    ObjectReference object_reference = m_new_object_fptr(/* keep_alive */ true, nullptr, nullptr, nullptr, nullptr);

    return Object(this, object_reference);
}

Object Class::NewObject(const HypClass *hyp_class, void *owning_object_ptr)
{
    EnsureLoaded();

    AssertThrow(hyp_class != nullptr);
    AssertThrow(owning_object_ptr != nullptr);

    AssertThrowMsg(m_new_object_fptr != nullptr, "New object function pointer not set for managed class %s", m_name.Data());

    ObjectReference object_reference = m_new_object_fptr(/* keep_alive */ true, hyp_class, owning_object_ptr, nullptr, nullptr);

    return Object(this, object_reference);
}

ObjectReference Class::NewManagedObject(void *context_ptr, InitializeObjectCallbackFunction callback)
{
    EnsureLoaded();

    AssertThrowMsg(m_new_object_fptr != nullptr, "New object function pointer not set for managed class %s", m_name.Data());

    return m_new_object_fptr(/* keep_alive */ false, nullptr, nullptr, context_ptr, callback);
}

bool Class::HasParentClass(UTF8StringView parent_class_name) const
{
    const Class *parent_class = m_parent_class;

    while (parent_class) {
        if (parent_class->GetName() == parent_class_name) {
            return true;
        }

        parent_class = parent_class->GetParentClass();
    }

    return false;
}

bool Class::HasParentClass(const Class *parent_class) const
{
    const Class *current_parent_class = m_parent_class;

    while (current_parent_class) {
        if (current_parent_class == parent_class) {
            return true;
        }

        current_parent_class = current_parent_class->GetParentClass();
    }

    return false;
}

void Class::InvokeStaticMethod_Internal(const Method *method_ptr, const HypData **args_hyp_data, HypData *out_return_hyp_data)
{
    EnsureLoaded();
    
    AssertThrowMsg(m_class_holder->GetInvokeMethodFunction() != nullptr, "Invoke method function pointer not set");

    m_class_holder->GetInvokeMethodFunction()(method_ptr->GetGuid(), {}, &args_hyp_data[0], out_return_hyp_data);
}

} // namespace hyperion::dotnet