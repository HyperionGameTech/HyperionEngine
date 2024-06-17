/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/HypClassProperty.hpp>
#include <core/HypClassRegistry.hpp>

#include <Engine.hpp>

namespace hyperion {

const HypClass *HypClassProperty::GetHypClass() const
{
    return HypClassRegistry::GetInstance().GetClass(type_id);
}

} // namespace hyperion