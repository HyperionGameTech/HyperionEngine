/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/util/ShaderPropertyDictionary.hpp>
#include <rendering/util/ShaderCompiler.hpp>

#include <core/io/ByteWriter.hpp>
#include <core/io/BufferedByteReader.hpp>

#include <core/threading/SharedMutex.hpp>

#include <core/utilities/IdGenerator.hpp>

namespace Hyperion {

// properties that are created at program initialization
// are used to version the dictionary, as adding or removing
// these properties will offset all subsequent runtime-added properties,
// so we need to force a new version when that happens.
HashCode s_staticPropertyIdHashCode {};
static bool s_initShaderPropertyDictionary = false;

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

static HashMap<HashedShaderProperty, ShaderPropertyId>& GetShaderPropertyMap()
{
    static HashMap<HashedShaderProperty, ShaderPropertyId> s_propertyToIdMap;
    return s_propertyToIdMap;
}

static SparsePagedArray<ShaderProperty, 128>& GetShaderPropertyReverseMap()
{
    static SparsePagedArray<ShaderProperty, 128> s_idToPropertyMap;
    return s_idToPropertyMap;
}

static SharedMutex& GetShaderPropertyCacheMutex()
{
    static SharedMutex s_propertyToIdMapMutex;
    return s_propertyToIdMapMutex;
}

ShaderPropertyId InternShaderProperty(const ShaderProperty& property)
{
    auto& shaderPropertyCacheMap = GetShaderPropertyMap();

    TSharedLock lock(GetShaderPropertyCacheMutex());

    HashedShaderProperty hashed = HashedShaderProperty(property);

    auto it = shaderPropertyCacheMap.Find(hashed);
    if (it != shaderPropertyCacheMap.End())
    {
        const ShaderPropertyId id = it->second;
        if (!GetShaderPropertyReverseMap().HasIndex(uint32(id)))
        {
            // upgrade to unique lock to set in reverse map
            lock.Reset();
            
            TUniqueLock uniqueLock(GetShaderPropertyCacheMutex());
            GetShaderPropertyReverseMap().Emplace(uint32(id), property);
        }

        return id;
    }

    lock.Reset();

    TUniqueLock uniqueLock(GetShaderPropertyCacheMutex());

    // double check in case another thread added it while we were unlocking
    it = shaderPropertyCacheMap.Find(hashed);
    if (it != shaderPropertyCacheMap.End())
    {
        return it->second;
    }

    const ShaderPropertyId newId = static_cast<ShaderPropertyId>(shaderPropertyCacheMap.Size());
    shaderPropertyCacheMap.Insert(hashed, newId);

    GetShaderPropertyReverseMap().Emplace(uint32(newId), property);

    if (!s_initShaderPropertyDictionary)
    {
        s_staticPropertyIdHashCode = s_staticPropertyIdHashCode.Combine(hashed.GetHashCode());
    }

    return newId;
}

bool GetShaderPropertyById(ShaderPropertyId propertyId, ShaderProperty& outProperty)
{
    TSharedLock lock(GetShaderPropertyCacheMutex());

    auto& reverseMap = GetShaderPropertyReverseMap();

    if (!reverseMap.HasIndex(uint32(propertyId)))
    {
        return false;
    }

    outProperty = reverseMap.Get(uint32(propertyId));

    return true;
}

constexpr uint16 FormatVersion = 1;
constexpr SizeType SizeOfHashedProperty = sizeof(HashedShaderProperty);
constexpr SizeType SizeOfEntry = 16; // 8 + 4 + 4 bytes padding

void InitShaderPropertyDictionary()
{
    if (s_initShaderPropertyDictionary)
    {
        return;
    }

    s_initShaderPropertyDictionary = true;
}

void WriteShaderPropertyDictionary(ByteWriter& stream)
{
    TSharedLock lock(GetShaderPropertyCacheMutex());

    HashSet<ShaderPropertyId> visited;
    visited.Reserve(GetShaderPropertyMap().Size());

    const uint32 headerOffset = stream.Position();

    // reserve 64 bytes space at start
    stream.Seek(64);

    const SizeType entryMapOffset = stream.Position();

    uint32 maxShaderId = 0;

    // now, do runtime hashed propertyies
    for (const auto& it : GetShaderPropertyMap())
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

        // write padding bytes at end of entry
        constexpr uint16 NumPaddingBytes = SizeOfEntry - sizeof(ShaderPropertyId) - sizeof(HashedShaderProperty);
        if constexpr (NumPaddingBytes > 0)
        {
            uint8 paddingBytes[NumPaddingBytes] {};
            stream.Write(paddingBytes, NumPaddingBytes);
        }

        maxShaderId = MathUtil::Max(maxShaderId, uint32(id));

        visited.Add(id);
    }

    stream.Seek(headerOffset);

    // version
    stream.Write<uint16>(FormatVersion);
    stream.Write<uint16>(0);
    // entry count
    stream.Write<uint32>(maxShaderId + 1);
    // static property hash
    stream.Write<uint64>(s_staticPropertyIdHashCode.Value());
}

void ReadShaderPropertyDictionary(BufferedByteReader& stream)
{
    const SizeType readOffset = stream.Position();

    uint16 version;
    stream.Read<uint16>(&version);
    if (version != FormatVersion)
    {
        HYP_LOG(Core, Error, "Shader property dictionary format version mismatch: expected {} but got {}", FormatVersion, version);
        return;
    }

    stream.Skip(sizeof(uint16));

    uint32 entryCount;
    stream.Read<uint32>(&entryCount);

    uint64 staticPropertyHashValue;
    stream.Read<uint64>(&staticPropertyHashValue);

    if (staticPropertyHashValue != s_staticPropertyIdHashCode.Value())
    {
        HYP_LOG(Core, Warning, "Shader property dictionary static property hash mismatch: expected {} but got {}. "
                               "This usually indicates that the static shader properties have changed since the dictionary was created.\n"
                               "The dictionary will be removed and regenerated to mitigate potential issues.",
            s_staticPropertyIdHashCode.Value(), staticPropertyHashValue);

        stream.Seek(readOffset); // roll back to start

        return;
    }

    stream.Seek(readOffset + 64); // skip reserved space
    
    TUniqueLock lock(GetShaderPropertyCacheMutex());
    
    auto& shaderPropertyCacheMap = GetShaderPropertyMap();
    shaderPropertyCacheMap.Reserve(entryCount);

    ubyte* bytes = (ubyte*)Memory::Allocate(entryCount * SizeOfEntry);
    SizeType readBytes = 0;

    if ((readBytes = stream.ReadBytes(bytes, entryCount * SizeOfEntry)) != entryCount * SizeOfEntry)
    {
        HYP_LOG(Core, Error, "Shader property dictionary is corrupt! Read {} bytes, expected {}.",
            readBytes, entryCount * SizeOfEntry);

        Memory::Free(bytes);

        return;
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
