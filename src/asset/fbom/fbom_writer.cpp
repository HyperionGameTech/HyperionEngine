#include "fbom.h"
#include "../byte_writer.h"

#include <stack>
#include <algorithm>

namespace hyperion {
namespace fbom {

FBOMWriter::FBOMWriter()
{
}

FBOMWriter::~FBOMWriter()
{
}

FBOMResult FBOMWriter::Serialize(FBOMLoadable *in, FBOMObject *out) const
{
    AssertThrow(in != nullptr);
    AssertThrow(out != nullptr);

    std::string object_type = in->GetLoadableType().name;

    auto it = FBOMLoader::loaders.find(object_type);

    if (it == FBOMLoader::loaders.end()) {
        return FBOMResult(FBOMResult::FBOM_ERR, std::string("No loader for type ") + object_type);
    }

    return it->second.m_serializer(this, in, out);
}

FBOMResult FBOMWriter::Append(FBOMLoadable *loadable)
{
    if (loadable == nullptr) {
        return (m_write_stream.m_last_result = FBOMResult(FBOMResult::FBOM_ERR, "null object appended to write stream"));
    }

    if (auto err = m_write_stream.m_last_result) {
        return err;
    }

    FBOMObject base(loadable->GetLoadableType());

    if (auto err = Serialize(loadable, &base)) {
        m_write_stream.m_last_result = err;

        return err;
    }

    return Append(base);
}

FBOMResult FBOMWriter::Append(FBOMObject object)
{
    AddObjectData(object);

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::Emit(ByteWriter *out)
{
    if (auto err = m_write_stream.m_last_result) {
        return err;
    }

    BuildStaticData();
    WriteStaticDataToByteStream(out);

    for (const auto &it : m_write_stream.m_object_data) {
        if (auto err = WriteObject(out, it)) {
            m_write_stream.m_last_result = err;

            return err;
        }
    }

    return FBOMResult::FBOM_OK;
}

void FBOMWriter::BuildStaticData()
{
    for (auto &object : m_write_stream.m_object_data) {
        Prune(&object); // TODO: ensure that no invalidation occurs by m_object_data being modified
    }
}

void FBOMWriter::Prune(FBOMObject *object)
{
    AddStaticData(object->m_object_type);

    for (const auto &node : object->nodes) {
        AssertContinue(node != nullptr);

        Prune(node.get());
    }

    for (auto &prop : object->properties) {
        AssertContinue(prop.second != nullptr);

        int property_value_usage = m_write_stream.m_hash_use_count_map[prop.second->GetHashCode().Value()];

        // if (property_value_usage > 1) {
        AddStaticData(prop.second);
        // }
    }

    // convert use counts into static objects
    HashCode::Value_t hc = object->GetHashCode().Value();
    int object_usage = 0;

    auto it = m_write_stream.m_hash_use_count_map.find(hc);

    if (it != m_write_stream.m_hash_use_count_map.end()) {
        object_usage = it->second;
    }

    //if (object_usage > 1) {
        AddStaticData(*object);
    //}
}

FBOMResult FBOMWriter::WriteStaticDataToByteStream(ByteWriter *out)
{
    std::vector<FBOMStaticData> static_data_ordered;
    static_data_ordered.reserve(m_write_stream.m_static_data.size());

    for (auto &it : m_write_stream.m_static_data) {
        static_data_ordered.push_back(it.second);
    }

    std::sort(static_data_ordered.begin(), static_data_ordered.end(), [](auto &a, auto &b) {
        return a.offset < b.offset;
    });

    out->Write(uint8_t(FBOM_STATIC_DATA_START));

    // write SIZE of static data as uint32_t
    out->Write(uint32_t(static_data_ordered.size()));
    // 8 bytes of padding
    out->Write(uint64_t(0));

    // static data
    //   uint32_t as index/offset
    //   uint8_t as type of static data
    //   then, the actual size of the data will vary depending on the held type

    for (auto &it : static_data_ordered) {
        out->Write(uint32_t(it.offset));
        out->Write(uint8_t(it.type));

        switch (it.type) {
        case FBOMStaticData::FBOM_STATIC_DATA_OBJECT:
            if (auto err = WriteObject(out, it.object_data)) {
                return err;
            }
            break;
        case FBOMStaticData::FBOM_STATIC_DATA_TYPE:
            if (auto err = WriteObjectType(out, it.type_data)) {
                return err;
            }
            break;
        case FBOMStaticData::FBOM_STATIC_DATA_DATA:
            if (auto err = WriteData(out, it.data_data)) {
                return err;
            }
            break;
        default:
            return FBOMResult(FBOMResult::FBOM_ERR, "cannot write static object to bytestream");
        }
    }

    out->Write(uint8_t(FBOM_STATIC_DATA_END));

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteObject(ByteWriter *out, const FBOMObject &object)
{
    out->Write(uint8_t(FBOM_OBJECT_START));

    FBOMStaticData static_data;
    const HashCode::Value_t hash_code = object.GetHashCode().Value();
    uint8_t data_location = uint8_t(m_write_stream.GetDataLocation(hash_code, static_data));
    out->Write(data_location);

    if (data_location == FBOM_DATA_LOCATION_STATIC) {
        return WriteStaticDataUsage(out, static_data);
    }

    if (data_location == FBOM_DATA_LOCATION_INPLACE) {
        // write typechain
        if (auto err = WriteObjectType(out, object.m_object_type)) {
            return err;
        }

        // add all properties
        for (auto &it : object.properties) {
            out->Write(uint8_t(FBOM_DEFINE_PROPERTY));

            // // write property name
            out->WriteString(it.first);

            if (auto err = WriteData(out, it.second)) {
                return err;
            }
        }

        // now write out all child nodes
        for (auto &node : object.nodes) {
            AssertContinue(node != nullptr);

            if (auto err = WriteObject(out, *node)) {
                return err;
            }
        }

        out->Write(uint8_t(FBOM_OBJECT_END));

        m_write_stream.MarkStaticDataWritten(hash_code);
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteObject(ByteWriter *out, FBOMLoadable *loadable)
{
    AssertThrow(out != nullptr);

    FBOMObject base(loadable->GetLoadableType());
    if (auto err = Serialize(loadable, &base)) {
        return err;
    }

    return WriteObject(out, base);
}

FBOMResult FBOMWriter::WriteObjectType(ByteWriter *out, const FBOMType &type)
{
    FBOMStaticData static_data;
    const HashCode::Value_t hash_code = type.GetHashCode().Value();
    uint8_t data_location = uint8_t(m_write_stream.GetDataLocation(hash_code, static_data));
    out->Write(data_location);

    if (data_location == FBOM_DATA_LOCATION_STATIC) {
        return WriteStaticDataUsage(out, static_data);
    }

    if (data_location == FBOM_DATA_LOCATION_INPLACE) {
        std::stack<const FBOMType *> type_chain;
        const FBOMType *type_ptr = &type;

        while (type_ptr != nullptr) {
            type_chain.push(type_ptr);
            type_ptr = type_ptr->extends;
        }

        // TODO: assert length < max of uint8_t
        uint8_t typechain_length = type_chain.size();
        out->Write(typechain_length);

        while (!type_chain.empty()) {
            // write string of object type (loader to use)
            out->WriteString(type_chain.top()->name);

            // write size of the type
            out->Write(uint64_t(type_chain.top()->size));

            type_chain.pop();
        }

        m_write_stream.MarkStaticDataWritten(hash_code);
    } else {
        return FBOMResult::FBOM_ERR;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteData(ByteWriter *out, const std::shared_ptr<FBOMData> &data)
{
    AssertThrow(data != nullptr);

    FBOMStaticData static_data;
    HashCode::Value_t hash_code = data->GetHashCode().Value();
    uint8_t data_location = uint8_t(m_write_stream.GetDataLocation(hash_code, static_data));
    out->Write(data_location);

    if (data_location == FBOM_DATA_LOCATION_STATIC) {
        return WriteStaticDataUsage(out, static_data);
    }

    if (data_location == FBOM_DATA_LOCATION_INPLACE) {
        // write type first
        if (auto err = WriteObjectType(out, data->GetType())) {
            return err;
        }

        size_t sz = data->TotalSize();

        unsigned char *bytes = new unsigned char[sz];

        data->ReadBytes(sz, bytes);

        // size of data as uint32_t
        out->Write(uint32_t(sz));
        out->Write(bytes, sz);

        delete[] bytes;

        m_write_stream.MarkStaticDataWritten(hash_code);
    } else {
        return FBOMResult::FBOM_ERR;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteStaticDataUsage(ByteWriter *out, const FBOMStaticData &static_data) const
{
    uint32_t offset = static_data.offset;

    out->Write(offset);

    return FBOMResult::FBOM_OK;
}

void FBOMWriter::AddObjectData(const FBOMObject &object)
{
    m_write_stream.m_object_data.push_back(object);

    HashCode::Value_t hc = object.GetHashCode().Value();

    auto it = m_write_stream.m_hash_use_count_map.find(hc);

    if (it == m_write_stream.m_hash_use_count_map.end()) {
        m_write_stream.m_hash_use_count_map[hc] = 0;
    }

    m_write_stream.m_hash_use_count_map[hc]++;
}

FBOMStaticData FBOMWriter::AddStaticData(const FBOMType &type)
{

    FBOMStaticData sd;
    sd.type = FBOMStaticData::FBOM_STATIC_DATA_TYPE;
    sd.type_data = type;

    const HashCode::Value_t hash_code = sd.GetHashCode().Value();

    auto it = m_write_stream.m_static_data.find(hash_code);

    if (it == m_write_stream.m_static_data.end()) {
        sd.offset = m_write_stream.m_static_data_offset++;
        m_write_stream.m_static_data[hash_code] = sd;
    }

    return sd;
}

FBOMStaticData FBOMWriter::AddStaticData(const FBOMObject &object)
{
    AddStaticData(object.m_object_type);

    FBOMStaticData sd;
    sd.type = FBOMStaticData::FBOM_STATIC_DATA_OBJECT;
    sd.object_data = object;

    const HashCode::Value_t hash_code = sd.GetHashCode().Value();

    auto it = m_write_stream.m_static_data.find(hash_code);

    if (it == m_write_stream.m_static_data.end()) {
        sd.offset = m_write_stream.m_static_data_offset++;
        m_write_stream.m_static_data[hash_code] = sd;
    }

    return sd;
}

FBOMStaticData FBOMWriter::AddStaticData(const std::shared_ptr<FBOMData> &data)
{
    AssertThrow(data != nullptr);

    AddStaticData(data->GetType());

    FBOMStaticData sd;
    sd.type = FBOMStaticData::FBOM_STATIC_DATA_DATA;
    sd.data_data = data;

    const HashCode::Value_t hash_code = sd.GetHashCode().Value();

    auto it = m_write_stream.m_static_data.find(hash_code);

    if (it == m_write_stream.m_static_data.end()) {
        sd.offset = m_write_stream.m_static_data_offset++;
        m_write_stream.m_static_data[hash_code] = sd;
    }

    return sd;
}

FBOMDataLocation FBOMWriter::WriteStream::GetDataLocation(HashCode::Value_t hash_code, FBOMStaticData &out) const
{
    FBOMDataLocation data_location = FBOM_DATA_LOCATION_INPLACE;

    auto it = m_static_data.find(hash_code);

    if (it != m_static_data.end() && it->second.written) {
        out = it->second;
        data_location = FBOM_DATA_LOCATION_STATIC;
    }

    return data_location;
}

void FBOMWriter::WriteStream::MarkStaticDataWritten(HashCode::Value_t hash_code)
{
    auto it = m_static_data.find(hash_code);

    AssertThrow(it != m_static_data.end());

    m_static_data[hash_code].written = true;
}

} // namespace fbom
} // namespace hyperion