#pragma once

#include <Core/Containers/FixedArray.hpp>

#include <Core/Reflection/TypeId.hpp>

#include <Core/HashCode.hpp>

namespace Hyperion {

using EntitySetId = uint64;

// stub type used to skip for entity set id creation
struct VoidComponentType;

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
        if (typeId == TypeId::ForType<VoidComponentType>())
        {
            continue;
        }

        hashCode.Add(typeId);
    }

    return EntitySetId(hashCode.Value());
}

} // namespace Hyperion
