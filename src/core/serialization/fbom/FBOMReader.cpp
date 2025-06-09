/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOMReader.hpp>
#include <core/serialization/fbom/FBOMArray.hpp>
#include <core/serialization/fbom/FBOMLoadContext.hpp>
#include <core/serialization/fbom/FBOM.hpp>

#include <core/io/BufferedByteReader.hpp>

#include <core/utilities/Format.hpp>

#include <core/object/HypData.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/compression/Archive.hpp>

namespace hyperion {
namespace utilities {

template <class StringType>
struct Formatter<StringType, FBOMVersion>
{
    auto operator()(const FBOMVersion& value) const
    {
        return Format<HYP_STATIC_STRING("{}.{}.{}")>(value.GetMajor(), value.GetMinor(), value.GetPatch());
    }
};

} // namespace utilities

namespace serialization {

#pragma region FBOMStaticDataIndexMap

FBOMResult FBOMReader::FBOMStaticDataIndexMap::Element::Initialize(FBOMLoadContext& context, FBOMReader* reader)
{
    AssertThrow(reader != nullptr);

    AssertThrow(IsValid());

    if (IsInitialized())
    {
        return { FBOMResult::FBOM_OK };
    }

    ConstByteView view = reader->m_static_data_buffer.ToByteView()
                             .Slice(offset, size);

    MemoryBufferedReaderSource source { view };
    BufferedReader byte_reader { &source };

    switch (type)
    {
    case FBOMStaticData::FBOM_STATIC_DATA_NONE:
        return { FBOMResult::FBOM_ERR, "Cannot process static data type, unknown type" };
    case FBOMStaticData::FBOM_STATIC_DATA_OBJECT:
    {
        ptr = MakeUnique<FBOMObject>();

        if (FBOMResult err = reader->ReadObject(context, &byte_reader, *static_cast<FBOMObject*>(ptr.Get()), nullptr))
        {
            ptr.Reset();

            return err;
        }

        break;
    }
    case FBOMStaticData::FBOM_STATIC_DATA_TYPE:
    {
        ptr = MakeUnique<FBOMType>();

        if (FBOMResult err = reader->ReadObjectType(context, &byte_reader, *static_cast<FBOMType*>(ptr.Get())))
        {
            ptr.Reset();

            return err;
        }

        break;
    }
    case FBOMStaticData::FBOM_STATIC_DATA_DATA:
    {
        ptr = MakeUnique<FBOMData>();

        if (FBOMResult err = reader->ReadData(context, &byte_reader, *static_cast<FBOMData*>(ptr.Get())))
        {
            ptr.Reset();

            return err;
        }

        AssertThrow(static_cast<FBOMData*>(ptr.Get())->TotalSize() != 0);

        break;
    }
    case FBOMStaticData::FBOM_STATIC_DATA_ARRAY:
    {
        ptr = MakeUnique<FBOMArray>(FBOMUnset());

        if (FBOMResult err = reader->ReadArray(context, &byte_reader, *static_cast<FBOMArray*>(ptr.Get())))
        {
            ptr.Reset();

            return err;
        }

        break;
    }
    default:
        return { FBOMResult::FBOM_ERR, "Cannot process static data type, unknown type" };
    }

    return { FBOMResult::FBOM_OK };
}

FBOMSerializableBase* FBOMReader::FBOMStaticDataIndexMap::GetOrInitializeElement(FBOMLoadContext& context, FBOMReader* reader, SizeType index)
{
    if (index >= elements.Size())
    {
        return nullptr;
    }

    Element& element = elements[index];

    if (!element.IsValid())
    {
        return nullptr;
    }

    if (!element.IsInitialized())
    {
        if (FBOMResult err = element.Initialize(context, reader))
        {
            HYP_LOG(Serialization, Error, "Error initializing static data element at index {}: {}", index, err.message);

            return nullptr;
        }
    }

    return element.ptr.Get();
}

void FBOMReader::FBOMStaticDataIndexMap::SetElementDesc(SizeType index, FBOMStaticData::Type type, SizeType offset, SizeType size)
{
    if (index >= elements.Size())
    {
        elements.Resize(index + 1);
    }

    elements[index] = Element { type, offset, size };
}

#pragma endregion FBOMStaticDataIndexMap

#pragma region FBOMReader

template <class TString>
FBOMResult FBOMReader::ReadString(BufferedReader* reader, TString& out_string)
{
    // read 4 bytes of string header
    uint32 string_header;
    reader->Read(&string_header);
    CheckEndianness(string_header);

    const uint32 string_length = (string_header & ByteWriter::string_length_mask) >> 8;
    const uint32 string_type = (string_header & ByteWriter::string_type_mask);

    if (string_type != 0 && string_type != TString::string_type)
    {
        return FBOMResult(FBOMResult::FBOM_ERR, "Error reading string: string type mismatch");
    }

    ByteBuffer string_buffer(string_length + 1);

    uint32 read_length;

    if ((read_length = reader->Read(string_buffer.Data(), string_length)) != string_length)
    {
        return FBOMResult(FBOMResult::FBOM_ERR, "Error reading string: string length mismatch");
    }

    out_string = TString(string_buffer.ToByteView());

    return FBOMResult::FBOM_OK;
}

FBOMReader::FBOMReader(const FBOMReaderConfig& config)
    : m_config(config),
      m_in_static_data(false),
      m_swap_endianness(false)
{
}

FBOMReader::~FBOMReader() = default;

FBOMResult FBOMReader::Deserialize(FBOMLoadContext& context, BufferedReader& reader, FBOMObjectLibrary& out, bool read_header)
{
    if (reader.Eof())
    {
        return { FBOMResult::FBOM_ERR, "Stream not open" };
    }

    FBOMObject root(FBOMObjectType("ROOT"));

    if (read_header)
    {
        ubyte header_bytes[FBOM::header_size];

        if (reader.Read(header_bytes, FBOM::header_size) != FBOM::header_size
            || Memory::StrCmp(reinterpret_cast<const char*>(header_bytes), FBOM::header_identifier, sizeof(FBOM::header_identifier) - 1) != 0)
        {
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

        if (compatibility_test_result != 0)
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Unsupported binary version! Got {} but current is {}. Result: {}", binary_version, FBOM::version, compatibility_test_result) };
        }
    }

    m_static_data_index_map = FBOMStaticDataIndexMap();
    m_static_data_buffer = ByteBuffer();
    m_in_static_data = false;

    // expect first FBOMObject defined
    FBOMCommand command = FBOM_NONE;

    while (!reader.Eof())
    {
        command = PeekCommand(&reader);

        if (FBOMResult err = Handle(context, &reader, command, &root))
        {
            return err;
        }
    }

    if (root.GetChildren().Empty())
    {
        return { FBOMResult::FBOM_ERR, "No object added to root" };
    }

    out.object_data = std::move(root.m_children);

    return { FBOMResult::FBOM_OK };
}

FBOMResult FBOMReader::Deserialize(FBOMLoadContext& context, BufferedReader& reader, FBOMObject& out)
{
    FBOMObjectLibrary library;

    if (FBOMResult err = Deserialize(context, reader, library))
    {
        return err;
    }

    if (library.object_data.Empty())
    {
        return { FBOMResult::FBOM_ERR, "Loaded library contains no objects." };
    }

    if (library.object_data.Size() > 1)
    {
        HYP_LOG(Serialization, Warning, "Loaded libary contains more than one object when attempting to load a single object. The first object will be used.");
    }

    if (!library.TryGet(0, out))
    {
        return { FBOMResult::FBOM_ERR, "Invalid object in library at index 0" };
    }

    return { FBOMResult::FBOM_OK };
}

FBOMResult FBOMReader::Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out)
{
    const FBOMMarshalerBase* marshal = GetMarshalForType(in.m_object_type);

    if (!marshal)
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Marshal class not registered object type {}", in.m_object_type.name) };
    }

    if (FBOMResult err = marshal->Deserialize(context, in, out))
    {
        return err;
    }

    return { FBOMResult::FBOM_OK };
}

FBOMResult FBOMReader::Deserialize(FBOMLoadContext& context, BufferedReader& reader, HypData& out)
{
    FBOMObject obj;

    if (FBOMResult err = Deserialize(context, reader, obj))
    {
        return err;
    }

    return Deserialize(context, obj, out);
}

FBOMResult FBOMReader::LoadFromFile(FBOMLoadContext& context, const String& path, FBOMObjectLibrary& out)
{
    // Include our root dir as part of the path
    if (m_config.base_path.Empty())
    {
        m_config.base_path = FileSystem::RelativePath(StringUtil::BasePath(path.Data()), FileSystem::CurrentPath()).c_str();
    }

    FilePath read_path = FilePath(path);

    if (!read_path.Exists())
    {
        read_path = FilePath { FileSystem::Join(FileSystem::CurrentPath(), m_config.base_path.Data(), FilePath(path).Basename().Data()).c_str() };
    }

    if (!read_path.Exists())
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("File does not exist: {}", read_path) };
    }

    FileBufferedReaderSource source { read_path };
    BufferedReader reader { &source };

    return Deserialize(context, reader, out);
}

FBOMResult FBOMReader::LoadFromFile(const String& path, FBOMObject& out)
{
    // Include our root dir as part of the path
    if (m_config.base_path.Empty())
    {
        m_config.base_path = FileSystem::RelativePath(StringUtil::BasePath(path.Data()), FileSystem::CurrentPath()).c_str();
    }

    FilePath read_path = FilePath(path);

    if (!read_path.Exists())
    {
        read_path = FilePath { FileSystem::Join(FileSystem::CurrentPath(), m_config.base_path.Data(), FilePath(path).Basename().Data()).c_str() };
    }

    if (!read_path.Exists())
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("File does not exist: {}", read_path) };
    }

    FileBufferedReaderSource source { read_path };
    BufferedReader reader { &source };

    FBOMLoadContext context;

    return Deserialize(context, reader, out);
}

FBOMResult FBOMReader::LoadFromFile(const String& path, HypData& out)
{
    FBOMObject object;

    if (FBOMResult err = LoadFromFile(path, object))
    {
        return err;
    }

    AssertThrow(object.m_deserialized_object != nullptr);

    if (object.m_deserialized_object != nullptr)
    {
        out = std::move(*object.m_deserialized_object);
        object.m_deserialized_object.Reset();
    }

    return { FBOMResult::FBOM_OK };
}

FBOMCommand FBOMReader::NextCommand(BufferedReader* reader)
{
    AssertThrow(!reader->Eof());

    uint8 ins = -1;
    reader->Read(&ins);
    CheckEndianness(ins);

    return FBOMCommand(ins);
}

FBOMCommand FBOMReader::PeekCommand(BufferedReader* reader)
{
    AssertThrow(!reader->Eof());

    uint8 ins = -1;
    reader->Peek(&ins);
    CheckEndianness(ins);

    return FBOMCommand(ins);
}

FBOMResult FBOMReader::Eat(BufferedReader* reader, FBOMCommand command, bool read)
{
    FBOMCommand received;

    if (read)
    {
        received = NextCommand(reader);
    }
    else
    {
        received = PeekCommand(reader);
    }

    if (received != command)
    {
        return FBOMResult(FBOMResult::FBOM_ERR, "Unexpected command");
    }

    return FBOMResult::FBOM_OK;
}

FBOMMarshalerBase* FBOMReader::GetMarshalForType(const FBOMType& type) const
{
    if (type.HasNativeTypeID())
    {
        if (FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(type.GetNativeTypeID(), /* allow_fallback */ true))
        {
            return marshal;
        }
    }

    return FBOM::GetInstance().GetMarshal(type.name);
}

FBOMResult FBOMReader::RequestExternalObject(FBOMLoadContext& context, UUID library_id, uint32 index, FBOMObject& out_object)
{
    const auto it = context.object_libraries.Find(library_id);

    if (it != context.object_libraries.End())
    {
        if (!it->second.TryGet(index, out_object))
        {
            return { FBOMResult::FBOM_ERR, "Object not found in library" };
        }

        return { FBOMResult::FBOM_OK };
    }

    return { FBOMResult::FBOM_ERR, "Object library not found" };
}

FBOMResult FBOMReader::ReadDataAttributes(BufferedReader* reader, EnumFlags<FBOMDataAttributes>& out_attributes, FBOMDataLocation& out_location)
{
    constexpr uint8 loc_static = (1u << uint32(FBOMDataLocation::LOC_STATIC)) << 5;
    constexpr uint8 loc_inplace = (1u << uint32(FBOMDataLocation::LOC_INPLACE)) << 5;
    constexpr uint8 loc_ext_ref = (1u << uint32(FBOMDataLocation::LOC_EXT_REF)) << 5;

    uint8 attributes_value;
    reader->Read(&attributes_value);
    CheckEndianness(attributes_value);

    if (attributes_value & loc_static)
    {
        out_location = FBOMDataLocation::LOC_STATIC;
    }
    else if (attributes_value & loc_inplace)
    {
        out_location = FBOMDataLocation::LOC_INPLACE;
    }
    else if (attributes_value & loc_ext_ref)
    {
        out_location = FBOMDataLocation::LOC_EXT_REF;
    }
    else
    {
        return { FBOMResult::FBOM_ERR, "No data location on attributes" };
    }

    out_attributes = EnumFlags<FBOMDataAttributes>(attributes_value & ~uint8(FBOMDataAttributes::LOCATION_MASK));

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadObjectType(FBOMLoadContext& context, BufferedReader* reader, FBOMType& out_type)
{
    out_type = FBOMUnset();

    EnumFlags<FBOMDataAttributes> attributes;
    FBOMDataLocation location;

    if (FBOMResult err = ReadDataAttributes(reader, attributes, location))
    {
        return err;
    }

    switch (location)
    {
    case FBOMDataLocation::LOC_INPLACE:
    {
        FBOMType parent_type = FBOMUnset();

        uint8 has_parent;
        reader->Read(&has_parent);
        CheckEndianness(has_parent);

        if (has_parent)
        {
            if (FBOMResult err = ReadObjectType(context, reader, parent_type))
            {
                return err;
            }

            out_type = parent_type.Extend(out_type);
        }

        if (FBOMResult err = ReadString(reader, out_type.name))
        {
            return err;
        }

        // read type flags
        uint8 type_flags;
        reader->Read(&type_flags);
        CheckEndianness(type_flags);

        out_type.flags = EnumFlags<FBOMTypeFlags>(type_flags);

        // read size of object
        uint64 type_size;
        reader->Read(&type_size);
        CheckEndianness(type_size);

        out_type.size = type_size;

        // read native TypeID
        TypeID::ValueType type_id_value;
        reader->Read(&type_id_value);
        CheckEndianness(type_id_value);

        out_type.type_id = TypeID { type_id_value };

        break;
    }
    case FBOMDataLocation::LOC_STATIC:
    {
        // read offset as u32
        uint32 offset;
        reader->Read(&offset);
        CheckEndianness(offset);

        FBOMSerializableBase* element = m_static_data_index_map.GetOrInitializeElement(context, this, offset);

        if (FBOMType* as_type = dynamic_cast<FBOMType*>(element))
        {
            out_type = *as_type;
        }
        else
        {
            return { FBOMResult::FBOM_ERR, "Invalid type in static data pool" };
        }

        // AssertThrowMsg(offset < m_static_data_pool.Size(),
        //     "Offset out of bounds of static data pool: %u >= %u",
        //     offset,
        //     m_static_data_pool.Size());

        // // grab from static data pool
        // FBOMType *as_type = m_static_data_pool[offset].data.TryGetAsDynamic<FBOMType>();
        // AssertThrowMsg(as_type != nullptr, "Invalid value in static data pool at offset %u. Type: %u", offset, m_static_data_pool[offset].data.GetTypeID().Value());
        // out_type = *as_type;

        break;
    }
    default:
        AssertThrowMsg(false, "Invalid data location type");
        break;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadObjectLibrary(FBOMLoadContext& context, BufferedReader* reader, FBOMObjectLibrary& out_library)
{
    if (FBOMResult err = Eat(reader, FBOM_OBJECT_LIBRARY_START))
    {
        return err;
    }

    reader->Read(&out_library.uuid);
    CheckEndianness(out_library.uuid);

    uint8 flags = uint8(FBOMObjectLibraryFlags::NONE);
    reader->Read(&flags);
    CheckEndianness(flags);

    if (!(flags & uint8(FBOMObjectLibraryFlags::LOCATION_MASK)))
    {
        return { FBOMResult::FBOM_ERR, "No location flag set for object library" };
    }

    if (flags & uint8(FBOMObjectLibraryFlags::LOCATION_INLINE))
    {
        // read size of buffer
        uint64 buffer_size;
        reader->Read(&buffer_size);
        CheckEndianness(buffer_size);

        ByteBuffer buffer = reader->ReadBytes(buffer_size);

        if (buffer.Size() != buffer_size)
        {
            return { FBOMResult::FBOM_ERR, "Buffer size does not match expected size - file is likely corrupt" };
        }

        MemoryBufferedReaderSource source { buffer.ToByteView() };
        BufferedReader byte_reader { &source };

        FBOMReader deserializer(m_config);

        if (FBOMResult err = deserializer.Deserialize(context, byte_reader, out_library, /* read_header */ false))
        {
            return err;
        }
    }
    else if (flags & uint8(FBOMObjectLibraryFlags::LOCATION_EXTERNAL))
    {
        // read file with UUID as name

        // Read the relative path string
        String relative_path;
        if (FBOMResult err = ReadString(reader, relative_path))
        {
            return err;
        }

        FilePath base_path = FilePath::Current();

        if (m_config.base_path.Any())
        {
            base_path = m_config.base_path;
        }

        const FilePath combined_path = base_path / relative_path / (out_library.uuid.ToString() + ".hyp");

        if (FBOMResult err = FBOMReader(m_config).LoadFromFile(context, combined_path, out_library))
        {
            return err;
        }
    }

    if (FBOMResult err = Eat(reader, FBOM_OBJECT_LIBRARY_END))
    {
        return err;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadData(FBOMLoadContext& context, BufferedReader* reader, FBOMData& out_data)
{
    EnumFlags<FBOMDataAttributes> attributes;
    FBOMDataLocation location;

    if (FBOMResult err = ReadDataAttributes(reader, attributes, location))
    {
        return err;
    }

    if (location == FBOMDataLocation::LOC_INPLACE)
    {
        FBOMType object_type;

        if (FBOMResult err = ReadObjectType(context, reader, object_type))
        {
            return err;
        }

        BufferedReader compressed_data_reader;

        ByteBuffer byte_buffer;

        if (attributes & FBOMDataAttributes::COMPRESSED)
        {
            // Read archive
            Archive archive;

            if (FBOMResult err = ReadArchive(reader, archive))
            {
                return err;
            }

            if (!Archive::IsEnabled())
            {
                return { FBOMResult::FBOM_ERR, "Cannot decompress archive because the Archive feature is not enabled" };
            }

            if (Result result = archive.Decompress(byte_buffer); result.HasError())
            {
                return { FBOMResult::FBOM_ERR, result.GetError().GetMessage() };
            }
        }
        else
        {
            // Read bytebuffer of raw data
            uint32 sz;
            reader->Read(&sz);
            CheckEndianness(sz);

            byte_buffer = reader->ReadBytes(sz);

            if (byte_buffer.Size() != sz)
            {
                return { FBOMResult::FBOM_ERR, "Buffer is corrupted - size mismatch" };
            }
        }

        out_data = FBOMData(object_type, std::move(byte_buffer));
    }
    else if (location == FBOMDataLocation::LOC_STATIC)
    {
        // read offset as u32
        uint32 offset;
        reader->Read(&offset);
        CheckEndianness(offset);

        FBOMSerializableBase* element = m_static_data_index_map.GetOrInitializeElement(context, this, offset);

        if (FBOMData* as_data = dynamic_cast<FBOMData*>(element))
        {
            out_data = *as_data;
        }
        else
        {
            return { FBOMResult::FBOM_ERR, "Invalid data in static data pool" };
        }
    }
    else
    {
        return { FBOMResult::FBOM_ERR, "Unhandled data location" };
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadArray(FBOMLoadContext& context, BufferedReader* reader, FBOMArray& out_array)
{
    EnumFlags<FBOMDataAttributes> attributes;
    FBOMDataLocation location;

    if (FBOMResult err = ReadDataAttributes(reader, attributes, location))
    {
        return err;
    }

    if (location == FBOMDataLocation::LOC_INPLACE)
    {
        // Read array size
        uint32 sz;
        reader->Read(&sz);
        CheckEndianness(sz);

        // read array element type
        FBOMType element_type;

        if (FBOMResult err = ReadObjectType(context, reader, element_type))
        {
            return err;
        }

        ByteBuffer byte_buffer;

        BufferedReader compressed_data_reader;
        MemoryBufferedReaderSource compressed_data_source;

        BufferedReader* reader_ptr = reader;

        if (attributes & FBOMDataAttributes::COMPRESSED)
        {
            // Read archive
            Archive archive;

            if (FBOMResult err = ReadArchive(reader, archive))
            {
                return err;
            }

            if (!Archive::IsEnabled())
            {
                return { FBOMResult::FBOM_ERR, "Cannot decompress archive because the Archive feature is not enabled" };
            }

            if (Result result = archive.Decompress(byte_buffer); result.HasError())
            {
                return { FBOMResult::FBOM_ERR, result.GetError().GetMessage() };
            }

            compressed_data_source = MemoryBufferedReaderSource { byte_buffer.ToByteView() };
            compressed_data_reader = BufferedReader { &compressed_data_source };

            reader_ptr = &compressed_data_reader;
        }

        out_array = FBOMArray(element_type);

        if (element_type.IsPlaceholder() && sz > 0)
        {
            return { FBOMResult::FBOM_ERR, "Array element type is set to <placeholder>, however it has a non-zero number of elements, making it impossible to determine the actual element type to assign to the elements." };
        }

        for (uint32 index = 0; index < sz; index++)
        {
            uint32 data_size;
            reader_ptr->Read<uint32>(&data_size);
            CheckEndianness(data_size);

            ByteBuffer data_buffer = reader_ptr->ReadBytes(data_size);

            if (data_buffer.Size() < data_size)
            {
                return { FBOMResult::FBOM_ERR, HYP_FORMAT("Array element buffer is corrupt - expected size: {} bytes, but got {} bytes", data_size, data_buffer.Size()) };
            }

            FBOMData element_data { element_type, std::move(data_buffer) };
            out_array.AddElement(std::move(element_data));
        }
    }
    else if (location == FBOMDataLocation::LOC_STATIC)
    {
        // read offset as u32
        uint32 offset;
        reader->Read(&offset);
        CheckEndianness(offset);

        FBOMSerializableBase* element = m_static_data_index_map.GetOrInitializeElement(context, this, offset);

        if (FBOMArray* as_array = dynamic_cast<FBOMArray*>(element))
        {
            out_array = *as_array;
        }
        else
        {
            return { FBOMResult::FBOM_ERR, "Invalid array in static data pool" };
        }

        // AssertThrowMsg(offset < m_static_data_pool.Size(),
        //     "Offset out of bounds of static data pool: %u >= %u",
        //     offset,
        //     m_static_data_pool.Size());

        // // grab from static data pool
        // FBOMArray *as_array = m_static_data_pool[offset].data.TryGetAsDynamic<FBOMArray>();
        // AssertThrowMsg(as_array != nullptr, "Invalid value in static data pool at offset %u. Type: %u", offset, m_static_data_pool[offset].data.GetTypeID().Value());
        // out_array = *as_array;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadPropertyName(FBOMLoadContext& context, BufferedReader* reader, Name& out_property_name)
{
    FBOMData name_data;

    if (FBOMResult err = ReadData(context, reader, name_data))
    {
        return err;
    }

    AssertThrow(name_data.TotalSize() != 0);

    if (FBOMResult err = name_data.ReadName(&out_property_name))
    {
        const FBOMType* root_type = &name_data.GetType();

        while (root_type != nullptr)
        {
            DebugLog(LogType::Error, "root_type: %s\n", root_type->name.Data());

            root_type = root_type->extends;
        }

        HYP_BREAKPOINT;

        return FBOMResult { FBOMResult::FBOM_ERR, "Invalid property name: Expected data to be of type `Name`" };
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadObject(FBOMLoadContext& context, BufferedReader* reader, FBOMObject& out_object, FBOMObject* root)
{
    if (FBOMResult err = Eat(reader, FBOM_OBJECT_START))
    {
        return err;
    }

    FBOMResult result = FBOMResult::FBOM_OK;

    FBOMCommand command = FBOM_NONE;

    // read unique ID
    uint64 unique_id;
    reader->Read(&unique_id);
    CheckEndianness(unique_id);

    EnumFlags<FBOMDataAttributes> attributes;
    FBOMDataLocation location;

    if (FBOMResult err = ReadDataAttributes(reader, attributes, location))
    {
        return err;
    }

    switch (location)
    {
    case FBOMDataLocation::LOC_STATIC:
    {
        // read offset as u32
        uint32 offset;
        reader->Read(&offset);
        CheckEndianness(offset);

        FBOMSerializableBase* element = m_static_data_index_map.GetOrInitializeElement(context, this, offset);

        if (FBOMObject* as_object = dynamic_cast<FBOMObject*>(element))
        {
            out_object = *as_object;
        }
        else
        {
            return { FBOMResult::FBOM_ERR, "Invalid object in static data pool" };
        }

        return FBOMResult::FBOM_OK;
    }
    case FBOMDataLocation::LOC_INPLACE:
    {
        // read string of "type" - loader to use
        FBOMType object_type;

        if (FBOMResult err = ReadObjectType(context, reader, object_type))
        {
            return err;
        }

        out_object = FBOMObject(object_type);
        out_object.m_unique_id = unique_id;

        do
        {
            command = PeekCommand(reader);

            switch (command)
            {
            case FBOM_OBJECT_START:
            {
                FBOMObject subobject;

                if (FBOMResult err = ReadObject(context, reader, subobject, root))
                {
                    return err;
                }

                out_object.AddChild(std::move(subobject));

                break;
            }
            case FBOM_OBJECT_END:
            {
                if (object_type.UsesMarshal())
                {
                    if (GetMarshalForType(object_type) != nullptr)
                    {
                        // call deserializer function, writing into deserialized object
                        out_object.m_deserialized_object.Emplace();

                        if (FBOMResult err = Deserialize(context, out_object, *out_object.m_deserialized_object))
                        {
                            out_object.m_deserialized_object.Reset();

                            return err;
                        }
                    }
                    else
                    {
                        HYP_BREAKPOINT;
                        return { FBOMResult::FBOM_ERR, HYP_FORMAT("No marshal registered for type: {}", object_type.ToString(false)) };
                    }
                }

                break;
            }
            case FBOM_DEFINE_PROPERTY:
            {
                if (FBOMResult err = Eat(reader, FBOM_DEFINE_PROPERTY))
                {
                    return err;
                }

                ANSIString property_name;

                if (FBOMResult err = ReadString(reader, property_name))
                {
                    return err;
                }

                FBOMData data;

                if (FBOMResult err = ReadData(context, reader, data))
                {
                    return err;
                }

                out_object.SetProperty(property_name, std::move(data));

                break;
            }
            default:
                return FBOMResult(FBOMResult::FBOM_ERR, "Could not process command while reading object");
            }
        }
        while (command != FBOM_OBJECT_END && command != FBOM_NONE);

        // eat end
        if (FBOMResult err = Eat(reader, FBOM_OBJECT_END))
        {
            return err;
        }

        break;
    }
    case FBOMDataLocation::LOC_EXT_REF:
    {
        UUID library_id = UUID::Invalid();
        reader->Read(&library_id);
        CheckEndianness(library_id);

        // read object_index as u32
        uint32 object_index;
        reader->Read(&object_index);
        CheckEndianness(object_index);

        // read flags
        uint32 flags;
        reader->Read(&flags);
        CheckEndianness(flags);

        if (FBOMResult err = RequestExternalObject(context, library_id, object_index, out_object))
        {
            HYP_LOG(Serialization, Error, "Error requesting external object (library: {}, index: {}): {}",
                library_id.ToString(), object_index, err.message);

            return err;
        }

        break;
    }
    default:
        return FBOMResult(FBOMResult::FBOM_ERR, "Unknown object location type!");
    }

    if (attributes & FBOMDataAttributes::EXT_REF_PLACEHOLDER)
    {
        out_object.SetIsExternal(true);
    }

    // if (unique_id != uint64(object.m_unique_id)) {
    //     DebugLog(
    //         LogType::Warn,
    //         "unique id header for object does not match unique id stored in external object (%llu != %llu)\n",
    //         unique_id,
    //         uint64(object.m_unique_id)
    //     );
    // }

    return result;
}

FBOMResult FBOMReader::ReadArchive(BufferedReader* reader, Archive& out_archive)
{
    uint64 uncompressed_size = 0;

    if (reader->Read<uint64>(&uncompressed_size) != sizeof(uint64))
    {
        return { FBOMResult::FBOM_ERR, "Failed to read uncompressed size" };
    }

    CheckEndianness(uncompressed_size);

    uint64 compressed_size = 0;

    if (reader->Read<uint64>(&compressed_size) != sizeof(uint64))
    {
        return { FBOMResult::FBOM_ERR, "Failed to read compressed size" };
    }

    CheckEndianness(compressed_size);

    ByteBuffer compressed_buffer = reader->ReadBytes(compressed_size);

    if (compressed_buffer.Size() != compressed_size)
    {
        return { FBOMResult::FBOM_ERR, "Failed to read compressed buffer - buffer size mismatch" };
    }

    out_archive = Archive(std::move(compressed_buffer), uncompressed_size);

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadArchive(const ByteBuffer& in_buffer, ByteBuffer& out_buffer)
{
    // Read archive
    Archive archive;

    MemoryBufferedReaderSource source { in_buffer.ToByteView() };
    BufferedReader reader { &source };

    if (FBOMResult err = ReadArchive(&reader, archive))
    {
        return err;
    }

    if (!Archive::IsEnabled())
    {
        return { FBOMResult::FBOM_ERR, "Cannot decompress archive because the Archive feature is not enabled" };
    }

    if (Result result = archive.Decompress(out_buffer); result.HasError())
    {
        return { FBOMResult::FBOM_ERR, result.GetError().GetMessage() };
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadRawData(BufferedReader* reader, SizeType count, ByteBuffer& out_buffer)
{
    if (reader->Position() + count > reader->Max())
    {
        return { FBOMResult::FBOM_ERR, "File is corrupted: attempted to read past end" };
    }

    out_buffer = reader->ReadBytes(count);

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::Handle(FBOMLoadContext& context, BufferedReader* reader, FBOMCommand command, FBOMObject* root)
{
    AssertThrow(root != nullptr);

    switch (command)
    {
    case FBOM_OBJECT_START:
    {
        FBOMObject object;

        if (FBOMResult err = ReadObject(context, reader, object, root))
        {
            return err;
        }

        root->AddChild(std::move(object));

        break;
    }
    case FBOM_STATIC_DATA_START:
    {
        if (m_static_data_index_map.elements.Size() != 0)
        {
            return { FBOMResult::FBOM_ERR, "Static data pool already exists!" };
        }

        AssertThrow(!m_in_static_data);

        if (FBOMResult err = Eat(reader, FBOM_STATIC_DATA_START))
        {
            return err;
        }

        m_in_static_data = true;

        if (FBOMResult err = Eat(reader, FBOM_STATIC_DATA_HEADER_START))
        {
            return err;
        }

        // read u32 describing number of elements
        uint32 num_elements;
        reader->Read(&num_elements);
        CheckEndianness(num_elements);

        // read u64 describing static data buffer size
        uint64 static_data_buffer_size;
        reader->Read(&static_data_buffer_size);
        CheckEndianness(static_data_buffer_size);

        m_static_data_index_map.Initialize(num_elements);

        for (uint32 i = 0; i < num_elements; i++)
        {
            uint32 index;
            reader->Read(&index);
            CheckEndianness(index);

            if (index >= num_elements)
            {
                return FBOMResult(FBOMResult::FBOM_ERR, "Element index out of bounds of static data pool");
            }

            uint8 type;
            reader->Read(&type);
            CheckEndianness(type);

            uint64 offset;
            reader->Read(&offset);
            CheckEndianness(offset);

            uint64 size;
            reader->Read(&size);
            CheckEndianness(size);

            if (offset + size > static_data_buffer_size)
            {
                return FBOMResult(FBOMResult::FBOM_ERR, "Offset out of bounds of static data buffer");
            }

            m_static_data_index_map.SetElementDesc(index, FBOMStaticData::Type(type), offset, size);
        }

        if (FBOMResult err = Eat(reader, FBOM_STATIC_DATA_HEADER_END))
        {
            return err;
        }

        m_static_data_buffer = reader->ReadBytes(static_data_buffer_size);

        if (m_static_data_buffer.Size() != static_data_buffer_size)
        {
            return { FBOMResult::FBOM_ERR, "Static data buffer size mismatch - file corrupted?" };
        }

        if (FBOMResult err = Eat(reader, FBOM_STATIC_DATA_END))
        {
            return err;
        }

        m_in_static_data = false;

        break;
    }
    case FBOM_OBJECT_LIBRARY_START:
    {
        FBOMObjectLibrary library;

        if (FBOMResult err = ReadObjectLibrary(context, reader, library))
        {
            HYP_LOG(Serialization, Error, "Error reading object library: {}", err.message);

            return err;
        }

        context.object_libraries.Set(library.uuid, std::move(library));

        break;
    }
    default:
        AssertThrowMsg(false, "Cannot process command %d in top level at position: %u", int(command), reader->Position() - sizeof(FBOMCommand));

        break;
    }

    return FBOMResult::FBOM_OK;
}

#pragma endregion FBOMReader

} // namespace serialization
} // namespace hyperion