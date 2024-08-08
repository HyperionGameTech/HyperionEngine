/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/FBOMArray.hpp>

#include <asset/BufferedByteReader.hpp>
#include <asset/ByteWriter.hpp>

#include <core/containers/Stack.hpp>

#include <core/compression/Archive.hpp>

#include <core/util/ForEach.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Constants.hpp>

#include <algorithm>

namespace hyperion::fbom {

#pragma region FBOMWriteStream

FBOMWriteStream::FBOMWriteStream() = default;

FBOMDataLocation FBOMWriteStream::GetDataLocation(const UniqueID &unique_id, const FBOMStaticData **out_static_data, const FBOMExternalObjectInfo **out_external_object_info) const
{
    *out_static_data = nullptr;
    *out_external_object_info = nullptr;

    { // check static data
        auto it = m_static_data.Find(unique_id);

        if (it != m_static_data.End()) {
            if (IsWritingStaticData() || it->second.IsWritten()) {
                *out_static_data = &it->second;

                if (it->second.IsWritten()) {
                    return FBOMDataLocation::LOC_STATIC;
                }
            }
        }
    }

    // check external objects
    for (const FBOMObjectLibrary &object_library : m_object_libraries) {
        const auto objects_it = object_library.object_data.FindIf([&unique_id](const FBOMObject &item)
        {
            return item.GetUniqueID() == unique_id;
        });

        if (objects_it == object_library.object_data.End()) {
            continue;
        }

        if (!objects_it->IsExternal()) {
            break;
        }

        *out_external_object_info = objects_it->GetExternalObjectInfo();

        return FBOMDataLocation::LOC_EXT_REF;
    }

    return FBOMDataLocation::LOC_INPLACE;
}

void FBOMWriteStream::MarkStaticDataWritten(const UniqueID &unique_id)
{
    // AssertThrow(IsWritingStaticData());

    // auto it = m_static_data.Find(unique_id);
    // AssertThrow(it != m_static_data.End());

    // it->second.SetIsWritten(true);
}

FBOMNameTable &FBOMWriteStream::GetNameTable()
{
    auto it = m_static_data.Find(m_name_table_id);
    AssertThrow(it != m_static_data.End());

    FBOMNameTable *name_table_ptr = it->second.data.TryGetAsDynamic<FBOMNameTable>();
    AssertThrow(name_table_ptr != nullptr);

    return *name_table_ptr;
}

void FBOMWriteStream::AddToObjectLibrary(FBOMObject &object)
{
    // static constexpr SizeType desired_max_size = 1024 * 1024 * 1024 * 32; // 32 MiB
    static constexpr SizeType desired_max_size = 10;

    FBOMExternalObjectInfo *external_object_info = object.GetExternalObjectInfo();
    AssertThrow(external_object_info != nullptr);
    AssertThrow(!external_object_info->IsLinked());

    // FBOMData object_data = FBOMData::FromObject(object);
    // const SizeType object_data_size = object_data.TotalSize();

    FBOMObjectLibrary *library_ptr = nullptr;

    for (auto it = m_object_libraries.Begin(); it != m_object_libraries.End(); ++it) {
        const SizeType library_size = it->CalculateTotalSize();

        if (library_size + 1 <= desired_max_size) {
            library_ptr = it;

            break;
        }
    }

    if (!library_ptr) {
        library_ptr = &m_object_libraries.EmplaceBack();
    }

    const uint32 index = library_ptr->Put(object);
    // const uint32 index = library_ptr->Put(std::move(object_data));

    external_object_info->library_id = library_ptr->uuid;
    external_object_info->index = index;
}

#pragma endregion FBOMWriteStream

#pragma region FBOMWriter

FBOMWriter::FBOMWriter()
    : FBOMWriter(RC<FBOMWriteStream>(new FBOMWriteStream()))
{
}

FBOMWriter::FBOMWriter(const RC<FBOMWriteStream> &write_stream)
    : m_write_stream(write_stream)
{
    // Add name table for the write stream
    AddStaticData(m_write_stream->m_name_table_id, FBOMStaticData(FBOMNameTable { }));
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

FBOMResult FBOMWriter::Append(FBOMObject &&object)
{
    const UniqueID id = object.GetUniqueID();

    AddObjectData(std::move(object), id);

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::Emit(ByteWriter *out, bool write_header)
{
    if (FBOMResult err = m_write_stream->m_last_result) {
        return err;
    }

    BuildStaticData();

    if (write_header) {
        if (FBOMResult err = WriteHeader(out)) {
            return err;
        }
    }
    
    if (FBOMResult err = WriteExternalObjects(out)) {
        return err;
    }

    if (FBOMResult err = WriteStaticData(out)) {
        return err;
    }

    for (const FBOMObject &object : m_write_stream->m_object_data) {
        if (FBOMResult err = object.Visit(this, out)) {
            return err;
        }
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteExternalObjects(ByteWriter *out)
{
    if (m_write_stream->m_object_libraries.Any()) {
        DebugLog(LogType::Debug, "Writing %u external library files\n", m_write_stream->m_object_libraries.Size());
    }

    AtomicVar<bool> any_errors = false;

    Mutex mtx;

    ParallelForEach(m_write_stream->m_object_libraries, [out, &any_errors, &mtx](const FBOMObjectLibrary &library, uint, uint)
    {
        FBOMWriter serializer;

        for (const FBOMObject &object : library.object_data) {
            FBOMObject object_copy(object);

            // unset to stop recursion
            object_copy.SetIsExternal(false);

            if (FBOMResult err = serializer.Append(object_copy)) {
                HYP_LOG(Serialization, LogLevel::ERR, "Failed to write external object: {}", err.message);

                any_errors.Set(true, MemoryOrder::RELAXED);

                return;
            }
        }

        MemoryByteWriter byte_writer;

        if (FBOMResult err = serializer.Emit(&byte_writer, /* write_header */ false)) {
            HYP_LOG(Serialization, LogLevel::ERR, "Failed to write external object: {}", err.message);

            any_errors.Set(true, MemoryOrder::RELAXED);

            return;
        }

        ByteBuffer buffer = std::move(byte_writer.GetBuffer());

        Mutex::Guard guard(mtx);

        out->Write<uint8>(FBOM_OBJECT_LIBRARY_START);

        out->Write<UUID>(library.uuid);

        out->Write<uint8>(uint8(FBOMObjectLibraryFlags::LOCATION_INLINE));

        // write size of buffer
        out->Write<uint64>(buffer.Size());

        // write actual buffer data
        out->Write(buffer.Data(), buffer.Size());

        out->Write<uint8>(FBOM_OBJECT_LIBRARY_END);
    });

    return any_errors.Get(MemoryOrder::RELAXED)
        ? FBOMResult::FBOM_ERR
        : FBOMResult::FBOM_OK;
}

void FBOMWriter::BuildStaticData()
{
    m_write_stream->LockObjectDataWriting();

    for (FBOMObject &object : m_write_stream->m_object_data) {
        Prune(object);
    }

    m_write_stream->UnlockObjectDataWriting();
}

void FBOMWriter::Prune(FBOMObject &object)
{
    // will be pruned by other instance when it is written
    if (object.IsExternal()) {
        FBOMExternalObjectInfo *external_object_info = object.GetExternalObjectInfo();
        AssertThrow(external_object_info != nullptr);
        AssertThrow(!external_object_info->IsLinked());

        m_write_stream->AddToObjectLibrary(object);

        return;
    }

    for (SizeType index = 0; index < object.nodes->Size(); index++) {
        FBOMObject &subobject = object.nodes->Get(index);

        Prune(subobject);
    }

    for (const auto &prop : object.properties) {
        AddStaticData(FBOMData::FromName(prop.first));
        AddStaticData(prop.second);
    }

    AddStaticData(object);
}

FBOMResult FBOMWriter::WriteStaticData(ByteWriter *out)
{
    m_write_stream->BeginStaticDataWriting();

    Array<FBOMStaticData *> static_data_ordered;
    static_data_ordered.Reserve(m_write_stream->m_static_data.Size());

    for (auto &it : m_write_stream->m_static_data) {
        static_data_ordered.PushBack(&it.second);
    }

    std::sort(static_data_ordered.Begin(), static_data_ordered.End(), [](const FBOMStaticData *a, const FBOMStaticData *b)
    {
        return a->offset < b->offset;
    });

#ifdef HYP_DEBUG_MODE
    // sanity check: make sure no duplicate offsets
    for (SizeType i = 1; i < static_data_ordered.Size(); i++) {
        AssertThrow(static_data_ordered[i]->offset == static_data_ordered[i - 1]->offset + 1);
    }
#endif

    AssertThrowMsg(static_data_ordered.Size() == m_write_stream->m_static_data_offset,
        "Values do not match, incorrect bookkeeping");

    MemoryByteWriter static_data_byte_writer;
    Array<SizeType> static_data_buffer_offsets;
    static_data_buffer_offsets.Resize(static_data_ordered.Size());

    for (FBOMStaticData *static_data : static_data_ordered) {
        AssertThrow(static_data->offset < static_data_ordered.Size());

        const SizeType buffer_offset = static_data_byte_writer.Position();

        AssertThrowMsg(!static_data->IsWritten(),
            "Static data object has already been written: %s",
            static_data->ToString().Data());

        if (FBOMResult err = static_data->data->Visit(static_data->GetUniqueID(), this, &static_data_byte_writer)) {
            m_write_stream->EndStaticDataWriting();

            return err;
        }

        static_data->SetIsWritten(true);

        // AssertThrowMsg(static_data->IsWritten(),
        //     "Static data object was not written: %s\t%p",
        //     static_data->ToString().Data(),
        //     static_data);

        static_data_buffer_offsets[static_data->offset] = buffer_offset;
    }

    out->Write<uint8>(FBOM_STATIC_DATA_START);

    out->Write<uint8>(FBOM_STATIC_DATA_HEADER_START);

    // write number of items as uint32
    out->Write<uint32>(uint32(static_data_ordered.Size()));

    // write buffer size
    out->Write<uint64>(uint64(static_data_byte_writer.GetBuffer().Size()));

    for (SizeType i = 0; i < static_data_ordered.Size(); i++) {
        const FBOMStaticData *static_data = static_data_ordered[i];

        // write index
        out->Write<uint32>(uint32(static_data->offset));

        // write type
        out->Write<uint8>(static_data->type);

        // write start offset
        out->Write<uint64>(static_data_buffer_offsets[i]);

        // write size of object
        if (i == static_data_ordered.Size() - 1) {
            out->Write<uint64>(static_data_byte_writer.GetBuffer().Size() - static_data_buffer_offsets[i]);
        } else {
            AssertThrow(static_data_buffer_offsets[i + 1] >= static_data_buffer_offsets[i]);

            out->Write<uint64>(static_data_buffer_offsets[i + 1] - static_data_buffer_offsets[i]);
        }
    }

    out->Write<uint8>(FBOM_STATIC_DATA_HEADER_END);

    // Write entire buffer
    out->Write(static_data_byte_writer.GetBuffer().Data(), static_data_byte_writer.GetBuffer().Size());

    out->Write<uint8>(FBOM_STATIC_DATA_END);

    m_write_stream->EndStaticDataWriting();

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

FBOMResult FBOMWriter::Write(ByteWriter *out, const FBOMObject &object, UniqueID id, EnumFlags<FBOMDataAttributes> attributes)
{
    AssertThrow(uint64(id) != 0);

    bool compress_properties = false;

    if (Archive::IsEnabled() && !(attributes & FBOMDataAttributes::COMPRESSED)) {
        // compress_properties = true;
    }

    if (object.IsExternal()) {
        // defer writing it. instead we pass the object into our external data,
        // which we will later be write
        const FBOMExternalObjectInfo *external_object_info = object.GetExternalObjectInfo();
        AssertThrow(external_object_info != nullptr);
        AssertThrow(external_object_info->IsLinked());

        return FBOMResult::FBOM_OK;
    }

    out->Write<uint8>(FBOM_OBJECT_START);
    out->Write<uint64>(id);

    const FBOMStaticData *static_data_ptr;
    const FBOMExternalObjectInfo *external_object_info_ptr;

    const FBOMDataLocation data_location = m_write_stream->GetDataLocation(id, &static_data_ptr, &external_object_info_ptr);
    
    if (FBOMResult err = WriteDataAttributes(out, attributes, data_location)) {
        return err;
    }

    switch (data_location) {
    case FBOMDataLocation::LOC_STATIC:
    {
        AssertThrow(static_data_ptr != nullptr);

        return WriteStaticDataUsage(out, *static_data_ptr);
    }
    case FBOMDataLocation::LOC_INPLACE:
    {
        // temp
        if (static_data_ptr != nullptr) {
            AssertThrow(static_data_ptr->GetUniqueID() == id);
            AssertThrow(static_data_ptr->GetUniqueID() == object.GetUniqueID());
        }

        // write typechain
        if (FBOMResult err = object.m_object_type.Visit(this, out)) {
            return err;
        }

        // add all properties
        for (const auto &it : object.properties) {
            EnumFlags<FBOMDataAttributes> property_attributes = FBOMDataAttributes::NONE;

            if (compress_properties) {
                property_attributes |= FBOMDataAttributes::COMPRESSED;
            }

            out->Write<uint8>(FBOM_DEFINE_PROPERTY);

            // write key
            if (FBOMResult err = FBOMData::FromName(it.first).Visit(this, out)) {
                return err;
            }

            // write value
            if (FBOMResult err = it.second.Visit(this, out, property_attributes)) {
                return err;
            }
        }

        for (FBOMObject &subobject : *object.nodes) {
            if (FBOMResult err = subobject.Visit(this, out)) {
                return err;
            }
        }

        out->Write<uint8>(FBOM_OBJECT_END);

        if (static_data_ptr != nullptr) {
            m_write_stream->MarkStaticDataWritten(id);
        }

        break;
    }
    case FBOMDataLocation::LOC_EXT_REF:
    {
        AssertThrow(external_object_info_ptr != nullptr);
        AssertThrow(external_object_info_ptr->IsLinked());

        out->Write<UUID>(external_object_info_ptr->library_id);

        // write object index as u32
        out->Write<uint32>(external_object_info_ptr->index);

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

FBOMResult FBOMWriter::Write(ByteWriter *out, const FBOMType &type, UniqueID id, EnumFlags<FBOMDataAttributes> attributes)
{
    const FBOMStaticData *static_data_ptr;
    const FBOMExternalObjectInfo *external_object_info_ptr;

    const FBOMDataLocation data_location = m_write_stream->GetDataLocation(id, &static_data_ptr, &external_object_info_ptr);
    
    if (FBOMResult err = WriteDataAttributes(out, attributes, data_location)) {
        return err;
    }

    if (data_location == FBOMDataLocation::LOC_STATIC) {
        AssertThrow(static_data_ptr != nullptr);

        return WriteStaticDataUsage(out, *static_data_ptr);
    }

    if (data_location == FBOMDataLocation::LOC_INPLACE) {
        if (type.extends != nullptr) {
            out->Write<uint8>(1);

            if (FBOMResult err = type.extends->Visit(this, out)) {
                return err;
            }
        } else {
            out->Write<uint8>(0);
        }

        // write string of object type (loader to use)
        out->WriteString(type.name, BYTE_WRITER_FLAGS_WRITE_SIZE | BYTE_WRITER_FLAGS_WRITE_STRING_TYPE);

        // write flags
        out->Write<uint8>(uint8(type.flags));

        // write size of the type
        out->Write<uint64>(type.size);

        // write native TypeID
        out->Write<TypeID::ValueType>(type.GetNativeTypeID().Value());

        if (static_data_ptr) {
            m_write_stream->MarkStaticDataWritten(id);
        }
    } else {
        // unsupported method
        return FBOMResult::FBOM_ERR;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::Write(ByteWriter *out, const FBOMData &data, UniqueID id, EnumFlags<FBOMDataAttributes> attributes)
{
    ByteWriter *writer_ptr = out;

    const FBOMStaticData *static_data_ptr;
    const FBOMExternalObjectInfo *external_object_info_ptr;

    const FBOMDataLocation data_location = m_write_stream->GetDataLocation(id, &static_data_ptr, &external_object_info_ptr);
    
    if (FBOMResult err = WriteDataAttributes(out, attributes, data_location)) {
        return err;
    }

    if (data_location == FBOMDataLocation::LOC_STATIC) {
        AssertThrow(static_data_ptr != nullptr);

        return WriteStaticDataUsage(out, *static_data_ptr);
    }

    if (data_location == FBOMDataLocation::LOC_INPLACE) {
        // write type first
        if (FBOMResult err = data.GetType().Visit(this, out)) {
            return err;
        }

        SizeType size = data.TotalSize();
        ByteBuffer byte_buffer;

        if (FBOMResult err = data.ReadBytes(size, byte_buffer)) {
            return err;
        }

        MemoryByteWriter writer;

        if (attributes & FBOMDataAttributes::COMPRESSED) {
            writer_ptr = &writer;
        }

        if (data.GetType().HasAnyFlagsSet(FBOMTypeFlags::CONTAINER)) {
            if (data.IsObject()) {
                FBOMObject object;
                
                if (FBOMResult err = data.ReadObject(object)) {
                    return err;
                }

                BufferedReader byte_reader(RC<BufferedReaderSource>(new MemoryBufferedReaderSource(byte_buffer.ToByteView())));

                FBOMReader deserializer(fbom::FBOMConfig { });

                if (FBOMResult err = deserializer.ReadObject(&byte_reader, object, nullptr)) {
                    return err;
                }

                if (FBOMResult err = object.Visit(this, writer_ptr)) {
                    return err;
                }
            } else if (data.IsArray()) {
                FBOMArray array;
                
                if (FBOMResult err = data.ReadArray(array)) {
                    return err;
                }

                BufferedReader byte_reader(RC<BufferedReaderSource>(new MemoryBufferedReaderSource(byte_buffer.ToByteView())));

                FBOMReader deserializer(fbom::FBOMConfig { });

                if (FBOMResult err = deserializer.ReadArray(&byte_reader, array)) {
                    return err;
                }

                if (FBOMResult err = array.Visit(this, writer_ptr)) {
                    return err;
                }
            } else {
                return FBOMResult { FBOMResult::FBOM_ERR, "Unhandled container type" };
            }
        } else {
            //if (size == 0) {
            //    return FBOMResult { FBOMResult::FBOM_ERR, "Data size is 0" };
            //}

            // raw bytebuffer - write size and then buffer
            writer_ptr->Write<uint32>(uint32(size));
            writer_ptr->Write(byte_buffer.Data(), byte_buffer.Size());
        }

        if (attributes & FBOMDataAttributes::COMPRESSED) {
            if (!Archive::IsEnabled()) {
                return { FBOMResult::FBOM_ERR, "Cannot write to archive because the Archive feature is not enabled" };
            }

            // Write compressed data
            ArchiveBuilder archive_builder;
            archive_builder.Append(std::move(writer.GetBuffer()));

            if (FBOMResult err = WriteArchive(out, archive_builder.Build())) {
                return err;
            }
        }

        if (static_data_ptr) {
            m_write_stream->MarkStaticDataWritten(id);
        }
    } else {
        // Unsupported method
        return FBOMResult::FBOM_ERR;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::Write(ByteWriter *out, const FBOMArray &array, UniqueID id, EnumFlags<FBOMDataAttributes> attributes)
{
    const FBOMStaticData *static_data_ptr;
    const FBOMExternalObjectInfo *external_object_info_ptr;
    
    const FBOMDataLocation data_location = m_write_stream->GetDataLocation(id, &static_data_ptr, &external_object_info_ptr);
    
    if (FBOMResult err = WriteDataAttributes(out, attributes, data_location)) {
        return err;
    }

    if (data_location == FBOMDataLocation::LOC_STATIC) {
        AssertThrow(static_data_ptr != nullptr);

        return WriteStaticDataUsage(out, *static_data_ptr);
    }

    if (data_location == FBOMDataLocation::LOC_INPLACE) {
        // Write array size
        out->Write<uint32>(uint32(array.Size()));

        // Write each element
        for (SizeType index = 0; index < array.Size(); index++) {
            const FBOMData *value = array.TryGetElement(index);

            if (!value) {
                return { FBOMResult::FBOM_ERR, "Array had invalid element" };
            }

            if (FBOMResult err = value->Visit(this, out)) {
                return err;
            }
        }

        if (static_data_ptr) {
            m_write_stream->MarkStaticDataWritten(id);
        }
    } else {
        // Unsupported method
        return FBOMResult::FBOM_ERR;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::Write(ByteWriter *out, const FBOMNameTable &name_table, UniqueID id, EnumFlags<FBOMDataAttributes> attributes)
{
    const FBOMStaticData *static_data_ptr;
    const FBOMExternalObjectInfo *external_object_info_ptr;
    
    const FBOMDataLocation data_location = m_write_stream->GetDataLocation(id, &static_data_ptr, &external_object_info_ptr);
    
    if (FBOMResult err = WriteDataAttributes(out, attributes, data_location)) {
        return err;
    }

    if (data_location == FBOMDataLocation::LOC_STATIC) {
        AssertThrow(static_data_ptr != nullptr);

        return WriteStaticDataUsage(out, *static_data_ptr);
    }

    if (data_location == FBOMDataLocation::LOC_INPLACE) {
        out->Write<uint32>(uint32(name_table.values.Size()));

        for (const auto &it : name_table.values) {
            out->WriteString(it.second, BYTE_WRITER_FLAGS_WRITE_SIZE | BYTE_WRITER_FLAGS_WRITE_STRING_TYPE);
            out->Write<NameID>(it.first.GetID());
        }

        if (static_data_ptr) {
            m_write_stream->MarkStaticDataWritten(id);
        }
    } else {
        // Unsupported method
        return FBOMResult::FBOM_ERR;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteArchive(ByteWriter *out, const Archive &archive) const
{
    out->Write<uint64>(archive.GetUncompressedSize());
    out->Write<uint64>(archive.GetCompressedSize());
    out->Write(archive.GetCompressedBuffer().Data(), archive.GetCompressedBuffer().Size());

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteDataAttributes(ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes) const
{
    out->Write<uint8>(uint8(attributes));

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteDataAttributes(ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes, FBOMDataLocation location) const
{
    constexpr uint8 loc_static = (1u << uint32(FBOMDataLocation::LOC_STATIC)) << 5;
    constexpr uint8 loc_inplace = (1u << uint32(FBOMDataLocation::LOC_INPLACE)) << 5;
    constexpr uint8 loc_ext_ref = (1u << uint32(FBOMDataLocation::LOC_EXT_REF)) << 5;

    uint8 attributes_value = uint8(attributes);

    switch (location) {
    case FBOMDataLocation::LOC_STATIC:
        attributes_value |= loc_static;

        break;
    case FBOMDataLocation::LOC_INPLACE:
        attributes_value |= loc_inplace;

        break;
    case FBOMDataLocation::LOC_EXT_REF:
        attributes_value |= loc_ext_ref;

        break;
    }

    out->Write<uint8>(attributes_value);

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteStaticDataUsage(ByteWriter *out, const FBOMStaticData &static_data) const
{
    const auto offset = static_data.offset;
    AssertThrow(offset < m_write_stream->m_static_data_offset);

#ifdef HYP_DEBUG_MODE
    const auto it = m_write_stream->m_static_data.Find(static_data.GetUniqueID());
    AssertThrow(it != m_write_stream->m_static_data.End());

    // AssertThrow(it->second.IsWritten());

    AssertThrow(it->second.type == static_data.type);
    AssertThrow(it->second.GetHashCode() == static_data.GetHashCode());
#endif

    out->Write<uint32>(uint32(offset));

    return FBOMResult::FBOM_OK;
}

void FBOMWriter::AddObjectData(const FBOMObject &object, UniqueID id)
{
    AssertThrow(uint64(id) != 0);

    AssertThrow(!m_write_stream->IsObjectDataWritingLocked());
    
    m_write_stream->m_object_data.PushBack(object);

    auto it = m_write_stream->m_hash_use_count_map.Find(id);

    if (it == m_write_stream->m_hash_use_count_map.End()) {
        it = m_write_stream->m_hash_use_count_map.Insert(id, 0).first;
    }

    it->second++;
}

void FBOMWriter::AddObjectData(FBOMObject &&object, UniqueID id)
{
    AssertThrow(uint64(id) != 0);

    AssertThrow(!m_write_stream->IsObjectDataWritingLocked());

    m_write_stream->m_object_data.PushBack(std::move(object));

    auto it = m_write_stream->m_hash_use_count_map.Find(id);

    if (it == m_write_stream->m_hash_use_count_map.End()) {
        it = m_write_stream->m_hash_use_count_map.Insert(id, 0).first;
    }

    it->second++;
}

UniqueID FBOMWriter::AddStaticData(UniqueID id, FBOMStaticData &&static_data)
{
    AssertThrow(!m_write_stream->IsWritingStaticData());

    auto it = m_write_stream->m_static_data.Find(id);

    if (it == m_write_stream->m_static_data.End()) { 
        static_data.SetUniqueID(id);
        static_data.offset = m_write_stream->m_static_data_offset++;

        const auto insert_result = m_write_stream->m_static_data.Insert(id, std::move(static_data));
        AssertThrow(insert_result.second);
    } else {
#if 0
        // sanity check - ensure pointing to correct object
        const HashCode hash_code = static_data.GetHashCode();
        const HashCode other_hash_code = it->second.GetHashCode();

        AssertThrowMsg(hash_code == other_hash_code,
            "Hash codes do not match: %llu != %llu\n\tLeft: %s\n\tRight: %s",
            hash_code.Value(), other_hash_code.Value(),
            static_data.ToString().Data(), it->second.ToString().Data());
#endif
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

UniqueID FBOMWriter::AddStaticData(const FBOMArray &array)
{
    return AddStaticData(FBOMStaticData(array));
}

UniqueID FBOMWriter::AddStaticData(const FBOMData &data)
{
    UniqueID id = UniqueID::Invalid();

    AddStaticData(data.GetType());

    if (data.GetType().HasAnyFlagsSet(FBOMTypeFlags::CONTAINER)) {
        // If it's a container type requiring custom logic, read the data and add it directly, so static data be more effectively shared

        if (data.IsObject()) {
            FBOMObject object;
            AssertThrowMsg(data.ReadObject(object).value == FBOMResult::FBOM_OK, "Invalid object, cannot write to stream");

            AddStaticData(object);
        } else if (data.IsArray()) {
            FBOMArray array;
            AssertThrowMsg(data.ReadArray(array).value == FBOMResult::FBOM_OK, "Invalid array, cannot write to stream");

            AddStaticData(array);
        } else {
            HYP_FAIL("Unhandled container type");
        }
    } else if (data.IsName()) {
        // Custom logic for `Name` objects: store their string data in the stream's name table

        Name name;
        AssertThrowMsg(data.ReadName(&name).value == FBOMResult::FBOM_OK, "Invalid name, cannot write to stream");

        m_write_stream->GetNameTable().Add(name.LookupString());
    }

    return AddStaticData(FBOMStaticData(data));
}

UniqueID FBOMWriter::AddStaticData(const FBOMNameTable &name_table)
{
    return AddStaticData(FBOMStaticData(name_table));
}

#pragma endregion FBOMWriter

} // namespace hyperion::fbom