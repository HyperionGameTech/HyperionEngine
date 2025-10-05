/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClassUtils.hpp>
#include <core/object/HypClassRegistry.hpp>

namespace hyperion {

HypClassRegistrationBase::HypClassRegistrationBase(TypeId typeId, HypClass* hypClass)
    : m_hypClass(hypClass)
{
    HypClassRegistry::GetInstance().RegisterClass(typeId, hypClass);
}

} // namespace hyperion