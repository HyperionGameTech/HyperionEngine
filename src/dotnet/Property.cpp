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

    object_ptr->GetClass()->EnsureLoaded();

    object_ptr->GetClass()->GetClassHolder()->GetInvokeGetterFunction()(m_guid, object_ptr->GetObjectReference().guid, nullptr, out_return_hyp_data);
}

void Property::InvokeSetter_Internal(const Object *object_ptr, const HypData **value_hyp_data)
{
    AssertThrow(object_ptr != nullptr);
    AssertThrow(object_ptr->GetClass() != nullptr);

    object_ptr->GetClass()->EnsureLoaded();

    object_ptr->GetClass()->GetClassHolder()->GetInvokeSetterFunction()(m_guid, object_ptr->GetObjectReference().guid, &value_hyp_data[0], nullptr);
}

} // namespace hyperion::dotnet