/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypProperty.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypMethod.hpp>
#include <core/object/HypClassRegistry.hpp>

namespace hyperion {

const HypClass* HypProperty::GetHypClass() const
{
    return HypClassRegistry::GetInstance().GetClass(m_typeId);
}

} // namespace hyperion
