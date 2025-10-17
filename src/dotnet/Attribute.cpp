/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Attribute.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/ManagedObject.hpp>

namespace hyperion::dotnet {

AttributeSet::AttributeSet(Array<Attribute>&& values)
    : m_values(std::move(values))
{
    for (Attribute& attribute : m_values)
    {
        Assert(attribute.object != nullptr);
        Assert(attribute.object->GetClass() != nullptr);

        m_valuesByName.Set(attribute.object->GetClass()->GetName(), &attribute);
    }
}

} // namespace hyperion::dotnet