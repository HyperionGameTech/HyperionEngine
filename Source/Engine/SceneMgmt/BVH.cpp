/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <scene/BVH.hpp>

#include <BVH.generated.inl>

namespace Hyperion {

HYP_NODISCARD RayTestResults BVHNode::TestRay(
    const Ray& ray,
    const VertexArrayView& vertices,
    Span<const uint32> indices) const
{
    RayTestResults results;

    if (!ray.TestAABB(aabb))
    {
        return results;
    }

    if (IsLeafNode())
    {
        for (size_t t = 0; t < triangleIds.Size(); ++t)
        {
            const uint32 triangleId = triangleIds[t];
            const uint32 i0 = indices[triangleId * 3 + 0];
            const uint32 i1 = indices[triangleId * 3 + 1];
            const uint32 i2 = indices[triangleId * 3 + 2];

            const float* floatDataOffset0 = vertices.floatData + (i0 * (vertices.layoutDesc.VertexSize() / sizeof(float)));
            const float* floatDataOffset1 = vertices.floatData + (i1 * (vertices.layoutDesc.VertexSize() / sizeof(float)));
            const float* floatDataOffset2 = vertices.floatData + (i2 * (vertices.layoutDesc.VertexSize() / sizeof(float)));

            const TVertexPacket<VT_Position>* packet0 = reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset0);
            const TVertexPacket<VT_Position>* packet1 = reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset1);
            const TVertexPacket<VT_Position>* packet2 = reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset2);

            const Triangle tri {
                {
                    packet0->posX, packet0->posY, packet0->posZ,
                    packet1->posX, packet1->posY, packet1->posZ,
                    packet2->posX, packet2->posY, packet2->posZ
                }
            };

            ray.TestTriangle(tri, triangleId, results);
        }
    }
    else
    {
        for (const BVHNode& node : children)
        {
            results.Merge(node.TestRay(ray, vertices, indices));
        }
    }

    return results;
}

HYP_NODISCARD RayTestResults BVHNode::TestRay(
    const Ray& ray,
    Span<const Vec3f> positions,
    Span<const uint32> indices) const
{
    RayTestResults results;

    if (!ray.TestAABB(aabb))
    {
        return results;
    }

    if (IsLeafNode())
    {
        for (size_t t = 0; t < triangleIds.Size(); ++t)
        {
            const uint32 triangleId = triangleIds[t];
            const uint32 i0 = indices[triangleId * 3 + 0];
            const uint32 i1 = indices[triangleId * 3 + 1];
            const uint32 i2 = indices[triangleId * 3 + 2];

            const Triangle tri {
                {
                    positions[i0].x, positions[i0].y, positions[i0].z,
                    positions[i1].x, positions[i1].y, positions[i1].z,
                    positions[i2].x, positions[i2].y, positions[i2].z
                }
            };

            ray.TestTriangle(tri, triangleId, results);
        }
    }
    else
    {
        for (const BVHNode& node : children)
        {
            results.Merge(node.TestRay(ray, positions, indices));
        }
    }

    return results;
}

void BVHNode::QuantizeTriangleData(
    const VertexArrayView& vertices,
    Span<const uint32> indexData,
    ByteBuffer& outQuantizedVertexData,
    ByteBuffer& outQuantizedIndexData)
{
    const size_t numVertices = vertices.vertexCount;
    const size_t quantizedVertexSize = numVertices * sizeof(Vec3f);

    outQuantizedVertexData.SetSize(quantizedVertexSize);

    Vec3f* quantizedVertices = reinterpret_cast<Vec3f*>(outQuantizedVertexData.Data());

    const size_t vertexSizeInFloats = vertices.layoutDesc.VertexSize() / sizeof(float);

    for (size_t i = 0; i < numVertices; i++)
    {
        const float* floatDataOffset = vertices.floatData + (i * vertexSizeInFloats);

        const TVertexPacket<VT_Position>* packet = reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset);

        quantizedVertices[i].x = packet->posX;
        quantizedVertices[i].y = packet->posY;
        quantizedVertices[i].z = packet->posZ;
    }

    // Copy index data directly as uint32
    const size_t numIndices = indexData.Size();
    const size_t indexBufferSize = numIndices * sizeof(uint32);

    outQuantizedIndexData.SetSize(indexBufferSize);
    outQuantizedIndexData.Write(indexBufferSize, 0, indexData.Data());
}

bool BVHNode::OverlapsTriangle(
    const BoundingBox& box,
    const VertexArrayView& vertices,
    Span<const uint32> indices,
    uint32 triangleId)
{
    const uint32 i0 = indices[triangleId * 3 + 0];
    const uint32 i1 = indices[triangleId * 3 + 1];
    const uint32 i2 = indices[triangleId * 3 + 2];

    Assert(i0 < vertices.vertexCount
        && i1 < vertices.vertexCount
        && i2 < vertices.vertexCount);

    const size_t vertexSizeInFloats = vertices.layoutDesc.VertexSize() / sizeof(float);

    const float* floatDataOffset0 = vertices.floatData + (i0 * vertexSizeInFloats);
    const float* floatDataOffset1 = vertices.floatData + (i1 * vertexSizeInFloats);
    const float* floatDataOffset2 = vertices.floatData + (i2 * vertexSizeInFloats);

    const TVertexPacket<VT_Position>* packet0 = reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset0);
    const TVertexPacket<VT_Position>* packet1 = reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset1);
    const TVertexPacket<VT_Position>* packet2 = reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset2);

    const Triangle tri {
        {
            packet0->posX, packet0->posY, packet0->posZ,
            packet1->posX, packet1->posY, packet1->posZ,
            packet2->posX, packet2->posY, packet2->posZ
        }
    };

    return box.OverlapsTriangle(tri);
}

void BVHNode::Split_Internal(
    int depth, int maxDepth,
    const VertexArrayView& vertices,
    Span<const uint32> indices)
{
    if (IsLeafNode() && triangleIds.Any() && depth < maxDepth)
    {
        const Vec3f center = aabb.GetCenter();
        const Vec3f extent = aabb.GetExtent();

        const Vec3f& min = aabb.GetMin();
        const Vec3f& max = aabb.GetMax();

        for (int i = 0; i < 2; i++)
        {
            for (int j = 0; j < 2; j++)
            {
                for (int k = 0; k < 2; k++)
                {
                    const Vec3f newMin(
                        i == 0 ? min.x : center.x,
                        j == 0 ? min.y : center.y,
                        k == 0 ? min.z : center.z);

                    const Vec3f newMax(
                        i == 0 ? center.x : max.x,
                        j == 0 ? center.y : max.y,
                        k == 0 ? center.z : max.z);

                    children.EmplaceBack(BoundingBox(newMin, newMax));
                }
            }
        }

        for (const uint32 triangleId : triangleIds)
        {
            for (BVHNode& node : children)
            {
                if (OverlapsTriangle(node.aabb, vertices, indices, triangleId))
                {
                    node.triangleIds.PushBack(triangleId);
                }
            }
        }

        triangleIds.Clear();
        triangleIds.Refit();

        isLeafNode = false;
    }

    for (BVHNode& node : children)
    {
        node.Split_Internal(depth + 1, maxDepth, vertices, indices);
    }
}

void BVHNode::Shake_Internal()
{
    if (IsLeafNode())
    {
        return;
    }

    for (auto it = children.Begin(); it != children.End();)
    {
        BVHNode& node = *it;

        if (node.IsLeafNode())
        {
            if (node.triangleIds.Empty())
            {
                it = children.Erase(it);
                continue;
            }
        }
        else
        {
            node.Shake_Internal();
        }

        ++it;
    }

    if (children.Empty())
    {
        isLeafNode = true;
    }
}

#pragma region BVHNode Serialization

static void SerializeBVHNodeInto(ByteWriter& writer, const BVHNode& node)
{
    writer.Write(&node.aabb.min, sizeof(Vec3f));
    writer.Write(&node.aabb.max, sizeof(Vec3f));

    const int8 flags = int8(node.isLeafNode);
    writer.Write(&flags, sizeof(int8));

    const uint32 numTriangles = uint32(node.triangleIds.Size());
    writer.Write(&numTriangles, sizeof(uint32));
    for (uint32 i = 0; i < numTriangles; i++)
    {
        writer.Write(&node.triangleIds[i], sizeof(uint32));
    }

    const uint32 numChildren = uint32(node.children.Size());
    writer.Write(&numChildren, sizeof(uint32));
    for (const BVHNode& child : node.children)
    {
        SerializeBVHNodeInto(writer, child);
    }
}

static bool DeserializeBVHNodeFrom(ByteReader& reader, BVHNode& node)
{
    if (reader.Position() + sizeof(Vec3f) * 2 + sizeof(uint32) * 3 > reader.Max())
    {
        return false;
    }

    reader.Read(&node.aabb.min, sizeof(Vec3f));
    reader.Read(&node.aabb.max, sizeof(Vec3f));

    int8 isLeafNode = 0;
    reader.Read(&isLeafNode, sizeof(int8));
    node.isLeafNode = bool(isLeafNode);

    uint32 numTriangles = 0;
    reader.Read(&numTriangles, sizeof(uint32));
    node.triangleIds.Resize(numTriangles);
    for (uint32 i = 0; i < numTriangles; i++)
    {
        reader.Read(&node.triangleIds[i], sizeof(uint32));
    }

    uint32 numChildren = 0;
    reader.Read(&numChildren, sizeof(uint32));
    node.children.Resize(numChildren);
    for (uint32 i = 0; i < numChildren; i++)
    {
        if (!DeserializeBVHNodeFrom(reader, node.children[i]))
        {
            return false;
        }
    }

    return true;
}

ByteBuffer BVHNode::Serialize(const BVHNode& node)
{
    MemoryByteWriter writer;
    SerializeBVHNodeInto(writer, node);
    writer.Close();
    return std::move(writer.GetBuffer());
}

bool BVHNode::Deserialize(BVHNode& outNode, const void* data, size_t size)
{
    MemoryByteReader reader(ConstByteView(reinterpret_cast<const ubyte*>(data), size));
    return DeserializeBVHNodeFrom(reader, outNode);
}

#pragma endregion BVHNode Serialization

} // namespace Hyperion
