/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/asset/MeshAsset.hpp>

#include <core/containers/SparsePagedArray.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <scene/BVH.hpp>

#include <MeshAsset.generated.inl>

namespace Hyperion {

BoundingBox MeshData::CalculateAABB() const
{
    HYP_SCOPE;

    BoundingBox aabb = BoundingBox::Empty();

    for (const Vertex& vertex : vertexData)
    {
        aabb = aabb.Union(vertex.GetPosition());
    }

    return aabb;
}

#define PACKED_SET_ATTR(rawValues, argSize)                                                           \
    do                                                                                                \
    {                                                                                                 \
        Memory::Copy((void*)(floatBuffer + currentOffset), (rawValues), (argSize) * sizeof(float)); \
        currentOffset += (argSize);                                                                   \
    }                                                                                                 \
    while (0)

Array<float> MeshData::BuildVertexBuffer(const VertexAttributeSet& vertexAttributes) const
{
    const SizeType vertexSize = vertexAttributes.CalculateVertexSize();

    Array<float> packedBuffer;
    packedBuffer.Resize(vertexSize * vertexData.Size());

    float* floatBuffer = packedBuffer.Data();
    SizeType currentOffset = 0;

    for (SizeType i = 0; i < vertexData.Size(); i++)
    {
        const Vertex& vertex = vertexData[i];
        /* Offset aligned to the current vertex */
        // currentOffset = i * vertexSize;

        /* Position and normals */
        if (vertexAttributes.Has(VertexAttribute::Position))
            PACKED_SET_ATTR(vertex.GetPosition().values, 3);
        if (vertexAttributes.Has(VertexAttribute::Normal))
            PACKED_SET_ATTR(vertex.GetNormal().values, 3);
        /* Texture coordinates */
        if (vertexAttributes.Has(VertexAttribute::TexCoord0))
            PACKED_SET_ATTR(vertex.GetTexCoord0().values, 2);
        if (vertexAttributes.Has(VertexAttribute::TexCoord1))
            PACKED_SET_ATTR(vertex.GetTexCoord1().values, 2);
        /* Tangents and Bitangents */
        if (vertexAttributes.Has(VertexAttribute::Tangent))
            PACKED_SET_ATTR(vertex.GetTangent().values, 3);
        if (vertexAttributes.Has(VertexAttribute::Bitangent))
            PACKED_SET_ATTR(vertex.GetBitangent().values, 3);

        if (vertexAttributes.Has(VertexAttribute::BoneWeights))
        {
            float weights[4] = {
                vertex.GetBoneWeight(0), vertex.GetBoneWeight(1),
                vertex.GetBoneWeight(2), vertex.GetBoneWeight(3)
            };
            PACKED_SET_ATTR(weights, HYP_ARRAY_SIZE(weights));
        }

        if (vertexAttributes.Has(VertexAttribute::BoneIndices))
        {
            float indices[4] = {
                (float)vertex.GetBoneIndex(0), (float)vertex.GetBoneIndex(1),
                (float)vertex.GetBoneIndex(2), (float)vertex.GetBoneIndex(3)
            };
            PACKED_SET_ATTR(indices, HYP_ARRAY_SIZE(indices));
        }
    }

    return packedBuffer;
}

#undef PACKED_SET_ATTR

Array<PackedVertex> MeshData::BuildPackedVertices() const
{
    HYP_SCOPE;

    Array<PackedVertex> packedVertices;
    packedVertices.Resize(vertexData.Size());

    for (SizeType i = 0; i < vertexData.Size(); i++)
    {
        const Vertex& vertex = vertexData[i];

        packedVertices[i] = PackedVertex {
            .positionX = vertex.GetPosition().x,
            .positionY = vertex.GetPosition().y,
            .positionZ = vertex.GetPosition().z,
            .normalX = vertex.GetNormal().x,
            .normalY = vertex.GetNormal().y,
            .normalZ = vertex.GetNormal().z,
            .texcoord0X = vertex.GetTexCoord0().x,
            .texcoord0Y = vertex.GetTexCoord0().y
        };
    }

    return packedVertices;
}

Array<uint32> MeshData::BuildPackedIndices() const
{
    HYP_SCOPE;

    Assert((indexData.Size() / sizeof(uint32)) % 3 == 0);

    Array<uint32> packedIndices;
    packedIndices.Resize(indexData.Size() / sizeof(uint32));
    Memory::Copy(packedIndices.Data(), indexData.Data(), indexData.Size());

    // Ensure indices are a multiple of 3
    if (packedIndices.Size() % 3 != 0)
    {
        packedIndices.Resize(packedIndices.Size() + (3 - (packedIndices.Size() % 3)));
    }

    // Ensure indices are not empty
    if (packedIndices.Empty())
    {
        packedIndices.Resize(3);
        packedIndices[0] = 0;
        packedIndices[1] = 1;
        packedIndices[2] = 2;
    }

#ifdef HYP_DEBUG_MODE
    for (SizeType i = 0; i < packedIndices.Size(); i++)
    {
        uint32 idx = packedIndices[i];
        AssertDebug(idx < vertexData.Size());
    }
#endif

    return packedIndices;
}

void MeshData::InvertNormals()
{
    HYP_SCOPE;

    for (SizeType i = 0; i < vertexData.Size(); i++)
    {
        vertexData[i].SetNormal(vertexData[i].GetNormal() * -1.0f);
    }
}

#define ADD_NORMAL(ary, idx, normal)     \
    do                                   \
    {                                    \
        auto* idx_it = ary.TryGet(idx);  \
        if (!idx_it)                     \
        {                                \
            idx_it = &*ary.Emplace(idx); \
        }                                \
        idx_it->PushBack(normal);        \
    }                                    \
    while (0)

void MeshData::CalculateNormals(bool weighted)
{
    HYP_SCOPE;

    Assert(indexData.Size() % sizeof(uint32) == 0);

    const SizeType numIndices = indexData.Size() / sizeof(uint32);
    const SizeType numVertices = vertexData.Size();

    uint32* uIndexData = reinterpret_cast<uint32*>(indexData.Data());

    SparsePagedArray<Array<Vec3f, InlineAllocator<3>>, 1 << 6> normals;

    // compute per-face normals (facet normals)
    for (SizeType i = 0; i < numIndices; i += 3)
    {
        const uint32 i0 = uIndexData[i];
        const uint32 i1 = uIndexData[i + 1];
        const uint32 i2 = uIndexData[i + 2];

        const Vec3f& p0 = vertexData[i0].GetPosition();
        const Vec3f& p1 = vertexData[i1].GetPosition();
        const Vec3f& p2 = vertexData[i2].GetPosition();

        const Vec3f u = p2 - p0;
        const Vec3f v = p1 - p0;
        const Vec3f n = v.Cross(u).Normalize();

        ADD_NORMAL(normals, i0, n);
        ADD_NORMAL(normals, i1, n);
        ADD_NORMAL(normals, i2, n);
    }

    for (SizeType i = 0; i < numVertices; i++)
    {
        AssertDebug(normals.HasIndex(uint32(i)));

        if (weighted)
        {
            vertexData[i].SetNormal(normals.Get(uint32(i)).Sum());
        }
        else
        {
            vertexData[i].SetNormal(normals.Get(uint32(i)).Sum().Normalize());
        }
    }

    if (!weighted)
    {
        return;
    }

    normals.Clear();

    // weighted (smooth) normals

    for (SizeType i = 0; i < numIndices; i += 3)
    {
        const uint32 i0 = uIndexData[i];
        const uint32 i1 = uIndexData[i + 1];
        const uint32 i2 = uIndexData[i + 2];

        const Vec3f& p0 = vertexData[i0].GetPosition();
        const Vec3f& p1 = vertexData[i1].GetPosition();
        const Vec3f& p2 = vertexData[i2].GetPosition();

        const Vec3f& n0 = vertexData[i0].GetNormal();
        const Vec3f& n1 = vertexData[i1].GetNormal();
        const Vec3f& n2 = vertexData[i2].GetNormal();

        // Vector3 n = FixedArray { n0, n1, n2 }.Avg();

        FixedArray<Vec3f, 3> weightedNormals { n0, n1, n2 };

        // nested loop through faces to get weighted neighbours
        // any code that uses this really should bake the normals in
        // especially for any production code. this is an expensive process
        for (SizeType j = 0; j < numIndices; j += 3)
        {
            if (j == i)
            {
                continue;
            }

            const uint32 j0 = uIndexData[j];
            const uint32 j1 = uIndexData[j + 1];
            const uint32 j2 = uIndexData[j + 2];

            const FixedArray<Vec3f, 3> facePositions {
                vertexData[j0].GetPosition(),
                vertexData[j1].GetPosition(),
                vertexData[j2].GetPosition()
            };

            const FixedArray<Vec3f, 3> faceNormals {
                vertexData[j0].GetNormal(),
                vertexData[j1].GetNormal(),
                vertexData[j2].GetNormal()
            };

            const Vec3f a = p1 - p0;
            const Vec3f b = p2 - p0;
            const Vec3f c = a.Cross(b);

            const float area = 0.5f * MathUtil::Sqrt(c.Dot(c));

            if (facePositions.Contains(p0))
            {
                const float angle = (p0 - p1).AngleBetween(p0 - p2);
                weightedNormals[0] += faceNormals.Avg() * area * angle;
            }

            if (facePositions.Contains(p1))
            {
                const float angle = (p1 - p0).AngleBetween(p1 - p2);
                weightedNormals[1] += faceNormals.Avg() * area * angle;
            }

            if (facePositions.Contains(p2))
            {
                const float angle = (p2 - p0).AngleBetween(p2 - p1);
                weightedNormals[2] += faceNormals.Avg() * area * angle;
            }

            // if (facePositions.Contains(p0)) {
            //     weightedNormals[0] += faceNormals.Avg();
            // }

            // if (facePositions.Contains(p1)) {
            //     weightedNormals[1] += faceNormals.Avg();
            // }

            // if (facePositions.Contains(p2)) {
            //     weightedNormals[2] += faceNormals.Avg();
            // }
        }

        ADD_NORMAL(normals, i0, weightedNormals[0].Normalized());
        ADD_NORMAL(normals, i1, weightedNormals[1].Normalized());
        ADD_NORMAL(normals, i2, weightedNormals[2].Normalized());
    }

    for (SizeType i = 0; i < numVertices; i++)
    {
        AssertDebug(normals.HasIndex(i));

        vertexData[i].SetNormal(normals.Get(i).Sum().Normalized());
    }

    normals.Clear();
}

#undef ADD_NORMAL

#define ADD_TANGENTS(ary, idx, tangents) \
    do                                   \
    {                                    \
        auto* idx_it = ary.TryGet(idx);  \
        if (!idx_it)                     \
        {                                \
            idx_it = &*ary.Emplace(idx); \
        }                                \
        idx_it->PushBack(tangents);      \
    }                                    \
    while (0)

void MeshData::CalculateTangents()
{
    HYP_SCOPE;

    Assert(indexData.Size() % sizeof(uint32) == 0);

    const SizeType numIndices = indexData.Size() / sizeof(uint32);
    const SizeType numVertices = vertexData.Size();

    uint32* uIndexData = reinterpret_cast<uint32*>(indexData.Data());

    struct TangentBitangentPair
    {
        Vec3f tangent;
        Vec3f bitangent;
    };

    static const Array<TangentBitangentPair, InlineAllocator<1>> placeholderTangentBitangents {};

    SparsePagedArray<Array<TangentBitangentPair, InlineAllocator<1>>, 1 << 6> data;

    for (SizeType i = 0; i < numIndices;)
    {
        const SizeType count = MathUtil::Min(3, numIndices - i);

        Vertex v[3];
        Vec2f uv[3];

        for (uint32 j = 0; j < count; j++)
        {
            v[j] = vertexData[uIndexData[i + j]];
            uv[j] = v[j].GetTexCoord0();
        }

        uint32 i0 = uIndexData[i];
        uint32 i1 = uIndexData[i + 1];
        uint32 i2 = uIndexData[i + 2];

        const Vec3f edge1 = v[1].GetPosition() - v[0].GetPosition();
        const Vec3f edge2 = v[2].GetPosition() - v[0].GetPosition();
        const Vec2f edge1uv = uv[1] - uv[0];
        const Vec2f edge2uv = uv[2] - uv[0];

        const float cp = edge1uv.x * edge2uv.y - edge1uv.y * edge2uv.x;

        if (cp != 0.0f)
        {
            const float mul = 1.0f / cp;

            const TangentBitangentPair tangentBitangent {
                .tangent = ((edge1 * edge2uv.y - edge2 * edge1uv.y) * mul).Normalize(),
                .bitangent = ((edge1 * edge2uv.x - edge2 * edge1uv.x) * mul).Normalize()
            };

            ADD_TANGENTS(data, i0, tangentBitangent);
            ADD_TANGENTS(data, i1, tangentBitangent);
            ADD_TANGENTS(data, i2, tangentBitangent);
        }

        i += count;
    }

    for (SizeType i = 0; i < numVertices; i++)
    {
        const Array<TangentBitangentPair, InlineAllocator<1>>* tangentBitangents = data.TryGet(i);

        if (!tangentBitangents)
        {
            tangentBitangents = &placeholderTangentBitangents;
        }

        // find average
        Vec3f averageTangent, averageBitangent;

        for (const auto& item : *tangentBitangents)
        {
            averageTangent += item.tangent * (1.0f / tangentBitangents->Size());
            averageBitangent += item.bitangent * (1.0f / tangentBitangents->Size());
        }

        averageTangent.Normalize();
        averageBitangent.Normalize();

        vertexData[i].SetTangent(averageTangent);
        vertexData[i].SetBitangent(averageBitangent);
    }
}

#undef ADD_TANGENTS

bool MeshData::BuildBVH(BVHNode& bvhNode, int maxDepth) const
{
    const BoundingBox meshAabb = CalculateAABB();

    Assert(indexData.Size() % sizeof(uint32) == 0);
    Assert((indexData.Size() / sizeof(uint32)) % 3 == 0);

    const SizeType numIndices = indexData.Size() / sizeof(uint32);
    const SizeType numTriangles = numIndices / 3;

    const uint32* indexDataU32 = reinterpret_cast<const uint32*>(indexData.Data());

    bvhNode = BVHNode(meshAabb);
    bvhNode.triangleIds.Reserve(numTriangles);

    for (uint32 triangleId = 0; triangleId < numTriangles; triangleId++)
    {
        bvhNode.AddTriangleId(triangleId);
    }

    // pass mesh spans so Split can do AABB/triangle overlap without copying triangles
    bvhNode.Split(
        maxDepth,
        Span<const Vertex>(vertexData.Data(), vertexData.Size()),
        Span<const uint32>(indexDataU32, numIndices));

    bvhNode.Shake();

    return true;
}

} // namespace Hyperion
