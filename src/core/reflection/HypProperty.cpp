/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/reflection/HypProperty.hpp>
#include <core/reflection/HypField.hpp>
#include <core/reflection/HypMethod.hpp>
#include <core/reflection/HypClassRegistry.hpp>

#include <core/reflection/TypeInfo.hpp>

namespace hyperion {

const HypClass* HypProperty::GetHypClass() const
{
    if (!m_typeInfo)
    {
        return nullptr;
    }

    return m_typeInfo->GetHypClass();
}

} // namespace hyperion
