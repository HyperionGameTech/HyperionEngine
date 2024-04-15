/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>

namespace hyperion::v2::fbom {

FBOMReader::FBOMReader(const FBOMConfig &config)
    : m_config(config),
      m_in_static_data(false),
      m_swap_endianness(false)
{
    m_registered_types = {
        FBOMUnsignedInt(),
        FBOMUnsignedLong(),
        FBOMInt(),
        FBOMLong(),
        FBOMFloat(),
        FBOMBool(),
        FBOMByte(),
        FBOMString(),
        FBOMStruct(0),
        FBOMArray()
    };
}

FBOMReader::~FBOMReader() = default;

FBOMCommand FBOMReader::NextCommand(BufferedReader *reader)
{
    AssertThrow(!reader->Eof());

    uint8 ins = -1;
    reader->Read(&ins);
    CheckEndianness(ins);

    return FBOMCommand(ins);
}

FBOMCommand FBOMReader::PeekCommand(BufferedReader *reader)
{
    AssertThrow(!reader->Eof());

    uint8 ins = -1;
    reader->Peek(&ins);
    CheckEndianness(ins);

    return FBOMCommand(ins);
}

FBOMResult FBOMReader::Eat(BufferedReader *reader, FBOMCommand command, bool read)
{
    FBOMCommand received;

    if (read) {
        received = NextCommand(reader);
    } else {
        received = PeekCommand(reader);
    }

    if (received != command) {
        return FBOMResult(FBOMResult::FBOM_ERR, "Unexpected command");
    }

    return FBOMResult::FBOM_OK;
}

String FBOMReader::ReadString(BufferedReader *reader)
{
    // read 4 bytes of string length
    uint32 len;
    reader->Read(&len);
    CheckEndianness(len);

    char *bytes = new char[len + 1];
    memset(bytes, 0, len + 1);

    reader->Read(bytes, len);

    String str(bytes);

    delete[] bytes;

    return str;
}

FBOMType FBOMReader::ReadObjectType(BufferedReader *reader)
{
    FBOMType result = FBOMUnset();

    uint8 object_type_location = FBOM_DATA_LOCATION_NONE;
    reader->Read(&object_type_location);
    CheckEndianness(object_type_location);

    switch (object_type_location) {
    case FBOM_DATA_LOCATION_INPLACE:
    {
        uint8 extend_level;
        reader->Read(&extend_level);
        CheckEndianness(extend_level);

        for (int i = 0; i < extend_level; i++) {
            result.name = ReadString(reader);

            // read size of object
            uint64 type_size;
            reader->Read(&type_size);
            CheckEndianness(type_size);

            result.size = type_size;

            if (i == extend_level - 1) {
                break;
            }

            result = result.Extend(FBOMUnset());
        }

        break;
    }
    case FBOM_DATA_LOCATION_STATIC:
    {
        // read offset as u32
        uint32 offset;
        reader->Read(&offset);
        CheckEndianness(offset);

        // grab from static data pool
        AssertThrow(m_static_data_pool[offset].type == FBOMStaticData::FBOM_STATIC_DATA_TYPE);
        result = m_static_data_pool[offset].type_data;

        break;
    }
    default:
        AssertThrowMsg(false, "Invalid data location type");
        break;
    }

    return result;
}

FBOMResult FBOMReader::ReadData(BufferedReader *reader, FBOMData &data)
{
    // read data location
    uint8 object_type_location = FBOM_DATA_LOCATION_NONE;
    reader->Read(&object_type_location);
    CheckEndianness(object_type_location);

    if (object_type_location == FBOM_DATA_LOCATION_INPLACE) {
        FBOMType object_type = ReadObjectType(reader);

        uint32 sz;
        reader->Read(&sz);
        CheckEndianness(sz);

        ByteBuffer byte_buffer = reader->ReadBytes(sz);

        if (byte_buffer.Size() != sz) {
            return { FBOMResult::FBOM_ERR, "File is corrupted" };
        }

        data = FBOMData(object_type, std::move(byte_buffer));
    } else if (object_type_location == FBOM_DATA_LOCATION_STATIC) {
        // read offset as u32
        uint32 offset;
        reader->Read(&offset);
        CheckEndianness(offset);

        // grab from static data pool
        AssertThrow(m_static_data_pool[offset].type == FBOMStaticData::FBOM_STATIC_DATA_DATA);
        data = m_static_data_pool[offset].data_data.Get();
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadObject(BufferedReader *reader, FBOMObject &object, FBOMObject *root)
{
    if (auto err = Eat(reader, FBOM_OBJECT_START)) {
        return err;
    }

    FBOMCommand command = FBOM_NONE;

    // read unique ID
    uint64 unique_id;
    reader->Read(&unique_id);
    CheckEndianness(unique_id);

    // read data location
    uint8 object_type_location = FBOM_DATA_LOCATION_NONE;
    reader->Read(&object_type_location);
    CheckEndianness(object_type_location);

    switch (object_type_location) {
    case FBOM_DATA_LOCATION_STATIC:
    {
        // read offset as u32
        uint32 offset;
        reader->Read(&offset);
        CheckEndianness(offset);

        // grab from static data pool
        AssertThrow(m_static_data_pool[offset].type == FBOMStaticData::FBOM_STATIC_DATA_OBJECT);
        object = m_static_data_pool[offset].object_data;

        return FBOMResult::FBOM_OK;
    }
    case FBOM_DATA_LOCATION_INPLACE:
    {
        // read string of "type" - loader to use
        const FBOMType object_type = ReadObjectType(reader);

        // DebugLog("Read object ")

        object = FBOMObject(object_type);
        // object.m_unique_id = unique_id;

        do {
            command = PeekCommand(reader);

            switch (command) {
            case FBOM_OBJECT_START:
            {
                FBOMObject child;

                if (auto err = ReadObject(reader, child, root)) {
                    return err;
                }

                // // Debug sanity check
                // AssertThrow(object.nodes->FindIf([id = child.GetUniqueID()](const auto &item) { return item.GetUniqueID() == id; }) == object.nodes->End());

                object.nodes->PushBack(std::move(child));

                break;
            }
            case FBOM_OBJECT_END:
                // call deserializer function, writing into object.deserialized
                if (auto err = Deserialize(object, object.deserialized)) {
                    return err;
                }

                break;
            case FBOM_DEFINE_PROPERTY:
            {
                if (auto err = Eat(reader, FBOM_DEFINE_PROPERTY)) {
                    return err;
                }

                const auto property_name = ReadString(reader);

                FBOMData data;

                if (auto err = ReadData(reader, data)) {
                    return err;
                }

                object.SetProperty(property_name, std::move(data));

                break;
            }
            default:
                return FBOMResult(FBOMResult::FBOM_ERR, "Could not process command while reading object");
            }
        } while (command != FBOM_OBJECT_END && command != FBOM_NONE);

        // eat end
        if (auto err = Eat(reader, FBOM_OBJECT_END)) {
            return err;
        }

        break;
    }
    case FBOM_DATA_LOCATION_EXT_REF:
    {
        const auto ref_name = ReadString(reader);
        DebugLog(LogType::Debug, "FBOM: Ext ref: %s\n", ref_name.Data());
        
        // read object_index as u32,
        // for now this should just be zero but
        // later we can use to store other things in a sort of
        // "library" file, and page chunks in and out of memory
        uint32 object_index;
        reader->Read(&object_index);
        CheckEndianness(object_index);

        // read flags
        uint32 flags;
        reader->Read(&flags);
        CheckEndianness(flags);

        // load the file {ref_name}.chunk relative to current file
        // this could also be stored in a map

        String base_path = m_config.base_path;

        if (base_path.Empty()) {
            base_path = FilePath::Current();
        }

        const String ref_path(FileSystem::Join(FileSystem::CurrentPath(), base_path.Data(), ref_name.Data()).data());
        const String relative_path(FileSystem::RelativePath(std::string(ref_path.Data()), FileSystem::CurrentPath()).data());

        { // check in cache
            const auto it = m_config.external_data_cache.Find(Pair<String, uint32> { relative_path, object_index });

            if (it != m_config.external_data_cache.End()) {
                object = it->second;

                break;
            }
        }

        if (auto err = FBOMReader(m_config).LoadFromFile(ref_path, object)) {
            if (!m_config.continue_on_external_load_error) {
                return err;
            }
        }

        // cache it
        m_config.external_data_cache.Set(Pair<String, uint32> { relative_path, object_index }, object);

        break;
    }
    default:
        return FBOMResult(FBOMResult::FBOM_ERR, "Unknown object location type!");
    }

    // if (unique_id != uint64(object.m_unique_id)) {
    //     DebugLog(
    //         LogType::Warn,
    //         "unique id header for object does not match unique id stored in external object (%llu != %llu)\n",
    //         unique_id,
    //         uint64(object.m_unique_id)
    //     );
    // }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::Handle(BufferedReader *reader, FBOMCommand command, FBOMObject *root)
{
    AssertThrow(root != nullptr);

    switch (command) {
    case FBOM_OBJECT_START:
    {
        FBOMObject child;

        if (auto err = ReadObject(reader, child, root)) {
            return err;
        }

        // // Debug sanity check
        // AssertThrow(root->nodes->FindIf([id = child.GetUniqueID()](const auto &item) { return item.GetUniqueID() == id; }) == root->nodes->End());

        root->nodes->PushBack(child);

        break;
    }
    case FBOM_STATIC_DATA_START:
    {
        AssertThrow(!m_in_static_data);

        if (auto err = Eat(reader, FBOM_STATIC_DATA_START)) {
            return err;
        }

        m_in_static_data = true;

        // read u32 describing size of static data pool
        uint32 static_data_size;
        reader->Read(&static_data_size);
        CheckEndianness(static_data_size);

        const auto initial_static_data_size = static_data_size;

        // skip 8 bytes of padding
        uint64 tmp;
        reader->Read(&tmp);
        CheckEndianness(tmp);

        m_static_data_pool.Resize(static_data_size);

        // now read each item
        //   u32 as index/offset
        //   u8 as type of static data
        //   then, the actual size of the data will vary depending on the held type
        for (uint i = 0; i < static_data_size; i++) {
            uint32 offset;
            reader->Read(&offset);
            CheckEndianness(offset);

            if (offset >= initial_static_data_size) {
                return FBOMResult(FBOMResult::FBOM_ERR, "Offset out of bounds of static data pool");
            }

            uint8 type;
            reader->Read(&type);
            CheckEndianness(type);

            switch (type)
            {
            case FBOMStaticData::FBOM_STATIC_DATA_NONE:
                m_static_data_pool[offset] = FBOMStaticData();

                break;
            case FBOMStaticData::FBOM_STATIC_DATA_OBJECT:
            {
                FBOMObject object;

                if (auto err = ReadObject(reader, object, root)) {
                    return err;
                }

                m_static_data_pool[offset] = FBOMStaticData(std::move(object), offset);

                break;
            }
            case FBOMStaticData::FBOM_STATIC_DATA_TYPE:
                m_static_data_pool[offset] = FBOMStaticData(ReadObjectType(reader), offset);

                break;
            case FBOMStaticData::FBOM_STATIC_DATA_DATA:
            {
                FBOMData data;

                if (auto err = ReadData(reader, data)) {
                    return err;
                }

                m_static_data_pool[offset] = FBOMStaticData(std::move(data), offset);

                break;
            }
            default:
                return { FBOMResult::FBOM_ERR, "Cannot process static data type, unknown type" };
            }
        }

        break;
    }
    case FBOM_STATIC_DATA_END:
    {
        AssertThrow(m_in_static_data);

        if (auto err = Eat(reader, FBOM_STATIC_DATA_END)) {
            return err;
        }

        m_in_static_data = false;

        break;
    }
    default:
        AssertThrowMsg(false, "Cannot process command %d in top level", static_cast<int>(command));

        break;
    }

    return FBOMResult::FBOM_OK;
}

} // namespace hyperion::v2::fbom