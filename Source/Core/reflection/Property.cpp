/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/reflection/Property.hpp>
#include <Core/reflection/Field.hpp>
#include <Core/reflection/Method.hpp>
#include <Core/reflection/ClassRegistry.hpp>

#include <Core/reflection/TypeInfo.hpp>

namespace Hyperion {

const Class* Property::GetClass() const
{
    if (!m_typeInfo)
    {
        return nullptr;
    }

    return m_typeInfo->GetClass();
}

} // namespace Hyperion
