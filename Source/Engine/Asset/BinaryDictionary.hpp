/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

#include <Core/Containers/Map.hpp>
#include <Core/Containers/Set.hpp>
#include <Core/Containers/SparsePagedArray.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Memory/Memory.hpp>

#include <Core/Threading/SharedMutex.hpp>
#include <Core/Threading/LockGuard.hpp>

#include <Core/IO/ByteWriter.hpp>
#include <Core/IO/ByteReader.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Debug/Debug.hpp>

namespace Hyperion {

/*! \brief BinaryDictionary is a generic serializable dictionary that maps values to integral IDs
 *
 *  It assigns each unique value, identified by its HashCode, to a contiguous integer ID to allow
 *  the IDs to be used in bitsets, as that is a common pattern throughout the codebase.
 *
 *  \tparam Value     The value type to intern. Must be copyable and support HashCode::GetHashCode().
 *  \tparam IdType    An enum or integral type used as the ID. Must be at most 4 bytes.
 *  \tparam PageSize  Page size passed to the internal SparsePagedArray reverse map. Must be a power of two. */
template <class Value, class IdType, uint32 PageSize = 128>
class BinaryDictionary
{
    static_assert(sizeof(IdType) <= sizeof(uint32), "IdType type must be at most 4 bytes");

    static constexpr uint16 FormatVersion = 1;

    // Entry layout (fixed 16 bytes): HashCode value (uint64, 8 bytes) + id stored as uint32 (4 bytes) + 4 bytes padding
    static constexpr size_t EntryDataSize = sizeof(HashCode::ValueType) + sizeof(uint32);
    static constexpr size_t EntryPaddingSize = (16 - (EntryDataSize % 16)) % 16;
    static constexpr size_t SizeOfEntry = EntryDataSize + EntryPaddingSize;

public:
    BinaryDictionary() = default;
    virtual ~BinaryDictionary() = default;

    BinaryDictionary(const BinaryDictionary&) = delete;
    BinaryDictionary& operator=(const BinaryDictionary&) = delete;

    /*! \brief Mark the dictionary as initialized, stopping accumulation of the static-entry hash.
     *  Call this once all statically-known entries have been interned. */
    void Initialize()
    {
        if (m_initialized)
        {
            return;
        }

        m_initialized = true;
    }

    /*! \brief Intern a value, returning its stable ID. Thread-safe.
     *  If the value has not been seen before a new ID is assigned and the value is stored.
     *  Entries added before Initialize() is called contribute to the static-entry hash used for versioning. */
    IdType Intern(const Value& value)
    {
        const HashCode hash = HashCode::GetHashCode(value);

        TSharedLock lock(m_mutex);

        auto it = m_forwardMap.Find(hash);
        if (it != m_forwardMap.End())
        {
            const IdType id = it->second;

            if (!m_reverseMap.HasIndex(uint32(id)))
            {
                // upgrade to exclusive lock to populate the reverse map
                lock.Reset();

                TUniqueLock uniqueLock(m_mutex);
                m_reverseMap.Emplace(uint32(id), value);
            }

            return id;
        }

#if defined(HYP_SHIPPING) && HYP_SHIPPING
        // Shipping builds never insert - every value must already be in the cooked dictionary.
        Assert(false, "Value not found in cooked shaderprops.bin (hash={})", hash.Value());
        return IdType(0);
#else
        lock.Reset();

        TUniqueLock uniqueLock(m_mutex);

        // double-check in case another thread inserted while we were re-locking
        it = m_forwardMap.Find(hash);
        if (it != m_forwardMap.End())
        {
            return it->second;
        }

        const IdType newId = static_cast<IdType>(m_forwardMap.Size());
        m_forwardMap.Insert(hash, newId);
        m_reverseMap.Emplace(uint32(newId), value);

        if (!m_initialized)
        {
            m_staticEntryHashCode = m_staticEntryHashCode.Combine(hash);
        }

        return newId;
#endif
    }

    /*! \brief Look up a value by ID. Thread-safe.
     *  \return True if the ID was found and outValue was populated. */
    bool GetById(IdType id, Value& outValue) const
    {
        TSharedLock lock(m_mutex);

        if (!m_reverseMap.HasIndex(uint32(id)))
        {
            return false;
        }

        outValue = m_reverseMap.Get(uint32(id));

        return true;
    }

    /*! \brief Serialize the dictionary to a binary stream.
     *  The stream is written with a 64-byte reserved header followed by a flat array of fixed-size entries. */
    void Write(ByteWriter& stream) const
    {
#if defined(HYP_SHIPPING) && HYP_SHIPPING
        // Shipping builds treat the cooked dictionary as read-only.
        (void)stream;
#else
        TSharedLock lock(m_mutex);

        Set<IdType> visited;
        visited.Reserve(m_forwardMap.Size());

        const uint32 headerOffset = stream.Position();

        // reserve 64 bytes for header at start
        stream.Seek(headerOffset + 64);

        const size_t entryMapOffset = stream.Position();

        uint32 maxIdValue = 0;

        for (const auto& kvp : m_forwardMap)
        {
            const HashCode& hash = kvp.first;
            const IdType id = kvp.second;

            if (visited.Contains(id))
            {
                continue;
            }

            stream.Seek(entryMapOffset + uint32(id) * SizeOfEntry);

            const HashCode::ValueType hashValue = hash.Value();
            stream.Write(&hashValue, sizeof(HashCode::ValueType));

            const uint32 idValue = uint32(id);
            stream.Write(&idValue, sizeof(uint32));

            if constexpr (EntryPaddingSize > 0)
            {
                uint8 padding[EntryPaddingSize] {};
                stream.Write(padding, EntryPaddingSize);
            }

            maxIdValue = MathUtil::Max(maxIdValue, uint32(id));

            visited.Add(id);
        }

        stream.Seek(headerOffset);

        // Header layout (16 bytes used, 48 bytes reserved):
        //   uint16 version
        //   uint16 padding
        //   uint32 entryCount
        //   uint64 staticEntryHash
        stream.Write<uint16>(FormatVersion);
        stream.Write<uint16>(0);
        stream.Write<uint32>(visited.Empty() ? 0 : maxIdValue + 1);
        stream.Write<uint64>(m_staticEntryHashCode.Value());
#endif
    }

    /*! \brief Deserialize the dictionary from a binary stream.
     *  Only the forward (hash -> ID) map is populated; the reverse map is rebuilt lazily via Intern().
     *  \return True on success. Returns false and rolls back the stream on version or hash mismatch. */
    bool Read(ByteReader& stream)
    {
        const size_t readOffset = stream.Position();

        uint16 version;
        stream.Read<uint16>(&version);

        if (version != FormatVersion)
        {
            HYP_LOG(Core, Error, "BinaryDictionary format version mismatch: expected {} but got {}", FormatVersion, version);
            return false;
        }

        stream.Skip(sizeof(uint16));

        uint32 entryCount;
        stream.Read<uint32>(&entryCount);

        uint64 staticHashValue;
        stream.Read<uint64>(&staticHashValue);

#if !defined(HYP_SHIPPING) || !HYP_SHIPPING
        // Shipping treats the cooked dictionary as authoritative regardless of this hash -
        // static globals no longer intern during static init, so there's nothing to compare against.
        if (staticHashValue != m_staticEntryHashCode.Value())
        {
            HYP_LOG(Core, Warning,
                "BinaryDictionary static entry hash mismatch: expected {} but got {}. "
                "This usually indicates that the static entries have changed since the dictionary was created.\n"
                "The dictionary will be removed and regenerated to mitigate potential issues.",
                m_staticEntryHashCode.Value(), staticHashValue);

            stream.Seek(readOffset); // roll back to start

            return false;
        }
#endif

        stream.Seek(readOffset + 64); // skip reserved header space

        TUniqueLock lock(m_mutex);

        m_forwardMap.Reserve(entryCount);

        ubyte* bytes = (ubyte*)Memory::Allocate(entryCount * SizeOfEntry);
        size_t readBytes = 0;

        if ((readBytes = stream.Read(static_cast<void*>(bytes), size_t(entryCount * SizeOfEntry))) != entryCount * SizeOfEntry)
        {
            HYP_LOG(Core, Error, "BinaryDictionary is corrupt! Read {} bytes, expected {}.",
                readBytes, entryCount * SizeOfEntry);

            Memory::Free(bytes);

            return false;
        }

        const ubyte* pBytes = bytes;
        const ubyte* pEndBytes = bytes + (entryCount * SizeOfEntry);

        while (pBytes != pEndBytes)
        {
            const HashCode::ValueType hashValue = *reinterpret_cast<const HashCode::ValueType*>(pBytes);
            const uint32 idValue = *reinterpret_cast<const uint32*>(pBytes + sizeof(HashCode::ValueType));

            m_forwardMap.Insert(HashCode(hashValue), static_cast<IdType>(idValue));

            pBytes += SizeOfEntry;
        }

        Memory::Free(bytes);

        return true;
    }

protected:
    /*! \brief Accumulated hash of all entries added before Initialize() was called.
     *  Used to detect static-schema changes during deserialization. */
    HashCode m_staticEntryHashCode {};

    /*! \brief True once Initialize() has been called; stops accumulation of m_staticEntryHashCode. */
    bool m_initialized = false;

private:
    Map<HashCode, IdType> m_forwardMap;
    SparsePagedArray<Value, PageSize> m_reverseMap;
    mutable SharedMutex m_mutex;
};

} // namespace Hyperion
