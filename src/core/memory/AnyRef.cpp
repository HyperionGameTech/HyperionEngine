/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/memory/AnyRef.hpp>

#include <core/reflection/BoxedValue.hpp>
#include <core/reflection/TypeInfo.hpp>

namespace Hyperion {
namespace memory {

TypeId AnyRefBase::GetTypeId() const
{
    return m_typeInfo ? m_typeInfo->id : TypeId::Void();
}

const Class* AnyRefBase::GetClass() const
{
    return m_typeInfo ? m_typeInfo->GetClass() : nullptr;
}

} // namespace memory
} // namespace Hyperion