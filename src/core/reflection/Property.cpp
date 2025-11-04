/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/reflection/Property.hpp>
#include <core/reflection/Field.hpp>
#include <core/reflection/Method.hpp>
#include <core/reflection/ClassRegistry.hpp>

#include <core/reflection/TypeInfo.hpp>

namespace hyperion {

const Class* Property::GetClass() const
{
    if (!m_typeInfo)
    {
        return nullptr;
    }

    return m_typeInfo->GetClass();
}

} // namespace hyperion
