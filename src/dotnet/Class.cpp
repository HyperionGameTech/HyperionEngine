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

RC<Assembly> Class::GetAssembly() const
{
    RC<Assembly> assembly = m_assembly.Lock();

    if (!assembly || !assembly->IsLoaded()) {
        HYP_THROW("Cannot use managed class: assembly has been unloaded");
    }

    AssertThrowMsg(assembly->GetInvokeMethodFunction() != nullptr, "Invoke method function pointer not set!");

    return assembly;
}

Object *Class::NewObject()
{
    AssertThrowMsg(m_new_object_fptr != nullptr, "New object function pointer not set for managed class %s", m_name.Data());

    ObjectReference object_reference = m_new_object_fptr(/* keep_alive */ true, nullptr, nullptr, nullptr, nullptr);

    return new Object(RefCountedPtrFromThis(), object_reference);
}

Object *Class::NewObject(const HypClass *hyp_class, void *owning_object_ptr)
{
    AssertThrow(hyp_class != nullptr);
    AssertThrow(owning_object_ptr != nullptr);

    AssertThrowMsg(m_new_object_fptr != nullptr, "New object function pointer not set for managed class %s", m_name.Data());

    ObjectReference object_reference = m_new_object_fptr(/* keep_alive */ true, hyp_class, owning_object_ptr, nullptr, nullptr);

    return new Object(RefCountedPtrFromThis(), object_reference);
}

ObjectReference Class::NewManagedObject(void *context_ptr, InitializeObjectCallbackFunction callback)
{
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
    RC<Assembly> assembly = m_assembly.Lock();
    
    if (!assembly || !assembly->IsLoaded()) {
        HYP_THROW("Cannot use managed class: assembly has been unloaded");
    }
    
    AssertThrowMsg(assembly->GetInvokeMethodFunction() != nullptr, "Invoke method function pointer not set");

    assembly->GetInvokeMethodFunction()(method_ptr->GetGuid(), {}, &args_hyp_data[0], out_return_hyp_data);
}

} // namespace hyperion::dotnet