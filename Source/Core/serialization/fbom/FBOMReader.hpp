/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/containers/String.hpp>

#include <Core/utilities/EnumFlags.hpp>

#include <Core/memory/ByteBuffer.hpp>
#include <Core/memory/RefCountedPtr.hpp>

#include <Core/math/MathUtil.hpp>

#include <Core/io/ByteWriter.hpp>

#include <Core/serialization/fbom/FBOMObject.hpp>
#include <Core/serialization/fbom/FBOMResult.hpp>
#include <Core/serialization/fbom/FBOMType.hpp>
#include <Core/serialization/fbom/FBOMStaticData.hpp>
#include <Core/serialization/fbom/FBOMObjectLibrary.hpp>
#include <Core/serialization/fbom/FBOMConfig.hpp>
#include <Core/serialization/fbom/FBOMEnums.hpp>
#include <Core/serialization/fbom/util/UniqueId.hpp>

#include <Core/filesystem/FsUtil.hpp>

#include <Core/utilities/ByteUtil.hpp>

#include <Core/Constants.hpp>
#include <Core/Types.hpp>
#include <Core/HashCode.hpp>

namespace Hyperion {

struct BoxedValue;

namespace compression {
class Archive;
} // namespace compression

using compression::Archive;

namespace serialization {

struct FBOMObjectType;

class FBOMObject;
class FBOMReader;
class FBOMWriter;
class FBOMArray;
class FBOMData;
class FBOMMarshalerBase;
class FBOMLoadContext;

class FBOMReader
{
public:
    explicit FBOMReader(const FBOMReaderConfig& config);

    FBOMReader(const FBOMReader& other) = delete;
    FBOMReader& operator=(const FBOMReader& other) = delete;

    FBOMReader(FBOMReader&& other) noexcept = delete;
    FBOMReader& operator=(FBOMReader&& other) noexcept = delete;

    ~FBOMReader();

    HYP_FORCE_INLINE const FBOMReaderConfig& GetConfig() const
    {
        return m_config;
    }

    FBOMResult Deserialize(FBOMLoadContext& context, BufferedReader& reader, FBOMObjectLibrary& out, bool readHeader = true);
    FBOMResult Deserialize(FBOMLoadContext& context, BufferedReader& reader, FBOMObject& out);
    FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, BoxedValue& out);
    FBOMResult Deserialize(FBOMLoadContext& context, BufferedReader& reader, BoxedValue& out);

    FBOMResult LoadFromFile(const String& path, FBOMObject& out);
    FBOMResult LoadFromFile(const String& path, BoxedValue& out);

    FBOMResult ReadObject(FBOMLoadContext& context, BufferedReader* reader, FBOMObject& outObject, FBOMObject* root, bool deserializeObject = true);
    FBOMResult ReadObjectType(FBOMLoadContext& context, BufferedReader* reader, FBOMType& outType);
    FBOMResult ReadObjectLibrary(FBOMLoadContext& context, BufferedReader* reader, FBOMObjectLibrary& outLibrary);
    FBOMResult ReadData(FBOMLoadContext& context, BufferedReader* reader, FBOMData& outData);
    FBOMResult ReadArray(FBOMLoadContext& context, BufferedReader* reader, FBOMArray& outArray);
    FBOMResult ReadPropertyName(FBOMLoadContext& context, BufferedReader* reader, Name& outPropertyName);

private:
    struct FBOMStaticDataIndexMap
    {
        struct Element
        {
            FBOMStaticData::Type type = FBOMStaticData::FBOM_STATIC_DATA_NONE;
            size_t offset = 0;
            size_t size = 0;
            UniquePtr<FBOMSerializableBase> ptr;

            HYP_FORCE_INLINE bool IsValid() const
            {
                return type != FBOMStaticData::FBOM_STATIC_DATA_NONE && size != 0;
            }

            HYP_FORCE_INLINE bool IsInitialized() const
            {
                return ptr != nullptr;
            }

            FBOMResult Initialize(FBOMLoadContext& context, FBOMReader* reader);
        };

        Array<Element> elements;

        void Initialize(size_t size)
        {
            elements.Resize(size);
        }

        FBOMSerializableBase* GetOrInitializeElement(FBOMLoadContext& context, FBOMReader* reader, size_t index);
        void SetElementDesc(size_t index, FBOMStaticData::Type type, size_t offset, size_t size);
    };

    template <class T>
    void CheckEndianness(T& value)
    {
        static_assert(IsPodTypeV<T>, "T must be POD to use CheckEndianness()");

        if constexpr (sizeof(T) == 1)
        {
            return;
        }
        else if (m_swapEndianness)
        {
            SwapEndian(value);
        }
    }

    FBOMMarshalerBase* GetMarshalForType(const FBOMType& type) const;

    FBOMResult RequestExternalObject(FBOMLoadContext& context, UUID libraryId, uint32 index, FBOMObject& outObject);

    FBOMCommand NextCommand(BufferedReader*);
    FBOMCommand PeekCommand(BufferedReader*);
    FBOMResult Eat(BufferedReader*, FBOMCommand, bool read = true);

    FBOMResult ReadDataAttributes(BufferedReader*, EnumFlags<FBOMDataAttributes>& outAttributes, FBOMDataLocation& outLocation);

    template <class StringType>
    FBOMResult ReadString(BufferedReader*, StringType& outString);

    FBOMResult ReadArchive(BufferedReader*, Archive& outArchive);
    FBOMResult ReadArchive(const ByteBuffer& inBuffer, ByteBuffer& outBuffer);

    FBOMResult ReadRawData(BufferedReader*, size_t count, ByteBuffer& outBuffer);

    template <class T>
    FBOMResult ReadRawData(BufferedReader* reader, T* outPtr)
    {
        static_assert(IsPodTypeV<NormalizedType<T>>, "T must be POD to read as raw data");

        HYP_CORE_ASSERT(outPtr != nullptr);

        constexpr size_t size = sizeof(NormalizedType<T>);

        ByteBuffer byteBuffer;

        if (FBOMResult err = ReadRawData(reader, size, byteBuffer))
        {
            return err;
        }

        Memory::Copy(static_cast<void*>(outPtr), static_cast<const void*>(byteBuffer.Data()), size);

        CheckEndianness(*outPtr);

        return FBOMResult::FBOM_OK;
    }

    FBOMResult LoadFromFile(FBOMLoadContext& context, const String& path, FBOMObjectLibrary& out);

    FBOMResult Handle(FBOMLoadContext& context, BufferedReader* reader, FBOMCommand command, FBOMObject* root);

public:
    FBOMReaderConfig m_config;

    bool m_inStaticData;
    FBOMStaticDataIndexMap m_staticDataIndexMap;
    ByteBuffer m_staticDataBuffer;

    bool m_swapEndianness;
};

} // namespace serialization
} // namespace Hyperion
