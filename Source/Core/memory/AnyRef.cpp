/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Core/memory/AnyRef.hpp>

#include <Core/reflection/BoxedValue.hpp>
#include <Core/reflection/TypeInfo.hpp>

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