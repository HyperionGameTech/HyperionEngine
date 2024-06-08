/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/ByteWriter.hpp>

#include <core/containers/Stack.hpp>

#include <Constants.hpp>

#include <algorithm>

namespace hyperion::fbom {

FBOMWriter::WriteStream::WriteStream() = default;

FBOMWriter::FBOMWriter()
{
    // // Add name table for the write stream
    // AddStaticData(m_write_stream.m_name_table_id, FBOMStaticData(FBOMNameTable { }));
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
    AddObjectData(object, object.GetUniqueID());

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::Emit(ByteWriter *out)
{
    if (FBOMResult err = m_write_stream.m_last_result) {
        return err;
    }

    BuildStaticData();

    WriteHeader(out);
    WriteStaticDataToByteStream(out);

    for (const auto &it : m_write_stream.m_object_data) {
        if (FBOMResult err = WriteObject(out, it)) {
            m_write_stream.m_last_result = err;

            return err;
        }
    }

    if (FBOMResult err = WriteExternalObjects()) {
        return err;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteExternalObjects()
{
    for (const auto &it : m_write_stream.m_external_objects) {
        FBOMWriter chunk_writer;

        for (const auto &objects_it : it.second.objects) {
            FBOMObject object(objects_it.second);

            // set to empty to not keep recursing. we only write the external data once.
            object.SetExternalObjectInfo(FBOMExternalObjectInfo { });

            if (FBOMResult err = chunk_writer.Append(object)) {
                return err;
            }
        }

        FileByteWriter byte_writer(it.first);
        chunk_writer.Emit(&byte_writer);
        byte_writer.Close();
    }

    return FBOMResult::FBOM_OK;
}

void FBOMWriter::BuildStaticData()
{
    for (FBOMObject &object : m_write_stream.m_object_data) {
        Prune(object); // TODO: ensure that no invalidation occurs by m_object_data being modified
    }
}

void FBOMWriter::Prune(const FBOMObject &object)
{
    if (object.IsExternal()) {
        return;
    }

    for (SizeType index = 0; index < object.nodes->Size(); index++) {
        const FBOMObject &node = object.nodes->Get(index);

        Prune(node);
    }

    for (const auto &prop : object.properties) {
        // int property_value_usage = m_write_stream.m_hash_use_count_map[prop.second.GetUniqueID()];

        // if (property_value_usage > 1) {
        AddStaticData(FBOMData::FromName(prop.first));
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
    static_data_ordered.Reserve(m_write_stream.m_static_data.Size());
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

    for (const FBOMStaticData &it : static_data_ordered) {
        // temp
        if (it.type == FBOMStaticData::FBOM_STATIC_DATA_NAME_TABLE) {
            continue;
        }


        AssertThrow(it.offset < static_data_ordered.Size());

        out->Write<uint32>(uint32(it.offset));
        out->Write<uint8>(it.type);

        switch (it.type) {
        case FBOMStaticData::FBOM_STATIC_DATA_OBJECT:
            if (FBOMResult err = WriteObject(out, it.data.Get<FBOMObject>(), it.GetUniqueID())) {
                return err;
            }
            break;
        case FBOMStaticData::FBOM_STATIC_DATA_TYPE:
            if (FBOMResult err = WriteObjectType(out, it.data.Get<FBOMType>(), it.GetUniqueID())) {
                return err;
            }
            break;
        case FBOMStaticData::FBOM_STATIC_DATA_DATA:
            if (FBOMResult err = WriteData(out, it.data.Get<FBOMData>(), it.GetUniqueID())) {
                return err;
            }
            break;
        case FBOMStaticData::FBOM_STATIC_DATA_NAME_TABLE:
            if (FBOMResult err = WriteNameTable(out, it.data.Get<FBOMNameTable>(), it.GetUniqueID())) {
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
    out->Write<uint32>(FBOM::version.value);

    const SizeType position_change = SizeType(out->Position()) - position_before;
    AssertThrow(position_change <= FBOM::header_size);

    const SizeType remaining_bytes = FBOM::header_size - position_change;
    AssertThrow(remaining_bytes < 64);

    void *zeros = StackAlloc(remaining_bytes);
    Memory::MemSet(zeros, 0, remaining_bytes);

    out->Write(zeros, remaining_bytes);
    
    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteObject(ByteWriter *out, const FBOMObject &object, UniqueID id)
{
    if (object.IsExternal()) {
        // defer writing it. instead we pass the object into our external data,
        // which we will later be write

        FBOMExternalData &external_object = m_write_stream.m_external_objects[object.GetExternalObjectKey()];
        external_object.objects[id] = object;

        // return FBOMResult::FBOM_OK;
    }

    out->Write<uint8>(FBOM_OBJECT_START);

    out->Write<uint64>(id);

    const FBOMStaticData *static_data_ptr;

    String external_key;

    const FBOMDataLocation data_location = m_write_stream.GetDataLocation(id, &static_data_ptr, external_key);
    out->Write<uint8>(uint8(data_location));

    switch (data_location) {
    case FBOM_DATA_LOCATION_STATIC:
        AssertThrow(static_data_ptr != nullptr);

        return WriteStaticDataUsage(out, *static_data_ptr);
    case FBOM_DATA_LOCATION_INPLACE:
    {
        // write typechain
        if (FBOMResult err = WriteObjectType(out, object.m_object_type)) {
            return err;
        }

        // add all properties
        for (auto &it : object.properties) {
            out->Write<uint8>(FBOM_DEFINE_PROPERTY);

            // write key
            if (FBOMResult err = WriteData(out, FBOMData::FromName(it.first))) {
                return err;
            }

            // write value
            if (FBOMResult err = WriteData(out, it.second)) {
                return err;
            }
        }

        FlatSet<uint64> written_ids;

        // now write out all child nodes
        for (auto &node : *object.nodes) {
            if (FBOMResult err = WriteObject(out, node)) {
                return err;
            }
        }

        out->Write<uint8>(FBOM_OBJECT_END);

        m_write_stream.MarkStaticDataWritten(id);

        break;
    }
    case FBOM_DATA_LOCATION_EXT_REF:
    {
        AssertThrow(external_key.Any());

        out->WriteString(external_key, BYTE_WRITER_FLAGS_WRITE_SIZE | BYTE_WRITER_FLAGS_WRITE_STRING_TYPE);

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

FBOMResult FBOMWriter::WriteObjectType(ByteWriter *out, const FBOMType &type, UniqueID id)
{
    const FBOMStaticData *static_data_ptr;
    String external_key;

    const FBOMDataLocation data_location = m_write_stream.GetDataLocation(id, &static_data_ptr, external_key);
    out->Write<uint8>(uint8(data_location));

    if (data_location == FBOM_DATA_LOCATION_STATIC) {
        AssertThrow(static_data_ptr != nullptr);

        return WriteStaticDataUsage(out, *static_data_ptr);
    }

    if (data_location == FBOM_DATA_LOCATION_INPLACE) {
        if (type.extends != nullptr) {
            out->Write<uint8>(1);

            if (FBOMResult err = WriteObjectType(out, *type.extends)) {
                return err;
            }
        } else {
            out->Write<uint8>(0);
        }

        // write string of object type (loader to use)
        out->WriteString(type.name, BYTE_WRITER_FLAGS_WRITE_SIZE | BYTE_WRITER_FLAGS_WRITE_STRING_TYPE);

        // write size of the type
        out->Write<uint64>(type.size);

        m_write_stream.MarkStaticDataWritten(id);
    } else {
        return FBOMResult::FBOM_ERR;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteData(ByteWriter *out, const FBOMData &data, UniqueID id)
{
    const FBOMStaticData *static_data_ptr;
    String external_key;

    const FBOMDataLocation data_location = m_write_stream.GetDataLocation(id, &static_data_ptr, external_key);
    out->Write<uint8>(uint8(data_location));

    if (data_location == FBOM_DATA_LOCATION_STATIC) {
        AssertThrow(static_data_ptr != nullptr);

        return WriteStaticDataUsage(out, *static_data_ptr);
    }

    if (data_location == FBOM_DATA_LOCATION_INPLACE) {
        // // Custom logic for `Name` objects: store their string data in the stream's name table
        // if (data.IsName()) {
        //     Name name;

        //     if (!data.ReadName(&name)) {
        //         return FBOMResult(FBOMResult::FBOM_ERR, "Invalid name object, cannot write to stream");
        //     }

        //     m_write_stream.GetNameTable().Add(name.LookupString());
        // }

        // write type first
        if (FBOMResult err = WriteObjectType(out, data.GetType())) {
            return err;
        }

        const SizeType sz = data.TotalSize();

        unsigned char *bytes = new unsigned char[sz];

        data.ReadBytes(sz, bytes);

        // size of data as uint32_t
        out->Write<uint32>(uint32(sz));
        out->Write(bytes, sz);

        delete[] bytes;

        m_write_stream.MarkStaticDataWritten(id);
    } else {
        return FBOMResult::FBOM_ERR;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteNameTable(ByteWriter *out, const FBOMNameTable &name_table, UniqueID id)
{
    const FBOMStaticData *static_data_ptr;
    String external_key;
    
    const FBOMDataLocation data_location = m_write_stream.GetDataLocation(id, &static_data_ptr, external_key);
    out->Write<uint8>(uint8(data_location));

    if (data_location == FBOM_DATA_LOCATION_STATIC) {
        AssertThrow(static_data_ptr != nullptr);

        return WriteStaticDataUsage(out, *static_data_ptr);
    }

    if (data_location == FBOM_DATA_LOCATION_INPLACE) {
        out->Write<uint32>(uint32(name_table.values.Size()));

        // for (const auto &it : name_table.values) {
        //     out->WriteString(it.second, BYTE_WRITER_FLAGS_WRITE_SIZE | BYTE_WRITER_FLAGS_WRITE_STRING_TYPE);
        //     out->Write<NameID>(it.first.GetID());
        // }

        m_write_stream.MarkStaticDataWritten(id);
    } else {
        return FBOMResult::FBOM_ERR;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteStaticDataUsage(ByteWriter *out, const FBOMStaticData &static_data) const
{
    const auto offset = static_data.offset;

    // ensure its there
    const auto it = m_write_stream.m_static_data.Find(static_data.GetUniqueID());
    AssertThrow(it != m_write_stream.m_static_data.End());
    AssertThrow(it->second.type == static_data.type);
    AssertThrow(it->second.GetHashCode() == static_data.GetHashCode());

    out->Write<uint32>(uint32(offset));

    return FBOMResult::FBOM_OK;
}

void FBOMWriter::AddObjectData(const FBOMObject &object, UniqueID id)
{
    m_write_stream.m_object_data.PushBack(object);

    auto it = m_write_stream.m_hash_use_count_map.Find(id);

    if (it == m_write_stream.m_hash_use_count_map.End()) {
        m_write_stream.m_hash_use_count_map[id] = 0;
    }

    m_write_stream.m_hash_use_count_map[id]++;
}

UniqueID FBOMWriter::AddStaticData(UniqueID id, FBOMStaticData &&static_data)
{
    auto it = m_write_stream.m_static_data.Find(id);

    if (it == m_write_stream.m_static_data.End()) {
        static_data.SetUniqueID(id);
        static_data.offset = m_write_stream.m_static_data_offset++;
        m_write_stream.m_static_data[id] = std::move(static_data);
        AssertThrow(m_write_stream.m_static_data[id].GetUniqueID() == id);
    }

    return id;
}

UniqueID FBOMWriter::AddStaticData(const FBOMType &type)
{
    if (type.extends != nullptr) {
        AddStaticData(*type.extends);
    }

    return AddStaticData(FBOMStaticData(type));
}

UniqueID FBOMWriter::AddStaticData(const FBOMObject &object)
{
    AddStaticData(object.GetType());

    return AddStaticData(FBOMStaticData(object));
}

UniqueID FBOMWriter::AddStaticData(const FBOMData &data)
{
    AddStaticData(data.GetType());

    return AddStaticData(FBOMStaticData(data));
}

UniqueID FBOMWriter::AddStaticData(const FBOMNameTable &name_table)
{
    return AddStaticData(FBOMStaticData(name_table));
}

FBOMDataLocation FBOMWriter::WriteStream::GetDataLocation(const UniqueID &unique_id, const FBOMStaticData **out_static_data, String &out_external_key) const
{
    { // check static data
        auto it = m_static_data.Find(unique_id);

        if (it != m_static_data.End() && it->second.IsWritten()) {
            *out_static_data = &it->second;

            return FBOM_DATA_LOCATION_STATIC;
        }
    }

    // check external files
    for (auto &it : m_external_objects) {
        const FBOMExternalData &external_data = it.second;

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
    auto it = m_static_data.Find(unique_id);
    AssertThrow(it != m_static_data.End());

    m_static_data[unique_id].SetIsWritten(true);
}

} // namespace hyperion::fbom