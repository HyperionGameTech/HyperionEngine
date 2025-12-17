/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/BVH.hpp>

#include <BVH.generated.inl>

namespace hyperion {

void BVHNode::QuantizeTriangleData(
    Span<const Vertex> vertexData,
    Span<const uint32> indexData,
    ByteBuffer& outQuantizedVertexData,
    ByteBuffer& outQuantizedIndexData)
{
    HYP_SCOPE;

    const SizeType numVertices = vertexData.Size();
    const SizeType quantizedVertexSize = numVertices * sizeof(Vec3f);

    outQuantizedVertexData.SetSize(quantizedVertexSize);

    Vec3f* quantizedVertices = reinterpret_cast<Vec3f*>(outQuantizedVertexData.Data());

    for (SizeType i = 0; i < numVertices; i++)
    {
        quantizedVertices[i] = vertexData[i].GetPosition();
    }

    // Copy index data directly as uint32
    const SizeType numIndices = indexData.Size();
    const SizeType indexBufferSize = numIndices * sizeof(uint32);

    outQuantizedIndexData.SetSize(indexBufferSize);
    outQuantizedIndexData.Write(indexBufferSize, 0, indexData.Data());
}

} // namespace hyperion
