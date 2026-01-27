/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/util/ShaderPropertyCache.hpp>
#include <rendering/util/ShaderCompiler.hpp>

#include <core/io/ByteWriter.hpp>
#include <core/io/BufferedByteReader.hpp>

#include <core/threading/SharedMutex.hpp>

#include <core/utilities/IdGenerator.hpp>

namespace Hyperion {

static const ShaderProperty s_staticProperties[] = {
    ShaderProperty()
};

struct HashedShaderProperty
{
    HashCode nameAndValueHash;

    HashedShaderProperty() = default;

    explicit HashedShaderProperty(const ShaderProperty& property)
        : nameAndValueHash(property.GetHashCode())
    {
    }

    HYP_FORCE_INLINE constexpr bool operator==(const HashedShaderProperty& other) const
    {
        return nameAndValueHash == other.nameAndValueHash;
    }

    HYP_FORCE_INLINE constexpr const HashCode& GetHashCode() const
    {
        return nameAndValueHash;
    }
};

static HashMap<HashedShaderProperty, ShaderPropertyId>& GetShaderPropertyCacheMap()
{
    static HashMap<HashedShaderProperty, ShaderPropertyId> s_propertyToIdMap;
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

    HashedShaderProperty hashed = HashedShaderProperty(property);

    auto it = shaderPropertyCacheMap.Find(hashed);
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
            shaderPropertyCacheMap.Insert(hashed, static_cast<ShaderPropertyId>(i));

            return static_cast<ShaderPropertyId>(i);
        }
    }

    lock.Reset();

    TUniqueLock uniqueLock(GetShaderPropertyCacheMutex());

    // double check in case another thread added it while we were unlocking
    it = shaderPropertyCacheMap.Find(hashed);
    if (it != shaderPropertyCacheMap.End())
    {
        return it->second;
    }

    const ShaderPropertyId newId = static_cast<ShaderPropertyId>(uint32(std::size(s_staticProperties)) + shaderPropertyCacheMap.Size());
    shaderPropertyCacheMap.Insert(hashed, newId);

    return newId;
}

constexpr uint16 ShaderPropertyDatabaseVersion = 1;
constexpr SizeType SizeOfHashedProperty = sizeof(HashedShaderProperty);
constexpr SizeType SizeOfEntry = 16; // 8 + 4 + 4 bytes padding

void WriteShaderPropertyDatabase(ByteWriter& stream)
{
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
        const HashedShaderProperty& hashedProperty = it.first;
        ShaderPropertyId id = it.second;
        
        if (visited.Contains(id))
        {
            continue;
        }

        stream.Seek(entryMapOffset + uint32(id) * SizeOfEntry);
        stream.Write(&hashedProperty, sizeof(HashedShaderProperty));
        stream.Write(&id, sizeof(ShaderPropertyId));

        maxShaderId = MathUtil::Max(maxShaderId, uint32(id));

        visited.Add(id);
    }

    stream.Seek(headerOffset);

    // version
    stream.Write<uint16>(ShaderPropertyDatabaseVersion);
    stream.Write<uint16>(0);
    // entry count
    stream.Write<uint32>(maxShaderId + 1);
}

void ReadShaderPropertyDatabase(BufferedByteReader& stream)
{
    const SizeType readOffset = stream.Position();

    uint16 version;
    stream.Read<uint16>(&version);
    if (version != ShaderPropertyDatabaseVersion)
    {
        HYP_LOG(Core, Error, "Shader property database version mismatch: expected {} but got {}", ShaderPropertyDatabaseVersion, version);
        return;
    }

    stream.Skip(sizeof(uint16));

    uint32 entryCount;
    stream.Read<uint32>(&entryCount);

    stream.Seek(readOffset + 64); // skip reserved space
    
    TUniqueLock lock(GetShaderPropertyCacheMutex());
    
    auto& shaderPropertyCacheMap = GetShaderPropertyCacheMap();
    shaderPropertyCacheMap.Reserve(entryCount);

    ubyte* bytes = (ubyte*)Memory::Allocate(entryCount * SizeOfEntry);
    if (stream.ReadBytes(bytes, entryCount * SizeOfEntry) != entryCount * SizeOfEntry)
    {
        HYP_LOG(Core, Error, "Shader property database is corrupt!");
    }

    ubyte* pBytes = bytes;
    const ubyte* pEndBytes = bytes + (entryCount * SizeOfEntry);

    while (pBytes != pEndBytes)
    {
        HashedShaderProperty* hashed = reinterpret_cast<HashedShaderProperty*>(pBytes);
        ShaderPropertyId* id = reinterpret_cast<ShaderPropertyId*>(pBytes + sizeof(HashedShaderProperty));

        shaderPropertyCacheMap.Insert(*hashed, *id);

        pBytes += SizeOfEntry;
    }

    Memory::Free(bytes);
}

} // namespace Hyperion
