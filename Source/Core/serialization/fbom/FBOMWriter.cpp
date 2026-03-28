/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/serialization/fbom/FBOMWriter.hpp>
#include <Core/serialization/fbom/FBOMReader.hpp>
#include <Core/serialization/fbom/FBOMArray.hpp>
#include <Core/serialization/fbom/FBOMLoadContext.hpp>
#include <Core/serialization/fbom/FBOM.hpp>

#include <Core/io/ByteWriter.hpp>

#include <Core/containers/HashSet.hpp>

#include <Core/compression/Archive.hpp>

#include <Core/threading/TaskSystem.hpp>

#include <Core/logging/LogChannels.hpp>
#include <Core/logging/Logger.hpp>

#include <Core/Constants.hpp>

#include <algorithm>

namespace Hyperion::serialization {

#pragma region FBOMWriteStream

FBOMWriteStream::FBOMWriteStream() = default;

FBOMDataLocation FBOMWriteStream::GetDataLocation(const UniqueId& uniqueId, const FBOMStaticData** outStaticData, const FBOMExternalObjectInfo** outExternalObjectInfo) const
{
    *outStaticData = nullptr;
    *outExternalObjectInfo = nullptr;

    { // check static data
        auto it = m_staticData.Find(uniqueId);

        if (it != m_staticData.End())
        {
            if (IsWritingStaticData() || it->second.IsWritten())
            {
                *outStaticData = &it->second;

                if (it->second.IsWritten())
                {
                    return FBOMDataLocation::LOC_STATIC;
                }
            }
        }
    }

    // check external objects
    for (const FBOMObjectLibrary& objectLibrary : m_objectLibraries)
    {
        const auto objectsIt = objectLibrary.objectData.FindIf([&uniqueId](const FBOMObject& item)
            {
                return item.GetUniqueID() == uniqueId;
            });

        if (objectsIt == objectLibrary.objectData.End())
        {
            continue;
        }

        if (!objectsIt->IsExternal())
        {
            break;
        }

        *outExternalObjectInfo = objectsIt->GetExternalObjectInfo();

        return FBOMDataLocation::LOC_EXT_REF;
    }

    return FBOMDataLocation::LOC_INPLACE;
}

void FBOMWriteStream::AddToObjectLibrary(FBOMObject& object)
{
    // static constexpr size_t desiredMaxSize = 1024 * 1024 * 1024 * 32; // 32 MiB
    static constexpr size_t desiredMaxSize = 10;

    FBOMExternalObjectInfo* externalObjectInfo = object.GetExternalObjectInfo();
    HYP_CORE_ASSERT(externalObjectInfo != nullptr);
    HYP_CORE_ASSERT(!externalObjectInfo->IsLinked());

    // FBOMData objectData = FBOMData::FromObject(object);
    // const size_t objectDataSize = objectData.TotalSize();

    FBOMObjectLibrary* libraryPtr = nullptr;

    for (auto it = m_objectLibraries.Begin(); it != m_objectLibraries.End(); ++it)
    {
        const size_t librarySize = it->CalculateTotalSize();

        if (librarySize + 1 <= desiredMaxSize)
        {
            libraryPtr = it;

            break;
        }
    }

    if (!libraryPtr)
    {
        libraryPtr = &m_objectLibraries.EmplaceBack();
    }

    libraryPtr->Put(object);

    // sanity check
    HYP_CORE_ASSERT(externalObjectInfo->IsLinked());
}

#pragma endregion FBOMWriteStream

#pragma region FBOMWriter

FBOMWriter::FBOMWriter(const FBOMWriterConfig& config)
    : FBOMWriter(config, MakeRefCountedPtr<FBOMWriteStream>())
{
}

FBOMWriter::FBOMWriter(const FBOMWriterConfig& config, const RC<FBOMWriteStream>& writeStream)
    : m_config(config),
      m_writeStream(writeStream)
{
}

FBOMWriter::FBOMWriter(FBOMWriter&& other) noexcept
    : m_config(std::move(other.m_config)),
      m_writeStream(std::move(other.m_writeStream))
{
}

FBOMWriter& FBOMWriter::operator=(FBOMWriter&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    m_config = std::move(other.m_config);
    m_writeStream = std::move(other.m_writeStream);

    return *this;
}

FBOMWriter::~FBOMWriter()
{
}

FBOMResult FBOMWriter::Append(const FBOMObject& object)
{
    AddObjectData(object, object.GetUniqueID());

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::Append(FBOMObject&& object)
{
    const UniqueId id = object.GetUniqueID();

    AddObjectData(std::move(object), id);

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::Emit(ByteWriter* out, bool writeHeader)
{
    // Choose a base path to write external objects to
    FilePath externalPath = FilePath::Current() / "external";
    FilePath basePath = FilePath::Current();

    if (FileByteWriter* fileByteWriter = dynamic_cast<FileByteWriter*>(out))
    {
        basePath = fileByteWriter->GetFilePath().BasePath();
        /// \todo Use a config property instead of `fileByteWriter->GetFilePath()` - so resaving an object doesn't change the path
        externalPath = FilePath(fileByteWriter->GetFilePath().StripExtension() + "_external");
    }

    if (FBOMResult err = m_writeStream->m_lastResult)
    {
        return err;
    }

    FBOMLoadContext context;

    if (FBOMResult err = BuildStaticData(context))
    {
        return err;
    }

    if (writeHeader)
    {
        if (FBOMResult err = WriteHeader(out))
        {
            return err;
        }
    }

    if (FBOMResult err = WriteExternalObjects(out, basePath, externalPath))
    {
        return err;
    }

    if (FBOMResult err = WriteStaticData(out))
    {
        return err;
    }

    for (const FBOMObject& object : m_writeStream->m_objectData)
    {
        if (FBOMResult err = object.Visit(this, out))
        {
            return err;
        }
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteExternalObjects(ByteWriter* out, const FilePath& basePath, const FilePath& externalPath)
{
    if (m_writeStream->m_objectLibraries.Empty())
    {
        // No external objects to write
        return { FBOMResult::FBOM_OK };
    }

    if (!basePath.IsDirectory())
    {
        return { FBOMResult::FBOM_ERR, "Base path is not a directory" };
    }

    if (!externalPath.Exists() && !externalPath.MkDir())
    {
        return { FBOMResult::FBOM_ERR, "Failed to create external directory" };
    }

    if (!externalPath.IsDirectory())
    {
        return { FBOMResult::FBOM_ERR, "External path is not a directory" };
    }

    HashSet<FBOMResult> errors;

    // Mutex to lock while writing to the output stream
    Mutex outputMutex;
    Mutex errorsMutex;

    auto addError = [&errors, &errorsMutex](FBOMResult err)
    {
        Mutex::Guard guard(errorsMutex);

        errors.Insert(err);
    };

    auto fn = [&](const FBOMObjectLibrary& library, uint32) -> void
    {
        FBOMWriter serializer { FBOMWriterConfig {} };

        for (const FBOMObject& object : library.objectData)
        {
            const FBOMExternalObjectInfo* externalObjectInfo = object.GetExternalObjectInfo();
            HYP_CORE_ASSERT(externalObjectInfo != nullptr);
            HYP_CORE_ASSERT(externalObjectInfo->IsLinked());

            FBOMObject objectCopy(object);

            // unset to stop recursion
            objectCopy.SetIsExternal(false);

            if (FBOMResult err = serializer.Append(objectCopy))
            {
                addError(err);

                return;
            }
        }

        const FBOMObjectLibraryFlags flags = FBOMObjectLibraryFlags::LOCATION_EXTERNAL;

        MemoryByteWriter bufferedOutput;

        bufferedOutput.Write<uint8>(FBOM_OBJECT_LIBRARY_START);

        bufferedOutput.Write<UUID>(library.uuid);
        bufferedOutput.Write<uint8>(uint8(flags));

        if (flags & FBOMObjectLibraryFlags::LOCATION_INLINE)
        {
            MemoryByteWriter byteWriter;

            if (FBOMResult err = serializer.Emit(&byteWriter, /* writeHeader */ false))
            {
                addError(err);

                return;
            }

            ByteBuffer buffer = std::move(byteWriter.GetBuffer());

            // write size of buffer
            bufferedOutput.Write<uint64>(buffer.Size());

            // write actual buffer data
            bufferedOutput.Write(buffer.Data(), buffer.Size());
        }
        else if (flags & FBOMObjectLibraryFlags::LOCATION_EXTERNAL)
        {
            // write to external file

            const FilePath filepath = externalPath / (library.uuid.ToString() + ".hyp");
            const FilePath relativePath = FilePath::Relative(filepath, basePath).BasePath();

            FileByteWriter byteWriter { filepath };

            if (FBOMResult err = serializer.Emit(&byteWriter, /* writeHeader */ true))
            {
                addError(err);

                return;
            }

            bufferedOutput.WriteString(relativePath, BYTE_WRITER_FLAGS_WRITE_SIZE);
        }
        else
        {
            HYP_UNREACHABLE();
        }

        bufferedOutput.Write<uint8>(FBOM_OBJECT_LIBRARY_END);

        // Pipe the buffered data into the output stream
        {
            Mutex::Guard guard(outputMutex);

            out->Write(bufferedOutput.GetBuffer().Data(), bufferedOutput.GetBuffer().Size());
        }
    };

    TaskSystem::GetInstance().ParallelForEach(m_writeStream->m_objectLibraries, fn);

    return errors.Any() ? errors.Front() : FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::BuildStaticData(FBOMLoadContext& context)
{
    m_writeStream->LockObjectDataWriting();

    for (FBOMObject& object : m_writeStream->m_objectData)
    {
        if (FBOMResult err = AddExternalObjects(context, object))
        {
            return err;
        }
    }

    for (const FBOMObject& object : m_writeStream->m_objectData)
    {
        // will be added as static data by other instance when it is written
        if (object.IsExternal())
        {
            continue;
        }

        AddStaticData(context, object);
    }

    m_writeStream->UnlockObjectDataWriting();

    return {};
}

FBOMResult FBOMWriter::AddExternalObjects(FBOMLoadContext& context, FBOMObject& object)
{
    if (object.IsExternal())
    {
        FBOMExternalObjectInfo* externalObjectInfo = object.GetExternalObjectInfo();
        HYP_CORE_ASSERT(externalObjectInfo != nullptr);
        HYP_CORE_ASSERT(!externalObjectInfo->IsLinked());

        m_writeStream->AddToObjectLibrary(object);
    }

    for (FBOMObject& child : object.m_children)
    {
        if (FBOMResult err = AddExternalObjects(context, child))
        {
            return err;
        }
    }

    return {};
}

FBOMResult FBOMWriter::WriteStaticData(ByteWriter* out)
{
    EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE;

    if (m_config.compressStaticData)
    {
        attributes |= FBOMDataAttributes::COMPRESSED;
    }

    m_writeStream->BeginStaticDataWriting();

    Array<FBOMStaticData*> staticDataOrdered;
    staticDataOrdered.Reserve(m_writeStream->m_staticData.Size());

    for (auto& it : m_writeStream->m_staticData)
    {
        staticDataOrdered.PushBack(&it.second);
    }

    std::sort(staticDataOrdered.Begin(), staticDataOrdered.End(), [](const FBOMStaticData* a, const FBOMStaticData* b)
        {
            return a->offset < b->offset;
        });

#if HYP_DEBUG_MODE
    // sanity check: make sure no duplicate offsets
    for (size_t i = 1; i < staticDataOrdered.Size(); i++)
    {
        HYP_CORE_ASSERT(staticDataOrdered[i]->offset == staticDataOrdered[i - 1]->offset + 1);
    }
#endif

    HYP_CORE_ASSERT(staticDataOrdered.Size() == m_writeStream->m_staticDataOffset,
        "Values do not match, incorrect bookkeeping");

    MemoryByteWriter staticDataByteWriter;
    Array<size_t> staticDataBufferOffsets;
    staticDataBufferOffsets.Resize(staticDataOrdered.Size());

    for (FBOMStaticData* staticData : staticDataOrdered)
    {
        HYP_CORE_ASSERT(staticData->offset < staticDataOrdered.Size());

        const size_t bufferOffset = staticDataByteWriter.Position();

        HYP_CORE_ASSERT(!staticData->IsWritten(),
            "Static data object has already been written: %s",
            staticData->ToString().Data());

        if (FBOMResult err = staticData->data->Visit(staticData->GetUniqueID(), this, &staticDataByteWriter, attributes))
        {
            m_writeStream->EndStaticDataWriting();

            return err;
        }

        staticData->SetIsWritten(true);

        HYP_CORE_ASSERT(staticData->IsWritten(),
            "Static data object was not written: %s\t%p",
            staticData->ToString().Data(),
            staticData);

        staticDataBufferOffsets[staticData->offset] = bufferOffset;
    }

    out->Write<uint8>(FBOM_STATIC_DATA_START);

    out->Write<uint8>(FBOM_STATIC_DATA_HEADER_START);

    // write number of items as uint32
    out->Write<uint32>(uint32(staticDataOrdered.Size()));

    // write buffer size
    out->Write<uint64>(uint64(staticDataByteWriter.GetBuffer().Size()));

    for (size_t i = 0; i < staticDataOrdered.Size(); i++)
    {
        const FBOMStaticData* staticData = staticDataOrdered[i];

        // write index
        out->Write<uint32>(uint32(staticData->offset));

        // write type
        out->Write<uint8>(staticData->type);

        // write start offset
        out->Write<uint64>(staticDataBufferOffsets[i]);

        // write size of object
        if (i == staticDataOrdered.Size() - 1)
        {
            out->Write<uint64>(staticDataByteWriter.GetBuffer().Size() - staticDataBufferOffsets[i]);
        }
        else
        {
            HYP_CORE_ASSERT(staticDataBufferOffsets[i + 1] >= staticDataBufferOffsets[i]);

            out->Write<uint64>(staticDataBufferOffsets[i + 1] - staticDataBufferOffsets[i]);
        }
    }

    out->Write<uint8>(FBOM_STATIC_DATA_HEADER_END);

    // Write entire buffer
    out->Write(staticDataByteWriter.GetBuffer().Data(), staticDataByteWriter.GetBuffer().Size());

    out->Write<uint8>(FBOM_STATIC_DATA_END);

    m_writeStream->EndStaticDataWriting();

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteHeader(ByteWriter* out)
{
    const size_t positionBefore = out->Position();

    // hyp identifier string
    out->Write(FBOM::headerIdentifier, sizeof(FBOM::headerIdentifier));

    // endianness
    out->Write<uint8>(ByteUtil::IsBigEndian());

    // binary version
    out->Write<uint32>(FBOM::version.value);

    const size_t positionChange = size_t(out->Position()) - positionBefore;
    HYP_CORE_ASSERT(positionChange <= FBOM::headerSize);

    const size_t remainingBytes = FBOM::headerSize - positionChange;
    HYP_CORE_ASSERT(remainingBytes < 64);

    void* zeros = StackAlloc(remainingBytes);
    Memory::Fill(zeros, 0, remainingBytes);

    out->Write(zeros, remainingBytes);

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::Write(ByteWriter* out, const FBOMObject& object, UniqueId id, EnumFlags<FBOMDataAttributes> attributes)
{
    HYP_CORE_ASSERT(uint64(id) != 0);

    out->Write<uint8>(FBOM_OBJECT_START);
    out->Write<uint64>(id);

    const FBOMStaticData* staticDataPtr;
    const FBOMExternalObjectInfo* externalObjectInfoPtr;

    FBOMDataLocation dataLocation = FBOMDataLocation::INVALID;

    if (object.IsExternal() && object.GetExternalObjectInfo()->IsLinked())
    {
        dataLocation = FBOMDataLocation::LOC_EXT_REF;
        externalObjectInfoPtr = object.GetExternalObjectInfo();
    }
    else
    {
        dataLocation = m_writeStream->GetDataLocation(id, &staticDataPtr, &externalObjectInfoPtr);
    }

    if (FBOMResult err = WriteDataAttributes(out, attributes, dataLocation))
    {
        return err;
    }

    switch (dataLocation)
    {
    case FBOMDataLocation::LOC_STATIC:
    {
        HYP_CORE_ASSERT(staticDataPtr != nullptr);

        return WriteStaticDataUsage(out, *staticDataPtr);
    }
    case FBOMDataLocation::LOC_INPLACE:
    {
        // write typechain
        if (FBOMResult err = object.m_objectType.Visit(this, out))
        {
            return err;
        }

        // add all properties
        for (const auto& it : object.properties)
        {
            EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE;

            if (it.second.IsCompressed())
            {
                attributes |= FBOMDataAttributes::COMPRESSED;
            }

            out->Write<uint8>(FBOM_DEFINE_PROPERTY);

            // write key
            out->WriteString(it.first, BYTE_WRITER_FLAGS_WRITE_SIZE);

            // write value
            if (FBOMResult err = it.second.Visit(this, out, attributes))
            {
                return err;
            }
        }

        for (const FBOMObject& child : object.m_children)
        {
            if (FBOMResult err = child.Visit(this, out))
            {
                return err;
            }
        }

        out->Write<uint8>(FBOM_OBJECT_END);

        break;
    }
    case FBOMDataLocation::LOC_EXT_REF:
    {
        HYP_CORE_ASSERT(externalObjectInfoPtr != nullptr);
        HYP_CORE_ASSERT(externalObjectInfoPtr->IsLinked());

        out->Write<UUID>(externalObjectInfoPtr->libraryId);

        // write object index as u32
        out->Write<uint32>(externalObjectInfoPtr->index);

        // write flags -- i.e, lazy loaded, etc.
        // not yet implemented, just write 0 for now
        out->Write<uint32>(0);

        break;
    }
    default:
        HYP_UNREACHABLE();
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::Write(ByteWriter* out, const FBOMType& type, UniqueId id, EnumFlags<FBOMDataAttributes> attributes)
{
    const FBOMStaticData* staticDataPtr;
    const FBOMExternalObjectInfo* externalObjectInfoPtr;

    const FBOMDataLocation dataLocation = m_writeStream->GetDataLocation(id, &staticDataPtr, &externalObjectInfoPtr);

    if (FBOMResult err = WriteDataAttributes(out, attributes, dataLocation))
    {
        return err;
    }

    if (dataLocation == FBOMDataLocation::LOC_STATIC)
    {
        HYP_CORE_ASSERT(staticDataPtr != nullptr);

        return WriteStaticDataUsage(out, *staticDataPtr);
    }

    if (dataLocation == FBOMDataLocation::LOC_INPLACE)
    {
        if (type.extends != nullptr)
        {
            out->Write<uint8>(1);

            if (FBOMResult err = type.extends->Visit(this, out))
            {
                return err;
            }
        }
        else
        {
            out->Write<uint8>(0);
        }

        // write string of object type (loader to use)
        out->WriteString(type.name, BYTE_WRITER_FLAGS_WRITE_SIZE | BYTE_WRITER_FLAGS_WRITE_STRING_TYPE);

        // write flags
        out->Write<uint8>(uint8(type.flags));

        // write size of the type
        out->Write<uint64>(type.size);

        // write native TypeId
        out->Write<TypeId::ValueType>(type.GetNativeTypeId().Value());
    }
    else
    {
        // unsupported method
        return FBOMResult::FBOM_ERR;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::Write(ByteWriter* out, const FBOMData& data, UniqueId id, EnumFlags<FBOMDataAttributes> attributes)
{
    ByteWriter* writerPtr = out;

    const FBOMStaticData* staticDataPtr;
    const FBOMExternalObjectInfo* externalObjectInfoPtr;

    const FBOMDataLocation dataLocation = m_writeStream->GetDataLocation(id, &staticDataPtr, &externalObjectInfoPtr);

    if (FBOMResult err = WriteDataAttributes(out, attributes, dataLocation))
    {
        return err;
    }

    if (dataLocation == FBOMDataLocation::LOC_STATIC)
    {
        HYP_CORE_ASSERT(staticDataPtr != nullptr);

        return WriteStaticDataUsage(out, *staticDataPtr);
    }

    if (dataLocation == FBOMDataLocation::LOC_INPLACE)
    {
        // write type first
        if (FBOMResult err = data.GetType().Visit(this, out))
        {
            return err;
        }

        size_t size = data.TotalSize();
        ByteBuffer byteBuffer;

        if (FBOMResult err = data.ReadBytes(size, byteBuffer))
        {
            return err;
        }

        if (attributes & FBOMDataAttributes::COMPRESSED)
        {
            if (!Archive::IsEnabled())
            {
                return { FBOMResult::FBOM_ERR, "Cannot write to archive because the Archive feature is not enabled" };
            }

            // Write compressed data
            ArchiveBuilder archiveBuilder;
            archiveBuilder.Append(std::move(byteBuffer));

            if (FBOMResult err = WriteArchive(out, archiveBuilder.Build()))
            {
                return err;
            }
        }
        else
        {
            // raw bytebuffer - write size and then buffer
            out->Write<uint32>(uint32(size));
            out->Write(byteBuffer.Data(), byteBuffer.Size());
        }
    }
    else
    {
        // Unsupported method
        return FBOMResult::FBOM_ERR;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::Write(ByteWriter* out, const FBOMArray& array, UniqueId id, EnumFlags<FBOMDataAttributes> attributes)
{
    const FBOMStaticData* staticDataPtr;
    const FBOMExternalObjectInfo* externalObjectInfoPtr;

    const FBOMDataLocation dataLocation = m_writeStream->GetDataLocation(id, &staticDataPtr, &externalObjectInfoPtr);

    if (FBOMResult err = WriteDataAttributes(out, attributes, dataLocation))
    {
        return err;
    }

    if (dataLocation == FBOMDataLocation::LOC_STATIC)
    {
        HYP_CORE_ASSERT(staticDataPtr != nullptr);

        return WriteStaticDataUsage(out, *staticDataPtr);
    }

    if (dataLocation == FBOMDataLocation::LOC_INPLACE)
    {
        // Write total array size
        out->Write<uint32>(uint32(array.Size()));

        ByteWriter* writerPtr = out;
        MemoryByteWriter archiveWriter;

        if (attributes & FBOMDataAttributes::COMPRESSED)
        {
            if (!Archive::IsEnabled())
            {
                return { FBOMResult::FBOM_ERR, "Cannot write to archive because the Archive feature is not enabled" };
            }

            writerPtr = &archiveWriter;
        }

        // Write each element out - we write the type and the number of elements of that type
        // If type differs, we write the new type and the following elements are of that type
        Optional<FBOMType> currentType;
        Array<const FBOMData*> currentElements; // leftover elements of the current type to write when type changes

        ByteBuffer tempBuffer;

        auto writeElementData = [this, &tempBuffer, attributes, writerPtr](const FBOMData& element) -> FBOMResult
        {
            size_t size = element.TotalSize();

            HYP_DEFER({
                tempBuffer.SetSize(0); // keep capacity to avoid reallocations
            });

            if (FBOMResult err = element.ReadBytes(size, tempBuffer))
            {
                return err;
            }

            writerPtr->Write<uint32>(uint32(size));
            writerPtr->Write(tempBuffer.Data(), tempBuffer.Size());

            return {};
        };

        for (size_t index = 0; index < array.Size(); index++)
        {
            const FBOMData& element = array.GetElement(index);

            if (!currentType.HasValue() || element.GetType() != *currentType)
            {
                if (currentElements.Size() != 0)
                {
                    Assert(currentType.HasValue());

                    // Write out previous type and elements
                    if (FBOMResult err = currentType->Visit(this, writerPtr))
                    {
                        return err;
                    }

                    // Write number of elements of this type
                    writerPtr->Write<uint32>(uint32(currentElements.Size()));

                    for (const FBOMData* currentElement : currentElements)
                    {
                        writeElementData(*currentElement);
                    }

                    currentElements.Clear();
                }

                currentType = element.GetType();
            }

            currentElements.PushBack(&element);
        }

        if (currentElements.Any())
        {
            Assert(currentType.HasValue());

            // Write out previous type and elements
            if (FBOMResult err = currentType->Visit(this, writerPtr))
            {
                return err;
            }

            // Write number of elements of this type
            writerPtr->Write<uint32>(uint32(currentElements.Size()));

            for (const FBOMData* currentElement : currentElements)
            {
                writeElementData(*currentElement);
            }

            currentElements.Clear();
        }

        if (attributes & FBOMDataAttributes::COMPRESSED)
        {
            // Write compressed data
            ArchiveBuilder archiveBuilder;
            archiveBuilder.Append(std::move(archiveWriter.GetBuffer()));

            if (FBOMResult err = WriteArchive(out, archiveBuilder.Build()))
            {
                return err;
            }
        }
    }
    else
    {
        // Unsupported method
        return FBOMResult::FBOM_ERR;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteArchive(ByteWriter* out, const Archive& archive) const
{
    out->Write<uint64>(archive.GetUncompressedSize());
    out->Write<uint64>(archive.GetCompressedSize());
    out->Write(archive.GetCompressedBuffer().Data(), archive.GetCompressedBuffer().Size());

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteDataAttributes(ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes) const
{
    out->Write<uint8>(uint8(attributes));

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteDataAttributes(ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes, FBOMDataLocation location) const
{
    constexpr uint8 locStatic = (1u << uint32(FBOMDataLocation::LOC_STATIC)) << 5;
    constexpr uint8 locInplace = (1u << uint32(FBOMDataLocation::LOC_INPLACE)) << 5;
    constexpr uint8 locExtRef = (1u << uint32(FBOMDataLocation::LOC_EXT_REF)) << 5;

    uint8 attributesValue = uint8(attributes);

    switch (location)
    {
    case FBOMDataLocation::LOC_STATIC:
        attributesValue |= locStatic;

        break;
    case FBOMDataLocation::LOC_INPLACE:
        attributesValue |= locInplace;

        break;
    case FBOMDataLocation::LOC_EXT_REF:
        attributesValue |= locExtRef;

        break;
    }

    out->Write<uint8>(attributesValue);

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteStaticDataUsage(ByteWriter* out, const FBOMStaticData& staticData) const
{
    const auto offset = staticData.offset;
    HYP_CORE_ASSERT(offset < m_writeStream->m_staticDataOffset);

#if HYP_DEBUG_MODE
    const auto it = m_writeStream->m_staticData.Find(staticData.GetUniqueID());
    HYP_CORE_ASSERT(it != m_writeStream->m_staticData.End());

    // HYP_CORE_ASSERT(it->second.IsWritten());

    HYP_CORE_ASSERT(it->second.type == staticData.type);
    HYP_CORE_ASSERT(it->second.GetHashCode() == staticData.GetHashCode());
#endif

    out->Write<uint32>(uint32(offset));

    return FBOMResult::FBOM_OK;
}

void FBOMWriter::AddObjectData(const FBOMObject& object, UniqueId id)
{
    HYP_CORE_ASSERT(uint64(id) != 0);

    HYP_CORE_ASSERT(!m_writeStream->IsObjectDataWritingLocked());

    m_writeStream->m_objectData.PushBack(object);

    auto it = m_writeStream->m_hashUseCountMap.Find(id);

    if (it == m_writeStream->m_hashUseCountMap.End())
    {
        it = m_writeStream->m_hashUseCountMap.Insert(id, 0).first;
    }

    it->second++;
}

void FBOMWriter::AddObjectData(FBOMObject&& object, UniqueId id)
{
    HYP_CORE_ASSERT(uint64(id) != 0);

    HYP_CORE_ASSERT(!m_writeStream->IsObjectDataWritingLocked());

    m_writeStream->m_objectData.PushBack(std::move(object));

    auto it = m_writeStream->m_hashUseCountMap.Find(id);

    if (it == m_writeStream->m_hashUseCountMap.End())
    {
        it = m_writeStream->m_hashUseCountMap.Insert(id, 0).first;
    }

    it->second++;
}

UniqueId FBOMWriter::AddStaticData(UniqueId id, FBOMStaticData&& staticData)
{
    HYP_CORE_ASSERT(!m_writeStream->IsWritingStaticData());

    auto it = m_writeStream->m_staticData.Find(id);

    if (it == m_writeStream->m_staticData.End())
    {
        staticData.SetUniqueID(id);
        staticData.offset = int64(m_writeStream->m_staticDataOffset++);

        const auto insertResult = m_writeStream->m_staticData.Insert(id, std::move(staticData));
        HYP_CORE_ASSERT(insertResult.second);
    }
    else
    {
#if 0
        // sanity check - ensure pointing to correct object
        const HashCode hashCode = staticData.GetHashCode();
        const HashCode otherHashCode = it->second.GetHashCode();

        HYP_CORE_ASSERT(hashCode == otherHashCode,
            "Hash codes do not match: %llu != %llu\n\tLeft: %s\n\tRight: %s",
            hashCode.Value(), otherHashCode.Value(),
            staticData.ToString().Data(), it->second.ToString().Data());
#endif
    }

    return id;
}

UniqueId FBOMWriter::AddStaticData(FBOMLoadContext& context, const FBOMType& type)
{
    if (type.extends != nullptr)
    {
        AddStaticData(context, *type.extends);
    }

    return AddStaticData(FBOMStaticData(type));
}

UniqueId FBOMWriter::AddStaticData(FBOMLoadContext& context, const FBOMObject& object)
{
    AddStaticData(context, object.GetType());

    return AddStaticData(FBOMStaticData(object));
}

UniqueId FBOMWriter::AddStaticData(FBOMLoadContext& context, const FBOMArray& array)
{
    return AddStaticData(FBOMStaticData(array));
}

UniqueId FBOMWriter::AddStaticData(FBOMLoadContext& context, const FBOMData& data)
{
    UniqueId id = UniqueId::Invalid();

    AddStaticData(context, data.GetType());

    if (data.IsObject())
    {
        FBOMObject object;

        const bool result = data.ReadObject(context, object).value;
        HYP_CORE_ASSERT(result == FBOMResult::FBOM_OK, "Invalid object, cannot write to stream");

        AddStaticData(context, object);
    }
    else if (data.IsArray())
    {
        FBOMArray array;

        const FBOMResult result = data.ReadArray(context, array).value;
        HYP_CORE_ASSERT(result == FBOMResult::FBOM_OK, "Invalid array, cannot write to stream");

        AddStaticData(context, array);
    }
    else
    {
        HYP_FAIL("Unhandled container type");
    }

    return AddStaticData(FBOMStaticData(data));
}

#pragma endregion FBOMWriter

} // namespace Hyperion::serialization
