/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <dotnet/ManagedProperty.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/Assembly.hpp>

namespace Hyperion::dotnet {

void ManagedProperty::InvokeGetter_Internal(const ManagedObject* pManagedObject, BoxedValue* pOutBoxed)
{
    Assert(pManagedObject != nullptr);
    Assert(pManagedObject->GetClass() != nullptr);

    RC<Assembly> assembly = pManagedObject->GetClass()->GetAssembly();

    assembly->GetInvokeGetterFunction()(m_guid, const_cast<ObjectReference*>(&pManagedObject->GetObjectReference()), nullptr, pOutBoxed);
}

void ManagedProperty::InvokeSetter_Internal(const ManagedObject* pManagedObject, const BoxedValue** ppBoxed)
{
    Assert(pManagedObject != nullptr);
    Assert(pManagedObject->GetClass() != nullptr);

    RC<Assembly> assembly = pManagedObject->GetClass()->GetAssembly();

    assembly->GetInvokeSetterFunction()(m_guid, const_cast<ObjectReference*>(&pManagedObject->GetObjectReference()), ppBoxed, nullptr);
}

} // namespace Hyperion::dotnet
