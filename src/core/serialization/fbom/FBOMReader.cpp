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
    HYP_CORE_ASSERT(reader != nullptr);
    HYP_CORE_ASSERT(IsValid());

    if (IsInitialized())
    {
        return { FBOMResult::FBOM_OK };
    }

    ConstByteView view = reader->m_staticDataBuffer.ToByteView()
                             .Slice(offset, size);

    MemoryBufferedReaderSource source { view };
    BufferedReader byteReader { &source };

    switch (type)
    {
    case FBOMStaticData::FBOM_STATIC_DATA_NONE:
        return { FBOMResult::FBOM_ERR, "Cannot process static data type, unknown type" };
    case FBOMStaticData::FBOM_STATIC_DATA_OBJECT:
    {
        ptr = MakeUnique<FBOMObject>();

        if (FBOMResult err = reader->ReadObject(context, &byteReader, *static_cast<FBOMObject*>(ptr.Get()), nullptr))
        {
            ptr.Reset();

            return err;
        }

        break;
    }
    case FBOMStaticData::FBOM_STATIC_DATA_TYPE:
    {
        ptr = MakeUnique<FBOMType>();

        if (FBOMResult err = reader->ReadObjectType(context, &byteReader, *static_cast<FBOMType*>(ptr.Get())))
        {
            ptr.Reset();

            return err;
        }

        break;
    }
    case FBOMStaticData::FBOM_STATIC_DATA_DATA:
    {
        ptr = MakeUnique<FBOMData>();

        if (FBOMResult err = reader->ReadData(context, &byteReader, *static_cast<FBOMData*>(ptr.Get())))
        {
            ptr.Reset();

            return err;
        }

        HYP_CORE_ASSERT(static_cast<FBOMData*>(ptr.Get())->TotalSize() != 0);

        break;
    }
    case FBOMStaticData::FBOM_STATIC_DATA_ARRAY:
    {
        ptr = MakeUnique<FBOMArray>();

        if (FBOMResult err = reader->ReadArray(context, &byteReader, *static_cast<FBOMArray*>(ptr.Get())))
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
            HYP_LOG(Serialization, Error, "Deserialization error: {}", err.message);

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
FBOMResult FBOMReader::ReadString(BufferedReader* reader, TString& outString)
{
    // read 4 bytes of string header
    uint32 stringHeader;
    reader->Read(&stringHeader);
    CheckEndianness(stringHeader);

    const uint32 stringLength = (stringHeader & ByteWriter::stringLengthMask) >> 8;
    const uint32 stringType = (stringHeader & ByteWriter::stringTypeMask);

    if (stringType != 0 && stringType != TString::stringType)
    {
        return FBOMResult(FBOMResult::FBOM_ERR, "Error reading string: string type mismatch");
    }

    ByteBuffer stringBuffer(stringLength + 1);

    uint32 readLength;

    if ((readLength = reader->Read(stringBuffer.Data(), stringLength)) != stringLength)
    {
        return FBOMResult(FBOMResult::FBOM_ERR, "Error reading string: string length mismatch");
    }

    outString = TString(stringBuffer.ToByteView());

    return FBOMResult::FBOM_OK;
}

FBOMReader::FBOMReader(const FBOMReaderConfig& config)
    : m_config(config),
      m_inStaticData(false),
      m_swapEndianness(false)
{
}

FBOMReader::~FBOMReader() = default;

FBOMResult FBOMReader::Deserialize(FBOMLoadContext& context, BufferedReader& reader, FBOMObjectLibrary& out, bool readHeader)
{
    if (reader.Eof())
    {
        return { FBOMResult::FBOM_ERR, "Stream not open" };
    }

    FBOMObject root(FBOMObjectType("ROOT"));

    if (readHeader)
    {
        ubyte headerBytes[FBOM::headerSize];

        if (reader.Read(headerBytes, FBOM::headerSize) != FBOM::headerSize
            || Memory::StrCmp(reinterpret_cast<const char*>(headerBytes), FBOM::headerIdentifier, sizeof(FBOM::headerIdentifier) - 1) != 0)
        {
            return { FBOMResult::FBOM_ERR, "Invalid header identifier" };
        }

        // read endianness
        const ubyte endianness = headerBytes[sizeof(FBOM::headerIdentifier)];

        // set if it needs to swap endianness.
        m_swapEndianness = bool(endianness) != ByteUtil::IsBigEndian();

        // get version info
        FBOMVersion binaryVersion;
        Memory::MemCpy(&binaryVersion.value, headerBytes + sizeof(FBOM::headerIdentifier) + sizeof(uint8), sizeof(uint32));

        const int compatibilityTestResult = FBOMVersion::TestCompatibility(binaryVersion, FBOM::version);

        if (compatibilityTestResult != 0)
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Unsupported binary version! Got {} but current is {}. Result: {}", binaryVersion, FBOM::version, compatibilityTestResult) };
        }
    }

    m_staticDataIndexMap = FBOMStaticDataIndexMap();
    m_staticDataBuffer = ByteBuffer();
    m_inStaticData = false;

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

    out.objectData = std::move(root.m_children);

    return { FBOMResult::FBOM_OK };
}

FBOMResult FBOMReader::Deserialize(FBOMLoadContext& context, BufferedReader& reader, FBOMObject& out)
{
    FBOMObjectLibrary library;

    if (FBOMResult err = Deserialize(context, reader, library))
    {
        return err;
    }

    if (library.objectData.Empty())
    {
        return { FBOMResult::FBOM_ERR, "Loaded library contains no objects." };
    }

    if (library.objectData.Size() > 1)
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
    const FBOMMarshalerBase* marshal = GetMarshalForType(in.m_objectType);

    if (!marshal)
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Marshal class not registered object type {}", in.m_objectType.name) };
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
    if (m_config.basePath.Empty())
    {
        m_config.basePath = FilePath::Relative(FilePath(path).BasePath(), FilePath::Current());
    }

    FilePath readPath = FilePath(path);

    if (!readPath.Exists())
    {
        readPath = FilePath::Join(FilePath::Current(), m_config.basePath, FilePath(path).Basename());

        if (!readPath.Exists())
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("File does not exist: {}", readPath) };
        }
    }

    if (readPath.FileSize() == 0)
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("File is empty: {}", readPath) };
    }

    FileBufferedReaderSource source { readPath };

    if (!source.IsOK())
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Failed to open file: {}", readPath) };
    }

    BufferedReader reader { &source };

    HYP_CORE_ASSERT(!reader.Eof());

    return Deserialize(context, reader, out);
}

FBOMResult FBOMReader::LoadFromFile(const String& path, FBOMObject& out)
{
    // Include our root dir as part of the path
    if (m_config.basePath.Empty())
    {
        m_config.basePath = FileSystem::RelativePath(StringUtil::BasePath(path.Data()), FileSystem::CurrentPath()).c_str();
    }

    FilePath readPath = FilePath(path);

    if (!readPath.Exists())
    {
        readPath = FilePath::Join(FilePath::Current(), m_config.basePath, FilePath(path).Basename());

        if (!readPath.Exists())
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("File does not exist: {}", readPath) };
        }
    }

    if (readPath.FileSize() == 0)
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("File is empty: {}", readPath) };
    }

    FileBufferedReaderSource source { readPath };

    if (!source.IsOK())
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Failed to open file: {}", readPath) };
    }

    BufferedReader reader { &source };
    HYP_CORE_ASSERT(!reader.Eof());

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

    HYP_CORE_ASSERT(object.m_deserializedObject != nullptr);

    if (object.m_deserializedObject != nullptr)
    {
        out = std::move(*object.m_deserializedObject);
        object.m_deserializedObject.Reset();
    }

    return { FBOMResult::FBOM_OK };
}

FBOMCommand FBOMReader::NextCommand(BufferedReader* reader)
{
    HYP_CORE_ASSERT(!reader->Eof());

    uint8 ins = -1;
    reader->Read(&ins);
    CheckEndianness(ins);

    return FBOMCommand(ins);
}

FBOMCommand FBOMReader::PeekCommand(BufferedReader* reader)
{
    HYP_CORE_ASSERT(!reader->Eof());

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
    if (type.HasNativeTypeId())
    {
        if (FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(type.GetNativeTypeId(), /* allowFallback */ true))
        {
            return marshal;
        }
    }

    return FBOM::GetInstance().GetMarshal(type.name);
}

FBOMResult FBOMReader::RequestExternalObject(FBOMLoadContext& context, Uuid libraryId, uint32 index, FBOMObject& outObject)
{
    const auto it = context.objectLibraries.Find(libraryId);

    if (it != context.objectLibraries.End())
    {
        if (!it->second.TryGet(index, outObject))
        {
            return { FBOMResult::FBOM_ERR, "Object not found in library" };
        }

        return { FBOMResult::FBOM_OK };
    }

    return { FBOMResult::FBOM_ERR, "Object library not found" };
}

FBOMResult FBOMReader::ReadDataAttributes(BufferedReader* reader, EnumFlags<FBOMDataAttributes>& outAttributes, FBOMDataLocation& outLocation)
{
    constexpr uint8 locStatic = (1u << uint32(FBOMDataLocation::LOC_STATIC)) << 5;
    constexpr uint8 locInplace = (1u << uint32(FBOMDataLocation::LOC_INPLACE)) << 5;
    constexpr uint8 locExtRef = (1u << uint32(FBOMDataLocation::LOC_EXT_REF)) << 5;

    uint8 attributesValue;
    reader->Read(&attributesValue);
    CheckEndianness(attributesValue);

    if (attributesValue & locStatic)
    {
        outLocation = FBOMDataLocation::LOC_STATIC;
    }
    else if (attributesValue & locInplace)
    {
        outLocation = FBOMDataLocation::LOC_INPLACE;
    }
    else if (attributesValue & locExtRef)
    {
        outLocation = FBOMDataLocation::LOC_EXT_REF;
    }
    else
    {
        return { FBOMResult::FBOM_ERR, "No data location on attributes" };
    }

    outAttributes = EnumFlags<FBOMDataAttributes>(attributesValue & ~uint8(FBOMDataAttributes::LOCATION_MASK));

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadObjectType(FBOMLoadContext& context, BufferedReader* reader, FBOMType& outType)
{
    outType = FBOMUnset();

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
        FBOMType parentType = FBOMUnset();

        uint8 hasParent;
        reader->Read(&hasParent);
        CheckEndianness(hasParent);

        if (hasParent)
        {
            if (FBOMResult err = ReadObjectType(context, reader, parentType))
            {
                return err;
            }

            outType = parentType.Extend(outType);
        }

        if (FBOMResult err = ReadString(reader, outType.name))
        {
            return err;
        }

        // read type flags
        uint8 typeFlags;
        reader->Read(&typeFlags);
        CheckEndianness(typeFlags);

        outType.flags = EnumFlags<FBOMTypeFlags>(typeFlags);

        // read size of object
        uint64 typeSize;
        reader->Read(&typeSize);
        CheckEndianness(typeSize);

        outType.size = typeSize;

        // read native TypeId
        TypeId::ValueType typeIdValue;
        reader->Read(&typeIdValue);
        CheckEndianness(typeIdValue);

        outType.typeId = TypeId { typeIdValue };

        break;
    }
    case FBOMDataLocation::LOC_STATIC:
    {
        // read offset as u32
        uint32 offset;
        reader->Read(&offset);
        CheckEndianness(offset);

        FBOMSerializableBase* element = m_staticDataIndexMap.GetOrInitializeElement(context, this, offset);

        if (FBOMType* asType = dynamic_cast<FBOMType*>(element))
        {
            outType = *asType;
        }
        else
        {
            return { FBOMResult::FBOM_ERR, "Invalid type in static data pool" };
        }

        break;
    }
    default:
        HYP_FAIL("Invalid data location type");
        break;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadObjectLibrary(FBOMLoadContext& context, BufferedReader* reader, FBOMObjectLibrary& outLibrary)
{
    if (FBOMResult err = Eat(reader, FBOM_OBJECT_LIBRARY_START))
    {
        return err;
    }

    reader->Read(&outLibrary.uuid);
    CheckEndianness(outLibrary.uuid);

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
        uint64 bufferSize;
        reader->Read(&bufferSize);
        CheckEndianness(bufferSize);

        ByteBuffer buffer = reader->ReadBytes(bufferSize);

        if (buffer.Size() != bufferSize)
        {
            return { FBOMResult::FBOM_ERR, "Buffer size does not match expected size - file is likely corrupt" };
        }

        MemoryBufferedReaderSource source { buffer.ToByteView() };
        BufferedReader byteReader { &source };

        HYP_CORE_ASSERT(!byteReader.Eof());

        FBOMReader deserializer(m_config);

        if (FBOMResult err = deserializer.Deserialize(context, byteReader, outLibrary, /* readHeader */ false))
        {
            return err;
        }
    }
    else if (flags & uint8(FBOMObjectLibraryFlags::LOCATION_EXTERNAL))
    {
        // read file with Uuid as name

        // Read the relative path string
        String relativePath;
        if (FBOMResult err = ReadString(reader, relativePath))
        {
            return err;
        }

        FilePath basePath = FilePath::Current();

        if (m_config.basePath.Any())
        {
            basePath = m_config.basePath;
        }

        const FilePath combinedPath = basePath / relativePath / (outLibrary.uuid.ToString() + ".hyp");

        if (FBOMResult err = FBOMReader(m_config).LoadFromFile(context, combinedPath, outLibrary))
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

FBOMResult FBOMReader::ReadData(FBOMLoadContext& context, BufferedReader* reader, FBOMData& outData)
{
    EnumFlags<FBOMDataAttributes> attributes;
    FBOMDataLocation location;

    if (FBOMResult err = ReadDataAttributes(reader, attributes, location))
    {
        return err;
    }

    if (location == FBOMDataLocation::LOC_INPLACE)
    {
        FBOMType objectType;

        if (FBOMResult err = ReadObjectType(context, reader, objectType))
        {
            return err;
        }

        ByteBuffer byteBuffer;

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

            if (Result result = archive.Decompress(byteBuffer); result.HasError())
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

            byteBuffer = reader->ReadBytes(sz);

            if (byteBuffer.Size() != sz)
            {
                return { FBOMResult::FBOM_ERR, "Buffer is corrupted - size mismatch" };
            }
        }

        outData = FBOMData(objectType, std::move(byteBuffer));
    }
    else if (location == FBOMDataLocation::LOC_STATIC)
    {
        // read offset as u32
        uint32 offset;
        reader->Read(&offset);
        CheckEndianness(offset);

        FBOMSerializableBase* element = m_staticDataIndexMap.GetOrInitializeElement(context, this, offset);

        if (FBOMData* asData = dynamic_cast<FBOMData*>(element))
        {
            outData = *asData;
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

FBOMResult FBOMReader::ReadArray(FBOMLoadContext& context, BufferedReader* reader, FBOMArray& outArray)
{
    EnumFlags<FBOMDataAttributes> attributes;
    FBOMDataLocation location;

    if (FBOMResult err = ReadDataAttributes(reader, attributes, location))
    {
        return err;
    }

    if (location == FBOMDataLocation::LOC_INPLACE)
    {
        /// We write out type and number of elements in groups of the same type.
        // if there are derived types, then we'll write the type and number of consecutive elements of that type.
        // for ex: [TypeA, 3][TypeB, 1][TypeA, 2] means 3 elements of TypeA, then 1 of TypeB, then 2 of TypeA.
        // if all elements are the same type, then we just write [TypeA, 6] for 6 elements of TypeA.
        // this'll allow us to mix and match types in arrays while still being efficient with storage.

        // Read total array size
        uint32 totalArraySize;
        reader->Read(&totalArraySize);
        CheckEndianness(totalArraySize);

        ByteBuffer byteBuffer;

        BufferedReader compressedDataReader;
        MemoryBufferedReaderSource compressedDataSource;

        BufferedReader* readerPtr = reader;

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

            if (Result result = archive.Decompress(byteBuffer); result.HasError())
            {
                return { FBOMResult::FBOM_ERR, result.GetError().GetMessage() };
            }

            compressedDataSource = MemoryBufferedReaderSource { byteBuffer.ToByteView() };
            compressedDataReader = BufferedReader { &compressedDataSource };

            readerPtr = &compressedDataReader;
        }

        outArray = FBOMArray();

        // Read until we've read totalArraySize elements
        SizeType elementsRead = 0;

        while (elementsRead < totalArraySize)
        {
            // Read the type for this group of elements
            FBOMType elementType;

            if (FBOMResult err = ReadObjectType(context, readerPtr, elementType))
            {
                return err;
            }

            HYP_LOG_TEMP("Read element type: {}", elementType.ToString());

            // Read the number of consecutive elements of this type
            uint32 numElementsOfType;
            readerPtr->Read(&numElementsOfType);
            CheckEndianness(numElementsOfType);

            if (elementsRead + numElementsOfType > totalArraySize)
            {
                return { FBOMResult::FBOM_ERR, "Array element count mismatch - would exceed total array size" };
            }

            // Read each element of this type
            for (uint32 i = 0; i < numElementsOfType; i++)
            {
                // Read bytebuffer of raw data
                uint32 sz;
                reader->Read(&sz);
                CheckEndianness(sz);

                byteBuffer = reader->ReadBytes(sz);

                if (byteBuffer.Size() != sz)
                {
                    return { FBOMResult::FBOM_ERR, "Buffer is corrupted - size mismatch" };
                }

                outArray.AddElement(FBOMData(elementType, std::move(byteBuffer)));
            }

            elementsRead += numElementsOfType;
        }

        if (elementsRead != totalArraySize)
        {
            return { FBOMResult::FBOM_ERR, "Array element count mismatch - did not read expected number of elements" };
        }
    }
    else if (location == FBOMDataLocation::LOC_STATIC)
    {
        // read offset as u32
        uint32 offset;
        reader->Read(&offset);
        CheckEndianness(offset);

        FBOMSerializableBase* element = m_staticDataIndexMap.GetOrInitializeElement(context, this, offset);

        if (FBOMArray* asArray = dynamic_cast<FBOMArray*>(element))
        {
            outArray = *asArray;
        }
        else
        {
            return { FBOMResult::FBOM_ERR, "Invalid array in static data pool" };
        }

        // HYP_CORE_ASSERT(offset < m_staticDataPool.Size(),
        //     "Offset out of bounds of static data pool: %u >= %u",
        //     offset,
        //     m_staticDataPool.Size());

        // // grab from static data pool
        // FBOMArray *asArray = m_staticDataPool[offset].data.TryGetAsDynamic<FBOMArray>();
        // HYP_CORE_ASSERT(asArray != nullptr, "Invalid value in static data pool at offset %u. Type: %u", offset, m_staticDataPool[offset].data.GetTypeId().Value());
        // outArray = *asArray;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadPropertyName(FBOMLoadContext& context, BufferedReader* reader, Name& outPropertyName)
{
    FBOMData nameData;

    if (FBOMResult err = ReadData(context, reader, nameData))
    {
        return err;
    }

    HYP_CORE_ASSERT(nameData.TotalSize() != 0);

    if (FBOMResult err = nameData.ReadName(&outPropertyName))
    {
        const FBOMType* rootType = &nameData.GetType();

        while (rootType != nullptr)
        {
            rootType = rootType->extends;
        }

        return FBOMResult { FBOMResult::FBOM_ERR, "Invalid property name: Expected data to be of type `Name`" };
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadObject(
    FBOMLoadContext& context,
    BufferedReader* reader,
    FBOMObject& outObject,
    FBOMObject* root,
    bool deserializeObject)
{
    if (FBOMResult err = Eat(reader, FBOM_OBJECT_START))
    {
        return err;
    }

    FBOMResult result = FBOMResult::FBOM_OK;

    FBOMCommand command = FBOM_NONE;

    uint64 id;
    reader->Read(&id);
    CheckEndianness(id);

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

        FBOMSerializableBase* element = m_staticDataIndexMap.GetOrInitializeElement(context, this, offset);

        if (FBOMObject* asObject = dynamic_cast<FBOMObject*>(element))
        {
            outObject = *asObject;
        }
        else
        {
            return { FBOMResult::FBOM_ERR, "Invalid object in static data pool" };
        }

        return FBOMResult::FBOM_OK;
    }
    case FBOMDataLocation::LOC_INPLACE:
    {
        FBOMType objectType;

        if (FBOMResult err = ReadObjectType(context, reader, objectType))
        {
            return err;
        }

        outObject = FBOMObject(objectType);
        outObject.m_uniqueId = UniqueId { id };

        do
        {
            command = PeekCommand(reader);

            switch (command)
            {
            case FBOM_OBJECT_START:
            {
                FBOMObject subobject;

                if (FBOMResult err = ReadObject(context, reader, subobject, root, /* deserializeObject */ true))
                {
                    return err;
                }

                outObject.AddChild(std::move(subobject));

                break;
            }
            case FBOM_OBJECT_END:
            {
                if (deserializeObject && objectType.UsesMarshal())
                {
                    if (GetMarshalForType(objectType) != nullptr)
                    {
                        // call deserializer function, writing into deserialized object
                        outObject.m_deserializedObject.Emplace();

                        if (FBOMResult err = Deserialize(context, outObject, *outObject.m_deserializedObject); !err.IsOK())
                        {
                            HYP_LOG_TEMP("Error deserializing object of type {}: {}", objectType.ToString(false), err.message);
                            HYP_BREAKPOINT;
                            outObject.m_deserializedObject.Reset();

                            return err;
                        }
                    }
                    else
                    {
                        return { FBOMResult::FBOM_ERR, HYP_FORMAT("No marshal registered for type: {}", objectType.ToString(false)) };
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

                ANSIString propertyName;

                if (FBOMResult err = ReadString(reader, propertyName))
                {
                    return err;
                }

                FBOMData data;

                if (FBOMResult err = ReadData(context, reader, data))
                {
                    return err;
                }

                outObject.SetProperty(propertyName, std::move(data));

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
        Uuid libraryId = Uuid::Invalid();
        reader->Read(&libraryId);
        CheckEndianness(libraryId);

        // read objectIndex as u32
        uint32 objectIndex;
        reader->Read(&objectIndex);
        CheckEndianness(objectIndex);

        // read flags
        uint32 flags;
        reader->Read(&flags);
        CheckEndianness(flags);

        if (FBOMResult err = RequestExternalObject(context, libraryId, objectIndex, outObject))
        {
            HYP_LOG(Serialization, Error, "Error requesting external object (library: {}, index: {}): {}",
                libraryId.ToString(), objectIndex, err.message);

            return err;
        }

        break;
    }
    default:
        return FBOMResult(FBOMResult::FBOM_ERR, "Unknown object location type!");
    }

    if (attributes & FBOMDataAttributes::EXT_REF_PLACEHOLDER)
    {
        outObject.SetIsExternal(true);
    }

    // if (uniqueId != uint64(object.m_uniqueId)) {
    //     DebugLog(
    //         LogType::Warn,
    //         "unique id header for object does not match unique id stored in external object (%llu != %llu)\n",
    //         uniqueId,
    //         uint64(object.m_uniqueId)
    //     );
    // }

    return result;
}

FBOMResult FBOMReader::ReadArchive(BufferedReader* reader, Archive& outArchive)
{
    uint64 uncompressedSize = 0;

    if (reader->Read<uint64>(&uncompressedSize) != sizeof(uint64))
    {
        return { FBOMResult::FBOM_ERR, "Failed to read uncompressed size" };
    }

    CheckEndianness(uncompressedSize);

    uint64 compressedSize = 0;

    if (reader->Read<uint64>(&compressedSize) != sizeof(uint64))
    {
        return { FBOMResult::FBOM_ERR, "Failed to read compressed size" };
    }

    CheckEndianness(compressedSize);

    ByteBuffer compressedBuffer = reader->ReadBytes(compressedSize);

    if (compressedBuffer.Size() != compressedSize)
    {
        return { FBOMResult::FBOM_ERR, "Failed to read compressed buffer - buffer size mismatch" };
    }

    outArchive = Archive(std::move(compressedBuffer), uncompressedSize);

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadArchive(const ByteBuffer& inBuffer, ByteBuffer& outBuffer)
{
    // Read archive
    Archive archive;

    MemoryBufferedReaderSource source { inBuffer.ToByteView() };
    BufferedReader reader { &source };

    if (FBOMResult err = ReadArchive(&reader, archive))
    {
        return err;
    }

    if (!Archive::IsEnabled())
    {
        return { FBOMResult::FBOM_ERR, "Cannot decompress archive because the Archive feature is not enabled" };
    }

    if (Result result = archive.Decompress(outBuffer); result.HasError())
    {
        return { FBOMResult::FBOM_ERR, result.GetError().GetMessage() };
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::ReadRawData(BufferedReader* reader, SizeType count, ByteBuffer& outBuffer)
{
    if (reader->Position() + count > reader->Max())
    {
        return { FBOMResult::FBOM_ERR, "File is corrupted: attempted to read past end" };
    }

    outBuffer = reader->ReadBytes(count);

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMReader::Handle(FBOMLoadContext& context, BufferedReader* reader, FBOMCommand command, FBOMObject* root)
{
    HYP_CORE_ASSERT(root != nullptr);

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
        if (m_staticDataIndexMap.elements.Size() != 0)
        {
            return { FBOMResult::FBOM_ERR, "Static data pool already exists!" };
        }

        HYP_CORE_ASSERT(!m_inStaticData);

        if (FBOMResult err = Eat(reader, FBOM_STATIC_DATA_START))
        {
            return err;
        }

        m_inStaticData = true;

        if (FBOMResult err = Eat(reader, FBOM_STATIC_DATA_HEADER_START))
        {
            return err;
        }

        // read u32 describing number of elements
        uint32 numElements;
        reader->Read(&numElements);
        CheckEndianness(numElements);

        // read u64 describing static data buffer size
        uint64 staticDataBufferSize;
        reader->Read(&staticDataBufferSize);
        CheckEndianness(staticDataBufferSize);

        m_staticDataIndexMap.Initialize(numElements);

        for (uint32 i = 0; i < numElements; i++)
        {
            uint32 index;
            reader->Read(&index);
            CheckEndianness(index);

            if (index >= numElements)
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

            if (offset + size > staticDataBufferSize)
            {
                return FBOMResult(FBOMResult::FBOM_ERR, "Offset out of bounds of static data buffer");
            }

            m_staticDataIndexMap.SetElementDesc(index, FBOMStaticData::Type(type), offset, size);
        }

        if (FBOMResult err = Eat(reader, FBOM_STATIC_DATA_HEADER_END))
        {
            return err;
        }

        m_staticDataBuffer = reader->ReadBytes(staticDataBufferSize);

        if (m_staticDataBuffer.Size() != staticDataBufferSize)
        {
            return { FBOMResult::FBOM_ERR, "Static data buffer size mismatch - file corrupted?" };
        }

        if (FBOMResult err = Eat(reader, FBOM_STATIC_DATA_END))
        {
            return err;
        }

        m_inStaticData = false;

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

        context.objectLibraries.Set(library.uuid, std::move(library));

        break;
    }
    default:
        HYP_FAIL("Cannot process command %d in top level at position: %u", int(command), reader->Position() - sizeof(FBOMCommand));

        break;
    }

    return FBOMResult::FBOM_OK;
}

#pragma endregion FBOMReader

} // namespace serialization
} // namespace hyperion
