/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <dotnet/ManagedClass.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/ManagedObject.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion::dotnet {

ManagedClass::~ManagedClass() = default;

RC<Assembly> ManagedClass::GetAssembly() const
{
    RC<Assembly> assembly = m_assembly.Lock();

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

    return new ManagedObject(RefCountedPtrFromThis(), objectReference);
}

ManagedObject* ManagedClass::NewObject(const Class* pClass, void* pOwner)
{
    Assert(pClass != nullptr);
    Assert(pOwner != nullptr);

    Assert(m_newObjectFptr != nullptr, "New object function pointer not set for managed class {}", m_name);

    ObjectReference objectReference = m_newObjectFptr(/* keepAlive */ true, pClass, pOwner, nullptr, nullptr);

    return new ManagedObject(RefCountedPtrFromThis(), objectReference);
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

bool ManagedClass::HasParentClass(const ManagedClass* pParentClass) const
{
    const ManagedClass* pCurrent = m_parentClass;

    while (pCurrent)
    {
        if (pCurrent == pParentClass)
        {
            return true;
        }

        pCurrent = pCurrent->GetParentClass();
    }

    return false;
}

void ManagedClass::InvokeStaticMethod_Internal(const ManagedMethod* pMethod, const BoxedValue** ppArgs, BoxedValue* pOutReturn)
{
    RC<Assembly> assembly = m_assembly.Lock();

    if (!assembly || !assembly->IsLoaded())
    {
        HYP_THROW("Cannot use managed class: assembly has been unloaded");
    }

    pMethod->Invoke({}, ppArgs, pOutReturn);
}

} // namespace hyperion::dotnet
