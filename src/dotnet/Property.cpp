/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Property.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/Assembly.hpp>

namespace hyperion::dotnet {

void Property::InvokeGetter_Internal(const ManagedObject* pManagedObject, HypData* outReturnHypData)
{
    Assert(pManagedObject != nullptr);
    Assert(pManagedObject->GetClass() != nullptr);

    RC<Assembly> assembly = pManagedObject->GetClass()->GetAssembly();

    assembly->GetInvokeGetterFunction()(m_guid, const_cast<ObjectReference*>(&pManagedObject->GetObjectReference()), nullptr, outReturnHypData);
}

void Property::InvokeSetter_Internal(const ManagedObject* pManagedObject, const HypData** valueHypData)
{
    Assert(pManagedObject != nullptr);
    Assert(pManagedObject->GetClass() != nullptr);

    RC<Assembly> assembly = pManagedObject->GetClass()->GetAssembly();

    assembly->GetInvokeSetterFunction()(m_guid, const_cast<ObjectReference*>(&pManagedObject->GetObjectReference()), valueHypData, nullptr);
}

} // namespace hyperion::dotnet