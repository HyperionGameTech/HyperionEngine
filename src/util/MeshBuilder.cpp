/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <util/MeshBuilder.hpp>

#include <core/math/Triangle.hpp>

#include <rendering/Mesh.hpp>

#include <scene/util/VoxelOctree.hpp>

namespace Hyperion {

const Array<Vertex> MeshBuilder::quadVertices = {
    Vertex { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
    Vertex { { 1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
    Vertex { { 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
    Vertex { { -1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } }
};

const Array<uint32> MeshBuilder::quadIndices = {
    0, 3, 2,
    0, 2, 1
};

const Array<Vertex> MeshBuilder::cubeVertices = {
    Vertex { { -1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } },
    Vertex { { -1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } },
    Vertex { { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } },

    Vertex { { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } },
    Vertex { { -1.0f, -1.0f, 1.0f }, { 1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } },
    Vertex { { -1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } },

    Vertex { { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
    Vertex { { -1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
    Vertex { { -1.0f, -1.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },

    Vertex { { -1.0f, -1.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
    Vertex { { 1.0f, -1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
    Vertex { { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },

    Vertex { { 1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
    Vertex { { 1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
    Vertex { { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },

    Vertex { { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
    Vertex { { 1.0f, -1.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
    Vertex { { 1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },

    Vertex { { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
    Vertex { { -1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
    Vertex { { 1.0f, 1.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },

    Vertex { { 1.0f, 1.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
    Vertex { { 1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
    Vertex { { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },

    Vertex { { 1.0f, 1.0f, -1.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
    Vertex { { -1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
    Vertex { { -1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },

    Vertex { { -1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
    Vertex { { 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
    Vertex { { 1.0f, 1.0f, -1.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },

    Vertex { { -1.0f, -1.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } },
    Vertex { { -1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } },
    Vertex { { 1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } },

    Vertex { { 1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } },
    Vertex { { 1.0f, -1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } },
    Vertex { { -1.0f, -1.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } }
};

Handle<Mesh> MeshBuilder::Quad()
{
    const VertexAttributeSet vertexAttributes = VertexAttribute::Position
        | VertexAttribute::Normal
        | VertexAttribute::TexCoord0;

    MeshDesc meshDesc;
    meshDesc.meshAttributes.vertexAttributes = vertexAttributes;
    meshDesc.numIndices = uint32(quadIndices.Size());
    meshDesc.numVertices = uint32(quadVertices.Size());

    MeshData meshData;
    meshData.vertexData = quadVertices;
    meshData.indexData.SetSize(quadIndices.Size() * sizeof(uint32));
    meshData.indexData.Write(quadIndices.Size() * sizeof(uint32), 0, quadIndices.Data());

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetMeshData(meshDesc, meshData);
    mesh->SetName(NAME("MeshBuilder_Quad"));

    return mesh;
}

Handle<Mesh> MeshBuilder::Cube(bool originOnBottom)
{
    static const auto s_cubeVerticesAndIndices = Mesh::CalculateIndices(cubeVertices);

    MeshDesc meshDesc;
    meshDesc.meshAttributes.vertexAttributes = VertexAttributeSet::StaticMeshVertexAttributes;
    meshDesc.numIndices = uint32(s_cubeVerticesAndIndices.second.Size());
    meshDesc.numVertices = uint32(s_cubeVerticesAndIndices.first.Size());

    MeshData meshData;
    meshData.vertexData = s_cubeVerticesAndIndices.first;

    if (originOnBottom)
    {
        for (Vertex& vertex : meshData.vertexData)
        {
            vertex.position.y += 1.0f;
        }
    }

    meshData.indexData.SetSize(s_cubeVerticesAndIndices.second.Size() * sizeof(uint32));
    meshData.indexData.Write(s_cubeVerticesAndIndices.second.Size() * sizeof(uint32), 0, s_cubeVerticesAndIndices.second.Data());

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetName(NAME("MeshBuilder_Cube"));
    mesh->SetMeshData(meshDesc, meshData);

    return mesh;
}

Handle<Mesh> MeshBuilder::NormalizedCubeSphere(uint32 numDivisions)
{
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

    Array<Vertex> vertices;
    Array<uint32> indices;

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

                vertices.PushBack(Vertex(position, uv));
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
                    indices.PushBack(c);
                    indices.PushBack(b);
                    indices.PushBack(c);
                    indices.PushBack(d);
                    indices.PushBack(b);
                }
                else
                {
                    indices.PushBack(a);
                    indices.PushBack(c);
                    indices.PushBack(d);
                    indices.PushBack(a);
                    indices.PushBack(d);
                    indices.PushBack(b);
                }
            }
        }
    }

    MeshDesc meshDesc;
    meshDesc.meshAttributes.vertexAttributes = VertexAttributeSet::StaticMeshVertexAttributes;
    meshDesc.numIndices = uint32(indices.Size());
    meshDesc.numVertices = uint32(vertices.Size());

    MeshData meshData;
    meshData.vertexData = std::move(vertices);
    meshData.indexData.SetSize(indices.Size() * sizeof(uint32));
    meshData.indexData.Write(indices.Size() * sizeof(uint32), 0, indices.Data());

    meshData.CalculateNormals(true);

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetMeshData(meshDesc, meshData);
    mesh->SetName(NAME("MeshBuilder_NormalizedCubeSphere"));

    return mesh;
}

Handle<Mesh> MeshBuilder::ApplyTransform(const Mesh* mesh, const Transform& transform)
{
    Assert(mesh != nullptr);

    if (!mesh->GetAsset())
    {
        return Handle<Mesh> {};
    }

    ResourceGuard resGuard;

    if (mesh->GetAsset()->IsRegistered())
    {
        resGuard = ResourceGuard(*mesh->GetAsset()->GetResource());
    }

    const Mat4f modelMatrix = transform.GetMatrix();
    const Mat4f normalMatrix = modelMatrix.Inverted().Transposed();

    const MeshDesc& meshDesc = mesh->GetAsset()->GetMeshDesc();

    MeshData* meshData = mesh->GetAsset()->GetMeshData();
    Assert(meshData != nullptr);

    MeshData newMeshData = *meshData;

    resGuard.Reset();

    for (Vertex& vertex : newMeshData.vertexData)
    {
        vertex.SetPosition(modelMatrix * vertex.GetPosition());
        vertex.SetNormal(normalMatrix * vertex.GetNormal());
        vertex.SetTangent(normalMatrix * vertex.GetTangent());
        vertex.SetBitangent(normalMatrix * vertex.GetBitangent());
    }

    Handle<Mesh> newMesh = MakeHandle<Mesh>();
    newMesh->SetMeshData(meshDesc, newMeshData);
    newMesh->SetName(mesh->GetName());

    return newMesh;
}

Handle<Mesh> MeshBuilder::Merge(const Mesh* a, const Mesh* b, const Transform& aTransform, const Transform& bTransform)
{
    Assert(a != nullptr && a->GetAsset().IsValid());
    Assert(b != nullptr && b->GetAsset().IsValid());

    Handle<Mesh> transformedMeshes[] = {
        ApplyTransform(a, aTransform),
        ApplyTransform(b, bTransform)
    };

    ResourceGuard resourceHandles[] = {
        transformedMeshes[0]->GetAsset()->IsRegistered() ? ResourceGuard(*transformedMeshes[0]->GetAsset()->GetResource()) : ResourceGuard(),
        transformedMeshes[1]->GetAsset()->IsRegistered() ? ResourceGuard(*transformedMeshes[1]->GetAsset()->GetResource()) : ResourceGuard()
    };

    MeshDesc const* meshDescs[] = {
        &transformedMeshes[0]->GetAsset()->GetMeshDesc(),
        &transformedMeshes[1]->GetAsset()->GetMeshDesc()
    };

    MeshData const* meshDatas[] = {
        transformedMeshes[0]->GetAsset()->GetMeshData(),
        transformedMeshes[1]->GetAsset()->GetMeshData()
    };

    Assert(meshDatas[0] != nullptr);
    Assert(meshDatas[1] != nullptr);

    const VertexAttributeSet mergedVertexAttributes = a->GetVertexAttributes() | b->GetVertexAttributes();

    Array<Vertex> allVertices;
    allVertices.Resize(meshDatas[0]->vertexData.Size() + meshDatas[1]->vertexData.Size());

    Array<uint32> allIndices;
    allIndices.Resize(
        (meshDatas[0]->indexData.Size() / GpuElemTypeSize(meshDescs[0]->meshAttributes.indexBufferElemType))
        + (meshDatas[1]->indexData.Size() / GpuElemTypeSize(meshDescs[1]->meshAttributes.indexBufferElemType)));

    SizeType vertexOffset = 0;
    SizeType indexOffset = 0;

    for (int meshIndex = 0; meshIndex < 2; meshIndex++)
    {
        const SizeType vertexOffsetBefore = vertexOffset;

        for (SizeType i = 0; i < meshDatas[meshIndex]->vertexData.Size(); i++)
        {
            allVertices[vertexOffset++] = meshDatas[meshIndex]->vertexData[i];
        }

        const uint32 stride = GpuElemTypeSize(meshDescs[meshIndex]->meshAttributes.indexBufferElemType);

        for (SizeType i = 0; i < meshDatas[meshIndex]->indexData.Size() / stride; i++)
        {
            allIndices[indexOffset++] = meshDatas[meshIndex]->indexData.Data()[i * stride] + vertexOffsetBefore;
        }
    }

    for (ResourceGuard& resGuard : resourceHandles)
    {
        resGuard.Reset();
    }

    MeshDesc mergedMeshDesc;
    mergedMeshDesc.meshAttributes.indexBufferElemType = GET_UNSIGNED_INT;
    mergedMeshDesc.meshAttributes.vertexAttributes = mergedVertexAttributes;
    mergedMeshDesc.numIndices = uint32(allIndices.Size());
    mergedMeshDesc.numVertices = uint32(allVertices.Size());

    MeshData mergedMeshData;
    mergedMeshData.vertexData = std::move(allVertices);
    mergedMeshData.indexData.SetSize(allIndices.Size() * sizeof(uint32));
    mergedMeshData.indexData.Write(allIndices.Size() * sizeof(uint32), 0, allIndices.Data());

    Handle<Mesh> newMesh = MakeHandle<Mesh>();
    newMesh->SetMeshData(mergedMeshDesc, mergedMeshData);
    newMesh->SetName(NAME("MeshBuilder_MergedMesh"));

    return newMesh;
}

Handle<Mesh> MeshBuilder::Merge(const Mesh* a, const Mesh* b)
{
    return Merge(a, b, Transform(), Transform());
}

Handle<Mesh> MeshBuilder::BuildVoxelMesh(const VoxelOctree& voxelOctree)
{
    static const auto cubeVerticesAndIndices = Mesh::CalculateIndices(cubeVertices);

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

    Array<Vertex> vertices;
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
                Vertex vert;
                vert.position = corners[faceCornerIdx[f][i]];
                vertices.PushBack(vert);
            }
            for (int k = 0; k < 6; ++k)
                indices.PushBack(vertexOffset + idxPattern[k]);
            vertexOffset += 4;
        }
    }

    MeshDesc meshDesc;
    meshDesc.meshAttributes.vertexAttributes = VertexAttributeSet::StaticMeshVertexAttributes;
    meshDesc.numIndices = (uint32)indices.Size();
    meshDesc.numVertices = (uint32)vertices.Size();
    meshDesc.meshAttributes.indexBufferElemType = GET_UNSIGNED_INT;
    meshDesc.meshAttributes.topology = TOP_LINES;

    MeshData meshData;
    meshData.vertexData = std::move(vertices);
    meshData.indexData.SetSize(indices.Size() * sizeof(uint32));
    meshData.indexData.Write(indices.Size() * sizeof(uint32), 0, indices.Data());

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetMeshData(meshDesc, meshData);
    mesh->SetName(NAME("MeshBuilder_VoxelMesh"));

    return mesh;
}

} // namespace Hyperion
