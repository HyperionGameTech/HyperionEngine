#include <streaming/StreamedMeshData.hpp>

#include <asset/serialization/Serialization.hpp>
#include <asset/serialization/fbom/marshals/MeshDataMarshal.hpp>
#include <asset/BufferedByteReader.hpp>
#include <asset/ByteWriter.hpp>

#include <Constants.hpp>

namespace hyperion::v2 {

RC<StreamedMeshData> StreamedMeshData::FromMeshData(const MeshData &mesh_data)
{
#if 0
    MemoryByteWriter writer;

    writer.Write<uint64>(engine_binary_magic_number);

    writer.Write<uint32>(uint32(mesh_data.vertices.Size()));
    writer.Write(mesh_data.vertices.Data(), mesh_data.vertices.Size() * sizeof(Vertex));

    writer.Write<uint32>(uint32(mesh_data.indices.Size()));
    writer.Write(mesh_data.indices.Data(), mesh_data.indices.Size() * sizeof(uint32));

    auto rc = RC<StreamedMeshData>::Construct(RC<StreamedData>(new MemoryStreamedData(writer.GetBuffer())));
    rc->Load();
    const auto &m = rc->GetMeshData();

    AssertThrowMsg(m.vertices.Size() == mesh_data.vertices.Size(),
        "StreamedMeshData: Vertex count mismatch, expected %u, got %u\t%llu\n",
        mesh_data.vertices.Size(),
        m.vertices.Size(),
        writer.GetBuffer().Size()
    );

    return rc;
#endif

#if 1
    MemoryByteWriter writer;

    fbom::FBOMWriter serializer;
    serializer.Append(mesh_data);
    serializer.Emit(&writer);

    return RC<StreamedMeshData>::Construct(RC<StreamedData>(new MemoryStreamedData(writer.GetBuffer())));
#endif
}

StreamedMeshData::StreamedMeshData()
    : m_streamed_data(RC<NullStreamedData>(new NullStreamedData())),
      m_mesh_data({ }),
      m_mesh_data_loaded(false)
{
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

    uint32 num_vertices = 0;
    uint32 num_indices = 0;

    BufferedReader reader(RC<BufferedReaderSource>(new MemoryBufferedReaderSource(byte_buffer)));

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

#if 0
    // read magic number
    uint64 magic = 0;
    reader.Read<uint64>(&magic);

    if (magic == engine_binary_magic_number) {
        reader.Read<uint32>(&num_vertices);
        m_mesh_data.vertices.Resize(num_vertices);
        reader.Read(m_mesh_data.vertices.Data(), num_vertices * sizeof(Vertex));

        reader.Read<uint32>(&num_indices);
        m_mesh_data.indices.Resize(num_indices);
        reader.Read(m_mesh_data.indices.Data(), num_indices * sizeof(uint32));
    } else {
        DebugLog(
            LogType::Warn,
            "StreamedMeshData: Magic number mismatch, expected %llu, got %llu\n",
            engine_binary_magic_number,
            magic
        );
    }
#endif

    m_mesh_data_loaded = true;
}

const MeshData &StreamedMeshData::GetMeshData() const
{
    if (!IsInMemory()) {
        (void)Load();
    }

    return m_mesh_data;
}

} // namespace hyperion::v2