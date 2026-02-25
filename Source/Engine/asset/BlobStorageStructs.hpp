/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Types.hpp>

#include <Core/io/ByteReader.hpp>
#include <Core/io/ByteWriter.hpp>

namespace Hyperion {

struct BlobHeader
{
    uint8 magic[4];
    uint32 version : 8;
    uint32 payloadOffset : 24;
    uint64 payloadSize;
};

constexpr uint64 InvalidBufferOffset = uint64(-1);

HYP_STRUCT()
struct BlobDataReference
{
    HYP_STRUCT_BODY(BlobDataReference)

    HYP_FIELD()
    Name key;
    
    HYP_FIELD()
    uint64 size = 0;
    
    HYP_FIELD(Transient)
    void* raw = nullptr;
    
    HYP_FIELD(Transient)
    bool readOnly = false;

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(key)
            .Combine(size);
    }
};

class BlobTableOfContents
{
    enum class SlotState : uint8
    {
        Empty = 0,
        Occupied = 1,
        Deleted = 2
    };

public:
    static constexpr uint32 MaxKeyLength = 512;

    struct Value
    {
        uint32 page;
        uint64 offset;
        uint64 size;
    };

    struct Entry
    {
        StringHash key;
        Value value;
        SlotState state;
    };

private:
    struct MapHeader
    {
        uint64 capacity;
        uint64 size;
        uint64 deleted;
    };

    ByteBuffer mem;

    MapHeader* GetHeader()
    { 
        return reinterpret_cast<MapHeader*>(mem.Data()); 
    }

    const MapHeader* GetHeader() const
    { 
        return reinterpret_cast<const MapHeader*>(mem.Data()); 
    }

    Entry* GetBuckets()
    {
        return reinterpret_cast<Entry*>(mem.Data() + sizeof(MapHeader));
    }

public:
    explicit BlobTableOfContents(SizeType capacity = 1024)
    {
        AllocateNew(capacity);
    }

    bool Insert(StringHash key, const Value& value)
    {
        MapHeader* header = GetHeader();

        if (header->size * 10 >= header->capacity * 7)
        {
            Resize(header->capacity * 2);
        }

        return Insert_Internal(header, GetBuckets(), key, value);
    }

    bool Get(StringHash key, Value& outValue) const
    {
        const MapHeader* header = GetHeader();
        const Entry* entries = reinterpret_cast<const Entry*>(mem.Data() + sizeof(MapHeader));

        SizeType idx = key.GetHashCode().Value() % header->capacity;
        SizeType startIdx = idx;

        while (entries[idx].state != SlotState::Empty)
        {
            if (entries[idx].state == SlotState::Occupied && key == entries[idx].key)
            {
                outValue = entries[idx].value;

                return true;
            }

            idx = (idx + 1) % header->capacity;

            if (idx == startIdx)
                return false;
        }

        return false;
    }

    void Put(StringHash key, const Value& value)
    {
        MapHeader* header = GetHeader();

        // resize if (occupied + deleted) > 70% of Capacity
        SizeType totalLoad = header->size + header->deleted;
        if (totalLoad * 10 >= header->capacity * 7)
        {
            Resize(header->capacity * 2);
        }

        header = GetHeader();

        Entry* entries = GetBuckets();

        size_t idx = key.GetHashCode().Value() % header->capacity;
        
        int64 firstDeleted = -1; 

        while (entries[idx].state != SlotState::Empty)
        {
            if (entries[idx].state == SlotState::Occupied)
            {
                if (key == entries[idx].key)
                {
                    // FOUND: Update existing value
                    entries[idx].value = value;
                    return;
                }
            }
            else if (entries[idx].state == SlotState::Deleted)
            {
                // remember deleted elem to recycle slot
                if (firstDeleted == -1)
                {
                    firstDeleted = idx;
                }
            }
            
            idx = (idx + 1) % header->capacity;
        }

        // key not found. insert new.
        // if we passed a deleted element, recycle it. otherwise use the current EMPTY slot.
        SizeType insertIdx = (firstDeleted != -1) ? SizeType(firstDeleted) : idx;

        if (entries[insertIdx].state == SlotState::Deleted)
        {
            header->deleted--; // We are reviving a dead slot
        }
        
        entries[idx].key = key;
        entries[insertIdx].value = value;
        entries[insertIdx].state = SlotState::Occupied;

        header->size++;
    }

    bool Delete(StringHash key)
    {
        MapHeader* header = GetHeader();
        Entry* entries = GetBuckets();

        SizeType idx = key.GetHashCode().Value() % header->capacity;
        SizeType startIdx = idx;

        while (entries[idx].state != SlotState::Empty)
        {
            if (entries[idx].state == SlotState::Occupied && key == entries[idx].key)
            {
                // mark deleted
                entries[idx].state = SlotState::Deleted;
                header->size--;
                header->deleted++;
                return true;
            }
            
            idx = (idx + 1) % header->capacity;

            if (idx == startIdx)
                return false;
        }

        return false; // Not found
    }

    const void* Data() const
    {
        return mem.Data();
    }

    void Save(ByteWriter& stream) const
    {
        stream.Write(mem);
    }

    static Result Load(ByteReader& stream, BlobTableOfContents& outToc)
    {
        if (stream.Eof())
        {
            return HYP_MAKE_ERROR(Error, "Unexpected end of file");
        }

        const SizeType numToRead = stream.Max() - stream.Position();

        outToc.mem.SetSize(numToRead);

        SizeType numRead = stream.Read((void*)outToc.mem.Data(), numToRead);

        if (numRead != numToRead)
        {
            return HYP_MAKE_ERROR(Error, "Read size ({}) does not equal expected size ({})", numRead, numToRead);
        }

        return {};
    }

private:
    bool Insert_Internal(MapHeader* header, Entry* entry, StringHash key, const Value& value)
    {
        SizeType idx = key.GetHashCode().Value() % header->capacity;
        SizeType startIdx = idx;

        while (entry[idx].state == SlotState::Occupied) {

            if (key == entry[idx].key)
            {
                entry[idx].value = value; // Update

                return true;
            }

            idx = (idx + 1) % header->capacity;

            if (idx == startIdx)
                return false;
        }

        entry[idx].key = key;
        entry[idx].value = value;
        entry[idx].state = SlotState::Occupied;
        
        header->size++;

        return true;
    }

    void AllocateNew(SizeType capacity)
    {
        SizeType totalBytes = sizeof(MapHeader) + (capacity * sizeof(Entry));
        mem.SetSize(totalBytes, /* zeroize */ true);

        MapHeader* header = GetHeader();
        
        header->capacity = capacity;
        header->size = 0;
        header->deleted = 0;
    }

    void Resize(SizeType newCapacity)
    {
        ByteBuffer newMem;
        SizeType totalBytes = sizeof(MapHeader) + (newCapacity * sizeof(Entry));
        newMem.SetSize(totalBytes, /* zeroize */ true);

        Entry* newBuckets = reinterpret_cast<Entry*>(newMem.Data() + sizeof(MapHeader));
        
        MapHeader* newHeader = reinterpret_cast<MapHeader*>(newMem.Data());
        newHeader->capacity = newCapacity;
        newHeader->size = 0;
        newHeader->deleted = 0;

        // rehash
        MapHeader* oldHeader = GetHeader();
        Entry* oldBuckets = GetBuckets();
        
        for (SizeType i = 0; i < oldHeader->capacity; ++i) 
        {
            // Only copy OCCUPIED
            if (oldBuckets[i].state == SlotState::Occupied)
            {
                Insert_Internal(newHeader, newBuckets, oldBuckets[i].key, oldBuckets[i].value);
            }
        }

        std::swap(newMem, mem);
    }
};

} // namespace Hyperion
