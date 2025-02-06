/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>
#include <asset/serialization/fbom/FBOMObject.hpp>
#include <asset/serialization/fbom/FBOMConfig.hpp>
#include <asset/serialization/fbom/FBOMWriter.hpp>
#include <asset/serialization/fbom/FBOMReader.hpp>

#include <asset/BufferedByteReader.hpp>
#include <asset/ByteWriter.hpp>

#include <core/object/HypData.hpp>

#include <core/memory/Memory.hpp>

#include <core/json/JSON.hpp>

#include <sstream>

namespace hyperion::fbom {

FBOMData::FBOMData(EnumFlags<FBOMDataFlags> flags)
    : type(FBOMUnset()),
      m_flags(flags)
{
}

FBOMData::FBOMData(const FBOMType &type, EnumFlags<FBOMDataFlags> flags)
    : type(type),
      m_flags(flags)
{
    if (!type.IsUnbounded()) {
        bytes.SetSize(type.size);
    }
}

FBOMData::FBOMData(const FBOMType &type, ByteBuffer &&byte_buffer, EnumFlags<FBOMDataFlags> flags)
    : bytes(std::move(byte_buffer)),
      type(type),
      m_flags(flags)
{
}

FBOMData::FBOMData(const FBOMData &other)
    : type(other.type),
      bytes(other.bytes),
      m_flags(other.m_flags),
      m_deserialized_object(other.m_deserialized_object)
{
}

FBOMData &FBOMData::operator=(const FBOMData &other)
{
    type = other.type;
    bytes = other.bytes;
    m_flags = other.m_flags;
    m_deserialized_object = other.m_deserialized_object;

    return *this;
}

FBOMData::FBOMData(FBOMData &&other) noexcept
    : bytes(std::move(other.bytes)),
      type(std::move(other.type)),
      m_flags(other.m_flags),
      m_deserialized_object(std::move(other.m_deserialized_object))
{
    other.type = FBOMUnset();
    other.m_flags = FBOMDataFlags::NONE;
}

FBOMData &FBOMData::operator=(FBOMData &&other) noexcept
{
    bytes = std::move(other.bytes);
    type = std::move(other.type);
    m_flags = other.m_flags;
    m_deserialized_object = std::move(other.m_deserialized_object);

    other.type = FBOMUnset();
    other.m_flags = FBOMDataFlags::NONE;

    return *this;
}

FBOMData::~FBOMData()
{
}

FBOMResult FBOMData::ReadObject(FBOMObject &out_object) const
{
    if (!IsObject()) {
        return { FBOMResult::FBOM_ERR, "Not an object" };
    }

    BufferedReader byte_reader(MakeRefCountedPtr<MemoryBufferedReaderSource>(bytes.ToByteView()));
    
    FBOMReader deserializer(FBOMReaderConfig { });

    // return deserializer.Deserialize(byte_reader, out_object);
    if (FBOMResult err = deserializer.ReadObject(&byte_reader, out_object, nullptr)) {
        return err;
    }

    return { FBOMResult::FBOM_OK };
}

FBOMData FBOMData::FromObject(const FBOMObject &object, bool keep_native_object)
{
    FBOMWriterConfig config;
    config.enable_static_data = false;

    MemoryByteWriter byte_writer;

    FBOMWriter serializer { config };

    if (FBOMResult err = object.Visit(&serializer, &byte_writer)) {
        AssertThrowMsg(false, "Failed to serialize object: %s", err.message.Data());
    }

    FBOMData value = FBOMData(FBOMBaseObjectType(), std::move(byte_writer.GetBuffer()));
    AssertThrowMsg(value.IsObject(), "Expected value to be object: Got type: %s", value.GetType().ToString().Data());

    if (keep_native_object) {
        value.m_deserialized_object = object.m_deserialized_object;
    }

    return value;
}

FBOMData FBOMData::FromObject(FBOMObject &&object, bool keep_native_object)
{
    FBOMWriterConfig config;
    config.enable_static_data = false;

    MemoryByteWriter byte_writer;

    FBOMWriter serializer { config };

    if (FBOMResult err = object.Visit(&serializer, &byte_writer)) {
        AssertThrowMsg(false, "Failed to serialize object: %s", err.message.Data());
    }

    FBOMData value = FBOMData(FBOMBaseObjectType(), std::move(byte_writer.GetBuffer()));
    AssertThrowMsg(value.IsObject(), "Expected value to be object: Got type: %s", value.GetType().ToString().Data());

    if (keep_native_object) {
        value.m_deserialized_object = std::move(object.m_deserialized_object);
    }

    return value;
}

FBOMResult FBOMData::ReadArray(FBOMArray &out_array) const
{
    if (!IsArray()) {
        return { FBOMResult::FBOM_ERR, "Not an array" };
    }

    BufferedReader byte_reader(MakeRefCountedPtr<MemoryBufferedReaderSource>(bytes.ToByteView()));
    
    FBOMReader deserializer { FBOMReaderConfig { } };
    return deserializer.ReadArray(&byte_reader, out_array);
}

FBOMData FBOMData::FromArray(const FBOMArray &array)
{
    FBOMWriterConfig config;
    config.enable_static_data = false;

    MemoryByteWriter byte_writer;

    FBOMWriter serializer { config };

    if (FBOMResult err = array.Visit(&serializer, &byte_writer)) {
        AssertThrowMsg(false, "Failed to serialize array: %s", err.message.Data());
    }

    FBOMData value = FBOMData(FBOMArrayType(), std::move(byte_writer.GetBuffer()));
    AssertThrowMsg(value.IsArray(), "Expected value to be array: Got type: %s", value.GetType().ToString().Data());

    return value;
}

SizeType FBOMData::ReadBytes(SizeType n, void *out) const
{
    if (!type.IsUnbounded()) {
        AssertThrowMsg(n <= bytes.Size(), "Attempt to read past max size of object");
    }

    SizeType to_read = MathUtil::Min(n, bytes.Size());
    Memory::MemCpy(out, bytes.Data(), to_read);
    return to_read;
}

ByteBuffer FBOMData::ReadBytes() const
{
    return bytes;
}

ByteBuffer FBOMData::ReadBytes(SizeType n) const
{
    if (!type.IsUnbounded()) {
        AssertThrowMsg(n <= bytes.Size(), "Attempt to read past max size of object");
    }

    SizeType to_read = MathUtil::Min(n, bytes.Size());

    return ByteBuffer(to_read, bytes.Data());
}

void FBOMData::SetBytes(const ByteBuffer &byte_buffer)
{
    if (!type.IsUnbounded()) {
        AssertThrowMsg(byte_buffer.Size() <= type.size, "Attempt to insert data past size max size of object (%llu > %llu)", byte_buffer.Size(), type.size);
    }

    bytes = byte_buffer;
}

void FBOMData::SetBytes(SizeType count, const void *data)
{
    if (!type.IsUnbounded()) {
        AssertThrowMsg(count <= type.size, "Attempt to insert data past size max size of object (%llu > %llu)", count, type.size);
    }

    bytes.SetSize(count);
    bytes.SetData(count, data);
}

FBOMData FBOMData::FromJSON(const json::JSONValue &json_value)
{
    if (json_value.IsNumber()) {
        const bool is_integer = MathUtil::Floor<double, double>(json_value.AsNumber()) == json_value.AsNumber();

        if (is_integer) {
            return FBOMData::FromInt64(int64(json_value.AsNumber()));
        } else {
            return FBOMData::FromDouble(json_value.AsNumber());
        }
    }

    if (json_value.IsString()) {
        return FBOMData::FromString(json_value.AsString().ToUTF8());
    }

    if (json_value.IsBool()) {
        return FBOMData::FromBool(json_value.AsBool());
    }

    if (json_value.IsArray()) {
        FBOMArray array { fbom::FBOMUnset() };

        const json::JSONArray &json_array = json_value.AsArray();

        if (json_array.Any()) {
            Array<FBOMData> elements;
            elements.Reserve(json_array.Size());

            for (const json::JSONValue &element : json_array) {
                elements.EmplaceBack(FBOMData::FromJSON(element));
            }

            array = fbom::FBOMArray(elements[0].GetType(), std::move(elements));
        }

        return FBOMData::FromArray(std::move(array));
    }

    if (json_value.IsObject()) {
        FBOMObject object;

        const json::JSONObject &json_object = json_value.AsObject();

        if (json_object.Any()) {
            for (const Pair<json::JSONString, json::JSONValue> &pair : json_object) {
                object.SetProperty(ANSIString(pair.first), FBOMData::FromJSON(pair.second));
            }
        }

        return FBOMData::FromObject(std::move(object));
    }

    return FBOMData();
}

FBOMData::FBOMData(const json::JSONValue &json_value)
    : FBOMData(FromJSON(json_value))
{
}

FBOMResult FBOMData::Visit(UniqueID id, FBOMWriter *writer, ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes) const
{
    return writer->Write(out, *this, id, attributes);
}

String FBOMData::ToString(bool deep) const
{
    std::stringstream stream;
    stream << "FBOM[";
    stream << "type: " << type.name << ", ";
    stream << "size: " << String::ToString(bytes.Size()) << ", ";
    stream << "data: { ";

    if (deep) {
        for (SizeType i = 0; i < bytes.Size(); i++) {
            stream << std::hex << int(bytes[i]) << " ";
        }
    } else {
        stream << bytes.Size();
    }

    stream << " } ";

    //if (next != nullptr) {
     //   stream << "<" << next->GetType().name << ">";
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

    hc.Add(bytes.Size());
    hc.Add(type.GetHashCode());
    hc.Add(bytes.GetHashCode());

    return hc;
}

} // namespace hyperion::fbom
