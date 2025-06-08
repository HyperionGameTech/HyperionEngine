/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamedTextureData.hpp>

#include <core/serialization/fbom/FBOMMarshaler.hpp>
#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>
#include <core/serialization/fbom/FBOMLoadContext.hpp>

#include <core/io/ByteWriter.hpp>
#include <core/io/BufferedByteReader.hpp>

#include <core/object/HypData.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Streaming);

StreamedTextureData::StreamedTextureData(StreamedDataState initial_state, TextureData texture_data, ResourceHandle& out_resource_handle)
    : StreamedDataBase(initial_state, out_resource_handle),
      m_texture_desc(texture_data.desc),
      m_buffer_size(texture_data.buffer.Size())
{
    switch (initial_state)
    {
    case StreamedDataState::NONE:
        m_streamed_data.EmplaceAs<NullStreamedData>();

        break;
    case StreamedDataState::LOADED: // fallthrough
    case StreamedDataState::UNPAGED:
        m_texture_data.Set(std::move(texture_data));

        m_streamed_data.EmplaceAs<MemoryStreamedData>(m_texture_data->GetHashCode(), [this](HashCode hc, ByteBuffer& out) -> bool
            {
                if (!m_texture_data)
                {
                    HYP_LOG(Streaming, Error, "Texture data is unset when trying to load from memory");

                    return false;
                }

                MemoryByteWriter writer;

                FBOMWriter serializer { FBOMWriterConfig {} };

                if (FBOMResult err = serializer.Append(*m_texture_data))
                {
                    HYP_LOG(Streaming, Error, "Failed to write streamed data: {}", err.message);

                    return false;
                }

                if (FBOMResult err = serializer.Emit(&writer))
                {
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
    : StreamedDataBase(),
      m_streamed_data(MakeRefCountedPtr<NullStreamedData>())
{
}

StreamedTextureData::StreamedTextureData(const TextureData& texture_data, ResourceHandle& out_resource_handle)
    : StreamedTextureData(StreamedDataState::LOADED, texture_data, out_resource_handle)
{
}

StreamedTextureData::StreamedTextureData(TextureData&& texture_data, ResourceHandle& out_resource_handle)
    : StreamedTextureData(StreamedDataState::LOADED, std::move(texture_data), out_resource_handle)
{
}

bool StreamedTextureData::IsInMemory_Internal() const
{
    return m_texture_data.HasValue();
}

void StreamedTextureData::Unpage_Internal()
{
    if (m_streamed_data)
    {
        m_streamed_data->Unpage();
    }

    m_texture_data.Unset();
}

void StreamedTextureData::Load_Internal() const
{
    AssertThrow(m_streamed_data != nullptr);
    m_streamed_data->Load();

    if (!m_texture_data.HasValue())
    {
        LoadTextureData(m_streamed_data->GetByteBuffer());
    }
}

void StreamedTextureData::LoadTextureData(const ByteBuffer& byte_buffer) const
{
    m_texture_data.Unset();

    MemoryBufferedReaderSource source { byte_buffer.ToByteView() };
    BufferedReader reader { &source };

    if (!reader.IsOpen())
    {
        return;
    }

    HypData value;

    FBOMReader deserializer { FBOMReaderConfig {} };
    FBOMLoadContext context;

    if (FBOMResult err = deserializer.Deserialize(context, reader, value))
    {
        HYP_LOG(Streaming, Warning, "StreamedTextureData: Error deserializing texture data for StreamedTextureData with hash: {}\tError: {}", GetDataHashCode().Value(), err.message);
        return;
    }

    m_texture_data = value.Get<TextureData>();

    m_texture_desc = m_texture_data->desc;
    m_buffer_size = m_texture_data->buffer.Size();
}

const TextureData& StreamedTextureData::GetTextureData() const
{
    WaitForTaskCompletion();

    if (!m_texture_data.HasValue())
    {
        static const TextureData default_value {};

        return default_value;
    }

    return m_texture_data.Get();
}

void StreamedTextureData::SetTextureData(TextureData&& texture_data)
{
    Execute([this, texture_data = std::move(texture_data)]()
        {
            m_texture_desc = texture_data.desc;
            m_buffer_size = texture_data.buffer.Size();

            m_texture_data.Set(std::move(texture_data));
        });
}

const TextureDesc& StreamedTextureData::GetTextureDesc() const
{
    WaitForTaskCompletion();

    if (!m_texture_data.HasValue())
    {
        static const TextureDesc default_value {};

        return default_value;
    }

    return m_texture_data->desc;
}

void StreamedTextureData::SetTextureDesc(const TextureDesc& texture_desc)
{
    Execute([this, texture_desc]()
        {
            m_texture_desc = texture_desc;

            if (m_texture_data.HasValue())
            {
                m_texture_data->desc = texture_desc;
            }
        });
}

} // namespace hyperion