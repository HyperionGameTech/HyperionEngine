/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamedTextureData.hpp>

#include <core/serialization/fbom/FBOMMarshaler.hpp>
#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>
#include <core/serialization/fbom/FBOMLoadContext.hpp>

#include <core/io/ByteWriter.hpp>
#include <core/io/BufferedByteReader.hpp>

#include <core/object/HypData.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Streaming);

StreamedTextureData::StreamedTextureData(StreamedDataState initial_state, TextureData texture_data, ResourceHandle &out_resource_handle)
    : StreamedData(initial_state, out_resource_handle),
      m_texture_desc(texture_data.desc),
      m_buffer_size(texture_data.buffer.Size())
{
    switch (initial_state) {
    case StreamedDataState::NONE:
        m_streamed_data.EmplaceAs<NullStreamedData>();

        break;
    case StreamedDataState::LOADED: // fallthrough
    case StreamedDataState::UNPAGED:
        m_texture_data.Set(std::move(texture_data));

        m_streamed_data.EmplaceAs<MemoryStreamedData>(m_texture_data->GetHashCode(), [this](HashCode hc, ByteBuffer &out) -> bool
        {
            if (!m_texture_data) {
                return false;
            }

            MemoryByteWriter writer;

            fbom::FBOMWriter serializer { fbom::FBOMWriterConfig { } };
            
            if (fbom::FBOMResult err = serializer.Append(*m_texture_data)) {
                HYP_LOG(Streaming, Error, "Failed to write streamed data: {}", err.message);

                return false;
            }

            if (fbom::FBOMResult err = serializer.Emit(&writer)) {
                HYP_LOG(Streaming, Error, "Failed to write streamed data: {}", err.message);

                return false;
            }

            out = std::move(writer.GetBuffer());

            return true;
        });

        break;
    default:
        HYP_NOT_IMPLEMENTED_VOID();
    }
}

StreamedTextureData::StreamedTextureData()
    : StreamedData(),
      m_streamed_data(MakeRefCountedPtr<NullStreamedData>())
{
}

StreamedTextureData::StreamedTextureData(const TextureData &texture_data, ResourceHandle &out_resource_handle)
    : StreamedTextureData(StreamedDataState::LOADED, texture_data, out_resource_handle)
{
}

StreamedTextureData::StreamedTextureData(TextureData &&texture_data, ResourceHandle &out_resource_handle)
    : StreamedTextureData(StreamedDataState::LOADED, std::move(texture_data), out_resource_handle)
{
}

bool StreamedTextureData::IsInMemory_Internal() const
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

    BufferedReader reader(MakeRefCountedPtr<MemoryBufferedReaderSource>(byte_buffer.ToByteView()));

    if (!reader.IsOpen()) {
        return;
    }

    HypData value;

    fbom::FBOMReader deserializer { fbom::FBOMReaderConfig { } };
    fbom::FBOMLoadContext context;

    if (fbom::FBOMResult err = deserializer.Deserialize(context, reader, value)) {
        HYP_LOG(Streaming, Warning, "StreamedTextureData: Error deserializing texture data: {}", err.message);
        return;
    }

    m_texture_data = value.Get<TextureData>();
    
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