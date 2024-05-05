/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamedMeshData.hpp>

#include <asset/serialization/fbom/marshals/MeshDataMarshal.hpp>
#include <asset/BufferedByteReader.hpp>
#include <asset/ByteWriter.hpp>

namespace hyperion {

RC<StreamedMeshData> StreamedMeshData::FromMeshData(MeshData mesh_data)
{
    return RC<StreamedMeshData>::Construct(std::move(mesh_data));
}

StreamedMeshData::StreamedMeshData()
    : m_streamed_data(RC<NullStreamedData>(new NullStreamedData())),
      m_mesh_data({ }),
      m_mesh_data_loaded(false)
{
}

StreamedMeshData::StreamedMeshData(MeshData &&mesh_data)
{
    MemoryByteWriter writer;

    fbom::FBOMWriter serializer;
    serializer.Append(mesh_data);
    serializer.Emit(&writer);

    m_streamed_data.Reset(new MemoryStreamedData(writer.GetBuffer()));

    m_mesh_data = std::move(mesh_data);
    m_mesh_data_loaded = true;
}

StreamedMeshData::StreamedMeshData(RC<StreamedData> streamed_data)
    : m_streamed_data(std::move(streamed_data)),
      m_mesh_data({ }),
      m_mesh_data_loaded(false)
{
    AssertThrow(m_streamed_data != nullptr);
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
        && m_mesh_data_loaded;
}

void StreamedMeshData::Unpage()
{
    m_mesh_data = { };
    m_mesh_data_loaded = false;

    if (!m_streamed_data) {
        return;
    }

    m_streamed_data->Unpage();
}

const ByteBuffer &StreamedMeshData::Load() const
{
    AssertThrow(m_streamed_data != nullptr);

    const ByteBuffer &byte_buffer = m_streamed_data->Load();
    LoadMeshData(byte_buffer);

    return byte_buffer;
}

void StreamedMeshData::LoadMeshData(const ByteBuffer &byte_buffer) const
{
    m_mesh_data = { };
    m_mesh_data_loaded = false;

    BufferedReader reader(RC<BufferedReaderSource>(new MemoryBufferedReaderSource(byte_buffer.ToByteView())));

    if (!reader.IsOpen()) {
        return;
    }

    fbom::FBOMReader deserializer(fbom::FBOMConfig { });
    fbom::FBOMDeserializedObject object;

    if (auto err = deserializer.Deserialize(reader, object)) {
        DebugLog(LogType::Warn, "StreamedMeshData: Error deserializing mesh data: %s\n", err.message);
        return;
    }

    m_mesh_data = std::move(*object.Get<MeshData>());

    m_mesh_data_loaded = true;
}

const MeshData &StreamedMeshData::GetMeshData() const
{
    if (!IsInMemory()) {
        (void)Load();
    }

    return m_mesh_data;
}

} // namespace hyperion