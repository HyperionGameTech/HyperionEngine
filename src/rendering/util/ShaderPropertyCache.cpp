/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/util/ShaderPropertyCache.hpp>
#include <rendering/util/ShaderCompiler.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/threading/SharedMutex.hpp>

#include <core/utilities/IdGenerator.hpp>

namespace Hyperion {

static const ShaderProperty s_staticProperties[] = {
    ShaderProperty()
};

static HashMap<ShaderProperty, ShaderPropertyId>& GetShaderPropertyCacheMap()
{
    static HashMap<ShaderProperty, ShaderPropertyId> s_propertyToIdMap;
    return s_propertyToIdMap;
}

static SharedMutex& GetShaderPropertyCacheMutex()
{
    static SharedMutex s_propertyToIdMapMutex;
    return s_propertyToIdMapMutex;
}

ShaderPropertyId InternShaderProperty(const ShaderProperty& property)
{
    auto& shaderPropertyCacheMap = GetShaderPropertyCacheMap();

    TSharedLock lock(GetShaderPropertyCacheMutex());

    auto it = shaderPropertyCacheMap.Find(property);
    if (it != shaderPropertyCacheMap.End())
    {
        return it->second;
    }

    // check static properties
    for (SizeType i = 0; i < std::size(s_staticProperties); i++)
    {
        if (s_staticProperties[i] == property)
        {
            // found in static properties; put in map for faster access next time
            lock.Reset();

            TUniqueLock uniqueLock(GetShaderPropertyCacheMutex());
            shaderPropertyCacheMap.Insert(property, static_cast<ShaderPropertyId>(i));

            return static_cast<ShaderPropertyId>(i);
        }
    }

    lock.Reset();

    TUniqueLock uniqueLock(GetShaderPropertyCacheMutex());

    // double check in case another thread added it while we were unlocking
    it = shaderPropertyCacheMap.Find(property);
    if (it != shaderPropertyCacheMap.End())
    {
        return it->second;
    }

    const ShaderPropertyId newId = static_cast<ShaderPropertyId>(uint32(std::size(s_staticProperties)) + shaderPropertyCacheMap.Size());
    shaderPropertyCacheMap.Insert(property, newId);

    return newId;
}


struct HashedShaderProperty
{
    HashCode nameAndValueHash;

    explicit HashedShaderProperty(const ShaderProperty& property)
        : nameAndValueHash(property.GetHashCode())
    {
    }
};

void WriteShaderDatabase(ByteWriter& stream)
{
    constexpr SizeType SizeOfHashedProperty = sizeof(HashedShaderProperty);
    constexpr SizeType SizeOfEntry = SizeOfHashedProperty + sizeof(ShaderPropertyId);

    TSharedLock lock(GetShaderPropertyCacheMutex());

    HashSet<ShaderPropertyId> visited;
    visited.Reserve(GetShaderPropertyCacheMap().Size());

    const uint32 headerOffset = stream.Position();

    // reserve 64 bytes space at start
    stream.Seek(64);

    const SizeType entryMapOffset = stream.Position();

    uint32 maxShaderId = 0;

    for (uint32 i = 0; i < uint32(std::size(s_staticProperties)); i++)
    {
        ShaderPropertyId id = static_cast<ShaderPropertyId>(i);
        const ShaderProperty& property = s_staticProperties[i];
        
        HashedShaderProperty hashed = HashedShaderProperty(property);

        stream.Seek(entryMapOffset + uint32(id) * SizeOfEntry);
        stream.Write(&hashed, sizeof(HashedShaderProperty));
        stream.Write(&id, sizeof(ShaderPropertyId));

        maxShaderId = MathUtil::Max(maxShaderId, uint32(id));

        visited.Add(id);
    }

    // now, do runtime hashed propertyies
    for (const auto& it : GetShaderPropertyCacheMap())
    {
        const ShaderProperty& property = it.first;
        ShaderPropertyId id = it.second;
        
        if (visited.Contains(id))
        {
            continue;
        }
        
        HashedShaderProperty hashed = HashedShaderProperty(property);

        stream.Seek(entryMapOffset + uint32(id) * SizeOfEntry);
        stream.Write(&hashed, sizeof(HashedShaderProperty));
        stream.Write(&id, sizeof(ShaderPropertyId));

        maxShaderId = MathUtil::Max(maxShaderId, uint32(id));

        visited.Add(id);
    }

    // seek back to start, write (max id + 1) as a marker
    stream.Seek(headerOffset);

    stream.Write<uint32>(maxShaderId + 1);
}

} // namespace Hyperion
