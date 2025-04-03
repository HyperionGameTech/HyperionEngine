/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamedMeshData.hpp>

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

StreamedMeshData::StreamedMeshData(StreamedDataState initial_state, MeshData mesh_data, ResourceHandle &out_resource_handle)
    : StreamedData(initial_state, out_resource_handle),
      m_streamed_data(nullptr),
      m_num_vertices(mesh_data.vertices.Size()),
      m_num_indices(mesh_data.indices.Size())
{
    switch (initial_state) {
    case StreamedDataState::NONE:
        m_streamed_data = MakeRefCountedPtr<NullStreamedData>();

        break;
    case StreamedDataState::LOADED: // fallthrough
    case StreamedDataState::UNPAGED:
        m_mesh_data.Set(std::move(mesh_data));

        m_streamed_data = MakeRefCountedPtr<MemoryStreamedData>(m_mesh_data->GetHashCode(), [this](HashCode hc, ByteBuffer &out) -> bool
        {
            if (!m_mesh_data.HasValue()) {
                HYP_LOG(Streaming, Error, "StreamedMeshData: Mesh data is not set when attempting to load from memory!");

                return false;
            }

            MemoryByteWriter writer;

            fbom::FBOMWriter serializer { fbom::FBOMWriterConfig { } };
            
            if (fbom::FBOMResult err = serializer.Append(*m_mesh_data)) {
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

StreamedMeshData::StreamedMeshData()
    : StreamedData(),
      m_streamed_data(MakeRefCountedPtr<NullStreamedData>())
{
}

StreamedMeshData::StreamedMeshData(const MeshData &mesh_data, ResourceHandle &out_resource_handle)
    : StreamedMeshData(StreamedDataState::LOADED, mesh_data, out_resource_handle)
{
}

StreamedMeshData::StreamedMeshData(MeshData &&mesh_data, ResourceHandle &out_resource_handle)
    : StreamedMeshData(StreamedDataState::LOADED, std::move(mesh_data), out_resource_handle)
{
}

StreamedMeshData::~StreamedMeshData()
{
    if (m_streamed_data != nullptr) {
        m_streamed_data->WaitForFinalization();
        m_streamed_data.Reset();
    }
}

bool StreamedMeshData::IsInMemory_Internal() const
{
    return m_mesh_data.HasValue();
}

void StreamedMeshData::Unpage_Internal()
{
    m_mesh_data.Unset();

    if (!m_streamed_data) {
        return;
    }

    m_streamed_data->Unpage();
}

void StreamedMeshData::Load_Internal() const
{
    AssertThrow(m_streamed_data != nullptr);
    m_streamed_data->Load();

    if (!m_mesh_data.HasValue()) {
        LoadMeshData(m_streamed_data->GetByteBuffer());
    }
}

void StreamedMeshData::LoadMeshData(const ByteBuffer &byte_buffer) const
{
    AssertThrow(byte_buffer.Size() >= 3);
    AssertThrow(byte_buffer.Data()[0] == 'H');
    AssertThrow(byte_buffer.Data()[1] == 'Y');
    AssertThrow(byte_buffer.Data()[2] == 'P');

    m_mesh_data.Unset();

    BufferedReader reader(MakeRefCountedPtr<MemoryBufferedReaderSource>(byte_buffer.ToByteView()));

    if (!reader.IsOpen()) {
        return;
    }
    
    HypData value;

    fbom::FBOMReader deserializer { fbom::FBOMReaderConfig { } };
    fbom::FBOMLoadContext context;

    if (fbom::FBOMResult err = deserializer.Deserialize(context, reader, value)) {
        HYP_LOG(Streaming, Warning, "StreamedMeshData: Error deserializing mesh data: {}", err.message);
        return;
    }

    m_mesh_data = value.Get<MeshData>();

    if (m_mesh_data->vertices.Size() != m_num_vertices) {
        HYP_LOG(Streaming, Warning, "StreamedMeshData: Vertex count mismatch! Expected {} vertices, but loaded data has {} vertices", m_num_vertices, m_mesh_data->vertices.Size());
    }

    if (m_mesh_data->indices.Size() != m_num_indices) {
        HYP_LOG(Streaming, Warning, "StreamedMeshData: Index count mismatch! Expected {} indices, but loaded data has {} indices", m_num_indices, m_mesh_data->indices.Size());
    }
}

const MeshData &StreamedMeshData::GetMeshData() const
{
    WaitForTaskCompletion();

    if (!m_mesh_data.HasValue()) {
        static const MeshData default_value { };
    
        return default_value;
    }

    return m_mesh_data.Get();
}

} // namespace hyperion