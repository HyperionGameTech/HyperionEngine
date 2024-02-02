#include <dotnet_support/ClassObject.hpp>
#include <dotnet_support/Assembly.hpp>

namespace hyperion::dotnet {

void *ClassObject::InvokeMethod(ManagedMethod *method_object, void **args_vptr)
{
    AssertThrowMsg(m_parent != nullptr, "Parent not set!");
    AssertThrowMsg(m_parent->GetInvokeMethodFunction() != nullptr, "Invoke method function pointer not set!");

    return m_parent->GetInvokeMethodFunction()(method_object, args_vptr);
}

} // namespace hyperion::dotnet