/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypProperty.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <Engine.hpp>

namespace hyperion {

const HypClass *HypProperty::GetHypClass() const
{
    return HypClassRegistry::GetInstance().GetClass(type_id);
}

} // namespace hyperion