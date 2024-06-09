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
    : bytes(type.size),
      type(type)
{
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
    BufferedReader byte_reader(RC<BufferedReaderSource>(new MemoryBufferedReaderSource(bytes.ToByteView())));
    
    FBOMReader deserializer(fbom::FBOMConfig { });
    return deserializer.Deserialize(byte_reader, out_object);
}

FBOMData FBOMData::FromObject(const FBOMObject &object)
{
    MemoryByteWriter byte_writer;

    FBOMWriter serializer;
    serializer.Append(object);

    const FBOMResult serialize_result = serializer.Emit(&byte_writer);
    AssertThrowMsg(serialize_result == FBOMResult::FBOM_OK, "Failed to serialize object: %s", *serialize_result.message);

    return FBOMData::FromByteBuffer(byte_writer.GetBuffer());
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

} // namespace hyperion::fbom
