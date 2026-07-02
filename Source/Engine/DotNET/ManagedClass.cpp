/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <DotNET/ManagedClass.hpp>
#include <DotNET/Assembly.hpp>
#include <DotNET/ManagedObject.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

namespace Hyperion::dotnet {

ManagedClass::~ManagedClass() = default;

SharedPtr<Assembly> ManagedClass::GetAssembly() const
{
    SharedPtr<Assembly> assembly = m_assembly.Lock();

    if (!assembly || !assembly->IsLoaded())
    {
        HYP_THROW("Cannot use managed class: assembly has been unloaded");
    }

    return assembly;
}

ManagedObject* ManagedClass::NewObject()
{
    Assert(m_newObjectFptr != nullptr, "New object function pointer not set for managed class {}", m_name);

    ObjectReference objectReference = m_newObjectFptr(/* keepAlive */ true, nullptr, nullptr, nullptr, nullptr);

    return new ManagedObject(SharedThis(), objectReference);
}

ManagedObject* ManagedClass::NewObject(const Class* cls, void* owner)
{
    Assert(cls != nullptr);
    Assert(owner != nullptr);

    Assert(m_newObjectFptr != nullptr, "New object function pointer not set for managed class {}", m_name);

    ObjectReference objectReference = m_newObjectFptr(/* keepAlive */ true, cls, owner, nullptr, nullptr);

    return new ManagedObject(SharedThis(), objectReference);
}

ObjectReference ManagedClass::NewManagedObject(void* pCtx, InitializeObjectCallbackFunction pCallback)
{
    Assert(m_newObjectFptr != nullptr, "New object function pointer not set for managed class {}", m_name);

    return m_newObjectFptr(/* keepAlive */ false, nullptr, nullptr, pCtx, pCallback);
}

bool ManagedClass::HasParentClass(ANSIStringView parentClassName) const
{
    const ManagedClass* parentClass = m_parentClass;

    while (parentClass)
    {
        if (parentClass->GetName() == parentClassName)
        {
            return true;
        }

        parentClass = parentClass->GetParentClass();
    }

    return false;
}

bool ManagedClass::HasParentClass(const ManagedClass* parentClass) const
{
    const ManagedClass* current = parentClass;

    while (current)
    {
        if (current == parentClass)
        {
            return true;
        }

        current = current->GetParentClass();
    }

    return false;
}

void ManagedClass::InvokeStaticMethod_Internal(const ManagedMethod* method, const BoxedValue** args, BoxedValue* outReturn)
{
    SharedPtr<Assembly> assembly = m_assembly.Lock();

    if (!assembly || !assembly->IsLoaded())
    {
        HYP_THROW("Cannot use managed class: assembly has been unloaded");
    }

    method->Invoke({}, args, outReturn);
}

} // namespace Hyperion::dotnet
