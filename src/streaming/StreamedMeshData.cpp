/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamedMeshData.hpp>

#include <asset/serialization/fbom/FBOMMarshaler.hpp>
#include <asset/serialization/fbom/FBOM.hpp>

#include <asset/BufferedByteReader.hpp>
#include <asset/ByteWriter.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Streaming);

RC<StreamedMeshData> StreamedMeshData::FromMeshData(MeshData mesh_data)
{
    return RC<StreamedMeshData>::Construct(std::move(mesh_data));
}

StreamedMeshData::StreamedMeshData()
    : m_streamed_data(RC<NullStreamedData>(new NullStreamedData())),
      m_num_vertices(0),
      m_num_indices(0)
{
}

StreamedMeshData::StreamedMeshData(MeshData &&mesh_data)
    : m_streamed_data(nullptr),
      m_num_vertices(mesh_data.vertices.Size()),
      m_num_indices(mesh_data.indices.Size())
{
    MemoryByteWriter writer;

    fbom::FBOMWriter serializer;
    AssertThrow(serializer.Append(mesh_data).IsOK());
    AssertThrow(serializer.Emit(&writer).IsOK());

    m_streamed_data.Reset(new MemoryStreamedData(writer.GetBuffer()));

    m_mesh_data = std::move(mesh_data);
}

bool StreamedMeshData::IsNull() const
{
    return m_streamed_data == nullptr
        || m_streamed_data->IsNull();
}

bool StreamedMeshData::IsInMemory() const
{
    return m_streamed_data != nullptr
        && m_streamed_data->IsInMemory()
        && m_mesh_data.HasValue();
}

void StreamedMeshData::Unpage_Internal()
{
    m_mesh_data.Unset();

    if (!m_streamed_data) {
        return;
    }

    m_streamed_data->Unpage();
}

const ByteBuffer &StreamedMeshData::Load_Internal() const
{
    AssertThrow(m_streamed_data != nullptr);

    const ByteBuffer &byte_buffer = m_streamed_data->Load();

    if (!m_mesh_data.HasValue()) {
        LoadMeshData(byte_buffer);
    }

    return byte_buffer;
}

void StreamedMeshData::LoadMeshData(const ByteBuffer &byte_buffer) const
{
    m_mesh_data.Unset();

    BufferedReader reader(RC<BufferedReaderSource>(new MemoryBufferedReaderSource(byte_buffer.ToByteView())));

    if (!reader.IsOpen()) {
        return;
    }

    fbom::FBOMReader deserializer(fbom::FBOMConfig { });
    fbom::FBOMDeserializedObject object;

    if (fbom::FBOMResult err = deserializer.Deserialize(reader, object)) {
        HYP_LOG(Streaming, LogLevel::WARNING, "StreamedMeshData: Error deserializing mesh data: {}", err.message);
        return;
    }

    m_mesh_data = *object.Get<MeshData>();

    if (m_mesh_data->vertices.Size() != m_num_vertices) {
        HYP_LOG(Streaming, LogLevel::WARNING, "StreamedMeshData: Vertex count mismatch! Expected {} vertices, but loaded data has {} vertices", m_num_vertices, m_mesh_data->vertices.Size());
    }

    if (m_mesh_data->indices.Size() != m_num_indices) {
        HYP_LOG(Streaming, LogLevel::WARNING, "StreamedMeshData: Index count mismatch! Expected {} indices, but loaded data has {} indices", m_num_indices, m_mesh_data->indices.Size());
    }
}

const MeshData &StreamedMeshData::GetMeshData() const
{
    static const MeshData default_value { };

    if (!m_mesh_data.HasValue()) {
        return default_value;
    }

    return m_mesh_data.Get();
}

} // namespace hyperion