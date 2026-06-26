/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainMeshBuilder.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Vertex.hpp>

#include <Asset/AssetRegistry.hpp>

namespace Hyperion {

#pragma region Helpers

static Array<SimpleVertex> BuildVertices(uint32 cellSize, float scale)
{
    Array<SimpleVertex> vertices;
    vertices.Resize(cellSize * cellSize);

    for (uint32 z = 0; z < cellSize; z++)
    {
        for (uint32 x = 0; x < cellSize; x++)
        {
            const uint32 i = z * cellSize + x;

            const Vec3f position = Vec3f { float(x), 0.0f, float(z) } * scale;
            const Vec2f texcoord(float(x) / float(cellSize), float(z) / float(cellSize));

            vertices[i] = SimpleVertex { position, Vec3f::Zero(), texcoord };
        }
    }

    return vertices;
}

static Array<uint32> BuildIndices(uint32 cellSize)
{
    Array<uint32> indices;
    indices.Resize(size_t(6 * (cellSize - 1) * (cellSize - 1)));

    uint32 pitch = uint32(cellSize);
    uint32 row = 0;

    uint32 i0 = row;
    uint32 i1 = row + 1;
    uint32 i2 = pitch + i1;
    uint32 i3 = pitch + row;

    uint32 i = 0;

    for (uint32 z = 0; z < cellSize - 1; z++)
    {
        for (uint32 x = 0; x < cellSize - 1; x++)
        {
            indices[i++] = i0;
            indices[i++] = i2;
            indices[i++] = i3;
            indices[i++] = i0;
            indices[i++] = i1;
            indices[i++] = i2;

            i0++;
            i1++;
            i2++;
            i3++;
        }

        row += pitch;

        i0 = row;
        i1 = row + 1;
        i2 = pitch + i1;
        i3 = pitch + row;
    }

    return indices;
}

#pragma endregion Helpers

#pragma region TerrainMeshBuilder

TerrainMeshBuilder::TerrainMeshBuilder(uint32 cellSize)
    : m_cellSize(cellSize)
{
}

TerrainMeshBuilder::~TerrainMeshBuilder() = default;

const Handle<Mesh>& TerrainMeshBuilder::GetMesh()
{
    if (m_mesh.IsValid())
    {
        return m_mesh;
    }

    Array<SimpleVertex> vertices = BuildVertices(m_cellSize, 1.0f);
    Array<uint32> indices = BuildIndices(m_cellSize);

    MeshDesc meshDesc;
    meshDesc.meshAttributes.inputLayout = { VT_Simple };
    meshDesc.numIndices = uint32(indices.Size());
    meshDesc.numVertices = uint32(vertices.Size());

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetName(NAME("TerrainChunkMesh"));

    VertexArrayView vertexArrayView {};
    vertexArrayView.floatData = reinterpret_cast<const float*>(vertices.Data());
    vertexArrayView.vertexCount = vertices.Size();
    vertexArrayView.layoutDesc = meshDesc.meshAttributes.inputLayout;

    mesh->SetMeshData(meshDesc, vertexArrayView, indices.ToByteView());

    GetCurrentAssetRegistry()->PutAsset(m_mesh);

    return mesh;
}

#pragma endregion TerrainMeshBuilder

} // namespace Hyperion
