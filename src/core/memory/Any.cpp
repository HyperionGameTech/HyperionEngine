/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <core/memory/Any.hpp>

#include <core/utilities/TypeInfo.hpp>

namespace hyperion {
namespace memory {

TypeId Any::GetTypeId() const
{
    return HasValue() ? reinterpret_cast<const Block*>(m_block)->typeInfo->id : TypeId::Void();
}

} // namespace memory
} // namespace hyperion