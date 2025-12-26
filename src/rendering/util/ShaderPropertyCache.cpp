/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/util/ShaderPropertyCache.hpp>
#include <rendering/util/ShaderCompiler.hpp>

#include <core/threading/Mutex.hpp>

#include <core/utilities/IdGenerator.hpp>

namespace Hyperion {

struct ShaderPropertyCacheImpl
{
    Mutex mutex;
    HashMap<HashCode, ShaderPropertyId> propertyHashCodeToId;
    HashMap<ShaderPropertyId, ShaderProperty, DynamicNodeAllocator> idToProperty;
    IdGenerator idGenerator;
};

ShaderPropertyCache::ShaderPropertyCache()
    : m_pImpl(new ShaderPropertyCacheImpl())
{
}

ShaderPropertyCache::~ShaderPropertyCache()
{
    delete m_pImpl;
}

ShaderPropertyId ShaderPropertyCache::GetOrAssignIndex(const ShaderProperty& property)
{
    HYP_SCOPE;

    HashCode hashCode = property.GetHashCode();

    Mutex::Guard guard(m_pImpl->mutex);

    auto it = m_pImpl->propertyHashCodeToId.Find(hashCode);
    if (it != m_pImpl->propertyHashCodeToId.End())
    {
        return it->second;
    }
    // we want to start at zero to remove wasted slot in bitsets/arrays based on this
    ShaderPropertyId id = static_cast<ShaderPropertyId>(m_pImpl->idGenerator.Next() - 1);

    m_pImpl->propertyHashCodeToId.Insert(hashCode, id);
    m_pImpl->idToProperty.Insert(id, property);

    return id;
}

const ShaderProperty* ShaderPropertyCache::GetPropertyById(ShaderPropertyId id) const
{
    HYP_SCOPE;

    Mutex::Guard guard(m_pImpl->mutex);

    auto it = m_pImpl->idToProperty.Find(id);
    if (it != m_pImpl->idToProperty.End())
    {
        return &it->second;
    }

    return nullptr;
}

} // namespace Hyperion
