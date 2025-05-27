/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Attribute.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/Object.hpp>

namespace hyperion::dotnet {

AttributeSet::AttributeSet(Array<Attribute>&& values)
    : m_values(std::move(values))
{
    for (Attribute& attribute : m_values)
    {
        AssertThrow(attribute.object != nullptr);
        AssertThrow(attribute.object->GetClass() != nullptr);

        m_values_by_name.Set(attribute.object->GetClass()->GetName(), &attribute);
    }
}

} // namespace hyperion::dotnet