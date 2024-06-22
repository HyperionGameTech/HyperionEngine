/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/FBOMArray.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/compression/Archive.hpp>

#include <math/MathUtil.hpp>

namespace hyperion {
namespace utilities {

template <class StringType>
struct Formatter<StringType, fbom::FBOMVersion>
{
    auto operator()(const fbom::FBOMVersion &value) const
    {
        return Format< StaticString("{}.{}.{}") >(value.GetMajor(), value.GetMinor(), value.GetPatch());
    }
};

} // namespace utilities

namespace fbom {

template <class StringType>
FBOMResult FBOMReader::ReadString(BufferedReader *reader, StringType &out_string)
{
    // read 4 bytes of string header
    uint32 string_header;
    reader->Read(&string_header);
    CheckEndianness(string_header);

    const uint32 string_length = (string_header & ByteWriter::string_length_mask) >> 8;
    const uint32 string_type = (string_header & ByteWriter::string_type_mask);
    
    if (string_type != 0 && string_type != StringType::string_type) {
        return FBOMResult(FBOMResult::FBOM_ERR, "Error reading string: string type mismatch");
    }

    ByteBuffer string_buffer(string_length + 1);

    uint32 read_length;

    if ((read_length = reader->Read(string_buffer.Data(), string_length)) != string_length) {
        return FBOMResult(FBOMResult::FBOM_ERR, "Error reading string: string length mismatch");
    }

    out_string = StringType(string_buffer);

    return FBOMResult::FBOM_OK;
}

FBOMReader::FBOMReader(const FBOMConfig &config)
    : m_config(config),
      m_in_static_data(false),
      m_swap_endianness(false)
{
}

FBOMReader::~FBOMReader() = default;

FBOMResult FBOMReader::Deserialize(BufferedReader &reader, FBOMObjectLibrary &out)
{
    if (reader.Eof()) {
        return { FBOMResult::FBOM_ERR, "Stream not open" };
    }

    FBOMObject root(FBOMObjectType("ROOT"));

    { // read header
        ubyte header_bytes[FBOM::header_size];

        if (reader.Read(header_bytes, FBOM::header_size) != FBOM::header_size) {
            return { FBOMResult::FBOM_ERR, "Invalid header" };
        }

        if (Memory::StrCmp(reinterpret_cast<const char *>(header_bytes), FBOM::header_identifier, sizeof(FBOM::header_identifier) - 1) != 0) {
            return { FBOMResult::FBOM_ERR, "Invalid header identifier" };
        }

        // read endianness
        const ubyte endianness = header_bytes[sizeof(FBOM::header_identifier)];
        
        // set if it needs to swap endianness.
        m_swap_endianness = bool(endianness) != IsBigEndian();

        // get version info
        FBOMVersion binary_version;
        Memory::MemCpy(&binary_version.value, header_bytes + sizeof(FBOM::header_identifier) + sizeof(uint8), sizeof(uint32));

        const int compatibility_test_result = FBOMVersion::TestCompatibility(binary_version, FBOM::version);
        
        if (compatibility_test_result != 0) {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Unsupported binary version! Got {} but current is {}. Result: {}", binary_version, FBOM::version, compatibility_test_result) };
        }
    }

    m_static_data_pool.Clear();
    m_in_static_data = false;

    // expect first FBOMObject defined
    FBOMCommand command = FBOM_NONE;

    while (!reader.Eof()) {
        command = PeekCommand(&reader);

        if (FBOMResult err = Handle(&reader, command, &root)) {
            return err;
        }
    }

    if (root.nodes->Empty()) {
        return { FBOMResult::FBOM_ERR, "No object added to root" };
    }

    if (root.nodes->Size() > 1) {
        return { FBOMResult::FBOM_ERR, "> 1 objects added to root (not supported in current implementation)" };
    }

    out = FBOMObjectLibrary { *root.nodes };

    return { FBOMResult::FBOM_OK };
}

FBOMResult FBOMReader::Deserialize(BufferedReader &reader, FBOMObject &out)
{
    FBOMObjectLibrary library;

    if (FBOMResult err = Deserialize(reader, library)) {
        return err;
    }

    if (library.objects.Empty()) {
        return { FBOMResult::FBOM_ERR, "Loaded library contains no objects." };
    }

    if (library.objects.Size() > 1) {
        HYP_LOG(Serialization, LogLevel::WARNING, "Loaded libary contains more than one object when attempting to load a single object. The first object will be used.");
    }

    out = std::move(library.objects.Front());

    return { FBOMResult::FBOM_OK };
}

FBOMResult FBOMReader::Deserialize(const FBOMObject &in, FBOMDeserializedObject &out_object)
{
    const FBOMMarshalerBase *marshal = FBOM::GetInstance().GetMarshal(in.m_object_type.name);

    if (!marshal) {
        return { FBOMResult::FBOM_ERR, "Marshal class not registered for type" };
    }

    return marshal->Deserialize(in, out_object.m_value);
}

FBOMResult FBOMReader::Deserialize(BufferedReader &reader, FBOMDeserializedObject &out_object)
{
    FBOMObject obj;
    FBOMResult res = Deserialize(reader, obj);

    if (res.value != FBOMResult::FBOM_OK) {
        return res;
    }

    return Deserialize(obj, out_object);
}

FBOMResult FBOMReader::LoadFromFile(const String &path, FBOMObjectLibrary &out)
{
    // Include our root dir as part of the path
    if (m_config.base_path.Empty()) {
        m_config.base_path = FileSystem::RelativePath(StringUtil::BasePath(path.Data()), FileSystem::CurrentPath()).c_str();
    }

    const FilePath read_path { FileSystem::Join(m_config.base_path.Data(), FilePath(path).Basename().Data()).c_str()};

    BufferedReader reader(RC<FileBufferedReaderSource>(new FileBufferedReaderSource(read_path)));

    return Deserialize(reader, out);
}

FBOMResult FBOMReader::LoadFromFile(const String &path, FBOMObject &out)
{
    // Include our root dir as part of the path
    if (m_config.base_path.Empty()) {
        m_config.base_path = FileSystem::RelativePath(StringUtil::BasePath(path.Data()), FileSystem::CurrentPath()).c_str();
    }

    const FilePath read_path { FileSystem::Join(m_config.base_path.Data(), FilePath(path).Basename().Data()).c_str()};

    BufferedReader reader(RC<FileBufferedReaderSource>(new FileBufferedReaderSource(read_path)));

    return Deserialize(reader, out);
}

FBOMResult FBOMReader::LoadFromFile(const String &path, FBOMDeserializedObject &out)
{
    FBOMObject object;
    
    if (FBOMResult err = LoadFromFile(path, object)) {
        return err;
    }

    if (object.m_deserialized_object != nullptr) {
        out = std::move(*object.m_deserialized_object);
    }

    return { FBOMResult::FBOM_OK };
}

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

FBOMResult FBOMReader::RequestExternalObject(const ANSIStringView &key, uint32 index, FBOMObject &out_object)
{
    String base_path = m_config.base_path;

    if (base_path.Empty()) {
        base_path = FilePath::Current();
    }

    const String ref_path(FileSystem::Join(FileSystem::CurrentPath(), base_path.Data(), key.Data()).data());
    const String relative_path(FileSystem::RelativePath(std::string(ref_path.Data()), FileSystem::CurrentPath()).data());

    { // check in cache
        const auto it = m_config.external_data_cache.Find(relative_path);

        if (it != m_config.external_data_cache.End()) {
            if (FBOMObject *found_object = it->second.TryGet(index)) {
                out_object = *found_object;

                return { FBOMResult::FBOM_OK };
            } else {
                return { FBOMResult::FBOM_ERR, "Object not found in library" };
            }
        }
    }

    FBOMObjectLibrary library;

    if (FBOMResult err = FBOMReader(m_config).LoadFromFile(ref_path, library)) {
        if (!m_config.continue_on_external_load_error) {
            return err;
        }
    }

    // cache it
    auto insert_result = m_config.external_data_cache.Set(relative_path, std::move(library));
    AssertThrow(insert_result.second);

    auto it = insert_result.first;
    
    if (FBOMObject *found_object = it->second.TryGet(index)) {
        out_object = *found_object;

        return { FBOMResult::FBOM_OK };
    } else {
        return { FBOMResult::FBOM_ERR, "Object not found in library" };
    }

    return { FBOMResult::FBOM_OK };
}

FBOMResult FBOMReader::ReadDataAttributes(BufferedReader *reader, EnumFlags<FBOMDataAttributes> &out_attributes, FBOMDataLocation &out_location)
{
    constexpr uint8 loc_static = (1u << uint32(FBOMDataLocation::LOC_STATIC)) << 5;
    constexpr uint8 loc_inplace = (1u << uint32(FBOMDataLocation::LOC_INPLACE)) << 5;
    constexpr uint8 loc_ext_ref = (1u << uint32(FBOMDataLocation::LOC_EXT_REF)) << 5;

    uint8 attributes_value;
    reader->Read(&attributes_value);
    CheckEndianness(attributes_value);

    if (attributes_value & loc_static) {
        out_location = FBOMDataLocation::LOC_STATIC;
    } else if (attributes_value & loc_inplace) {
        out_location = FBOMDataLocation::LOC_INPLACE;
    } else if (attributes_value & loc_ext_ref) {
        out_location = FBOMDataLocation::LOC_EXT_REF;
    } else {
        return { FBOMResult::FBOM_ERR, "No data location on attributes" };
    }
    
    out_attributes = EnumFlags<FBOMDataAttributes>(attributes_value & ~uint8(FBOMDataAttributes::LOCATION_MASK));

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadObjectType(BufferedReader *reader, FBOMType &out_type)
{
    out_type = FBOMUnset();

    EnumFlags<FBOMDataAttributes> attributes;
    FBOMDataLocation location;

    if (FBOMResult err = ReadDataAttributes(reader, attributes, location)) {
        return err;
    }

    switch (location) {
    case FBOMDataLocation::LOC_INPLACE:
    {
        FBOMType parent_type = FBOMUnset();

        uint8 has_parent;
        reader->Read(&has_parent);
        CheckEndianness(has_parent);

        if (has_parent) {
            if (FBOMResult err = ReadObjectType(reader, parent_type)) {
                return err;
            }

            out_type = parent_type.Extend(out_type);
        }

        if (FBOMResult err = ReadString(reader, out_type.name)) {
            return err;
        }

        // read size of object
        uint64 type_size;
        reader->Read(&type_size);
        CheckEndianness(type_size);

        out_type.size = type_size;

        break;
    }
    case FBOMDataLocation::LOC_STATIC:
    {
        // read offset as u32
        uint32 offset;
        reader->Read(&offset);
        CheckEndianness(offset);
        
        AssertThrowMsg(offset < m_static_data_pool.Size(),
            "Offset out of bounds of static data pool: %u >= %u",
            offset,
            m_static_data_pool.Size());

        // grab from static data pool
        FBOMType *as_type = m_static_data_pool[offset].data.TryGetAsDynamic<FBOMType>();
        AssertThrow(as_type != nullptr);
        out_type = *as_type;

        break;
    }
    default:
        AssertThrowMsg(false, "Invalid data location type");
        break;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadData(BufferedReader *reader, FBOMData &out_data)
{
    BufferedReader *reader_ptr = reader;

    EnumFlags<FBOMDataAttributes> attributes;
    FBOMDataLocation location;

    if (FBOMResult err = ReadDataAttributes(reader, attributes, location)) {
        return err;
    }

    if (location == FBOMDataLocation::LOC_INPLACE) {
        FBOMType object_type;
        
        if (FBOMResult err = ReadObjectType(reader, object_type)) {
            return err;
        }

        BufferedReader compressed_data_reader;

        ByteBuffer byte_buffer;
        
        if (attributes & FBOMDataAttributes::COMPRESSED) {
            // Read archive
            Archive archive;

            if (FBOMResult err = ReadArchive(reader, archive)) {
                return err;
            }

            if (!Archive::IsEnabled()) {
                return { FBOMResult::FBOM_ERR, "Cannot decompress archive because the Archive feature is not enabled" };
            }

            if (ArchiveResult err = archive.Decompress(byte_buffer)) {
                return { FBOMResult::FBOM_ERR, err.message.Data() };
            }

            compressed_data_reader = BufferedReader(RC<BufferedReaderSource>(new MemoryBufferedReaderSource(byte_buffer.ToByteView())));
            reader_ptr = &compressed_data_reader;
        }

        if (object_type.IsOrExtends(FBOMBaseObjectType())) {
            FBOMObject object;

            if (FBOMResult err = ReadObject(reader_ptr, object, nullptr)) {
                return err;
            }

            out_data = FBOMData::FromObject(object);
        } else if (object_type.IsOrExtends(FBOMArrayType())) {
            FBOMArray array;

            if (FBOMResult err = ReadArray(reader_ptr, array)) {
                return err;
            }

            out_data = FBOMData::FromArray(array);
        } else {
            // Read bytebuffer of raw data
            uint32 sz;
            reader_ptr->Read(&sz);
            CheckEndianness(sz);

            byte_buffer = reader_ptr->ReadBytes(sz);

            if (byte_buffer.Size() != sz) {
                return { FBOMResult::FBOM_ERR, "File is corrupted" };
            }

            out_data = FBOMData(object_type, std::move(byte_buffer));
        }
    } else if (location == FBOMDataLocation::LOC_STATIC) {
        // read offset as u32
        uint32 offset;
        reader->Read(&offset);
        CheckEndianness(offset);
        
        AssertThrowMsg(offset < m_static_data_pool.Size(),
            "Offset out of bounds of static data pool: %u >= %u",
            offset,
            m_static_data_pool.Size());

        // grab from static data pool
        FBOMData *as_data = m_static_data_pool[offset].data.TryGetAsDynamic<FBOMData>();
        AssertThrow(as_data != nullptr);
        out_data = *as_data;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadArray(BufferedReader *reader, FBOMArray &out_array)
{
    EnumFlags<FBOMDataAttributes> attributes;
    FBOMDataLocation location;

    if (FBOMResult err = ReadDataAttributes(reader, attributes, location)) {
        return err;
    }

    if (location == FBOMDataLocation::LOC_INPLACE) {
        // Read array size
        uint32 sz;
        reader->Read(&sz);
        CheckEndianness(sz);

        // Read array items
        for (uint32 index = 0; index < sz; index++) {
            FBOMData element_data;

            if (FBOMResult err = ReadData(reader, element_data)) {
                return err;
            }

            out_array.AddElement(std::move(element_data));
        }
    } else if (location == FBOMDataLocation::LOC_STATIC) {
        // read offset as u32
        uint32 offset;
        reader->Read(&offset);
        CheckEndianness(offset);
        
        AssertThrowMsg(offset < m_static_data_pool.Size(),
            "Offset out of bounds of static data pool: %u >= %u",
            offset,
            m_static_data_pool.Size());

        // grab from static data pool
        FBOMArray *as_array = m_static_data_pool[offset].data.TryGetAsDynamic<FBOMArray>();
        AssertThrow(as_array != nullptr);
        out_array = *as_array;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadNameTable(BufferedReader *reader, FBOMNameTable &out_name_table)
{
    EnumFlags<FBOMDataAttributes> attributes;
    FBOMDataLocation location;

    if (FBOMResult err = ReadDataAttributes(reader, attributes, location)) {
        return err;
    }

    if (location == FBOMDataLocation::LOC_INPLACE) {
        uint32 count;
        reader->Read(&count);
        CheckEndianness(count);

        for (uint32 i = 0; i < count; i++) {
            ANSIString str;
            NameID name_data;

            if (FBOMResult err = ReadString(reader, str)) {
                return err;
            }

            if (FBOMResult err = ReadRawData<NameID>(reader, &name_data)) {
                return err;
            }

            out_name_table.Add(str, WeakName(name_data));
        }
    } else if (location == FBOMDataLocation::LOC_STATIC) {
        // read offset as u32
        uint32 offset;
        reader->Read(&offset);
        CheckEndianness(offset);
        
        AssertThrowMsg(offset < m_static_data_pool.Size(),
            "Offset out of bounds of static data pool: %u >= %u",
            offset,
            m_static_data_pool.Size());

        // grab from static data pool
        FBOMNameTable *as_name_table = m_static_data_pool[offset].data.TryGetAsDynamic<FBOMNameTable>();
        AssertThrow(as_name_table != nullptr);
        out_name_table = *as_name_table;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadPropertyName(BufferedReader *reader, Name &out_property_name)
{
    FBOMData name_data;

    if (FBOMResult err = ReadData(reader, name_data)) {
        return err;
    }

    if (FBOMResult err = name_data.ReadName(&out_property_name)) {
        const FBOMType *root_type = &name_data.GetType();

        while (root_type != nullptr) {
            DebugLog(LogType::Error, "root_type: %s\n", root_type->name.Data());

            root_type = root_type->extends;
        }

        HYP_BREAKPOINT;

        return FBOMResult(FBOMResult::FBOM_ERR, "Invalid property name: Expected data to be of type `Name`");
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadObject(BufferedReader *reader, FBOMObject &out_object, FBOMObject *root)
{
    if (FBOMResult err = Eat(reader, FBOM_OBJECT_START)) {
        return err;
    }

    FBOMCommand command = FBOM_NONE;

    // read unique ID
    uint64 unique_id;
    reader->Read(&unique_id);
    CheckEndianness(unique_id);

    EnumFlags<FBOMDataAttributes> attributes;
    FBOMDataLocation location;

    if (FBOMResult err = ReadDataAttributes(reader, attributes, location)) {
        return err;
    }

    switch (location) {
    case FBOMDataLocation::LOC_STATIC:
    {
        // read offset as u32
        uint32 offset;
        reader->Read(&offset);
        CheckEndianness(offset);
        
        AssertThrowMsg(offset < m_static_data_pool.Size(),
            "Offset out of bounds of static data pool: %u >= %u",
            offset,
            m_static_data_pool.Size());

        // grab from static data pool
        FBOMObject *as_object = m_static_data_pool[offset].data.TryGetAsDynamic<FBOMObject>();
        AssertThrow(as_object != nullptr);
        out_object = *as_object;

        return FBOMResult::FBOM_OK;
    }
    case FBOMDataLocation::LOC_INPLACE:
    {
        // read string of "type" - loader to use
        FBOMType object_type;
        
        if (FBOMResult err = ReadObjectType(reader, object_type)) {
            return err;
        }

        // DebugLog("Read object ")

        out_object = FBOMObject(object_type);
        // object.m_unique_id = unique_id;

        do {
            command = PeekCommand(reader);

            switch (command) {
            case FBOM_OBJECT_START:
            {
                FBOMObject child;

                if (FBOMResult err = ReadObject(reader, child, root)) {
                    return err;
                }

                out_object.nodes->PushBack(std::move(child));

                break;
            }
            case FBOM_OBJECT_END:
                if (HasMarshalForType(object_type)) {
                    // call deserializer function, writing into deserialized object
                    out_object.m_deserialized_object.Reset(new FBOMDeserializedObject());

                    if (FBOMResult err = Deserialize(out_object, *out_object.m_deserialized_object)) {
                        return err;
                    }
                }

                break;
            case FBOM_DEFINE_PROPERTY:
            {
                if (FBOMResult err = Eat(reader, FBOM_DEFINE_PROPERTY)) {
                    return err;
                }

                Name property_name;
                
                if (FBOMResult err = ReadPropertyName(reader, property_name)) {
                    return err;
                }

                FBOMData data;

                if (FBOMResult err = ReadData(reader, data)) {
                    return err;
                }

                out_object.SetProperty(property_name, std::move(data));

                break;
            }
            default:
                return FBOMResult(FBOMResult::FBOM_ERR, "Could not process command while reading object");
            }
        } while (command != FBOM_OBJECT_END && command != FBOM_NONE);

        // eat end
        if (FBOMResult err = Eat(reader, FBOM_OBJECT_END)) {
            return err;
        }

        break;
    }
    case FBOMDataLocation::LOC_EXT_REF:
    {
        ANSIString ref_name;
        
        if (FBOMResult err = ReadString(reader, ref_name)) {
            return err;
        }
        
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

        if (FBOMResult err = RequestExternalObject(ref_name, object_index, out_object)) {
            return err;
        }

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

FBOMResult FBOMReader::ReadArchive(BufferedReader *reader, Archive &out_archive)
{
    uint64 uncompressed_size = 0;

    if (reader->Read<uint64>(&uncompressed_size) != sizeof(uint64)) {
        return { FBOMResult::FBOM_ERR, "Failed to read uncompressed size" };
    }

    CheckEndianness(uncompressed_size);

    uint64 compressed_size = 0;

    if (reader->Read<uint64>(&compressed_size) != sizeof(uint64)) {
        return { FBOMResult::FBOM_ERR, "Failed to read compressed size" };
    }

    CheckEndianness(compressed_size);

    ByteBuffer compressed_buffer = reader->ReadBytes(compressed_size);

    if (compressed_buffer.Size() != compressed_size) {
        return { FBOMResult::FBOM_ERR, "Failed to read compressed buffer - buffer size mismatch" };
    }

    out_archive = Archive(std::move(compressed_buffer), uncompressed_size);

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadArchive(const ByteBuffer &in_buffer, ByteBuffer &out_buffer)
{
    // Read archive
    Archive archive;

    BufferedReader reader(RC<BufferedReaderSource>(new MemoryBufferedReaderSource(in_buffer.ToByteView())));

    if (FBOMResult err = ReadArchive(&reader, archive)) {
        return err;
    }

    if (!Archive::IsEnabled()) {
        return { FBOMResult::FBOM_ERR, "Cannot decompress archive because the Archive feature is not enabled" };
    }

    if (ArchiveResult err = archive.Decompress(out_buffer)) {
        return { FBOMResult::FBOM_ERR, err.message.Data() };
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadRawData(BufferedReader *reader, SizeType count, ByteBuffer &out_buffer)
{
    if (reader->Position() + count > reader->Max()) {
        return { FBOMResult::FBOM_ERR, "File is corrupted: attempted to read past end" };
    }

    out_buffer = reader->ReadBytes(count);

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::Handle(BufferedReader *reader, FBOMCommand command, FBOMObject *root)
{
    AssertThrow(root != nullptr);

    switch (command) {
    case FBOM_OBJECT_START:
    {
        FBOMObject child;

        if (FBOMResult err = ReadObject(reader, child, root)) {
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

        if (FBOMResult err = Eat(reader, FBOM_STATIC_DATA_START)) {
            return err;
        }

        m_in_static_data = true;

        // read u32 describing size of static data pool
        uint32 static_data_size;
        reader->Read(&static_data_size);
        CheckEndianness(static_data_size);

        const uint32 initial_static_data_size = static_data_size;

        // skip 8 bytes of padding
        uint64 tmp;
        reader->Read(&tmp);
        CheckEndianness(tmp);

        m_static_data_pool.Resize(static_data_size);

        // now read each item
        //   u32 as index/offset
        //   u8 as type of static data
        //   then, the actual size of the data will vary depending on the held type
        for (uint32 i = 0; i < static_data_size; i++) {
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

                if (FBOMResult err = ReadObject(reader, object, root)) {
                    return err;
                }

                m_static_data_pool[offset] = FBOMStaticData(std::move(object), offset);

                break;
            }
            case FBOMStaticData::FBOM_STATIC_DATA_TYPE:
            {
                FBOMType type;

                if (FBOMResult err = ReadObjectType(reader, type)) {
                    return err;
                }

                m_static_data_pool[offset] = FBOMStaticData(std::move(type), offset);

                break;
            }
            case FBOMStaticData::FBOM_STATIC_DATA_DATA:
            {
                FBOMData data;

                if (FBOMResult err = ReadData(reader, data)) {
                    return err;
                }

                m_static_data_pool[offset] = FBOMStaticData(std::move(data), offset);

                break;
            }
            case FBOMStaticData::FBOM_STATIC_DATA_ARRAY:
            {
                FBOMArray array;

                if (FBOMResult err = ReadArray(reader, array)) {
                    return err;
                }

                m_static_data_pool[offset] = FBOMStaticData(std::move(array), offset);

                break;
            }
            case FBOMStaticData::FBOM_STATIC_DATA_NAME_TABLE:
            {
                FBOMNameTable name_table;

                if (FBOMResult err = ReadNameTable(reader, name_table)) {
                    return err;
                }

                name_table.RegisterAllNamesGlobally();

                m_static_data_pool[offset] = FBOMStaticData(std::move(name_table), offset);

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

        if (FBOMResult err = Eat(reader, FBOM_STATIC_DATA_END)) {
            return err;
        }

        m_in_static_data = false;

        break;
    }
    default:
        AssertThrowMsg(false, "Cannot process command %d in top level at position: %u", int(command), reader->Position() - sizeof(FBOMCommand));

        break;
    }

    return FBOMResult::FBOM_OK;
}

} // namespace fbom
} // namespace hyperion