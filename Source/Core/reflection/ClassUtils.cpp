/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/reflection/ClassUtils.hpp>
#include <Core/reflection/ClassRegistry.hpp>

namespace Hyperion {

ClassRegistrationBase::ClassRegistrationBase(const TypeId& typeId, Class* cls)
    : m_class(cls)
{
    ClassRegistry::GetInstance().RegisterClass(typeId, cls);
}

} // namespace Hyperion