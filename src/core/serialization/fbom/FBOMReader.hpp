/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/String.hpp>

#include <core/utilities/UniqueId.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/math/MathUtil.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/serialization/fbom/FBOMObject.hpp>
#include <core/serialization/fbom/FBOMResult.hpp>
#include <core/serialization/fbom/FBOMType.hpp>
#include <core/serialization/fbom/FBOMStaticData.hpp>
#include <core/serialization/fbom/FBOMObjectLibrary.hpp>
#include <core/serialization/fbom/FBOMConfig.hpp>
#include <core/serialization/fbom/FBOMEnums.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <core/utilities/ByteUtil.hpp>

#include <core/Constants.hpp>
#include <core/Types.hpp>
#include <core/HashCode.hpp>

namespace hyperion {

struct HypData;

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
    FBOMReader(const FBOMReaderConfig& config);
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
    FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out);
    FBOMResult Deserialize(FBOMLoadContext& context, BufferedReader& reader, HypData& out);

    FBOMResult LoadFromFile(const String& path, FBOMObject& out);
    FBOMResult LoadFromFile(const String& path, HypData& out);

    FBOMResult ReadObject(FBOMLoadContext& context, BufferedReader* reader, FBOMObject& outObject, FBOMObject* root);
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
            SizeType offset = 0;
            SizeType size = 0;
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

        void Initialize(SizeType size)
        {
            elements.Resize(size);
        }

        FBOMSerializableBase* GetOrInitializeElement(FBOMLoadContext& context, FBOMReader* reader, SizeType index);
        void SetElementDesc(SizeType index, FBOMStaticData::Type type, SizeType offset, SizeType size);
    };

    template <class T>
    void CheckEndianness(T& value)
    {
        static_assert(isPodType<T>, "T must be POD to use CheckEndianness()");

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

    FBOMResult ReadRawData(BufferedReader*, SizeType count, ByteBuffer& outBuffer);

    template <class T>
    FBOMResult ReadRawData(BufferedReader* reader, T* outPtr)
    {
        static_assert(isPodType<NormalizedType<T>>, "T must be POD to read as raw data");

        HYP_CORE_ASSERT(outPtr != nullptr);

        constexpr SizeType size = sizeof(NormalizedType<T>);

        ByteBuffer byteBuffer;

        if (FBOMResult err = ReadRawData(reader, size, byteBuffer))
        {
            return err;
        }

        Memory::MemCpy(static_cast<void*>(outPtr), static_cast<const void*>(byteBuffer.Data()), size);

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
} // namespace hyperion
