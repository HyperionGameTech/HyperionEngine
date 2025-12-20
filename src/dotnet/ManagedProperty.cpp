/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <dotnet/ManagedProperty.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/Assembly.hpp>

namespace hyperion::dotnet {

void ManagedProperty::InvokeGetter_Internal(const ManagedObject* pManagedObject, BoxedValue* outReturnHypData)
{
    Assert(pManagedObject != nullptr);
    Assert(pManagedObject->GetClass() != nullptr);

    RC<Assembly> assembly = pManagedObject->GetClass()->GetAssembly();

    assembly->GetInvokeGetterFunction()(m_guid, const_cast<ObjectReference*>(&pManagedObject->GetObjectReference()), nullptr, outReturnHypData);
}

void ManagedProperty::InvokeSetter_Internal(const ManagedObject* pManagedObject, const BoxedValue** valueHypData)
{
    Assert(pManagedObject != nullptr);
    Assert(pManagedObject->GetClass() != nullptr);

    RC<Assembly> assembly = pManagedObject->GetClass()->GetAssembly();

    assembly->GetInvokeSetterFunction()(m_guid, const_cast<ObjectReference*>(&pManagedObject->GetObjectReference()), valueHypData, nullptr);
}

} // namespace hyperion::dotnet
