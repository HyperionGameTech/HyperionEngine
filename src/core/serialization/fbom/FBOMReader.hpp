/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_READER_HPP
#define HYPERION_FBOM_READER_HPP

#include <core/containers/TypeMap.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/Variant.hpp>
#include <core/utilities/UniqueID.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/TypeAttributes.hpp>

#include <core/memory/Any.hpp>
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

#include <Constants.hpp>
#include <Types.hpp>
#include <HashCode.hpp>

#include <type_traits>
#include <cstring>

namespace hyperion {

struct HypData;

namespace compression {
class Archive;
} // namespace compression

using compression::Archive;

namespace fbom {

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

    FBOMResult Deserialize(FBOMLoadContext& context, BufferedReader& reader, FBOMObjectLibrary& out, bool read_header = true);
    FBOMResult Deserialize(FBOMLoadContext& context, BufferedReader& reader, FBOMObject& out);
    FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out);
    FBOMResult Deserialize(FBOMLoadContext& context, BufferedReader& reader, HypData& out);

    FBOMResult LoadFromFile(const String& path, FBOMObject& out);
    FBOMResult LoadFromFile(const String& path, HypData& out);

    FBOMResult ReadObject(FBOMLoadContext& context, BufferedReader* reader, FBOMObject& out_object, FBOMObject* root);
    FBOMResult ReadObjectType(FBOMLoadContext& context, BufferedReader* reader, FBOMType& out_type);
    FBOMResult ReadObjectLibrary(FBOMLoadContext& context, BufferedReader* reader, FBOMObjectLibrary& out_library);
    FBOMResult ReadData(FBOMLoadContext& context, BufferedReader* reader, FBOMData& out_data);
    FBOMResult ReadArray(FBOMLoadContext& context, BufferedReader* reader, FBOMArray& out_array);
    FBOMResult ReadPropertyName(FBOMLoadContext& context, BufferedReader* reader, Name& out_property_name);

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
        static_assert(is_pod_type<T>, "T must be POD to use CheckEndianness()");

        if constexpr (sizeof(T) == 1)
        {
            return;
        }
        else if (m_swap_endianness)
        {
            SwapEndianness(value);
        }
    }

    FBOMMarshalerBase* GetMarshalForType(const FBOMType& type) const;

    FBOMResult RequestExternalObject(FBOMLoadContext& context, UUID library_id, uint32 index, FBOMObject& out_object);

    FBOMCommand NextCommand(BufferedReader*);
    FBOMCommand PeekCommand(BufferedReader*);
    FBOMResult Eat(BufferedReader*, FBOMCommand, bool read = true);

    FBOMResult ReadDataAttributes(BufferedReader*, EnumFlags<FBOMDataAttributes>& out_attributes, FBOMDataLocation& out_location);

    template <class StringType>
    FBOMResult ReadString(BufferedReader*, StringType& out_string);

    FBOMResult ReadArchive(BufferedReader*, Archive& out_archive);
    FBOMResult ReadArchive(const ByteBuffer& in_buffer, ByteBuffer& out_buffer);

    FBOMResult ReadRawData(BufferedReader*, SizeType count, ByteBuffer& out_buffer);

    template <class T>
    FBOMResult ReadRawData(BufferedReader* reader, T* out_ptr)
    {
        static_assert(is_pod_type<NormalizedType<T>>, "T must be POD to read as raw data");

        AssertThrow(out_ptr != nullptr);

        constexpr SizeType size = sizeof(NormalizedType<T>);

        ByteBuffer byte_buffer;

        if (FBOMResult err = ReadRawData(reader, size, byte_buffer))
        {
            return err;
        }

        Memory::MemCpy(static_cast<void*>(out_ptr), static_cast<const void*>(byte_buffer.Data()), size);

        CheckEndianness(*out_ptr);

        return FBOMResult::FBOM_OK;
    }

    FBOMResult LoadFromFile(FBOMLoadContext& context, const String& path, FBOMObjectLibrary& out);

    FBOMResult Handle(FBOMLoadContext& context, BufferedReader* reader, FBOMCommand command, FBOMObject* root);

public:
    FBOMReaderConfig m_config;

    bool m_in_static_data;
    FBOMStaticDataIndexMap m_static_data_index_map;
    ByteBuffer m_static_data_buffer;

    bool m_swap_endianness;
};

} // namespace fbom
} // namespace hyperion

#endif
