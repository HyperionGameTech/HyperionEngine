#include "FBOM.hpp"
#include <core/lib/Path.hpp>

namespace hyperion::v2::fbom {

FBOMLoader::FBOMLoader(Engine *engine)
    : m_engine(engine),
      m_in_static_data(false)
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

FBOMLoader::~FBOMLoader() = default;

FBOMCommand FBOMLoader::NextCommand(ByteReader *reader)
{
    AssertThrow(!reader->Eof());

    UInt8 ins = 0;
    reader->Read(&ins);

    return FBOMCommand(ins);
}

FBOMCommand FBOMLoader::PeekCommand(ByteReader *reader)
{
    AssertThrow(!reader->Eof());

    UInt8 ins = 0;
    reader->Peek(&ins);

    return FBOMCommand(ins);
}

FBOMResult FBOMLoader::Eat(ByteReader *reader, FBOMCommand command, bool read)
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

String FBOMLoader::ReadString(ByteReader *reader)
{
    // read 4 bytes of string length
    UInt32 len;
    reader->Read(&len);

    char *bytes = new char[len + 1];
    memset(bytes, 0, len + 1);

    reader->Read(bytes, len);

    String str(bytes);

    delete[] bytes;

    return str;
}

FBOMType FBOMLoader::ReadObjectType(ByteReader *reader)
{
    FBOMType result = FBOMUnset();

    UInt8 object_type_location = FBOM_DATA_LOCATION_NONE;
    reader->Read(&object_type_location);

    switch (object_type_location) {
    case FBOM_DATA_LOCATION_INPLACE:
    {
        UInt8 extend_level;
        reader->Read(&extend_level);

        for (int i = 0; i < extend_level; i++) {
            result.name = ReadString(reader);

            // read size of object
            UInt64 type_size;
            reader->Read(&type_size);
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
        UInt32 offset;
        reader->Read(&offset);

        // grab from static data pool
        AssertThrow(m_static_data_pool.at(offset).type == FBOMStaticData::FBOM_STATIC_DATA_TYPE);
        result = m_static_data_pool.at(offset).type_data;

        break;
    }
    default:
        AssertThrowMsg(false, "Invalid data location type");
        break;
    }

    return result;
}

FBOMResult FBOMLoader::ReadData(ByteReader *reader, FBOMData &data)
{
    // read data location
    UInt8 object_type_location = FBOM_DATA_LOCATION_NONE;
    reader->Read(&object_type_location);

    if (object_type_location == FBOM_DATA_LOCATION_INPLACE) {
        FBOMType object_type = ReadObjectType(reader);

        UInt32 sz;
        reader->Read(&sz);

        unsigned char *bytes = new unsigned char[sz];
        reader->Read(bytes, sz);

        data = FBOMData(object_type);
        data.SetBytes(sz, bytes);

        delete[] bytes;
    } else if (object_type_location == FBOM_DATA_LOCATION_STATIC) {
        // read offset as u32
        UInt32 offset;
        reader->Read(&offset);

        // grab from static data pool
        AssertThrow(m_static_data_pool.at(offset).type == FBOMStaticData::FBOM_STATIC_DATA_DATA);
        data = m_static_data_pool.at(offset).data_data.Get();
    }

    return FBOMResult::FBOM_OK;
}


FBOMResult FBOMLoader::ReadObject(ByteReader *reader, FBOMObject &object, FBOMObject *parent)
{
    if (auto err = Eat(reader, FBOM_OBJECT_START)) {
        return err;
    }

    FBOMCommand command = FBOM_NONE;

    // read data location
    UInt8 object_type_location = FBOM_DATA_LOCATION_NONE;
    reader->Read(&object_type_location);

    switch (object_type_location) {
    case FBOM_DATA_LOCATION_STATIC:
    {
        // read offset as u32
        UInt32 offset;
        reader->Read(&offset);

        // grab from static data pool
        AssertThrow(m_static_data_pool.at(offset).type == FBOMStaticData::FBOM_STATIC_DATA_OBJECT);
        object = m_static_data_pool.at(offset).object_data;

        return FBOMResult::FBOM_OK;
    }
    case FBOM_DATA_LOCATION_INPLACE:
    {
        // read string of "type" - loader to use
        const FBOMType object_type = ReadObjectType(reader);

        object = FBOMObject(object_type);

        do {
            command = PeekCommand(reader);

            switch (command) {
            case FBOM_OBJECT_START:
            {
                FBOMObject child;

                if (auto err = ReadObject(reader, child, &object)) {
                    return err;
                }

                object.nodes->PushBack(std::move(child));

                break;
            }
            case FBOM_OBJECT_END:
                // call deserializer function, writing into object.deserialized
                if (auto err = Deserialize(object, object.deserialized)) {
                    return FBOMResult(FBOMResult::FBOM_ERR, "Could not deserialize object");
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

                object.SetProperty(property_name, data);

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
        
        // read object_index as u32,
        // for now this should just be zero but
        // later we can use to store other things in a sort of
        // "library" file, and page chunks in and out of memory
        UInt32 object_index;
        reader->Read(&object_index);

        // read flags
        UInt32 flags;
        reader->Read(&flags);

        // load the file {ref_name}.chunk relative to current file
        // this could also be stored in a map

        String base_path;

        if (parent != nullptr && parent->GetType().IsOrExtends("ROOT")) {
            if (auto err = parent->GetProperty("base_path").ReadString(base_path)) {
                return err;
            }
        }

        const String ref_path(FileSystem::Join(std::string(base_path.Data()), std::string(ref_name.Data())).data());

        if (auto err = FBOMLoader(m_engine).LoadFromFile(ref_path, object)) {
            return err;
        }

        break;
    }
    default:
        return FBOMResult(FBOMResult::FBOM_ERR, "Unknown object location type");
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMLoader::Handle(ByteReader *reader, FBOMCommand command, FBOMObject *parent)
{
    AssertThrow(parent != nullptr);

    switch (command) {
    case FBOM_OBJECT_START:
    {
        FBOMObject child;

        if (auto err = ReadObject(reader, child, parent)) {
            return err;
        }

        parent->nodes->PushBack(std::move(child));

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
        UInt32 static_data_size;
        reader->Read(&static_data_size);
        const auto initial_static_data_size = static_data_size;

        // skip 8 bytes of padding
        UInt64 tmp;
        reader->Read(&tmp);

        m_static_data_pool.resize(static_data_size);

        // now read each item
        //   u32 as index/offset
        //   u8 as type of static data
        //   then, the actual size of the data will vary depending on the held type
        for (UInt i = 0; i < static_data_size; i++) {
            UInt32 offset;
            reader->Read(&offset);

            if (offset >= initial_static_data_size) {
                return FBOMResult(FBOMResult::FBOM_ERR, "Offset out of bounds of static data pool");
            }

            UInt8 type;
            reader->Read(&type);

            switch (type)
            {
            case FBOMStaticData::FBOM_STATIC_DATA_NONE:
                m_static_data_pool[offset] = FBOMStaticData();

                break;
            case FBOMStaticData::FBOM_STATIC_DATA_OBJECT:
            {
                FBOMObject object;

                if (auto err = ReadObject(reader, object, parent)) {
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

                m_static_data_pool[offset] = FBOMStaticData(data, offset);

                break;
            }
            default:
                return FBOMResult(FBOMResult::FBOM_ERR, "Cannot process static data type, unknown type");
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