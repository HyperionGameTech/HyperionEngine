#pragma once

#include <core/containers/FixedArray.hpp>

#include <core/reflection/TypeId.hpp>

#include <core/HashCode.hpp>

namespace hyperion {

enum class EntitySetId : uint64;

template <class... Components>
constexpr EntitySetId GetEntitySetId()
{
    FixedArray<TypeId, sizeof...(Components)> componentTypeIds = { TypeId::ForType<Components>()... };
    std::sort(
        componentTypeIds.m_values,
        componentTypeIds.m_values + componentTypeIds.Size(),
        [](const TypeId& a, const TypeId& b)
        {
            return a.Value() < b.Value();
        });

    HashCode hashCode;
    for (const TypeId& typeId : componentTypeIds)
    {
        hashCode.Add(typeId);
    }

    return EntitySetId(hashCode.Value());
}

} // namespace hyperion
