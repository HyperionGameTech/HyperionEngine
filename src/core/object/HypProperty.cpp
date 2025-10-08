/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypProperty.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypMethod.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <core/utilities/TypeInfo.hpp>

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
