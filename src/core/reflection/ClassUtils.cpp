/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <core/reflection/ClassUtils.hpp>
#include <core/reflection/ClassRegistry.hpp>

namespace Hyperion {

ClassRegistrationBase::ClassRegistrationBase(TypeId typeId, Class* cls)
    : m_class(cls)
{
    ClassRegistry::GetInstance().RegisterClass(typeId, cls);
}

} // namespace Hyperion