/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/memory/AnyRef.hpp>

#include <core/object/HypData.hpp>

#include <core/utilities/TypeInfo.hpp>

namespace hyperion {
namespace memory {

TypeId AnyRefBase::GetTypeId() const
{
    return m_typeInfo ? m_typeInfo->id : TypeId::Void();
}

const HypClass* AnyRefBase::GetHypClass() const
{
    return m_typeInfo ? m_typeInfo->GetHypClass() : nullptr;
}

} // namespace memory
} // namespace hyperion