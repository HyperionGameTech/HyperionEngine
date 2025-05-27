/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMData.hpp>
#include <core/serialization/fbom/FBOMObject.hpp>
#include <core/serialization/fbom/FBOMConfig.hpp>
#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>

#include <core/io/BufferedByteReader.hpp>
#include <core/io/ByteWriter.hpp>

#include <core/object/HypData.hpp>

#include <core/memory/Memory.hpp>

#include <core/json/JSON.hpp>

#include <sstream>

namespace hyperion::fbom {

FBOMData::FBOMData(EnumFlags<FBOMDataFlags> flags)
    : m_type(FBOMUnset()),
      m_flags(flags)
{
}

FBOMData::FBOMData(const FBOMType& type, EnumFlags<FBOMDataFlags> flags)
    : m_type(type),
      m_flags(flags)
{
    if (!type.IsUnbounded())
    {
        m_bytes.SetSize(type.size);
    }
}

FBOMData::FBOMData(const FBOMType& type, ByteBuffer&& byte_buffer, EnumFlags<FBOMDataFlags> flags)
    : m_bytes(std::move(byte_buffer)),
      m_type(type),
      m_flags(flags)
{
}

FBOMData::FBOMData(const FBOMData& other)
    : FBOMSerializableBase(static_cast<const FBOMSerializableBase&>(other)),
      m_type(other.m_type),
      m_bytes(other.m_bytes),
      m_flags(other.m_flags),
      m_deserialized_object(other.m_deserialized_object)
{
}

FBOMData& FBOMData::operator=(const FBOMData& other)
{
    if (this == &other)
    {
        return *this;
    }

    FBOMSerializableBase::operator=(static_cast<const FBOMSerializableBase&>(other));

    m_type = other.m_type;
    m_bytes = other.m_bytes;
    m_flags = other.m_flags;
    m_deserialized_object = other.m_deserialized_object;

    return *this;
}

FBOMData::FBOMData(FBOMData&& other) noexcept
    : FBOMSerializableBase(static_cast<FBOMSerializableBase&&>(std::move(other))),
      m_bytes(std::move(other.m_bytes)),
      m_type(std::move(other.m_type)),
      m_flags(other.m_flags),
      m_deserialized_object(std::move(other.m_deserialized_object))
{
    other.m_type = FBOMUnset();
    other.m_flags = FBOMDataFlags::NONE;
}

FBOMData& FBOMData::operator=(FBOMData&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    FBOMSerializableBase::operator=(static_cast<FBOMSerializableBase&&>(std::move(other)));

    m_bytes = std::move(other.m_bytes);
    m_type = std::move(other.m_type);
    m_flags = other.m_flags;
    m_deserialized_object = std::move(other.m_deserialized_object);

    other.m_type = FBOMUnset();
    other.m_flags = FBOMDataFlags::NONE;

    return *this;
}

FBOMData::~FBOMData()
{
}

FBOMResult FBOMData::ReadObject(FBOMLoadContext& context, FBOMObject& out_object) const
{
    if (!IsObject())
    {
        return { FBOMResult::FBOM_ERR, "Not an object" };
    }

    BufferedReader byte_reader(MakeRefCountedPtr<MemoryBufferedReaderSource>(m_bytes.ToByteView()));

    FBOMReader deserializer(FBOMReaderConfig {});

    // return deserializer.Deserialize(byte_reader, out_object);
    if (FBOMResult err = deserializer.ReadObject(context, &byte_reader, out_object, nullptr))
    {
        return err;
    }

    return {};
}

FBOMData FBOMData::FromObject(const FBOMObject& object, bool keep_native_object)
{
    FBOMWriterConfig config;
    config.enable_static_data = false;

    MemoryByteWriter byte_writer;

    FBOMWriter serializer { config };

    if (FBOMResult err = object.Visit(&serializer, &byte_writer))
    {
        AssertThrowMsg(false, "Failed to serialize object: %s", err.message.Data());
    }

    FBOMData value = FBOMData(FBOMBaseObjectType(), std::move(byte_writer.GetBuffer()));
    AssertThrowMsg(value.IsObject(), "Expected value to be object: Got type: %s", value.GetType().ToString().Data());

    if (keep_native_object)
    {
        value.m_deserialized_object = object.m_deserialized_object;
    }

    return value;
}

FBOMData FBOMData::FromObject(FBOMObject&& object, bool keep_native_object)
{
    FBOMWriterConfig config;
    config.enable_static_data = false;

    MemoryByteWriter byte_writer;

    FBOMWriter serializer { config };

    if (FBOMResult err = object.Visit(&serializer, &byte_writer))
    {
        AssertThrowMsg(false, "Failed to serialize object: %s", err.message.Data());
    }

    FBOMData value = FBOMData(FBOMBaseObjectType(), std::move(byte_writer.GetBuffer()));
    AssertThrowMsg(value.IsObject(), "Expected value to be object: Got type: %s", value.GetType().ToString().Data());

    if (keep_native_object)
    {
        value.m_deserialized_object = std::move(object.m_deserialized_object);
    }

    return value;
}

FBOMResult FBOMData::ReadArray(FBOMLoadContext& context, FBOMArray& out_array) const
{
    if (!IsArray())
    {
        return { FBOMResult::FBOM_ERR, "Not an array" };
    }

    BufferedReader byte_reader(MakeRefCountedPtr<MemoryBufferedReaderSource>(m_bytes.ToByteView()));

    FBOMReader deserializer(FBOMReaderConfig {});
    return deserializer.ReadArray(context, &byte_reader, out_array);
}

FBOMData FBOMData::FromArray(const FBOMArray& array)
{
    FBOMWriterConfig config;
    config.enable_static_data = false;

    MemoryByteWriter byte_writer;

    FBOMWriter serializer { config };

    if (FBOMResult err = array.Visit(&serializer, &byte_writer))
    {
        AssertThrowMsg(false, "Failed to serialize array: %s", err.message.Data());
    }

    FBOMData value = FBOMData(FBOMArrayType(), std::move(byte_writer.GetBuffer()));
    AssertThrowMsg(value.IsArray(), "Expected value to be array: Got type: %s", value.GetType().ToString().Data());

    return value;
}

SizeType FBOMData::ReadBytes(SizeType n, void* out) const
{
    if (!m_type.IsUnbounded())
    {
        AssertThrowMsg(n <= m_bytes.Size(), "Attempt to read past max size of object");
    }

    SizeType to_read = MathUtil::Min(n, m_bytes.Size());
    Memory::MemCpy(out, m_bytes.Data(), to_read);
    return to_read;
}

ByteBuffer FBOMData::ReadBytes() const
{
    return m_bytes;
}

ByteBuffer FBOMData::ReadBytes(SizeType n) const
{
    if (!m_type.IsUnbounded())
    {
        AssertThrowMsg(n <= m_bytes.Size(), "Attempt to read past max size of object");
    }

    SizeType to_read = MathUtil::Min(n, m_bytes.Size());

    return ByteBuffer(to_read, m_bytes.Data());
}

void FBOMData::SetBytes(const ByteBuffer& byte_buffer)
{
    if (!m_type.IsUnbounded())
    {
        AssertThrowMsg(byte_buffer.Size() <= m_type.size, "Attempt to insert data past size max size of object (%llu > %llu)", byte_buffer.Size(), m_type.size);
    }

    m_bytes = byte_buffer;
}

void FBOMData::SetBytes(SizeType count, const void* data)
{
    if (!m_type.IsUnbounded())
    {
        AssertThrowMsg(count <= m_type.size, "Attempt to insert data past size max size of object (%llu > %llu)", count, m_type.size);
    }

    m_bytes.SetSize(count);
    m_bytes.SetData(count, data);
}

FBOMResult FBOMData::ToJSON(FBOMLoadContext& context, json::JSONValue& out_json) const
{
    if (IsInt8())
    {
        int8 value;

        if (FBOMResult err = ReadInt8(&value))
        {
            return err;
        }

        out_json = json::JSONNumber(value);

        return {};
    }

    if (IsInt16())
    {
        int16 value;

        if (FBOMResult err = ReadInt16(&value))
        {
            return err;
        }

        out_json = json::JSONNumber(value);

        return {};
    }

    if (IsInt32())
    {
        int32 value;

        if (FBOMResult err = ReadInt32(&value))
        {
            return err;
        }

        out_json = json::JSONNumber(value);

        return {};
    }

    if (IsInt64())
    {
        int64 value;

        if (FBOMResult err = ReadInt64(&value))
        {
            return err;
        }

        out_json = json::JSONNumber(value);

        return {};
    }

    if (IsUInt8())
    {
        uint8 value;

        if (FBOMResult err = ReadUInt8(&value))
        {
            return err;
        }

        out_json = json::JSONNumber(value);

        return {};
    }

    if (IsUInt16())
    {
        uint16 value;

        if (FBOMResult err = ReadUInt16(&value))
        {
            return err;
        }

        out_json = json::JSONNumber(value);

        return {};
    }

    if (IsUInt32())
    {
        uint32 value;

        if (FBOMResult err = ReadUInt32(&value))
        {
            return err;
        }

        out_json = json::JSONNumber(value);

        return {};
    }

    if (IsUInt64())
    {
        uint64 value;

        if (FBOMResult err = ReadUInt64(&value))
        {
            return err;
        }

        out_json = json::JSONNumber(value);

        return {};
    }

    if (IsFloat())
    {
        float value;

        if (FBOMResult err = ReadFloat(&value))
        {
            return err;
        }

        out_json = json::JSONNumber(value);

        return {};
    }

    if (IsDouble())
    {
        double value;

        if (FBOMResult err = ReadDouble(&value))
        {
            return err;
        }

        out_json = json::JSONNumber(value);

        return {};
    }

    if (IsBool())
    {
        bool value;

        if (FBOMResult err = ReadBool(&value))
        {
            return err;
        }

        out_json = json::JSONBool(value);

        return {};
    }

    if (IsString())
    {
        String str;

        if (FBOMResult err = ReadString(str))
        {
            return err;
        }

        out_json = json::JSONString(std::move(str));

        return {};
    }

    if (IsArray())
    {
        FBOMArray array { FBOMUnset() };

        if (FBOMResult err = ReadArray(context, array))
        {
            return err;
        }

        json::JSONArray array_json;

        for (SizeType i = 0; i < array.Size(); i++)
        {
            json::JSONValue element_json;

            if (FBOMResult err = array.GetElement(i).ToJSON(context, element_json))
            {
                return err;
            }

            array_json.PushBack(std::move(element_json));
        }

        out_json = std::move(array_json);

        return {};
    }

    if (IsObject())
    {
        FBOMObject object;

        if (FBOMResult err = ReadObject(context, object))
        {
            return err;
        }

        json::JSONObject object_json;

        for (const auto& it : object.GetProperties())
        {
            json::JSONValue value_json;

            if (FBOMResult err = it.second.ToJSON(context, value_json))
            {
                return err;
            }

            object_json[it.first] = std::move(value_json);
        }

        out_json = std::move(object_json);

        return {};
    }

    return { FBOMResult::FBOM_ERR, "Data could not be converted to JSON" };
}

FBOMData FBOMData::FromJSON(const json::JSONValue& json_value)
{
    if (json_value.IsNumber())
    {
        const bool is_integer = MathUtil::Floor<double, double>(json_value.AsNumber()) == json_value.AsNumber();

        if (is_integer)
        {
            return FBOMData::FromInt64(int64(json_value.AsNumber()));
        }
        else
        {
            return FBOMData::FromDouble(json_value.AsNumber());
        }
    }

    if (json_value.IsString())
    {
        return FBOMData::FromString(json_value.AsString().ToUTF8());
    }

    if (json_value.IsBool())
    {
        return FBOMData::FromBool(json_value.AsBool());
    }

    if (json_value.IsArray())
    {
        FBOMArray array { fbom::FBOMUnset() };

        const json::JSONArray& json_array = json_value.AsArray();

        if (json_array.Any())
        {
            Array<FBOMData> elements;
            elements.Reserve(json_array.Size());

            for (const json::JSONValue& element : json_array)
            {
                elements.EmplaceBack(FBOMData::FromJSON(element));
            }

            array = fbom::FBOMArray(elements[0].GetType(), std::move(elements));
        }

        return FBOMData::FromArray(std::move(array));
    }

    if (json_value.IsObject())
    {
        FBOMObject object;

        const json::JSONObject& json_object = json_value.AsObject();

        if (json_object.Any())
        {
            for (const Pair<json::JSONString, json::JSONValue>& pair : json_object)
            {
                object.SetProperty(ANSIString(pair.first), FBOMData::FromJSON(pair.second));
            }
        }

        return FBOMData::FromObject(std::move(object));
    }

    return FBOMData();
}

FBOMData::FBOMData(const json::JSONValue& json_value)
    : FBOMData(FromJSON(json_value))
{
}

FBOMResult FBOMData::Visit(UniqueID id, FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes) const
{
    return writer->Write(out, *this, id, attributes);
}

String FBOMData::ToString(bool deep) const
{
    std::stringstream stream;
    stream << "FBOM[";
    stream << "type: " << m_type.name << ", ";
    stream << "size: " << String::ToString(m_bytes.Size()) << ", ";
    stream << "data: { ";

    if (deep)
    {
        for (SizeType i = 0; i < m_bytes.Size(); i++)
        {
            stream << std::hex << int(m_bytes[i]) << " ";
        }
    }
    else
    {
        stream << m_bytes.Size();
    }

    stream << " } ";

    // if (next != nullptr) {
    //    stream << "<" << next->GetType().name << ">";
    //}

    stream << "]";

    return String(stream.str().c_str());
}

UniqueID FBOMData::GetUniqueID() const
{
    return UniqueID(GetHashCode());
}

HashCode FBOMData::GetHashCode() const
{
    HashCode hc;

    hc.Add(m_bytes.Size());
    hc.Add(m_type.GetHashCode());
    hc.Add(m_bytes.GetHashCode());

    return hc;
}

} // namespace hyperion::fbom
