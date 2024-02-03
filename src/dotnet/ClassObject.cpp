#include <dotnet/ClassObject.hpp>
#include <dotnet/Assembly.hpp>

namespace hyperion::dotnet {

ManagedObject ClassObject::NewObject()
{
    AssertThrowMsg(m_new_object_fptr != nullptr, "New object function pointer not set!");

    return m_new_object_fptr();
}

void ClassObject::FreeObject(ManagedObject object)
{
    AssertThrowMsg(m_free_object_fptr != nullptr, "Free object function pointer not set!");

    m_free_object_fptr(object);
}

void *ClassObject::InvokeMethod(ManagedMethod *method_object, void *this_vptr, void **args_vptr, void *return_value_vptr)
{
    AssertThrowMsg(m_parent != nullptr, "Parent not set!");
    AssertThrowMsg(m_parent->GetInvokeMethodFunction() != nullptr, "Invoke method function pointer not set!");

    return m_parent->GetInvokeMethodFunction()(method_object, this_vptr, args_vptr, return_value_vptr);
}

} // namespace hyperion::dotnet