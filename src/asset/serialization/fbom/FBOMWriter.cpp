#include "FBOM.hpp"
#include <asset/ByteWriter.hpp>
#include <core/lib/Path.hpp>
#include <core/lib/SortedArray.hpp>

#include <Constants.hpp>

#include <stack>
#include <algorithm>

namespace hyperion::v2::fbom {

FBOMWriter::FBOMWriter()
{
}

FBOMWriter::FBOMWriter(FBOMWriter &&other) noexcept
    : m_write_stream(std::move(other.m_write_stream))
{
}

FBOMWriter &FBOMWriter::operator=(FBOMWriter &&other) noexcept
{
    m_write_stream = std::move(other.m_write_stream);

    return *this;
}

FBOMWriter::~FBOMWriter()
{
}

FBOMResult FBOMWriter::Append(const FBOMObject &object)
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

    WriteHeader(out);
    WriteStaticDataToByteStream(out);

    for (const auto &it : m_write_stream.m_object_data) {
        if (auto err = WriteObject(out, it)) {
            m_write_stream.m_last_result = err;

            return err;
        }
    }

    if (auto err = WriteExternalObjects()) {
        return err;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteExternalObjects()
{
    for (auto &it : m_write_stream.m_external_objects) {
        FBOMWriter chunk_writer;

        for (auto &objects_it : it.second.objects) {
            FBOMObject object(objects_it.second);

            // set to empty to not keep recursing. we only write the external data once.
            object.SetExternalObjectInfo(FBOMExternalObjectInfo { });

            if (auto err = chunk_writer.Append(object)) {
                return err;
            }
        }

        const Path output_path(it.first);

        FileByteWriter byte_writer(output_path.Data());
        chunk_writer.Emit(&byte_writer);
        byte_writer.Close();
    }

    return FBOMResult::FBOM_OK;
}

void FBOMWriter::BuildStaticData()
{
    for (auto &object : m_write_stream.m_object_data) {
        Prune(object); // TODO: ensure that no invalidation occurs by m_object_data being modified
    }
}

void FBOMWriter::Prune(const FBOMObject &object)
{
    if (object.IsExternal()) {
        return;
    }

    for (SizeType index = 0; index < object.nodes->Size(); index++) {
        const auto &node = object.nodes->Get(index);

        Prune(node);
    }

    for (const auto &prop : object.properties) {
        // int property_value_usage = m_write_stream.m_hash_use_count_map[prop.second.GetUniqueID()];

        // if (property_value_usage > 1) {
        AddStaticData(prop.second);
        // }
    }

    // convert use counts into static objects
    // HashCode::ValueType hc = object.GetHashCode().Value();
    // int object_usage = 0;

    // auto it = m_write_stream.m_hash_use_count_map.find(hc);

    // if (it != m_write_stream.m_hash_use_count_map.end()) {
    //     object_usage = it->second;
    // }

    //if (object_usage > 1) {
       AddStaticData(object);
    //}
}

FBOMResult FBOMWriter::WriteStaticDataToByteStream(ByteWriter *out)
{
    Array<FBOMStaticData> static_data_ordered;
    static_data_ordered.Reserve(m_write_stream.m_static_data.size());
    // static_data_ordered.Reserve(m_write_stream.m_static_data.size());

    for (auto &it : m_write_stream.m_static_data) {
        // static_data_ordered.Insert(it.second);
        static_data_ordered.PushBack(it.second);
    }

    std::sort(static_data_ordered.Begin(), static_data_ordered.End());

    AssertThrowMsg(static_data_ordered.Size() == m_write_stream.m_static_data_offset,
        "Values do not match, incorrect bookkeeping");

    out->Write<uint8>(FBOM_STATIC_DATA_START);

    // write SIZE of static data as uint32_t
    out->Write<uint32>(uint32(static_data_ordered.Size()));
    // 8 bytes of padding
    out->Write<uint64>(0);

    // static data
    //   uint32_t as index/offset
    //   uint8_t as type of static data
    //   then, the actual size of the data will vary depending on the held type

    for (auto &it : static_data_ordered) {
        AssertThrow(it.offset < static_data_ordered.Size());

        out->Write<uint32>(uint32(it.offset));
        out->Write<uint8>(it.type);

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
            if (auto err = WriteData(out, it.data_data.Get())) {
                return err;
            }
            break;
        default:
            return FBOMResult(FBOMResult::FBOM_ERR, "Cannot write static object to bytestream, unknown type");
        }
    }

    out->Write<uint8>(FBOM_STATIC_DATA_END);

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteHeader(ByteWriter *out)
{
    const SizeType position_before = out->Position();

    // hyp identifier string
    out->Write(FBOM::header_identifier, sizeof(FBOM::header_identifier));

    // endianness
    out->Write<uint8>(IsBigEndian());

    // binary version
    out->Write<uint32>(FBOM::version);

    const SizeType position_change = SizeType(out->Position()) - position_before;
    AssertThrow(position_change <= FBOM::header_size);

    const SizeType remaining_bytes = FBOM::header_size - position_change;
    AssertThrow(remaining_bytes < 64);

    void *zeros = StackAlloc(remaining_bytes);
    Memory::MemSet(zeros, 0, remaining_bytes);

    out->Write(zeros, remaining_bytes);
    
    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteObject(ByteWriter *out, const FBOMObject &object)
{
    if (object.IsExternal()) {
        // defer writing it. instead we pass the object into our external data,
        // which we will later be write

        auto &external_object = m_write_stream.m_external_objects[object.GetExternalObjectKey()];
        external_object.objects[object.GetUniqueID()] = object;

        // return FBOMResult::FBOM_OK;
    }

    out->Write<uint8>(FBOM_OBJECT_START);

    const auto unique_id = object.GetUniqueID();
    out->Write<uint64>(unique_id);

    FBOMStaticData static_data;
    String external_key;

    const FBOMDataLocation data_location = m_write_stream.GetDataLocation(unique_id, static_data, external_key);
    out->Write<uint8>(uint8(data_location));

    switch (data_location) {
    // case FBOM_DATA_LOCATION_STATIC: // fallthrough
    // case FBOM_DATA_LOCATION_INPLACE: // fallthrough
    case FBOM_DATA_LOCATION_STATIC:
        return WriteStaticDataUsage(out, static_data);
    case FBOM_DATA_LOCATION_INPLACE:
    {
        // write typechain
        if (auto err = WriteObjectType(out, object.m_object_type)) {
            return err;
        }

        // add all properties
        for (auto &it : object.properties) {
            out->Write<uint8>(FBOM_DEFINE_PROPERTY);

            // // write property name
            out->WriteString(it.first, BYTE_WRITER_FLAGS_WRITE_SIZE);

            if (auto err = WriteData(out, it.second)) {
                return err;
            }
        }

        FlatSet<uint64> written_ids;

        // now write out all child nodes
        for (auto &node : *object.nodes) {
            if (auto err = WriteObject(out, node)) {
                return err;
            }
        }

        out->Write<uint8>(FBOM_OBJECT_END);

        m_write_stream.MarkStaticDataWritten(unique_id);

        break;
    }
    case FBOM_DATA_LOCATION_EXT_REF:
    {
        AssertThrow(external_key.Any());

        out->WriteString(external_key, BYTE_WRITER_FLAGS_WRITE_SIZE);

        // write object index as u32
        // TODO!
        out->Write<uint32>(0);

        // write flags -- i.e, lazy loaded, etc.
        // not yet implemented, just write 0 for now
        out->Write<uint32>(0);

        break;
    }
    default:
        AssertThrowMsg(false, "Invalid data location type");
        break;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteObjectType(ByteWriter *out, const FBOMType &type)
{
    FBOMStaticData static_data;
    String external_key;

    const auto unique_id = type.GetUniqueID();
    const auto data_location = m_write_stream.GetDataLocation(unique_id, static_data, external_key);
    out->Write<uint8>(static_cast<uint8>(data_location));

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
        out->Write<uint8>(static_cast<uint8>(type_chain.size()));

        while (!type_chain.empty()) {
            // write string of object type (loader to use)
            out->WriteString(type_chain.top()->name, BYTE_WRITER_FLAGS_WRITE_SIZE);

            // write size of the type
            out->Write<uint64>(type_chain.top()->size);

            type_chain.pop();
        }

        m_write_stream.MarkStaticDataWritten(unique_id);
    } else {
        return FBOMResult::FBOM_ERR;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteData(ByteWriter *out, const FBOMData &data)
{
    FBOMStaticData static_data;
    String external_key;

    const auto unique_id = data.GetUniqueID();
    const FBOMDataLocation data_location = m_write_stream.GetDataLocation(unique_id, static_data, external_key);
    out->Write<uint8>(uint8(data_location));

    if (data_location == FBOM_DATA_LOCATION_STATIC) {
        return WriteStaticDataUsage(out, static_data);
    }

    if (data_location == FBOM_DATA_LOCATION_INPLACE) {
        // write type first
        if (auto err = WriteObjectType(out, data.GetType())) {
            return err;
        }

        const auto sz = data.TotalSize();

        unsigned char *bytes = new unsigned char[sz];

        data.ReadBytes(sz, bytes);

        // size of data as uint32_t
        out->Write<uint32>(uint32(sz));
        out->Write(bytes, sz);

        delete[] bytes;

        m_write_stream.MarkStaticDataWritten(unique_id);
    } else {
        return FBOMResult::FBOM_ERR;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteStaticDataUsage(ByteWriter *out, const FBOMStaticData &static_data) const
{
    const auto offset = static_data.offset;

    out->Write<uint32>(uint32(offset));

    return FBOMResult::FBOM_OK;
}

void FBOMWriter::AddObjectData(const FBOMObject &object)
{
    const auto unique_id = object.GetUniqueID();

    m_write_stream.m_object_data.push_back(object);

    auto it = m_write_stream.m_hash_use_count_map.Find(unique_id);

    if (it == m_write_stream.m_hash_use_count_map.End()) {
        m_write_stream.m_hash_use_count_map[unique_id] = 0;
    }

    m_write_stream.m_hash_use_count_map[unique_id]++;
}

void FBOMWriter::AddStaticData(const FBOMType &type)
{
    FBOMStaticData sd(type);

    const auto unique_id = sd.GetUniqueID();

    auto it = m_write_stream.m_static_data.find(unique_id);

    if (it == m_write_stream.m_static_data.end()) {
        sd.offset = m_write_stream.m_static_data_offset++;
        m_write_stream.m_static_data[unique_id] = std::move(sd);
    }
}

void FBOMWriter::AddStaticData(const FBOMObject &object)
{
    AddStaticData(object.m_object_type);

    FBOMStaticData sd(object);

    const auto unique_id = sd.GetUniqueID();

    auto it = m_write_stream.m_static_data.find(unique_id);

    if (it == m_write_stream.m_static_data.end()) {
        sd.offset = m_write_stream.m_static_data_offset++;
        m_write_stream.m_static_data[unique_id] = std::move(sd);
    }
}

void FBOMWriter::AddStaticData(const FBOMData &data)
{
    AddStaticData(data.GetType());

    FBOMStaticData sd(data);

    const auto unique_id = sd.GetUniqueID();

    auto it = m_write_stream.m_static_data.find(unique_id);

    if (it == m_write_stream.m_static_data.end()) {
        sd.offset = m_write_stream.m_static_data_offset++;
        m_write_stream.m_static_data[unique_id] = std::move(sd);
    }
}

FBOMDataLocation FBOMWriter::WriteStream::GetDataLocation(const UniqueID &unique_id, FBOMStaticData &out_static_data, String &out_external_key) const
{
    { // check static data
        auto it = m_static_data.find(unique_id);

        if (it != m_static_data.end() && it->second.written) {
            out_static_data = it->second;
            return FBOM_DATA_LOCATION_STATIC;
        }
    }

    // check external files
    for (auto &it : m_external_objects) {
        auto &external_data = it.second;

        const auto objects_it = external_data.objects.Find(unique_id);

        if (objects_it == external_data.objects.End()) {
            continue;
        }

        if (!objects_it->second.IsExternal()) {
            break;
        }

        out_external_key = objects_it->second.GetExternalObjectKey();

        return FBOM_DATA_LOCATION_EXT_REF;
    }

    return FBOM_DATA_LOCATION_INPLACE;
}

void FBOMWriter::WriteStream::MarkStaticDataWritten(const UniqueID &unique_id)
{
    auto it = m_static_data.find(unique_id);
    AssertThrow(it != m_static_data.end());

    m_static_data[unique_id].written = true;
}

} // namespace hyperion::v2::fbom