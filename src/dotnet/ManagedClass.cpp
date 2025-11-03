/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

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
    Assert(m_newObjectFptr != nullptr, "New object function pointer not set for managed class %s", m_name.Data());

    ObjectReference objectReference = m_newObjectFptr(/* keepAlive */ true, nullptr, nullptr, nullptr, nullptr);

    return new ManagedObject(RefCountedPtrFromThis(), objectReference);
}

ManagedObject* ManagedClass::NewObject(const Class* cls, void* pOwner)
{
    Assert(cls != nullptr);
    Assert(pOwner != nullptr);

    Assert(m_newObjectFptr != nullptr, "New object function pointer not set for managed class %s", m_name.Data());

    ObjectReference objectReference = m_newObjectFptr(/* keepAlive */ true, cls, pOwner, nullptr, nullptr);

    return new ManagedObject(RefCountedPtrFromThis(), objectReference);
}

ObjectReference ManagedClass::NewManagedObject(void* contextPtr, InitializeObjectCallbackFunction callback)
{
    Assert(m_newObjectFptr != nullptr, "New object function pointer not set for managed class %s", m_name.Data());

    return m_newObjectFptr(/* keepAlive */ false, nullptr, nullptr, contextPtr, callback);
}

bool ManagedClass::HasParentClass(UTF8StringView parentClassName) const
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
    const ManagedClass* currentParentClass = m_parentClass;

    while (currentParentClass)
    {
        if (currentParentClass == parentClass)
        {
            return true;
        }

        currentParentClass = currentParentClass->GetParentClass();
    }

    return false;
}

void ManagedClass::InvokeStaticMethod_Internal(const Method* methodPtr, const HypData** argsHypData, HypData* outReturnHypData)
{
    RC<Assembly> assembly = m_assembly.Lock();

    if (!assembly || !assembly->IsLoaded())
    {
        HYP_THROW("Cannot use managed class: assembly has been unloaded");
    }

    methodPtr->Invoke({}, argsHypData, outReturnHypData);
}

} // namespace hyperion::dotnet