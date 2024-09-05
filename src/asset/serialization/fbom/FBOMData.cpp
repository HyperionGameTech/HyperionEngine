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

#include <sstream>

namespace hyperion::fbom {

FBOMData::FBOMData()
    : type(FBOMUnset())
{
}

FBOMData::FBOMData(const FBOMType &type)
    : type(type)
{
    if (!type.IsUnbounded()) {
        bytes.SetSize(type.size);
    }
}

FBOMData::FBOMData(const FBOMType &type, ByteBuffer &&byte_buffer)
    : bytes(std::move(byte_buffer)),
      type(type)
{
}

FBOMData::FBOMData(const FBOMData &other)
    : type(other.type),
      bytes(other.bytes),
      m_deserialized_object(other.m_deserialized_object)
{
}

FBOMData &FBOMData::operator=(const FBOMData &other)
{
    type = other.type;
    bytes = other.bytes;
    m_deserialized_object = other.m_deserialized_object;

    return *this;
}

FBOMData::FBOMData(FBOMData &&other) noexcept
    : bytes(std::move(other.bytes)),
      type(std::move(other.type)),
      m_deserialized_object(std::move(other.m_deserialized_object))
{
    other.type = FBOMUnset();
}

FBOMData &FBOMData::operator=(FBOMData &&other) noexcept
{
    bytes = std::move(other.bytes);
    type = std::move(other.type);
    m_deserialized_object = std::move(other.m_deserialized_object);

    other.type = FBOMUnset();

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

    BufferedReader byte_reader(RC<BufferedReaderSource>(new MemoryBufferedReaderSource(bytes.ToByteView())));
    
    FBOMReader deserializer(fbom::FBOMConfig { });

    // return deserializer.Deserialize(byte_reader, out_object);
    if (FBOMResult err = deserializer.ReadObject(&byte_reader, out_object, nullptr)) {
        return err;
    }

    return { FBOMResult::FBOM_OK };
}

FBOMData FBOMData::FromObject(const FBOMObject &object, bool keep_native_object)
{
    MemoryByteWriter byte_writer;
    FBOMWriter serializer;

    if (FBOMResult err = object.Visit(&serializer, &byte_writer)) {
        AssertThrowMsg(false, "Failed to serialize object: %s", err.message.Data());
    }

    FBOMData value = FBOMData(object.GetType(), std::move(byte_writer.GetBuffer()));
    AssertThrowMsg(value.IsObject(), "Expected value to be object: Got type: %s", value.GetType().ToString().Data());

    if (keep_native_object) {
        value.m_deserialized_object = object.m_deserialized_object;
    }

    return value;
}

FBOMData FBOMData::FromObject(FBOMObject &&object, bool keep_native_object)
{
    MemoryByteWriter byte_writer;
    FBOMWriter serializer;

    if (FBOMResult err = object.Visit(&serializer, &byte_writer)) {
        AssertThrowMsg(false, "Failed to serialize object: %s", err.message.Data());
    }

    FBOMData value = FBOMData(object.GetType(), std::move(byte_writer.GetBuffer()));
    AssertThrowMsg(value.IsObject(), "Expected value to be object: Got type: %s", value.GetType().ToString().Data());

    if (keep_native_object) {
        AssertThrowMsg(object.m_deserialized_object != nullptr, "If keep_native_object is true, expected object to have a deserialized value");

        value.m_deserialized_object = std::move(object.m_deserialized_object);
    }

    return value;
}

FBOMResult FBOMData::ReadArray(FBOMArray &out_array) const
{
    if (!IsArray()) {
        return { FBOMResult::FBOM_ERR, "Not an array" };
    }

    BufferedReader byte_reader(RC<BufferedReaderSource>(new MemoryBufferedReaderSource(bytes.ToByteView())));
    
    FBOMReader deserializer(fbom::FBOMConfig { });
    return deserializer.ReadArray(&byte_reader, out_array);
}

FBOMData FBOMData::FromArray(const FBOMArray &array)
{
    MemoryByteWriter byte_writer;
    FBOMWriter serializer;

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
