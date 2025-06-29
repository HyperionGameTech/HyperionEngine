/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClassUtils.hpp>
#include <core/object/HypClassRegistry.hpp>

namespace hyperion {

HypClassRegistrationBase::HypClassRegistrationBase(TypeId type_id, HypClass* hyp_class)
{
    HypClassRegistry::GetInstance().RegisterClass(type_id, hyp_class);
}

} // namespace hyperion