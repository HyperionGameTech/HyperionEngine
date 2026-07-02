/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <DotNET/ManagedProperty.hpp>
#include <DotNET/ManagedObject.hpp>
#include <DotNET/ManagedClass.hpp>
#include <DotNET/Assembly.hpp>

namespace Hyperion::dotnet {

void ManagedProperty::InvokeGetter_Internal(const ManagedObject* pManagedObject, BoxedValue* pOutBoxed)
{
    Assert(pManagedObject != nullptr);
    Assert(pManagedObject->GetClass() != nullptr);

    SharedPtr<Assembly> assembly = pManagedObject->GetClass()->GetAssembly();

    assembly->GetInvokeGetterFunction()(m_guid, const_cast<ObjectReference*>(&pManagedObject->GetObjectReference()), nullptr, pOutBoxed);
}

void ManagedProperty::InvokeSetter_Internal(const ManagedObject* pManagedObject, const BoxedValue** boxedValue)
{
    Assert(pManagedObject != nullptr);
    Assert(pManagedObject->GetClass() != nullptr);

    SharedPtr<Assembly> assembly = pManagedObject->GetClass()->GetAssembly();

    assembly->GetInvokeSetterFunction()(m_guid, const_cast<ObjectReference*>(&pManagedObject->GetObjectReference()), boxedValue, nullptr);
}

} // namespace Hyperion::dotnet
