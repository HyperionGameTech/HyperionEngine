/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <util/MeshBuilder.hpp>

#include <Core/math/Triangle.hpp>

#include <rendering/Mesh.hpp>

#include <scene/util/VoxelOctree.hpp>

namespace Hyperion {

static Pair<Array<SimpleVertex>, Array<uint32>> CalculateIndices(const Array<SimpleVertex>& vertices)
{
    TMap<SimpleVertex, uint32> indexMap;

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
        0, 1, 2,
        0, 2, 3
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
        // Face 1
        SimpleVertex { Vec3f { -1.0f, -1.0f, -1.0f }, Vec3f { -1.0f, 0.0f, 0.0f }, Vec2f { 0.0f, 0.0f } },
        SimpleVertex { Vec3f { -1.0f, 1.0f, -1.0f }, Vec3f { -1.0f, 0.0f, 0.0f }, Vec2f { 0.0f, 1.0f } },
        SimpleVertex { Vec3f { -1.0f, 1.0f, 1.0f }, Vec3f { -1.0f, 0.0f, 0.0f }, Vec2f { 1.0f, 1.0f } },

        SimpleVertex { Vec3f { -1.0f, 1.0f, 1.0f }, Vec3f { -1.0f, 0.0f, 0.0f }, Vec2f { 1.0f, 1.0f } },
        SimpleVertex { Vec3f { -1.0f, -1.0f, 1.0f }, Vec3f { -1.0f, 0.0f, 0.0f }, Vec2f { 1.0f, 0.0f } },
        SimpleVertex { Vec3f { -1.0f, -1.0f, -1.0f }, Vec3f { -1.0f, 0.0f, 0.0f }, Vec2f { 0.0f, 0.0f } },

        // Face 2
        SimpleVertex { Vec3f { -1.0f, -1.0f, 1.0f }, Vec3f { 0.0f, 0.0f, 1.0f }, Vec2f { 0.0f, 0.0f } },
        SimpleVertex { Vec3f { -1.0f, 1.0f, 1.0f }, Vec3f { 0.0f, 0.0f, 1.0f }, Vec2f { 0.0f, 1.0f } },
        SimpleVertex { Vec3f { 1.0f, 1.0f, 1.0f }, Vec3f { 0.0f, 0.0f, 1.0f }, Vec2f { 1.0f, 1.0f } },

        SimpleVertex { Vec3f { 1.0f, 1.0f, 1.0f }, Vec3f { 0.0f, 0.0f, 1.0f }, Vec2f { 1.0f, 1.0f } },
        SimpleVertex { Vec3f { 1.0f, -1.0f, 1.0f }, Vec3f { 0.0f, 0.0f, 1.0f }, Vec2f { 1.0f, 0.0f } },
        SimpleVertex { Vec3f { -1.0f, -1.0f, 1.0f }, Vec3f { 0.0f, 0.0f, 1.0f }, Vec2f { 0.0f, 0.0f } },

        // Face 3
        SimpleVertex { Vec3f { 1.0f, 1.0f, 1.0f }, Vec3f { 1.0f, 0.0f, 0.0f }, Vec2f { 1.0f, 1.0f } },
        SimpleVertex { Vec3f { 1.0f, 1.0f, -1.0f }, Vec3f { 1.0f, 0.0f, 0.0f }, Vec2f { 0.0f, 1.0f } },
        SimpleVertex { Vec3f { 1.0f, -1.0f, -1.0f }, Vec3f { 1.0f, 0.0f, 0.0f }, Vec2f { 0.0f, 0.0f } },

        SimpleVertex { Vec3f { 1.0f, -1.0f, -1.0f }, Vec3f { 1.0f, 0.0f, 0.0f }, Vec2f { 0.0f, 0.0f } },
        SimpleVertex { Vec3f { 1.0f, -1.0f, 1.0f }, Vec3f { 1.0f, 0.0f, 0.0f }, Vec2f { 1.0f, 0.0f } },
        SimpleVertex { Vec3f { 1.0f, 1.0f, 1.0f }, Vec3f { 1.0f, 0.0f, 0.0f }, Vec2f { 1.0f, 1.0f } },

        // Face 4
        SimpleVertex { Vec3f { 1.0f, 1.0f, -1.0f }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 1.0f, 1.0f } },
        SimpleVertex { Vec3f { -1.0f, 1.0f, -1.0f }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 0.0f, 1.0f } },
        SimpleVertex { Vec3f { -1.0f, -1.0f, -1.0f }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 0.0f, 0.0f } },

        SimpleVertex { Vec3f { -1.0f, -1.0f, -1.0f }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 0.0f, 0.0f } },
        SimpleVertex { Vec3f { 1.0f, -1.0f, -1.0f }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 1.0f, 0.0f } },
        SimpleVertex { Vec3f { 1.0f, 1.0f, -1.0f }, Vec3f { 0.0f, 0.0f, -1.0f }, Vec2f { 1.0f, 1.0f } },

        // Face 5
        SimpleVertex { Vec3f { -1.0f, 1.0f, 1.0f }, Vec3f { 0.0f, 1.0f, 0.0f }, Vec2f { 1.0f, 1.0f } },
        SimpleVertex { Vec3f { -1.0f, 1.0f, -1.0f }, Vec3f { 0.0f, 1.0f, 0.0f }, Vec2f { 0.0f, 1.0f } },
        SimpleVertex { Vec3f { 1.0f, 1.0f, -1.0f }, Vec3f { 0.0f, 1.0f, 0.0f }, Vec2f { 0.0f, 0.0f } },

        SimpleVertex { Vec3f { 1.0f, 1.0f, -1.0f }, Vec3f { 0.0f, 1.0f, 0.0f }, Vec2f { 0.0f, 0.0f } },
        SimpleVertex { Vec3f { 1.0f, 1.0f, 1.0f }, Vec3f { 0.0f, 1.0f, 0.0f }, Vec2f { 1.0f, 0.0f } },
        SimpleVertex { Vec3f { -1.0f, 1.0f, 1.0f }, Vec3f { 0.0f, 1.0f, 0.0f }, Vec2f { 1.0f, 1.0f } },

        // Face 6
        SimpleVertex { Vec3f { 1.0f, -1.0f, -1.0f }, Vec3f { 0.0f, -1.0f, 0.0f }, Vec2f { 0.0f, 0.0f } },
        SimpleVertex { Vec3f { -1.0f, -1.0f, -1.0f }, Vec3f { 0.0f, -1.0f, 0.0f }, Vec2f { 0.0f, 1.0f } },
        SimpleVertex { Vec3f { -1.0f, -1.0f, 1.0f }, Vec3f { 0.0f, -1.0f, 0.0f }, Vec2f { 1.0f, 1.0f } },

        SimpleVertex { Vec3f { -1.0f, -1.0f, 1.0f }, Vec3f { 0.0f, -1.0f, 0.0f }, Vec2f { 1.0f, 1.0f } },
        SimpleVertex { Vec3f { 1.0f, -1.0f, 1.0f }, Vec3f { 0.0f, -1.0f, 0.0f }, Vec2f { 1.0f, 0.0f } },
        SimpleVertex { Vec3f { 1.0f, -1.0f, -1.0f }, Vec3f { 0.0f, -1.0f, 0.0f }, Vec2f { 0.0f, 0.0f } }
    };

    return s_cubeVertices;
}

Handle<Mesh> MeshBuilder::Quad()
{
    const auto& vertices = GetQuadVertices();
    const auto& indices = GetQuadIndices();

    MeshDesc meshDesc {};
    meshDesc.meshAttributes.inputLayout = { VT_Simple };
    meshDesc.numIndices = uint32(indices.Size());
    meshDesc.numVertices = uint32(vertices.Size());

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetName(NAME("MeshBuilder_Quad"));

    VertexArrayView vertexArrayView {};
    vertexArrayView.floatData = reinterpret_cast<const float*>(vertices.Data());
    vertexArrayView.layoutDesc = meshDesc.meshAttributes.inputLayout;
    vertexArrayView.vertexCount = vertices.Size();

    const ConstByteView indicesByteView = ConstByteView(
        reinterpret_cast<const ubyte*>(indices.Data()),
        reinterpret_cast<const ubyte*>(indices.Data() + indices.Size()));

    mesh->SetMeshData(meshDesc, vertexArrayView, indicesByteView);

    return mesh;
}

Handle<Mesh> MeshBuilder::DoubleSidedQuad()
{
    const auto& vertices = GetDoubleSidedQuadVertices();
    const auto& indices = GetDoubleSidedQuadIndices();

    MeshDesc meshDesc {};
    meshDesc.meshAttributes.inputLayout = { VT_Simple };
    meshDesc.numIndices = uint32(indices.Size());
    meshDesc.numVertices = uint32(vertices.Size());

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetName(NAME("MeshBuilder_DoubleSidedQuad"));

    VertexArrayView vertexArrayView {};
    vertexArrayView.floatData = reinterpret_cast<const float*>(vertices.Data());
    vertexArrayView.layoutDesc = meshDesc.meshAttributes.inputLayout;
    vertexArrayView.vertexCount = vertices.Size();

    const ConstByteView indicesByteView = ConstByteView(
        reinterpret_cast<const ubyte*>(indices.Data()),
        reinterpret_cast<const ubyte*>(indices.Data() + indices.Size()));

    mesh->SetMeshData(meshDesc, vertexArrayView, indicesByteView);

    return mesh;
}

Handle<Mesh> MeshBuilder::Cube(bool originOnBottom)
{
    static const auto s_cubeVerticesAndIndices = CalculateIndices(GetCubeVertices());

    MeshDesc meshDesc;
    meshDesc.meshAttributes.inputLayout = { VT_Simple };
    meshDesc.numIndices = uint32(s_cubeVerticesAndIndices.second.Size());
    meshDesc.numVertices = uint32(s_cubeVerticesAndIndices.first.Size());

    Array<SimpleVertex> vertexData = s_cubeVerticesAndIndices.first;

    if (originOnBottom)
    {
        for (SimpleVertex& vertex : vertexData)
        {
            vertex.posY += 1.0f;
        }
    }

    Array<ubyte> indexData;
    indexData.Resize(s_cubeVerticesAndIndices.second.Size() * sizeof(uint32));
    Memory::Copy(indexData.Data(), s_cubeVerticesAndIndices.second.Data(), s_cubeVerticesAndIndices.second.Size() * sizeof(uint32));

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetName(NAME("MeshBuilder_Cube"));

    VertexArrayView vertexArrayView {};
    vertexArrayView.floatData = reinterpret_cast<const float*>(vertexData.Data());
    vertexArrayView.vertexCount = vertexData.Size();
    vertexArrayView.layoutDesc = { VT_Simple };

    mesh->SetMeshData(meshDesc, vertexArrayView, indexData);

    return mesh;
}
Handle<Mesh> MeshBuilder::NormalizedCubeSphere(uint32 numDivisions){
    const float step = 1.0f / float(numDivisions);

    static const Vec3f origins[6] = {
        Vector3(-1.0f, -1.0f, -1.0f),
        Vector3(1.0f, -1.0f, -1.0f),
        Vector3(1.0f, -1.0f, 1.0f),
        Vector3(-1.0f, -1.0f, 1.0f),
        Vector3(-1.0f, 1.0f, -1.0f),
        Vector3(-1.0f, -1.0f, 1.0f)
    };

    static const Vec3f rights[6] = {
        Vector3(2.0f, 0.0f, 0.0f),
        Vector3(0.0f, 0.0f, 2.0f),
        Vector3(-2.0f, 0.0f, 0.0f),
        Vector3(0.0f, 0.0f, -2.0f),
        Vector3(2.0f, 0.0f, 0.0f),
        Vector3(2.0f, 0.0f, 0.0f)
    };

    static const Vec3f ups[6] = {
        Vector3(0.0f, 2.0f, 0.0f),
        Vector3(0.0f, 2.0f, 0.0f),
        Vector3(0.0f, 2.0f, 0.0f),
        Vector3(0.0f, 2.0f, 0.0f),
        Vector3(0.0f, 0.0f, 2.0f),
        Vector3(0.0f, 0.0f, -2.0f)
    };

    Array<SimpleVertex> vertices;
    Array<uint32> indices;

    const uint32 expectedVertices = 6 * (numDivisions + 1) * (numDivisions + 1);
    const uint32 expectedIndices = 36 * numDivisions * numDivisions;

    vertices.Reserve(expectedVertices);
    indices.Reserve(expectedIndices);

    for (uint32 face = 0; face < 6; face++)
    {
        const Vec3f& origin = origins[face];
        const Vec3f& right = rights[face];
        const Vec3f& up = ups[face];

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
    meshDesc.numIndices = uint32(indices.Size());
    meshDesc.numVertices = uint32(vertices.Size());

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetName(NAME_FMT("MeshBuilder_NormalizedCubeSphere_{}", numDivisions));

    VertexArrayView vertexArrayView {};
    vertexArrayView.floatData = reinterpret_cast<const float*>(vertices.Data());
    vertexArrayView.vertexCount = vertices.Size();
    vertexArrayView.layoutDesc = { VT_Simple };

    mesh->SetMeshData(meshDesc, vertexArrayView, indices.ToByteView());
    mesh->CalculateNormals();

    return mesh;
}

Handle<Mesh> MeshBuilder::ApplyTransform(const Mesh* mesh, const Transform& transform)
{
    Assert(mesh != nullptr);

    TSharedLock<AssetObject> resGuard;

    if (mesh->IsRegistered())
    {
        resGuard = mesh->GetReadScope();
    }

    const Mat4f modelMatrix = transform.GetMatrix();
    const Mat4f normalMatrix = modelMatrix.Inverse().Transpose();

    const MeshDesc& meshDesc = mesh->GetMeshDesc();
    const VertexArrayView vertexData = mesh->GetVertexData();
    const Span<const ubyte> indexData = mesh->GetIndexData();

    Array<float> newVertices;
    newVertices.Resize(vertexData.vertexCount * (vertexData.layoutDesc.VertexSize() / sizeof(float)));
    Memory::Copy(newVertices.Data(), vertexData.floatData, vertexData.vertexCount * vertexData.layoutDesc.VertexSize());

    Array<ubyte> newIndices;
    newIndices.Resize(indexData.Size());
    Memory::Copy(newIndices.Data(), indexData.Data(), indexData.Size());

    resGuard.Reset();

    for (float* f = newVertices.Begin(); f != newVertices.End(); f += (vertexData.layoutDesc.VertexSize() / sizeof(float)))
    {
        size_t offset = 0;

        if (vertexData.layoutDesc.mask & VT_Position)
        {
            TVertexPacket<VT_Position>* packet = reinterpret_cast<TVertexPacket<VT_Position>*>(f);
            packet->SetPosition(modelMatrix * packet->GetPosition());
            offset += sizeof(TVertexPacket<VT_Position>) / sizeof(float);
        }

        if (vertexData.layoutDesc.mask & VT_Normal)
        {
            TVertexPacket<VT_Normal>* packet = reinterpret_cast<TVertexPacket<VT_Normal>*>(f + offset);
            packet->SetNormal(modelMatrix * packet->GetNormal());
            offset += sizeof(TVertexPacket<VT_Normal>) / sizeof(float);
        }
    }

    Handle<Mesh> newMesh = MakeHandle<Mesh>();

    VertexArrayView vertexArrayView = vertexData;
    vertexArrayView.floatData = newVertices.Data();

    newMesh->SetMeshData(meshDesc, vertexArrayView, newIndices);

    newMesh->SetName(mesh->GetName());

    return newMesh;
}

Handle<Mesh> MeshBuilder::Merge(const Mesh* a, const Mesh* b, const Transform& aTransform, const Transform& bTransform)
{
    Assert(a != nullptr);
    Assert(b != nullptr);

    Handle<Mesh> transformedMeshes[] = {
        ApplyTransform(a, aTransform),
        ApplyTransform(b, bTransform)
    };

    TSharedLock<AssetObject> resourceHandles[] = {
        transformedMeshes[0]->IsRegistered() ? TSharedLock<AssetObject>(*transformedMeshes[0]) : TSharedLock<AssetObject>(),
        transformedMeshes[1]->IsRegistered() ? TSharedLock<AssetObject>(*transformedMeshes[1]) : TSharedLock<AssetObject>()
    };

    MeshDesc const* meshDescs[] = {
        &transformedMeshes[0]->GetMeshDesc(),
        &transformedMeshes[1]->GetMeshDesc()
    };

    VertexArrayView meshVertices[] = {
        transformedMeshes[0]->GetVertexData(),
        transformedMeshes[1]->GetVertexData()
    };

    Span<const ubyte> meshIndices[] = {
        transformedMeshes[0]->GetIndexData(),
        transformedMeshes[1]->GetIndexData()
    };

    // only simple for now.
    Array<SimpleVertex> allVertices;
    allVertices.Resize(meshDescs[0]->numVertices + meshDescs[1]->numVertices);

    Array<uint32> allIndices;
    allIndices.Resize(meshDescs[0]->numIndices + meshDescs[1]->numIndices);

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

    for (TSharedLock<AssetObject>& resGuard : resourceHandles)
    {
        resGuard.Reset();
    }

    MeshDesc mergedMeshDesc;
    mergedMeshDesc.meshAttributes.indexBufferElemType = GET_UNSIGNED_INT;
    mergedMeshDesc.meshAttributes.inputLayout = { VT_Simple };
    mergedMeshDesc.numIndices = uint32(allIndices.Size());
    mergedMeshDesc.numVertices = uint32(allVertices.Size());

    Handle<Mesh> newMesh = MakeHandle<Mesh>();


    VertexArrayView vertexArrayView {};
    vertexArrayView.floatData = reinterpret_cast<const float*>(allVertices.Data());
    vertexArrayView.vertexCount = allVertices.Size();
    vertexArrayView.layoutDesc = { VT_Simple };

    newMesh->SetMeshData(mergedMeshDesc, vertexArrayView, allIndices.ToByteView());

    newMesh->SetName(NAME("MeshBuilder_MergedMesh"));

    return newMesh;
}

Handle<Mesh> MeshBuilder::Merge(const Mesh* a, const Mesh* b)
{
    return Merge(a, b, Transform(), Transform());
}

Handle<Mesh> MeshBuilder::BuildVoxelMesh(const VoxelOctree& voxelOctree)
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
    meshDesc.numIndices = (uint32)indices.Size();
    meshDesc.numVertices = (uint32)vertices.Size();
    meshDesc.meshAttributes.indexBufferElemType = GET_UNSIGNED_INT;
    meshDesc.meshAttributes.topology = TOP_LINES;

    Handle<Mesh> mesh = MakeHandle<Mesh>();

    VertexArrayView vertexArrayView {};
    vertexArrayView.floatData = reinterpret_cast<const float*>(vertices.Data());
    vertexArrayView.vertexCount = vertices.Size();
    vertexArrayView.layoutDesc = { VT_Simple };

    mesh->SetMeshData(meshDesc, vertexArrayView, indices.ToByteView());

    mesh->SetName(NAME("MeshBuilder_VoxelMesh"));

    return mesh;
}

} // namespace Hyperion
