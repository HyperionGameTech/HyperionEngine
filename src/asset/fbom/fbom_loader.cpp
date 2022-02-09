#include "fbom.h"
#include "../byte_reader.h"

// marshal classes
#include "../../entity.h"
#include "../../terrain/noise_terrain/noise_terrain_control.h"
#include "../../rendering/mesh.h"
#include "../../rendering/material.h"
#include "../asset_manager.h"

namespace hyperion {
namespace fbom {

decltype(FBOMLoader::loaders) FBOMLoader::loaders = {
    { "ENTITY", FBOM_MARSHAL_CLASS(Entity) },
    { "NOISE_TERRAIN_CONTROL", FBOM_MARSHAL_CLASS(NoiseTerrainControl) },
    { "MESH", FBOM_MARSHAL_CLASS(Mesh) },
    { "MATERIAL", FBOM_MARSHAL_CLASS(Material) }
};

FBOMLoader::FBOMLoader()
    : root(new FBOMObject(FBOMObjectType("ROOT"))),
    m_in_static_data(false)
{
    last = root;

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

FBOMLoader::~FBOMLoader()
{
    if (root) {
        delete root;
    }
}

FBOMResult FBOMLoader::Deserialize(FBOMObject *in, FBOMDeserialized &out)
{
    ex_assert(in != nullptr);
    ex_assert(out == nullptr);
    ex_assert_msg(in->deserialized_object == nullptr, "Object was already deserialized");

    std::string object_type = in->m_object_type.name;

    auto it = loaders.find(object_type);

    if (it == loaders.end()) {
        return FBOMResult(FBOMResult::FBOM_ERR, std::string("No loader for type ") + object_type);
    }

    FBOMResult deserialize_result = it->second.m_deserializer(this, in, out);

    in->deserialized_object = out;

    return deserialize_result;
}

std::shared_ptr<Loadable> FBOMLoader::LoadFromFile(const std::string &path)
{
    // Include our root dir as part of the path
    std::string root_dir = AssetManager::GetInstance()->GetRootDir();
    ByteReader *reader = new FileByteReader(root_dir+path);

    m_static_data_pool.clear();
    m_in_static_data = false;

    // TODO: file header

    // expect first FBOMObject defined
    FBOMCommand command = FBOM_NONE;

    while (!reader->Eof()) {
        command = PeekCommand(reader);

        if (auto err = Handle(reader, command, root)) {
            throw std::runtime_error(err.message);
        }

        if (last == nullptr) {
            // end of file
            ex_assert_msg(reader->Eof(), "last was nullptr before eof reached");

            break;
        }
    }

    delete reader;

    hard_assert(root != nullptr);

    ex_assert_msg(root->nodes.size() == 1, "No object added to root (should be one)");
    ex_assert(root->nodes[0] != nullptr);

    return root->nodes[0]->deserialized_object;
}

FBOMCommand FBOMLoader::NextCommand(ByteReader *reader)
{
    hard_assert(!reader->Eof());

    uint8_t ins = 0;
    reader->Read(&ins);

    return FBOMCommand(ins);
}

FBOMCommand FBOMLoader::PeekCommand(ByteReader *reader)
{
    hard_assert(!reader->Eof());

    uint8_t ins = 0;
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
        return FBOMResult(FBOMResult::FBOM_ERR, std::string("unexpected command: expected ") + std::to_string(command) + ", received " + std::to_string(received));
    }

    return FBOMResult::FBOM_OK;
}

std::string FBOMLoader::ReadString(ByteReader *reader)
{
    // read 4 bytes of string length
    uint32_t len;
    reader->Read(&len);

    char *bytes = new char[len + 1];
    memset(bytes, 0, len + 1);

    reader->Read(bytes, len);

    std::string str(bytes);

    delete[] bytes;

    return str;
}

FBOMType FBOMLoader::ReadObjectType(ByteReader *reader)
{
    FBOMType result = FBOMUnset();

    uint8_t object_type_location = FBOM_DATA_LOCATION_NONE;
    reader->Read(&object_type_location);

    if (object_type_location == FBOM_DATA_LOCATION_INPLACE) {
        uint8_t extend_level;
        reader->Read(&extend_level);

        for (int i = 0; i < extend_level; i++) {
            std::string type_name = ReadString(reader);
            result.name = type_name;

            // read size of object
            uint64_t type_size;
            reader->Read(&type_size);
            result.size = type_size;

            if (i == extend_level - 1) {
                break;
            }

            result = result.Extend(FBOMUnset());
        }
    } else if (object_type_location == FBOM_DATA_LOCATION_STATIC) {
        // read offset as uint32_t
        uint32_t offset;
        reader->Read(&offset);

        // grab from static data pool
        ex_assert(m_static_data_pool.at(offset).type == FBOMStaticData::FBOM_STATIC_DATA_TYPE);
        result = m_static_data_pool.at(offset).type_data;
    }

    return result;
}

FBOMResult FBOMLoader::ReadData(ByteReader *reader, std::shared_ptr<FBOMData> &data)
{
    // read data location
    uint8_t object_type_location = FBOM_DATA_LOCATION_NONE;
    reader->Read(&object_type_location);

    if (object_type_location == FBOM_DATA_LOCATION_INPLACE) {
        FBOMType object_type = ReadObjectType(reader);

        uint32_t sz;
        reader->Read(&sz);

        unsigned char *bytes = new unsigned char[sz];
        reader->Read(bytes, sz);

        data.reset(new FBOMData(object_type));
        data->SetBytes(sz, bytes);

        delete[] bytes;
    } else if (object_type_location == FBOM_DATA_LOCATION_STATIC) {
        // read offset as uint32_t
        uint32_t offset;
        reader->Read(&offset);

        // grab from static data pool
        ex_assert(m_static_data_pool.at(offset).type == FBOMStaticData::FBOM_STATIC_DATA_DATA);
        data = m_static_data_pool.at(offset).data_data;
    }

    return FBOMResult::FBOM_OK;
}


FBOMResult FBOMLoader::ReadObject(ByteReader *reader, FBOMObject &object)
{
    if (auto err = Eat(reader, FBOM_OBJECT_START)) {
        return err;
    }

    FBOMCommand command = FBOM_NONE;

    // read data location
    uint8_t object_type_location = FBOM_DATA_LOCATION_NONE;
    reader->Read(&object_type_location);

    if (object_type_location == FBOM_DATA_LOCATION_STATIC) {
        // read offset as uint32_t
        uint32_t offset;
        reader->Read(&offset);

        // grab from static data pool
        ex_assert(m_static_data_pool.at(offset).type == FBOMStaticData::FBOM_STATIC_DATA_OBJECT);
        object = m_static_data_pool.at(offset).object_data;

        return FBOMResult::FBOM_OK;
    }

    if (object_type_location == FBOM_DATA_LOCATION_INPLACE) {
        // read string of "type" - loader to use
        FBOMType object_type = ReadObjectType(reader);

        auto it = loaders.find(object_type.name);

        if (it == loaders.end()) {
            return FBOMResult(FBOMResult::FBOM_ERR, std::string("Read object: No loader defined for ") + object_type.name);
        }

        object = FBOMObject(object_type);

        do {
            command = PeekCommand(reader);

            switch (command) {
            case FBOM_OBJECT_START:
            {
                FBOMObject child;
                if (auto err = ReadObject(reader, child)) {
                    return err;
                }

                object.nodes.push_back(std::make_shared<FBOMObject>(child));

                break;
            }
            case FBOM_OBJECT_END:
                if (auto err = Deserialize(&object, object.deserialized_object)) {
                    return FBOMResult(FBOMResult::FBOM_ERR, std::string("Read object: could not deserialize ") + object.m_object_type.name + " object: " + err.message);
                }

                break;
            case FBOM_DEFINE_PROPERTY:
            {
                if (auto err = Eat(reader, FBOM_DEFINE_PROPERTY)) {
                    return err;
                }

                std::string property_name = ReadString(reader);

                std::shared_ptr<FBOMData> data;

                if (auto err = ReadData(reader, data)) {
                    return err;
                }

                object.SetProperty(property_name, data);

                break;
            }
            default:
                return FBOMResult(FBOMResult::FBOM_ERR, std::string("Read object: cannot process command ") + std::to_string(command) + " while reading object");
            }
        } while (command != FBOM_OBJECT_END && command != FBOM_NONE);

        // eat end
        if (auto err = Eat(reader, FBOM_OBJECT_END)) {
            return err;
        }
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMLoader::Handle(ByteReader *reader, FBOMCommand command, FBOMObject *parent)
{
    std::string object_type;

    // TODO: pre-saving ObjectTypes at start, then using offset

    switch (command) {
    case FBOM_OBJECT_START:
    {
        FBOMObject child;

        if (auto err = ReadObject(reader, child)) {
            return err;
        }

        last->nodes.push_back(std::make_shared<FBOMObject>(child));

        break;
    }
    case FBOM_STATIC_DATA_START:
    {
        hard_assert(!m_in_static_data);

        if (auto err = Eat(reader, FBOM_STATIC_DATA_START)) {
            return err;
        }

        m_in_static_data = true;

        // read uint32_t describing size of static data pool
        uint32_t static_data_size;
        reader->Read(&static_data_size);
        const uint32_t initial_static_data_size = static_data_size;

        // skip 8 bytes of padding
        uint64_t tmp;
        reader->Read(&tmp);

        m_static_data_pool.resize(static_data_size);

        // now read each item
        //   uint32_t as index/offset
        //   uint8_t as type of static data
        //   then, the actual size of the data will vary depending on the held type
        for (uint32_t i = 0; i < static_data_size; i++) {
            uint32_t offset;
            reader->Read(&offset);

            if (offset >= initial_static_data_size) {
                return FBOMResult(FBOMResult::FBOM_ERR, std::string("read offset as ") + std::to_string(offset) + ", which is >= alotted size of static data pool (" + std::to_string(static_data_size) + ")");
            }

            uint8_t type;
            reader->Read(&type);

            switch (type)
            {
            case FBOMStaticData::FBOM_STATIC_DATA_NONE:
                m_static_data_pool[offset] = FBOMStaticData();

                break;
            case FBOMStaticData::FBOM_STATIC_DATA_OBJECT:
            {
                FBOMObject object;

                if (auto err = ReadObject(reader, object)) {
                    return err;
                }

                m_static_data_pool[offset] = FBOMStaticData(object, offset);

                break;
            }
            case FBOMStaticData::FBOM_STATIC_DATA_TYPE:
                m_static_data_pool[offset] = FBOMStaticData(ReadObjectType(reader), offset);

                break;
            case FBOMStaticData::FBOM_STATIC_DATA_DATA:
            {
                std::shared_ptr<FBOMData> data;

                if (auto err = ReadData(reader, data)) {
                    return err;
                }

                m_static_data_pool[offset] = FBOMStaticData(data, offset);

                break;
            }
            default:
                return FBOMResult(FBOMResult::FBOM_ERR, std::string("Cannot process static data type ") + std::to_string(type));
            }
        }

        break;
    }
    case FBOM_STATIC_DATA_END:
    {
        hard_assert(m_in_static_data);

        if (auto err = Eat(reader, FBOM_STATIC_DATA_END)) {
            return err;
        }

        m_in_static_data = false;

        break;
    }
    default:
        throw std::runtime_error(std::string("Cannot process command ") + std::to_string(int(command)) + " in top level");
    }

    return FBOMResult::FBOM_OK;
}

} // namespace fbom
} // namespace hyperion