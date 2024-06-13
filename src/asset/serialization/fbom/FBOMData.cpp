/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOMData.hpp>
#include <asset/serialization/fbom/FBOMObject.hpp>
#include <asset/serialization/fbom/FBOM.hpp>

#include <asset/BufferedByteReader.hpp>
#include <asset/ByteWriter.hpp>

#include <core/memory/Memory.hpp>

namespace hyperion::fbom {

const FBOMData FBOMData::UNSET = FBOMData();

FBOMData::FBOMData()
    : type(FBOMUnset())
{
}

FBOMData::FBOMData(const FBOMType &type)
    : type(type)
{
    if (!type.IsUnbouned()) {
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
      bytes(other.bytes)
{
}

FBOMData &FBOMData::operator=(const FBOMData &other)
{
    type = other.type;
    bytes = other.bytes;

    return *this;
}

FBOMData::FBOMData(FBOMData &&other) noexcept
    : bytes(std::move(other.bytes)),
      type(std::move(other.type))
{
    other.type = FBOMUnset();
}

FBOMData &FBOMData::operator=(FBOMData &&other) noexcept
{
    bytes = std::move(other.bytes);
    type = std::move(other.type);

    other.type = FBOMUnset();

    return *this;
}

FBOMData::~FBOMData() = default;

FBOMResult FBOMData::ReadObject(FBOMObject &out_object) const
{
    if (!IsObject()) {
        return { FBOMResult::FBOM_ERR, "Not an object" };
    }

    BufferedReader byte_reader(RC<BufferedReaderSource>(new MemoryBufferedReaderSource(bytes.ToByteView())));
    
    FBOMReader deserializer(fbom::FBOMConfig { });
    // return deserializer.Deserialize(byte_reader, out_object);
    return deserializer.ReadObject(&byte_reader, out_object, nullptr);
}

FBOMData FBOMData::FromObject(const FBOMObject &object)
{
    MemoryByteWriter byte_writer;
    
    FBOMWriter serializer;
    AssertThrow(object.Visit(&serializer, &byte_writer) == FBOMResult::FBOM_OK);

    auto value = FBOMData(object.GetType(), std::move(byte_writer.GetBuffer()));
    AssertThrowMsg(value.IsObject(), "Expected value to be object: Got type: %s", value.GetType().ToString().Data());
    return value;
}

SizeType FBOMData::ReadBytes(SizeType n, void *out) const
{
    if (!type.IsUnbouned()) {
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
    if (!type.IsUnbouned()) {
        AssertThrowMsg(n <= bytes.Size(), "Attempt to read past max size of object");
    }

    SizeType to_read = MathUtil::Min(n, bytes.Size());

    return ByteBuffer(to_read, bytes.Data());
}

void FBOMData::SetBytes(const ByteBuffer &byte_buffer)
{
    if (!type.IsUnbouned()) {
        AssertThrowMsg(byte_buffer.Size() <= type.size, "Attempt to insert data past size max size of object (%llu > %llu)", byte_buffer.Size(), type.size);
    }

    bytes = byte_buffer;
}

void FBOMData::SetBytes(SizeType count, const void *data)
{
    if (!type.IsUnbouned()) {
        AssertThrowMsg(count <= type.size, "Attempt to insert data past size max size of object (%llu > %llu)", count, type.size);
    }

    bytes.SetSize(count);
    bytes.SetData(count, data);
}

FBOMResult FBOMData::Visit(UniqueID id, FBOMWriter *writer, ByteWriter *out) const
{
    return writer->Write(out, *this, id);
}

} // namespace hyperion::fbom
