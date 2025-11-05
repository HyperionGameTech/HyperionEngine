/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <dotnet/ManagedAttribute.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/ManagedObject.hpp>

namespace hyperion::dotnet {

ManagedAttributeSet::ManagedAttributeSet(Array<UniquePtr<ManagedObject>>&& values)
    : m_values(std::move(values))
{
    for (UniquePtr<ManagedObject>& obj : m_values)
    {
        Assert(obj != nullptr);
        Assert(obj->GetClass() != nullptr);

        m_valuesByName.Set(obj->GetClass()->GetName(), obj.Get());
    }
}

} // namespace hyperion::dotnet
