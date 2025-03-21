/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Property.hpp>
#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>

namespace hyperion::dotnet {

void Property::InvokeGetter_Internal(const Object *object_ptr, HypData *out_return_hyp_data)
{
    AssertThrow(object_ptr != nullptr);
    AssertThrow(object_ptr->GetClass() != nullptr);

    RC<Assembly> assembly = object_ptr->GetClass()->GetAssembly();

    assembly->GetInvokeGetterFunction()(m_guid, const_cast<ObjectReference *>(&object_ptr->GetObjectReference()), nullptr, out_return_hyp_data);
}

void Property::InvokeSetter_Internal(const Object *object_ptr, const HypData **value_hyp_data)
{
    AssertThrow(object_ptr != nullptr);
    AssertThrow(object_ptr->GetClass() != nullptr);

    RC<Assembly> assembly = object_ptr->GetClass()->GetAssembly();

    assembly->GetInvokeSetterFunction()(m_guid, const_cast<ObjectReference *>(&object_ptr->GetObjectReference()), value_hyp_data, nullptr);
}

} // namespace hyperion::dotnet