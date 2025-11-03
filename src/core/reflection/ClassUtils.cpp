/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <core/reflection/ClassUtils.hpp>
#include <core/reflection/ClassRegistry.hpp>

namespace hyperion {

ClassRegistrationBase::ClassRegistrationBase(TypeId typeId, Class* cls)
    : m_class(cls)
{
    ClassRegistry::GetInstance().RegisterClass(typeId, cls);
}

} // namespace hyperion