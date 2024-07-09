/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamedTextureData.hpp>

#include <asset/serialization/fbom/FBOMMarshaler.hpp>
#include <asset/serialization/fbom/FBOM.hpp>

#include <asset/BufferedByteReader.hpp>
#include <asset/ByteWriter.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Streaming);

RC<StreamedTextureData> StreamedTextureData::FromTextureData(TextureData texture_data)
{
    return RC<StreamedTextureData>::Construct(std::move(texture_data));
}

StreamedTextureData::StreamedTextureData()
    : StreamedData(StreamedDataState::NONE),
      m_streamed_data(RC<NullStreamedData>(new NullStreamedData())),
      m_buffer_size(0)
{
}

StreamedTextureData::StreamedTextureData(const TextureData &texture_data)
    : StreamedData(StreamedDataState::LOADED),
      m_streamed_data(nullptr),
      m_texture_desc(texture_data.desc),
      m_buffer_size(texture_data.buffer.Size())
{
    MemoryByteWriter writer;

    fbom::FBOMWriter serializer;

    if (fbom::FBOMResult err = serializer.Append(texture_data)) {
        HYP_FAIL("Failed to write streamed data: %s", *err.message);
    }

    if (fbom::FBOMResult err = serializer.Emit(&writer)) {
        HYP_FAIL("Failed to write streamed data: %s", *err.message);
    }

    // Do not keep in memory, we already have what we want - but we need to calculate the hash
    m_streamed_data.Reset(new MemoryStreamedData(writer.GetBuffer().ToByteView()));

    m_texture_data.Set(texture_data);
}

StreamedTextureData::StreamedTextureData(TextureData &&texture_data)
    : StreamedData(StreamedDataState::LOADED),
      m_streamed_data(nullptr),
      m_texture_desc(texture_data.desc),
      m_buffer_size(texture_data.buffer.Size())
{
    MemoryByteWriter writer;

    fbom::FBOMWriter serializer;
    
    if (fbom::FBOMResult err = serializer.Append(texture_data)) {
        HYP_FAIL("Failed to write streamed data: %s", *err.message);
    }

    if (fbom::FBOMResult err = serializer.Emit(&writer)) {
        HYP_FAIL("Failed to write streamed data: %s", *err.message);
    }

    // Do not keep in memory, we already have what we want - but we need to calculate the hash
    m_streamed_data.Reset(new MemoryStreamedData(writer.GetBuffer()));

    m_texture_data.Set(texture_data);
}

bool StreamedTextureData::IsNull() const
{
    return m_streamed_data == nullptr
        || m_streamed_data->IsNull();
}

bool StreamedTextureData::IsInMemory() const
{
    return m_texture_data.HasValue();
}

void StreamedTextureData::Unpage_Internal()
{
    m_texture_data.Unset();

    if (!m_streamed_data) {
        return;
    }

    m_streamed_data->Unpage();
}

const ByteBuffer &StreamedTextureData::Load_Internal() const
{
    AssertThrow(m_streamed_data != nullptr);

    const ByteBuffer &byte_buffer = m_streamed_data->Load();

    if (!m_texture_data.HasValue()) {
        LoadTextureData(byte_buffer);
    }

    return byte_buffer;
}

void StreamedTextureData::LoadTextureData(const ByteBuffer &byte_buffer) const
{
    m_texture_data.Unset();

    BufferedReader reader(RC<BufferedReaderSource>(new MemoryBufferedReaderSource(byte_buffer.ToByteView())));

    if (!reader.IsOpen()) {
        return;
    }

    fbom::FBOMReader deserializer(fbom::FBOMConfig { });
    fbom::FBOMDeserializedObject object;

    if (fbom::FBOMResult err = deserializer.Deserialize(reader, object)) {
        HYP_LOG(Streaming, LogLevel::WARNING, "StreamedTextureData: Error deserializing texture data: {}", err.message);
        return;
    }

    m_texture_data = object.Get<TextureData>();
    
    m_texture_desc = m_texture_data->desc;
    m_buffer_size = m_texture_data->buffer.Size();
}

const TextureData &StreamedTextureData::GetTextureData() const
{
    static const TextureData default_value { };

    if (!m_texture_data.HasValue()) {
        return default_value;
    }

    return m_texture_data.Get();
}

} // namespace hyperion