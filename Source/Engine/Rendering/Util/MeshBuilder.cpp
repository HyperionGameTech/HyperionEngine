/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <Core/Math/Triangle.hpp>
#include <Core/Math/MathUtil.hpp>

#include <Rendering/Mesh.hpp>

#include <Scene/Util/VoxelOctree.hpp>

namespace Hyperion {
namespace MeshBuilder {

static Pair<Array<SimpleVertex>, Array<uint32>> CalculateIndices(const Array<SimpleVertex>& vertices)
{
    Map<SimpleVertex, uint32> indexMap;

    Array<uint32> indices;
    indices.Reserve(vertices.Size());

    /* This will be our resulting buffer with only the vertices we need. */
    Array<SimpleVertex> newVertices;
    newVertices.Reserve(vertices.Size());

    for (const auto& vertex : vertices)
    {
        /* Check if the vertex already exists in our map */
        auto it = indexMap.Find(vertex);

        /* If it does, push to our indices */
        if (it != indexMap.End())
        {
            indices.PushBack(it->second);

            continue;
        }

        const uint32 meshIndex = uint32(newVertices.Size());

        /* The vertex is unique, so we push it. */
        newVertices.PushBack(vertex);
        indices.PushBack(meshIndex);

        indexMap[vertex] = meshIndex;
    }

    return { std::move(newVertices), std::move(indices) };
}

static const FixedArray<SimpleVertex, 4>& GetQuadVertices()
{
    static const FixedArray<SimpleVertex, 4> s_vertices = {
        SimpleVertex { Vec3f { -1.0f, 1.0f, 0.0f }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 0.0f, 0.0f } },
        SimpleVertex { Vec3f { 1.0f, 1.0f, 0.0f }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 1.0f, 0.0f } },
        SimpleVertex { Vec3f { 1.0f, -1.0f, 0.0f }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 1.0f, 1.0f } },
        SimpleVertex { Vec3f { -1.0f, -1.0f, 0.0f }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 0.0f, 1.0f } }
    };

    return s_vertices;
}

static const FixedArray<uint32, 6>& GetQuadIndices()
{
    static const FixedArray<uint32, 6> s_indices = {
        0, 2, 1,
        0, 3, 2
    };

    return s_indices;
}

static const FixedArray<SimpleVertex, 8>& GetDoubleSidedQuadVertices()
{
    static const FixedArray<SimpleVertex, 8> s_vertices = {
        // Front face
        SimpleVertex { Vec3f { -1.0f, 1.0f, 0.0f }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 0.0f, 0.0f } },
        SimpleVertex { Vec3f { 1.0f, 1.0f, 0.0f }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 1.0f, 0.0f } },
        SimpleVertex { Vec3f { 1.0f, -1.0f, 0.0f }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 1.0f, 1.0f } },
        SimpleVertex { Vec3f { -1.0f, -1.0f, 0.0f }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 0.0f, 1.0f } },
        // Back face
        SimpleVertex { Vec3f { 1.0f, -1.0f, 0.0f }, Vec3f { 0.0f, 0.0f, 1.0f }, Vec2f { 0.0f, 1.0f } },
        SimpleVertex { Vec3f { -1.0f, -1.0f, 0.0f }, Vec3f { 0.0f, 0.0f, 1.0f }, Vec2f { 1.0f, 1.0f } },
        SimpleVertex { Vec3f { -1.0f, 1.0f, 0.0f }, Vec3f { 0.0f, 0.0f, 1.0f }, Vec2f { 1.0f, 0.0f } },
        SimpleVertex { Vec3f { 1.0f, 1.0f, 0.0f }, Vec3f { 0.0f, 0.0f, 1.0f }, Vec2f { 0.0f, 0.0f } }
    };

    return s_vertices;
}

static const FixedArray<uint32, 12>& GetDoubleSidedQuadIndices()
{
    static const FixedArray<uint32, 12> s_indices = {
        // Front face
        0, 1, 2,
        0, 2, 3,
        // Back face
        4, 6, 5,
        4, 7, 6
    };

    return s_indices;
}

static const Array<SimpleVertex>& GetCubeVertices()
{
    static const Array<SimpleVertex> s_cubeVertices = {
        // Face 1: -X (Left)
        SimpleVertex { Vec3f { -1.0f, -1.0f, -1.0f }, Vec3f { -1.0f, 0.0f, 0.0f }, Vec2f { 1.0f, 1.0f } },
        SimpleVertex { Vec3f { -1.0f,  1.0f, -1.0f }, Vec3f { -1.0f, 0.0f, 0.0f }, Vec2f { 1.0f, 0.0f } },
        SimpleVertex { Vec3f { -1.0f,  1.0f,  1.0f }, Vec3f { -1.0f, 0.0f, 0.0f }, Vec2f { 0.0f, 0.0f } },

        SimpleVertex { Vec3f { -1.0f,  1.0f,  1.0f }, Vec3f { -1.0f, 0.0f, 0.0f }, Vec2f { 0.0f, 0.0f } },
        SimpleVertex { Vec3f { -1.0f, -1.0f,  1.0f }, Vec3f { -1.0f, 0.0f, 0.0f }, Vec2f { 0.0f, 1.0f } },
        SimpleVertex { Vec3f { -1.0f, -1.0f, -1.0f }, Vec3f { -1.0f, 0.0f, 0.0f }, Vec2f { 1.0f, 1.0f } },

        // Face 2: +Z (Front)
        SimpleVertex { Vec3f { -1.0f, -1.0f,  1.0f }, Vec3f { 0.0f, 0.0f, 1.0f }, Vec2f { 0.0f, 1.0f } },
        SimpleVertex { Vec3f { -1.0f,  1.0f,  1.0f }, Vec3f { 0.0f, 0.0f, 1.0f }, Vec2f { 0.0f, 0.0f } },
        SimpleVertex { Vec3f {  1.0f,  1.0f,  1.0f }, Vec3f { 0.0f, 0.0f, 1.0f }, Vec2f { 1.0f, 0.0f } },

        SimpleVertex { Vec3f {  1.0f,  1.0f,  1.0f }, Vec3f { 0.0f, 0.0f, 1.0f }, Vec2f { 1.0f, 0.0f } },
        SimpleVertex { Vec3f {  1.0f, -1.0f,  1.0f }, Vec3f { 0.0f, 0.0f, 1.0f }, Vec2f { 1.0f, 1.0f } },
        SimpleVertex { Vec3f { -1.0f, -1.0f,  1.0f }, Vec3f { 0.0f, 0.0f, 1.0f }, Vec2f { 0.0f, 1.0f } },

        // Face 3: +X (Right)
        SimpleVertex { Vec3f {  1.0f,  1.0f,  1.0f }, Vec3f { 1.0f, 0.0f, 0.0f }, Vec2f { 0.0f, 0.0f } },
        SimpleVertex { Vec3f {  1.0f,  1.0f, -1.0f }, Vec3f { 1.0f, 0.0f, 0.0f }, Vec2f { 1.0f, 0.0f } },
        SimpleVertex { Vec3f {  1.0f, -1.0f, -1.0f }, Vec3f { 1.0f, 0.0f, 0.0f }, Vec2f { 1.0f, 1.0f } },

        SimpleVertex { Vec3f {  1.0f, -1.0f, -1.0f }, Vec3f { 1.0f, 0.0f, 0.0f }, Vec2f { 1.0f, 1.0f } },
        SimpleVertex { Vec3f {  1.0f, -1.0f,  1.0f }, Vec3f { 1.0f, 0.0f, 0.0f }, Vec2f { 0.0f, 1.0f } },
        SimpleVertex { Vec3f {  1.0f,  1.0f,  1.0f }, Vec3f { 1.0f, 0.0f, 0.0f }, Vec2f { 0.0f, 0.0f } },

        // Face 4: -Z (Back)
        SimpleVertex { Vec3f {  1.0f,  1.0f, -1.0f }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 0.0f, 0.0f } },
        SimpleVertex { Vec3f { -1.0f,  1.0f, -1.0f }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 1.0f, 0.0f } },
        SimpleVertex { Vec3f { -1.0f, -1.0f, -1.0f }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 1.0f, 1.0f } },

        SimpleVertex { Vec3f { -1.0f, -1.0f, -1.0f }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 1.0f, 1.0f } },
        SimpleVertex { Vec3f {  1.0f, -1.0f, -1.0f }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 0.0f, 1.0f } },
        SimpleVertex { Vec3f {  1.0f,  1.0f, -1.0f }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 0.0f, 0.0f } },

        // Face 5: +Y (Top)
        SimpleVertex { Vec3f { -1.0f,  1.0f,  1.0f }, Vec3f { 0.0f, 1.0f, 0.0f }, Vec2f { 0.0f, 0.0f } },
        SimpleVertex { Vec3f { -1.0f,  1.0f, -1.0f }, Vec3f { 0.0f, 1.0f, 0.0f }, Vec2f { 0.0f, 1.0f } },
        SimpleVertex { Vec3f {  1.0f,  1.0f, -1.0f }, Vec3f { 0.0f, 1.0f, 0.0f }, Vec2f { 1.0f, 1.0f } },

        SimpleVertex { Vec3f {  1.0f,  1.0f, -1.0f }, Vec3f { 0.0f, 1.0f, 0.0f }, Vec2f { 1.0f, 1.0f } },
        SimpleVertex { Vec3f {  1.0f,  1.0f,  1.0f }, Vec3f { 0.0f, 1.0f, 0.0f }, Vec2f { 1.0f, 0.0f } },
        SimpleVertex { Vec3f { -1.0f,  1.0f,  1.0f }, Vec3f { 0.0f, 1.0f, 0.0f }, Vec2f { 0.0f, 0.0f } },

        // Face 6: -Y (Bottom)
        SimpleVertex { Vec3f {  1.0f, -1.0f, -1.0f }, Vec3f { 0.0f, -1.0f, 0.0f }, Vec2f { 1.0f, 0.0f } },
        SimpleVertex { Vec3f { -1.0f, -1.0f, -1.0f }, Vec3f { 0.0f, -1.0f, 0.0f }, Vec2f { 0.0f, 0.0f } },
        SimpleVertex { Vec3f { -1.0f, -1.0f,  1.0f }, Vec3f { 0.0f, -1.0f, 0.0f }, Vec2f { 0.0f, 1.0f } },

        SimpleVertex { Vec3f { -1.0f, -1.0f,  1.0f }, Vec3f { 0.0f, -1.0f, 0.0f }, Vec2f { 0.0f, 1.0f } },
        SimpleVertex { Vec3f {  1.0f, -1.0f,  1.0f }, Vec3f { 0.0f, -1.0f, 0.0f }, Vec2f { 1.0f, 1.0f } },
        SimpleVertex { Vec3f {  1.0f, -1.0f, -1.0f }, Vec3f { 0.0f, -1.0f, 0.0f }, Vec2f { 1.0f, 0.0f } }
    };

    return s_cubeVertices;
}

ENGINE_API Handle<Mesh> Quad()
{
    const auto& vertices = GetQuadVertices();
    const auto& indices = GetQuadIndices();

    MeshDesc meshDesc {};
    meshDesc.meshAttributes.inputLayout = { VT_Simple };
    meshDesc.lods[0].numIndices = uint32(indices.Size());
    meshDesc.lods[0].numVertices = uint32(vertices.Size());

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetName(NAME("MeshBuilder_Quad"));

    VertexArrayView vertexArrayView {};
    vertexArrayView.floatData = reinterpret_cast<const float*>(vertices.Data());
    vertexArrayView.layoutDesc = meshDesc.meshAttributes.inputLayout;
    vertexArrayView.vertexCount = vertices.Size();

    const ConstByteView indicesByteView = ConstByteView(
        reinterpret_cast<const ubyte*>(indices.Data()),
        reinterpret_cast<const ubyte*>(indices.Data() + indices.Size()));

    MeshDataView meshData {};
    meshData.vertices[0] = vertexArrayView;
    meshData.indices[0] = indicesByteView;

    mesh->SetMeshData(meshDesc, meshData);

    return mesh;
}

ENGINE_API Handle<Mesh> DoubleSidedQuad()
{
    const auto& vertices = GetDoubleSidedQuadVertices();
    const auto& indices = GetDoubleSidedQuadIndices();

    MeshDesc meshDesc {};
    meshDesc.meshAttributes.inputLayout = { VT_Simple };
    meshDesc.lods[0].numIndices = uint32(indices.Size());
    meshDesc.lods[0].numVertices = uint32(vertices.Size());

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetName(NAME("MeshBuilder_DoubleSidedQuad"));

    VertexArrayView vertexArrayView {};
    vertexArrayView.floatData = reinterpret_cast<const float*>(vertices.Data());
    vertexArrayView.layoutDesc = meshDesc.meshAttributes.inputLayout;
    vertexArrayView.vertexCount = vertices.Size();

    const ConstByteView indicesByteView = ConstByteView(
        reinterpret_cast<const ubyte*>(indices.Data()),
        reinterpret_cast<const ubyte*>(indices.Data() + indices.Size()));

    MeshDataView meshData2 {};
    meshData2.vertices[0] = vertexArrayView;
    meshData2.indices[0] = indicesByteView;

    mesh->SetMeshData(meshDesc, meshData2);

    return mesh;
}

ENGINE_API Handle<Mesh> Cube(bool originOnBottom)
{
    static const auto s_cubeVerticesAndIndices = CalculateIndices(GetCubeVertices());

    MeshDesc meshDesc;
    meshDesc.meshAttributes.inputLayout = { VT_Simple };
    meshDesc.lods[0].numIndices = uint32(s_cubeVerticesAndIndices.second.Size());
    meshDesc.lods[0].numVertices = uint32(s_cubeVerticesAndIndices.first.Size());

    Array<SimpleVertex> vertices = s_cubeVerticesAndIndices.first;
    Array<uint32> indices = s_cubeVerticesAndIndices.second;

#if 0
    // Half-size dimensions for a 1x1x1 cube centered at the origin
    const float l = -0.5f; // left
    const float r =  0.5f; // right
    const float b = -0.5f; // bottom
    const float t =  0.5f; // top
    const float n = -0.5f; // near (-Z)
    const float f =  0.5f; // far (+Z)

    SimpleVertex vertices[] = {
        // Front Face (Facing -Z) - Normals: { 0, 0, -1 }
        SimpleVertex { Vec3f { l, b, n }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 0.0f, 1.0f } }, // 0: Bottom-Left
        SimpleVertex { Vec3f { l, t, n }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 0.0f, 0.0f } }, // 1: Top-Left
        SimpleVertex { Vec3f { r, t, n }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 1.0f, 0.0f } }, // 2: Top-Right
        SimpleVertex { Vec3f { r, b, n }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 1.0f, 1.0f } }, // 3: Bottom-Right

        // Back Face (Facing +Z) - Normals: { 0, 0, 1 }
        SimpleVertex { Vec3f { r, b, f }, Vec3f { 0.0f, 0.0f, 1.0f }, Vec2f { 0.0f, 1.0f } },  // 4: Bottom-Right
        SimpleVertex { Vec3f { r, t, f }, Vec3f { 0.0f, 0.0f, 1.0f }, Vec2f { 0.0f, 0.0f } },  // 5: Top-Right
        SimpleVertex { Vec3f { l, t, f }, Vec3f { 0.0f, 0.0f, 1.0f }, Vec2f { 1.0f, 0.0f } },  // 6: Top-Left
        SimpleVertex { Vec3f { l, b, f }, Vec3f { 0.0f, 0.0f, 1.0f }, Vec2f { 1.0f, 1.0f } },  // 7: Bottom-Left

        // Top Face (Facing +Y) - Normals: { 0, 1, 0 }
        SimpleVertex { Vec3f { l, t, n }, Vec3f { 0.0f, 1.0f, 0.0f }, Vec2f { 0.0f, 1.0f } },  // 8: Bottom-Left
        SimpleVertex { Vec3f { l, t, f }, Vec3f { 0.0f, 1.0f, 0.0f }, Vec2f { 0.0f, 0.0f } },  // 9: Top-Left
        SimpleVertex { Vec3f { r, t, f }, Vec3f { 0.0f, 1.0f, 0.0f }, Vec2f { 1.0f, 0.0f } },  // 10: Top-Right
        SimpleVertex { Vec3f { r, t, n }, Vec3f { 0.0f, 1.0f, 0.0f }, Vec2f { 1.0f, 1.0f } },  // 11: Bottom-Right

        // Bottom Face (Facing -Y) - Normals: { 0, -1, 0 }
        SimpleVertex { Vec3f { l, b, f }, Vec3f { 0.0f, -1.0f, 0.0f }, Vec2f { 0.0f, 1.0f } }, // 12: Bottom-Left
        SimpleVertex { Vec3f { l, b, n }, Vec3f { 0.0f, -1.0f, 0.0f }, Vec2f { 0.0f, 0.0f } }, // 13: Top-Left
        SimpleVertex { Vec3f { r, b, n }, Vec3f { 0.0f, -1.0f, 0.0f }, Vec2f { 1.0f, 0.0f } }, // 14: Top-Right
        SimpleVertex { Vec3f { r, b, f }, Vec3f { 0.0f, -1.0f, 0.0f }, Vec2f { 1.0f, 1.0f } }, // 15: Bottom-Right

        // Left Face (Facing -X) - Normals: { -1, 0, 0 }
        SimpleVertex { Vec3f { l, b, f }, Vec3f { -1.0f, 0.0f, 0.0f }, Vec2f { 0.0f, 1.0f } }, // 16: Bottom-Left
        SimpleVertex { Vec3f { l, t, f }, Vec3f { -1.0f, 0.0f, 0.0f }, Vec2f { 0.0f, 0.0f } }, // 17: Top-Left
        SimpleVertex { Vec3f { l, t, n }, Vec3f { -1.0f, 0.0f, 0.0f }, Vec2f { 1.0f, 0.0f } }, // 18: Top-Right
        SimpleVertex { Vec3f { l, b, n }, Vec3f { -1.0f, 0.0f, 0.0f }, Vec2f { 1.0f, 1.0f } }, // 19: Bottom-Right

        // Right Face (Facing +X) - Normals: { 1, 0, 0 }
        SimpleVertex { Vec3f { r, b, n }, Vec3f { 1.0f, 0.0f, 0.0f }, Vec2f { 0.0f, 1.0f } },  // 20: Bottom-Left
        SimpleVertex { Vec3f { r, t, n }, Vec3f { 1.0f, 0.0f, 0.0f }, Vec2f { 0.0f, 0.0f } },  // 21: Top-Left
        SimpleVertex { Vec3f { r, t, f }, Vec3f { 1.0f, 0.0f, 0.0f }, Vec2f { 1.0f, 0.0f } },  // 22: Top-Right
        SimpleVertex { Vec3f { r, b, f }, Vec3f { 1.0f, 0.0f, 0.0f }, Vec2f { 1.0f, 1.0f } }   // 23: Bottom-Right
    };

    static const uint32 s_indices[] = {
        // Front face (-Z)
        0, 2, 1,
        0, 3, 2,

        // Back face (+Z)
        4, 6, 5,
        4, 7, 6,

        // Top face (+Y)
        8, 10, 9,
        8, 11, 10,

        // Bottom face (-Y)
        12, 14, 13,
        12, 15, 14,

        // Left face (-X)
        16, 18, 17,
        16, 19, 18,

        // Right face (+X)
        20, 22, 21,
        20, 23, 22
    };

    meshDesc.lods[0].numIndices = uint32(std::size(s_indices));
    meshDesc.lods[0].numVertices = uint32(std::size(vertices));
#endif

    if (originOnBottom)
    {
        for (SimpleVertex& vertex : vertices)
        {
            vertex.posY += 1.0f;
        }
    }

    Array<ubyte> indexData;
    indexData.Resize(indices.Size() * sizeof(uint32));
    Memory::Copy(indexData.Data(), indices.Data(), indexData.Size());

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetName(NAME("MeshBuilder_Cube"));

    VertexArrayView vertexArrayView {};
    vertexArrayView.floatData = reinterpret_cast<const float*>(vertices.Data());
    vertexArrayView.vertexCount = uint32(vertices.Size());
    vertexArrayView.layoutDesc = { VT_Simple };

    MeshDataView meshData {};
    meshData.vertices[0] = vertexArrayView;
    meshData.indices[0] = indexData;

    mesh->SetMeshData(meshDesc, meshData);

    return mesh;
}

ENGINE_API Handle<Mesh> NormalizedCubeSphere(uint32 numDivisions){
    const float step = 1.0f / float(numDivisions);

    static constexpr Vec3f Origins[6] = {
        Vec3f(-1.0f, -1.0f, -1.0f),
        Vec3f(1.0f, -1.0f, -1.0f),
        Vec3f(1.0f, -1.0f, 1.0f),
        Vec3f(-1.0f, -1.0f, 1.0f),
        Vec3f(-1.0f, 1.0f, -1.0f),
        Vec3f(-1.0f, -1.0f, 1.0f)
    };

    static constexpr Vec3f Rights[6] = {
        Vec3f(2.0f, 0.0f, 0.0f),
        Vec3f(0.0f, 0.0f, 2.0f),
        Vec3f(-2.0f, 0.0f, 0.0f),
        Vec3f(0.0f, 0.0f, -2.0f),
        Vec3f(2.0f, 0.0f, 0.0f),
        Vec3f(2.0f, 0.0f, 0.0f)
    };

    static constexpr Vec3f Ups[6] = {
        Vec3f(0.0f, 2.0f, 0.0f),
        Vec3f(0.0f, 2.0f, 0.0f),
        Vec3f(0.0f, 2.0f, 0.0f),
        Vec3f(0.0f, 2.0f, 0.0f),
        Vec3f(0.0f, 0.0f, 2.0f),
        Vec3f(0.0f, 0.0f, -2.0f)
    };

    Array<SimpleVertex> vertices;
    Array<uint32> indices;

    const uint32 expectedVertices = 6 * (numDivisions + 1) * (numDivisions + 1);
    const uint32 expectedIndices = 36 * numDivisions * numDivisions;

    vertices.Reserve(expectedVertices);
    indices.Reserve(expectedIndices);

    for (uint32 face = 0; face < 6; face++)
    {
        const Vec3f& origin = Origins[face];
        const Vec3f& right = Rights[face];
        const Vec3f& up = Ups[face];

        for (uint32 j = 0; j < numDivisions + 1; j++)
        {
            for (uint32 i = 0; i < numDivisions + 1; i++)
            {
                const Vec3f point = (origin + Vec3f(step) * (Vec3f(i) * right + Vec3f(j) * up)).Normalized();
                Vec3f position = point;
                Vec3f normal = point;

                const Vec2f uv(
                    float(j + (face * numDivisions)) / float(numDivisions * 6),
                    float(i + (face * numDivisions)) / float(numDivisions * 6));

                vertices.PushBack(SimpleVertex { position, normal, uv });
            }
        }
    }

    const uint32 k = numDivisions + 1;

    for (uint32 face = 0; face < 6; face++)
    {
        for (uint32 j = 0; j < numDivisions; j++)
        {
            const bool isBottom = j < (numDivisions / 2);

            for (uint32 i = 0; i < numDivisions; i++)
            {
                const bool isLeft = i < (numDivisions / 2);

                const uint32 a = (face * k + j) * k + i;
                const uint32 b = (face * k + j) * k + i + 1;
                const uint32 c = (face * k + j + 1) * k + i;
                const uint32 d = (face * k + j + 1) * k + i + 1;

                if (isBottom ^ isLeft)
                {
                    indices.PushBack(a);
                    indices.PushBack(b);
                    indices.PushBack(c);
                    indices.PushBack(c);
                    indices.PushBack(b);
                    indices.PushBack(d);
                }
                else
                {
                    indices.PushBack(a);
                    indices.PushBack(d);
                    indices.PushBack(c);
                    indices.PushBack(a);
                    indices.PushBack(b);
                    indices.PushBack(d);
                }
            }
        }
    }

    MeshDesc meshDesc;
    meshDesc.meshAttributes.inputLayout = { VT_Simple };
    meshDesc.lods[0].numIndices = uint32(indices.Size());
    meshDesc.lods[0].numVertices = uint32(vertices.Size());

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetName(NAME_FMT("MeshBuilder_NormalizedCubeSphere_{}", numDivisions));

    VertexArrayView vertexArrayView {};
    vertexArrayView.floatData = reinterpret_cast<const float*>(vertices.Data());
    vertexArrayView.vertexCount = vertices.Size();
    vertexArrayView.layoutDesc = { VT_Simple };

    MeshDataView meshData {};
    meshData.vertices[0] = vertexArrayView;
    meshData.indices[0] = indices.ToByteView();

    mesh->SetMeshData(meshDesc, meshData);

    return mesh;
}

ENGINE_API Handle<Mesh> ApplyTransform(const Mesh* mesh, const Transform& transform)
{
    Assert(mesh != nullptr);

    auto readScope = mesh->GetReadScope();

    const Mat4f wMatrix = transform.GetMatrix();
    const Mat4f nMatrix = wMatrix.Inverse().Transpose();

    const MeshDesc meshDesc = mesh->GetMeshDesc();

    const VertexArrayView vertexData = mesh->GetVertexData(0);
    const Array<ubyte> indexData = mesh->GetIndexData(0);

    const size_t vertexSizeInFloats = vertexData.layoutDesc.VertexSize() / sizeof(float);

    Array<float> newVertices;
    newVertices.Resize(vertexData.vertexCount * vertexSizeInFloats);
    Memory::Copy(newVertices.Data(), vertexData.floatData, vertexData.vertexCount * vertexData.layoutDesc.VertexSize());

    for (size_t offset = 0; offset < newVertices.Size(); offset += vertexSizeInFloats)
    {
        size_t localOffset = offset;

        if (vertexData.layoutDesc.mask & VT_Position)
        {
            TVertexPacket<VT_Position>* packet = reinterpret_cast<TVertexPacket<VT_Position>*>(newVertices.Data() + localOffset);
            packet->SetPosition(wMatrix.TransformVector(packet->GetPosition()));

            localOffset += sizeof(TVertexPacket<VT_Position>) / sizeof(float);
        }

        if (vertexData.layoutDesc.mask & VT_Normal)
        {
            TVertexPacket<VT_Normal>* packet = reinterpret_cast<TVertexPacket<VT_Normal>*>(newVertices.Data() + localOffset);
            packet->SetNormal(nMatrix.TransformVector(Vec4f(packet->GetNormal(), 0.0f)).GetXYZ());

            localOffset += sizeof(TVertexPacket<VT_Normal>) / sizeof(float);
        }
    }

    Handle<Mesh> newMesh = MakeHandle<Mesh>();

    VertexArrayView vertexArrayView = vertexData;
    vertexArrayView.floatData = newVertices.Data();

    MeshDataView meshData {};
    meshData.vertices[0] = vertexArrayView;
    meshData.indices[0] = indexData;

    newMesh->SetMeshData(meshDesc, meshData);

    newMesh->SetName(mesh->GetName());

    return newMesh;
}

ENGINE_API Handle<Mesh> Merge(const Mesh* a, const Mesh* b, const Transform& aTransform, const Transform& bTransform)
{
    Assert(a != nullptr);
    Assert(b != nullptr);

    Handle<Mesh> transformedMeshes[] = {
        ApplyTransform(a, aTransform),
        ApplyTransform(b, bTransform)
    };

    TSharedResLock<AssetObject> readScopes[] = {
        transformedMeshes[0]->IsRegistered() ? TSharedResLock<AssetObject>(*transformedMeshes[0]) : TSharedResLock<AssetObject>(),
        transformedMeshes[1]->IsRegistered() ? TSharedResLock<AssetObject>(*transformedMeshes[1]) : TSharedResLock<AssetObject>()
    };

    MeshDesc const* meshDescs[] = {
        &transformedMeshes[0]->GetMeshDesc(),
        &transformedMeshes[1]->GetMeshDesc()
    };

    VertexArrayView meshVertices[] = {
        transformedMeshes[0]->GetVertexData(0),
        transformedMeshes[1]->GetVertexData(0)
    };

    Span<const ubyte> meshIndices[] = {
        transformedMeshes[0]->GetIndexData(0),
        transformedMeshes[1]->GetIndexData(0)
    };

    // only simple for now.
    Array<SimpleVertex> allVertices;
    allVertices.Resize(meshDescs[0]->lods[0].numVertices + meshDescs[1]->lods[0].numVertices);

    Array<uint32> allIndices;
    allIndices.Resize(meshDescs[0]->lods[0].numIndices + meshDescs[1]->lods[0].numIndices);

    size_t vertexOffset = 0;
    size_t indexOffset = 0;

    for (int meshIndex = 0; meshIndex < 2; meshIndex++)
    {
        const size_t vertexOffsetBefore = vertexOffset;

        for (size_t i = 0; i < meshVertices[meshIndex].vertexCount; i++)
        {
            SimpleVertex& dstVertex = allVertices[vertexOffset++];

            const float* srcVertexOffset = meshVertices[meshIndex].floatData + (i * (meshVertices[meshIndex].layoutDesc.VertexSize() / sizeof(float)));

            size_t offset = 0;

            if (meshVertices[meshIndex].layoutDesc.mask & VT_Position)
            {
                const TVertexPacket<VT_Position>* packet = reinterpret_cast<const TVertexPacket<VT_Position>*>(srcVertexOffset + offset);
                dstVertex.SetPosition(packet->GetPosition());

                offset += sizeof(TVertexPacket<VT_Position>) / sizeof(float);
            }

            if (meshVertices[meshIndex].layoutDesc.mask & VT_Normal)
            {
                const TVertexPacket<VT_Normal>* packet = reinterpret_cast<const TVertexPacket<VT_Normal>*>(srcVertexOffset + offset);
                dstVertex.SetNormal(packet->GetNormal());

                offset += sizeof(TVertexPacket<VT_Normal>) / sizeof(float);
            }

            if (meshVertices[meshIndex].layoutDesc.mask & VT_UV0)
            {
                const TVertexPacket<VT_UV0>* packet = reinterpret_cast<const TVertexPacket<VT_UV0>*>(srcVertexOffset + offset);
                dstVertex.SetUV0(packet->GetUV0());

                offset += sizeof(TVertexPacket<VT_UV0>) / sizeof(float);
            }
        }

        const uint32 stride = GpuElemTypeSize(meshDescs[meshIndex]->meshAttributes.indexBufferElemType);
        const size_t meshIndexCount = meshIndices[meshIndex].Size() / stride;

        for (size_t i = 0; i < meshIndexCount; i++)
        {
            switch (stride)
            {
            case 2:
                allIndices[indexOffset++] = uint32(*reinterpret_cast<const uint16*>(&meshIndices[meshIndex][i * stride])) + uint32(vertexOffsetBefore);
                break;
            case 4:
                allIndices[indexOffset++] = uint32(*reinterpret_cast<const uint32*>(&meshIndices[meshIndex][i * stride])) + uint32(vertexOffsetBefore);
                break;
            default:
                HYP_UNREACHABLE();
            }
        }
    }

    for (TSharedResLock<AssetObject>& scope : readScopes)
    {
        scope.Reset();
    }

    MeshDesc mergedMeshDesc;
    mergedMeshDesc.meshAttributes.indexBufferElemType = GET_UNSIGNED_INT;
    mergedMeshDesc.meshAttributes.inputLayout = { VT_Simple };
    mergedMeshDesc.lods[0].numIndices = uint32(allIndices.Size());
    mergedMeshDesc.lods[0].numVertices = uint32(allVertices.Size());

    Handle<Mesh> newMesh = MakeHandle<Mesh>();

    VertexArrayView vertexArrayView {};
    vertexArrayView.floatData = reinterpret_cast<const float*>(allVertices.Data());
    vertexArrayView.vertexCount = allVertices.Size();
    vertexArrayView.layoutDesc = { VT_Simple };

    MeshDataView meshData {};
    meshData.vertices[0] = vertexArrayView;
    meshData.indices[0] = allIndices.ToByteView();

    newMesh->SetMeshData(mergedMeshDesc, meshData);

    newMesh->SetName(NAME("MeshBuilder_MergedMesh"));

    return newMesh;
}

ENGINE_API Handle<Mesh> Merge(const Mesh* a, const Mesh* b)
{
    return Merge(a, b, Transform(), Transform());
}

ENGINE_API Handle<Mesh> BuildVoxelMesh(const VoxelOctree& voxelOctree)
{
    static const auto cubeVerticesAndIndices = CalculateIndices(GetCubeVertices());

    Array<BoundingBox> voxelAabbs;

    Proc<void(const VoxelOctree&)> traverse;
    traverse = [&](const VoxelOctree& octant)
    {
        if (octant.GetPayload().occupiedBit) // filled voxel node
        {
            // AssertDebug(!octant.IsDivided());

            voxelAabbs.PushBack(octant.GetAABB());
        }

        if (octant.IsDivided())
        {
            // AssertDebug(octant.GetEntries().Empty());

            for (auto& childOctant : octant.GetOctants())
            {
                Assert(childOctant.octree != nullptr);

                traverse(static_cast<const VoxelOctree&>(*childOctant.octree));
            }
        }
    };

    traverse(voxelOctree);

    Array<SimpleVertex> vertices;
    Array<uint32> indices;

    uint32 vertexOffset = 0;

    static const int faceCornerIdx[6][4] = {
        { 1, 5, 6, 2 }, // +X
        { 4, 0, 3, 7 }, // -X
        { 3, 2, 6, 7 }, // +Y
        { 0, 1, 5, 4 }, // -Y
        { 4, 5, 6, 7 }, // +Z
        { 0, 1, 2, 3 }  // -Z
    };
    static const Vec3i faceNormals[6] = {
        { 1, 0, 0 }, { -1, 0, 0 },
        { 0, 1, 0 }, { 0, -1, 0 },
        { 0, 0, 1 }, { 0, 0, -1 }
    };
    static const Vec2f uvs[4] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };
    static const uint32 idxPattern[6] = { 0, 1, 2, 0, 2, 3 };

    // Build full box for each voxel AABB
    for (const auto& aabb : voxelAabbs)
    {
        Vec3f mn = aabb.GetMin();
        Vec3f mx = aabb.GetMax();
        Vec3f corners[8] = {
            { mn.x, mn.y, mn.z }, { mx.x, mn.y, mn.z }, { mx.x, mx.y, mn.z }, { mn.x, mx.y, mn.z },
            { mn.x, mn.y, mx.z }, { mx.x, mn.y, mx.z }, { mx.x, mx.y, mx.z }, { mn.x, mx.y, mx.z }
        };
        for (int f = 0; f < 6; ++f)
        {
            for (int i = 0; i < 4; ++i)
            {
                SimpleVertex vert {};
                vert.SetPosition(corners[faceCornerIdx[f][i]]);

                vertices.PushBack(vert);
            }
            for (int k = 0; k < 6; ++k)
                indices.PushBack(vertexOffset + idxPattern[k]);
            vertexOffset += 4;
        }
    }

    MeshDesc meshDesc;
    meshDesc.meshAttributes.inputLayout = { VT_Simple };
    meshDesc.lods[0].numIndices = (uint32)indices.Size();
    meshDesc.lods[0].numVertices = (uint32)vertices.Size();
    meshDesc.meshAttributes.indexBufferElemType = GET_UNSIGNED_INT;
    meshDesc.meshAttributes.topology = TOP_LINES;

    Handle<Mesh> mesh = MakeHandle<Mesh>();

    VertexArrayView vertexArrayView {};
    vertexArrayView.floatData = reinterpret_cast<const float*>(vertices.Data());
    vertexArrayView.vertexCount = vertices.Size();
    vertexArrayView.layoutDesc = { VT_Simple };

    MeshDataView meshData {};
    meshData.vertices[0] = vertexArrayView;
    meshData.indices[0] = indices.ToByteView();

    mesh->SetMeshData(meshDesc, meshData);

    mesh->SetName(NAME("MeshBuilder_VoxelMesh"));

    return mesh;
}

ENGINE_API Handle<Mesh> Cylinder(float radius, float height, uint32 numSegments)
{
    numSegments = MathUtil::Max(numSegments, 3u);

    Array<SimpleVertex> vertices;
    vertices.Reserve(numSegments * 12); // 6 shaft verts + 3 top cap + 3 bottom cap per segment

    const float halfHeight = height * 0.5f;

    for (uint32 i = 0; i < numSegments; i++)
    {
        const float angle = (MathUtil::pi<float> * 2.0f * float(i)) / float(numSegments);
        const float nextAngle = (MathUtil::pi<float> * 2.0f * float(i + 1)) / float(numSegments);

        const float cosA = MathUtil::Cos(angle);
        const float sinA = MathUtil::Sin(angle);
        const float cosNext = MathUtil::Cos(nextAngle);
        const float sinNext = MathUtil::Sin(nextAngle);

        const Vec3f normalA(cosA, 0.0f, sinA);
        const Vec3f normalNext(cosNext, 0.0f, sinNext);

        const Vec3f bottomA(radius * cosA, -halfHeight, radius * sinA);
        const Vec3f bottomNext(radius * cosNext, -halfHeight, radius * sinNext);
        const Vec3f topA(radius * cosA, halfHeight, radius * sinA);
        const Vec3f topNext(radius * cosNext, halfHeight, radius * sinNext);

        vertices.PushBack(SimpleVertex { bottomA, normalA, Vec2f(float(i) / float(numSegments), 0.0f) });
        vertices.PushBack(SimpleVertex { bottomNext, normalNext, Vec2f(float(i + 1) / float(numSegments), 0.0f) });
        vertices.PushBack(SimpleVertex { topA, normalA, Vec2f(float(i) / float(numSegments), 1.0f) });

        vertices.PushBack(SimpleVertex { topA, normalA, Vec2f(float(i) / float(numSegments), 1.0f) });
        vertices.PushBack(SimpleVertex { bottomNext, normalNext, Vec2f(float(i + 1) / float(numSegments), 0.0f) });
        vertices.PushBack(SimpleVertex { topNext, normalNext, Vec2f(float(i + 1) / float(numSegments), 1.0f) });

        const Vec3f bottomNormal(0.0f, -1.0f, 0.0f);
        const Vec3f topNormal(0.0f, 1.0f, 0.0f);
        const Vec3f bottomCenter(0.0f, -halfHeight, 0.0f);
        const Vec3f topCenter(0.0f, halfHeight, 0.0f);

        vertices.PushBack(SimpleVertex { bottomCenter, bottomNormal, Vec2f(cosA * 0.5f + 0.5f, sinA * 0.5f + 0.5f) });
        vertices.PushBack(SimpleVertex { bottomNext, bottomNormal, Vec2f(cosNext * 0.5f + 0.5f, sinNext * 0.5f + 0.5f) });
        vertices.PushBack(SimpleVertex { bottomA, bottomNormal, Vec2f(cosA * 0.5f + 0.5f, sinA * 0.5f + 0.5f) });

        vertices.PushBack(SimpleVertex { topCenter, topNormal, Vec2f(cosA * 0.5f + 0.5f, sinA * 0.5f + 0.5f) });
        vertices.PushBack(SimpleVertex { topA, topNormal, Vec2f(cosA * 0.5f + 0.5f, sinA * 0.5f + 0.5f) });
        vertices.PushBack(SimpleVertex { topNext, topNormal, Vec2f(cosNext * 0.5f + 0.5f, sinNext * 0.5f + 0.5f) });
    }

    auto deduped = CalculateIndices(vertices);

    MeshDesc meshDesc {};
    meshDesc.meshAttributes.inputLayout = { VT_Simple };
    meshDesc.lods[0].numIndices = uint32(deduped.second.Size());
    meshDesc.lods[0].numVertices = uint32(deduped.first.Size());

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetName(NAME("MeshBuilder_Cylinder"));

    VertexArrayView vertexArrayView {};
    vertexArrayView.floatData = reinterpret_cast<const float*>(deduped.first.Data());
    vertexArrayView.vertexCount = deduped.first.Size();
    vertexArrayView.layoutDesc = { VT_Simple };

    MeshDataView meshData {};
    meshData.vertices[0] = vertexArrayView;
    meshData.indices[0] = deduped.second.ToByteView();

    mesh->SetMeshData(meshDesc, meshData);

    return mesh;
}

ENGINE_API Handle<Mesh> Cone(float radius, float height, uint32 numSegments)
{
    numSegments = MathUtil::Max(numSegments, 3u);

    Array<SimpleVertex> vertices;
    vertices.Reserve(numSegments * 9); // 6 side verts + 3 bottom cap per segment

    const float halfHeight = height * 0.5f;
    const Vec3f tip(0.0f, halfHeight, 0.0f);

    for (uint32 i = 0; i < numSegments; i++)
    {
        const float angle = (MathUtil::pi<float> * 2.0f * float(i)) / float(numSegments);
        const float nextAngle = (MathUtil::pi<float> * 2.0f * float(i + 1)) / float(numSegments);

        const float cosA = MathUtil::Cos(angle);
        const float sinA = MathUtil::Sin(angle);
        const float cosNext = MathUtil::Cos(nextAngle);
        const float sinNext = MathUtil::Sin(nextAngle);

        const Vec3f baseA(radius * cosA, -halfHeight, radius * sinA);
        const Vec3f baseNext(radius * cosNext, -halfHeight, radius * sinNext);

        const Vec3f sideNormalA = Vec3f(cosA * height, radius, sinA * height).Normalize();
        const Vec3f sideNormalNext = Vec3f(cosNext * height, radius, sinNext * height).Normalize();

        vertices.PushBack(SimpleVertex { baseA, sideNormalA, Vec2f(float(i) / float(numSegments), 0.0f) });
        vertices.PushBack(SimpleVertex { baseNext, sideNormalNext, Vec2f(float(i + 1) / float(numSegments), 0.0f) });
        vertices.PushBack(SimpleVertex { tip, sideNormalA, Vec2f(float(i) / float(numSegments), 1.0f) });

        vertices.PushBack(SimpleVertex { tip, sideNormalNext, Vec2f(float(i + 1) / float(numSegments), 1.0f) });
        vertices.PushBack(SimpleVertex { baseNext, sideNormalNext, Vec2f(float(i + 1) / float(numSegments), 0.0f) });
        vertices.PushBack(SimpleVertex { baseA, sideNormalA, Vec2f(float(i) / float(numSegments), 0.0f) });

        const Vec3f bottomNormal(0.0f, -1.0f, 0.0f);
        const Vec3f bottomCenter(0.0f, -halfHeight, 0.0f);

        vertices.PushBack(SimpleVertex { bottomCenter, bottomNormal, Vec2f(cosA * 0.5f + 0.5f, sinA * 0.5f + 0.5f) });
        vertices.PushBack(SimpleVertex { baseNext, bottomNormal, Vec2f(cosNext * 0.5f + 0.5f, sinNext * 0.5f + 0.5f) });
        vertices.PushBack(SimpleVertex { baseA, bottomNormal, Vec2f(cosA * 0.5f + 0.5f, sinA * 0.5f + 0.5f) });
    }

    auto deduped = CalculateIndices(vertices);

    MeshDesc meshDesc {};
    meshDesc.meshAttributes.inputLayout = { VT_Simple };
    meshDesc.lods[0].numIndices = uint32(deduped.second.Size());
    meshDesc.lods[0].numVertices = uint32(deduped.first.Size());

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetName(NAME("MeshBuilder_Cone"));

    VertexArrayView vertexArrayView {};
    vertexArrayView.floatData = reinterpret_cast<const float*>(deduped.first.Data());
    vertexArrayView.vertexCount = deduped.first.Size();
    vertexArrayView.layoutDesc = { VT_Simple };

    MeshDataView meshData {};
    meshData.vertices[0] = vertexArrayView;
    meshData.indices[0] = deduped.second.ToByteView();

    mesh->SetMeshData(meshDesc, meshData);

    return mesh;
}

ENGINE_API Handle<Mesh> Torus(float majorRadius, float minorRadius, uint32 majorSegments, uint32 minorSegments)
{
    majorSegments = MathUtil::Max(majorSegments, 3u);
    minorSegments = MathUtil::Max(minorSegments, 3u);

    Array<SimpleVertex> vertices;
    vertices.Reserve(majorSegments * minorSegments * 6); // 2 triangles per quad

    for (uint32 i = 0; i < majorSegments; i++)
    {
        const float theta = (MathUtil::pi<float> * 2.0f * float(i)) / float(majorSegments);
        const float nextTheta = (MathUtil::pi<float> * 2.0f * float(i + 1)) / float(majorSegments);

        const float cosT = MathUtil::Cos(theta);
        const float sinT = MathUtil::Sin(theta);
        const float cosNextT = MathUtil::Cos(nextTheta);
        const float sinNextT = MathUtil::Sin(nextTheta);

        const Vec3f ringCenter(cosT, 0.0f, sinT);
        const Vec3f ringCenterNext(cosNextT, 0.0f, sinNextT);

        for (uint32 j = 0; j < minorSegments; j++)
        {
            const float phi = (MathUtil::pi<float> * 2.0f * float(j)) / float(minorSegments);
            const float nextPhi = (MathUtil::pi<float> * 2.0f * float(j + 1)) / float(minorSegments);

            const float cosP = MathUtil::Cos(phi);
            const float sinP = MathUtil::Sin(phi);
            const float cosNextP = MathUtil::Cos(nextPhi);
            const float sinNextP = MathUtil::Sin(nextPhi);

            const float r1 = majorRadius + minorRadius * cosP;
            const float r2 = majorRadius + minorRadius * cosNextP;

            const Vec3f v0(r1 * cosT, minorRadius * sinP, r1 * sinT);
            const Vec3f v1(r1 * cosNextT, minorRadius * sinP, r1 * sinNextT);
            const Vec3f v2(r2 * cosNextT, minorRadius * sinNextP, r2 * sinNextT);
            const Vec3f v3(r2 * cosT, minorRadius * sinNextP, r2 * sinT);

            const Vec3f n0(cosP * cosT, sinP, cosP * sinT);
            const Vec3f n1(cosP * cosNextT, sinP, cosP * sinNextT);
            const Vec3f n2(cosNextP * cosNextT, sinNextP, cosNextP * sinNextT);
            const Vec3f n3(cosNextP * cosT, sinNextP, cosNextP * sinT);

            const Vec2f uv0(float(i) / float(majorSegments), float(j) / float(minorSegments));
            const Vec2f uv1(float(i + 1) / float(majorSegments), float(j) / float(minorSegments));
            const Vec2f uv2(float(i + 1) / float(majorSegments), float(j + 1) / float(minorSegments));
            const Vec2f uv3(float(i) / float(majorSegments), float(j + 1) / float(minorSegments));

            vertices.PushBack(SimpleVertex { v0, n0, uv0 });
            vertices.PushBack(SimpleVertex { v1, n1, uv1 });
            vertices.PushBack(SimpleVertex { v2, n2, uv2 });

            vertices.PushBack(SimpleVertex { v0, n0, uv0 });
            vertices.PushBack(SimpleVertex { v2, n2, uv2 });
            vertices.PushBack(SimpleVertex { v3, n3, uv3 });
        }
    }

    auto deduped = CalculateIndices(vertices);

    MeshDesc meshDesc {};
    meshDesc.meshAttributes.inputLayout = { VT_Simple };
    meshDesc.lods[0].numIndices = uint32(deduped.second.Size());
    meshDesc.lods[0].numVertices = uint32(deduped.first.Size());

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetName(NAME("MeshBuilder_Torus"));

    VertexArrayView vertexArrayView {};
    vertexArrayView.floatData = reinterpret_cast<const float*>(deduped.first.Data());
    vertexArrayView.vertexCount = deduped.first.Size();
    vertexArrayView.layoutDesc = { VT_Simple };

    MeshDataView meshData {};
    meshData.vertices[0] = vertexArrayView;
    meshData.indices[0] = deduped.second.ToByteView();

    mesh->SetMeshData(meshDesc, meshData);

    return mesh;
}

} // namespace MeshBuilder
} // namespace Hyperion
