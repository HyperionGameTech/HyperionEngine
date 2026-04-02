/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <baking/lightmap_volume/LightmapVolumeBakeData.hpp>

#include <rendering/Mesh.hpp>

#include <scene/LightmapVolume.hpp>

#ifdef HYP_XATLAS
#include <xatlas.h>
#endif

namespace Hyperion {

namespace Baking {

BakeData<LightmapVolume>::BakeData(Span<const BakeEntity> bakeEntities, LightmapVolume* volume)
    : BakeDataBase(bakeEntities),
      m_volume(volume),
      m_meshVertexPositions(bakeEntities.Size()),
      m_meshVertexNormals(bakeEntities.Size()),
      m_meshVertexUvs(bakeEntities.Size()),
      m_meshIndices(bakeEntities.Size())
{
    // Output mesh data - this will be where we output the computed UVs to be used for tracing
    m_meshData.Resize(bakeEntities.Size());

    for (size_t i = 0; i < bakeEntities.Size(); i++)
    {
        const BakeEntity& bakeEntity = bakeEntities[i];

        BakeMesh& bakeMesh = m_meshData[i];

        if (!bakeEntity.mesh)
        {
            HYP_LOG(Lightmap, Warning, "Sub-element {} has no mesh, skipping", i);

            continue;
        }

        const Handle<Mesh>& mesh = bakeEntity.mesh;

        auto resGuard = mesh->GetReadScope();

        if (!resGuard)
        {
            return;
        }

        const MeshDesc& meshDesc = mesh->GetMeshDesc();

        const VertexArrayView vertexData = mesh->GetVertexData();
        const Span<const ubyte> indexData = mesh->GetIndexData();

        bakeMesh.mesh = bakeEntity.mesh;
        bakeMesh.material = bakeEntity.material;
        bakeMesh.transformMatrix = bakeEntity.transformMatrix;

        m_meshVertexPositions[i].Resize(vertexData.vertexCount * 3);
        m_meshVertexNormals[i].Resize(vertexData.vertexCount * 3);
        m_meshVertexUvs[i].Resize(vertexData.vertexCount * 2);

        const size_t indexSize = GpuElemTypeSize(meshDesc.meshAttributes.indexBufferElemType);
        const size_t numIndices = indexData.Size() / indexSize;

        m_meshIndices[i].Resize(numIndices);

        if (indexSize == sizeof(uint32))
        {
            Memory::Copy(m_meshIndices[i].Data(), indexData.Data(), numIndices * indexSize);
        }
        else
        {
            for (size_t j = 0; j < numIndices * indexSize; j += indexSize)
            {
                Memory::Copy(&m_meshIndices[i][j / indexSize], indexData.Data() + j, MathUtil::Min(indexSize, sizeof(uint32)));
            }
        }

        const Mat4f modelMatrix = bakeEntity.transformMatrix;
        const Mat4f normalMatrix = modelMatrix.Inverse().Transpose();

        const size_t vertexSizeInFloats = vertexData.layoutDesc.VertexSize() / sizeof(float);

        for (size_t vertexIndex = 0; vertexIndex < vertexData.vertexCount; vertexIndex++)
        {
            const float* srcVertexOffset = vertexData.floatData + (i * vertexSizeInFloats);

            size_t offset = 0;

            Vec3f position;
            Vec3f normal;
            Vec2f uv0;
            
            if (vertexData.layoutDesc.mask & VT_Position)
            {
                const TVertexPacket<VT_Position>* packet = reinterpret_cast<const TVertexPacket<VT_Position>*>(srcVertexOffset + offset);
                position = modelMatrix * packet->GetPosition();

                offset += sizeof(TVertexPacket<VT_Position>) / sizeof(float);
            }

            if (vertexData.layoutDesc.mask & VT_Normal)
            {
                const TVertexPacket<VT_Normal>* packet = reinterpret_cast<const TVertexPacket<VT_Normal>*>(srcVertexOffset + offset);
                normal = (normalMatrix * Vec4f(packet->GetNormal(), 0.0f)).GetXYZ().Normalize();

                offset += sizeof(TVertexPacket<VT_Normal>) / sizeof(float);
            }

            if (vertexData.layoutDesc.mask & VT_UV0)
            {
                const TVertexPacket<VT_UV0>* packet = reinterpret_cast<const TVertexPacket<VT_UV0>*>(srcVertexOffset + offset);
                uv0 = packet->GetUV0();

                offset += sizeof(TVertexPacket<VT_UV0>) / sizeof(float);
            }

            m_meshVertexPositions[i][vertexIndex * 3] = position.x;
            m_meshVertexPositions[i][vertexIndex * 3 + 1] = position.y;
            m_meshVertexPositions[i][vertexIndex * 3 + 2] = position.z;

            m_meshVertexNormals[i][vertexIndex * 3] = normal.x;
            m_meshVertexNormals[i][vertexIndex * 3 + 1] = normal.y;
            m_meshVertexNormals[i][vertexIndex * 3 + 2] = normal.z;

            m_meshVertexUvs[i][vertexIndex * 2] = uv0.x;
            m_meshVertexUvs[i][vertexIndex * 2 + 1] = uv0.y;
        }
    }
}

Result BakeData<LightmapVolume>::Build()
{
    if (m_meshData.Empty())
    {
        return HYP_MAKE_ERROR(Error, "No mesh data to build lightmap UVs from");
    }

#ifdef HYP_XATLAS
    xatlas::Atlas* atlas = xatlas::Create();

    for (size_t meshIndex = 0; meshIndex < m_meshData.Size(); meshIndex++)
    {
        Assert(meshIndex < m_meshIndices.Size());

        xatlas::MeshDecl meshDecl;
        meshDecl.indexData = m_meshIndices[meshIndex].Data();
        meshDecl.indexFormat = xatlas::IndexFormat::UInt32;
        meshDecl.indexCount = uint32(m_meshIndices[meshIndex].Size());
        meshDecl.vertexCount = uint32(m_meshVertexPositions[meshIndex].Size() / 3);
        meshDecl.vertexPositionData = m_meshVertexPositions[meshIndex].Data();
        meshDecl.vertexPositionStride = sizeof(float) * 3;
        meshDecl.vertexNormalData = m_meshVertexNormals[meshIndex].Data();
        meshDecl.vertexNormalStride = sizeof(float) * 3;
        meshDecl.vertexUvData = m_meshVertexUvs[meshIndex].Data();
        meshDecl.vertexUvStride = sizeof(float) * 2;

        xatlas::AddMeshError error = xatlas::AddMesh(atlas, meshDecl);

        if (error != xatlas::AddMeshError::Success)
        {
            xatlas::Destroy(atlas);

            return HYP_MAKE_ERROR(Error, "Error adding mesh: {}", 0, xatlas::StringForEnum(error));
        }

        xatlas::AddMeshJoin(atlas);
    }

    xatlas::PackOptions packOptions {};
    packOptions.maxChartSize = 2048;
    packOptions.texelsPerUnit = 16.0f;
    packOptions.bilinear = true;

    xatlas::ComputeCharts(atlas);
    xatlas::PackCharts(atlas, packOptions);

    // write lightmap data
    dimensions.x = atlas->width;
    dimensions.y = atlas->height;
    dimensions.z = 1;

    texels.Resize(atlas->width * atlas->height);
    m_rays.Resize(atlas->width * atlas->height);

    for (uint32 meshIndex = 0; meshIndex < atlas->meshCount; meshIndex++)
    {
        BakeMesh& bakeMesh = m_meshData[meshIndex];

        const Mat4f& transform = bakeMesh.transformMatrix;
        const Mat4f inverseTransform = transform.Inverse();
        const Mat4f normalMatrix = transform.Inverse().Transpose();
        const Mat4f inverseNormalMatrix = normalMatrix.Inverse();

        MeshIndexArray& currentUvIndices = meshToUvIndices[bakeMesh.mesh->Id()];

        const xatlas::Mesh& atlasMesh = atlas->meshes[meshIndex];

        Assert(m_meshIndices[meshIndex].Size() == atlasMesh.indexCount,
            "Mesh index size does not match atlas mesh index count! Mesh index count: {}, Atlas index count: {}",
            m_meshIndices[meshIndex].Size(), atlasMesh.indexCount);

        for (uint32 i = 0; i < atlasMesh.indexCount; i += 3)
        {
            bool skip = false;
            int atlasIndex = -1;
            FixedArray<Pair<uint32, Vec2i>, 3> verts;

            for (uint32 j = 0; j < 3; j++)
            {
                // Get UV coordinates for each edge
                const xatlas::Vertex& v = atlasMesh.vertexArray[atlasMesh.indexArray[i + j]];

                if (v.atlasIndex == -1)
                {
                    skip = true;

                    break;
                }

                atlasIndex = v.atlasIndex;

                verts[j] = { v.xref, { int(v.uv[0]), int(v.uv[1]) } };
            }

            if (skip)
            {
                continue;
            }

            const Vec2i pts[3] = { verts[0].second, verts[1].second, verts[2].second };

            const Vec2i clamp { int(dimensions.x - 1), int(dimensions.y - 1) };

            Vec2i bboxmin { int(dimensions.x - 1), int(dimensions.y - 1) };
            Vec2i bboxmax { 0, 0 };

            for (int j = 0; j < 3; j++)
            {
                bboxmin.x = MathUtil::Max(0, MathUtil::Min(bboxmin.x, pts[j].x));
                bboxmin.y = MathUtil::Max(0, MathUtil::Min(bboxmin.y, pts[j].y));

                bboxmax.x = MathUtil::Min(clamp.x, MathUtil::Max(bboxmax.x, pts[j].x));
                bboxmax.y = MathUtil::Min(clamp.y, MathUtil::Max(bboxmax.y, pts[j].y));
            }

            currentUvIndices.Reserve(currentUvIndices.Size() + (bboxmax.x - bboxmin.x + 1) * (bboxmax.y - bboxmin.y + 1));

            Vec2i point;

            for (point.x = bboxmin.x; point.x <= bboxmax.x; point.x++)
            {
                for (point.y = bboxmin.y; point.y <= bboxmax.y; point.y++)
                {
                    const Vec3f barycentricCoords = MathUtil::CalculateBarycentricCoordinates(Vec2f(pts[0]), Vec2f(pts[1]), Vec2f(pts[2]), Vec2f(point));

                    if (barycentricCoords.x < 0 || barycentricCoords.y < 0 || barycentricCoords.z < 0)
                    {
                        continue;
                    }

                    const uint32 triangleIndex = i / 3;

                    const uint32 triangleIndices[3] = {
                        m_meshIndices[meshIndex][triangleIndex * 3 + 0],
                        m_meshIndices[meshIndex][triangleIndex * 3 + 1],
                        m_meshIndices[meshIndex][triangleIndex * 3 + 2]
                    };

                    const Vec3f vertexPositions[3] = {
                        Vec3f(m_meshVertexPositions[meshIndex][triangleIndices[0] * 3], m_meshVertexPositions[meshIndex][triangleIndices[0] * 3 + 1], m_meshVertexPositions[meshIndex][triangleIndices[0] * 3 + 2]),
                        Vec3f(m_meshVertexPositions[meshIndex][triangleIndices[1] * 3], m_meshVertexPositions[meshIndex][triangleIndices[1] * 3 + 1], m_meshVertexPositions[meshIndex][triangleIndices[1] * 3 + 2]),
                        Vec3f(m_meshVertexPositions[meshIndex][triangleIndices[2] * 3], m_meshVertexPositions[meshIndex][triangleIndices[2] * 3 + 1], m_meshVertexPositions[meshIndex][triangleIndices[2] * 3 + 2])
                    };

                    const Vec3f vertexNormals[3] = {
                        (inverseNormalMatrix * Vec4f(Vec3f(m_meshVertexNormals[meshIndex][triangleIndices[0] * 3], m_meshVertexNormals[meshIndex][triangleIndices[0] * 3 + 1], m_meshVertexNormals[meshIndex][triangleIndices[0] * 3 + 2]), 0.0f)).GetXYZ(),
                        (inverseNormalMatrix * Vec4f(Vec3f(m_meshVertexNormals[meshIndex][triangleIndices[0] * 3], m_meshVertexNormals[meshIndex][triangleIndices[0] * 3 + 1], m_meshVertexNormals[meshIndex][triangleIndices[0] * 3 + 2]), 0.0f)).GetXYZ(),
                        (inverseNormalMatrix * Vec4f(Vec3f(m_meshVertexNormals[meshIndex][triangleIndices[0] * 3], m_meshVertexNormals[meshIndex][triangleIndices[0] * 3 + 1], m_meshVertexNormals[meshIndex][triangleIndices[0] * 3 + 2]), 0.0f)).GetXYZ(),
                    };

                    const Vec3f position = vertexPositions[0] * barycentricCoords.x
                        + vertexPositions[1] * barycentricCoords.y
                        + vertexPositions[2] * barycentricCoords.z;

                    const Vec3f normal = (normalMatrix * Vec4f((vertexNormals[0] * barycentricCoords.x + vertexNormals[1] * barycentricCoords.y + vertexNormals[2] * barycentricCoords.z), 0.0f)).GetXYZ().Normalize();

                    const uint32 texelIdx = (point.x + atlas->width) % atlas->width
                        + (atlas->height - point.y + atlas->height) % atlas->height * atlas->width;

                    LightmapRay& ray = m_rays[texelIdx];
                    ray = LightmapRay {
                        Ray { position, normal },
                        bakeMesh.mesh->Id(),
                        triangleIndex,
                        texelIdx
                    };

                    LightmapTexel& texel = texels[texelIdx];
                    texel.pRay = &ray;

                    currentUvIndices.PushBack(texelIdx);
                }
            }
        }
    }

    for (size_t meshIndex = 0; meshIndex < m_meshData.Size(); meshIndex++)
    {
        BakeMesh& bakeMesh = m_meshData[meshIndex];
        bakeMesh.vertices.Resize(atlas->meshes[meshIndex].vertexCount);
        bakeMesh.indices.Resize(atlas->meshes[meshIndex].indexCount);

        const Mat4f inverseTransform = bakeMesh.transformMatrix.Inverse();
        const Mat4f normalMatrix = bakeMesh.transformMatrix.Inverse().Transpose();
        const Mat4f inverseNormalMatrix = normalMatrix.Inverse();

        for (uint32 j = 0; j < atlas->meshes[meshIndex].indexCount; j++)
        {
            bakeMesh.indices[j] = atlas->meshes[meshIndex].indexArray[j];

            const uint32 vertexIndex = atlas->meshes[meshIndex].vertexArray[atlas->meshes[meshIndex].indexArray[j]].xref;
            const Vec2f uv = {
                atlas->meshes[meshIndex].vertexArray[atlas->meshes[meshIndex].indexArray[j]].uv[0],
                atlas->meshes[meshIndex].vertexArray[atlas->meshes[meshIndex].indexArray[j]].uv[1]
            };

            BakeVertex& vertex = bakeMesh.vertices[bakeMesh.indices[j]];

            vertex.SetPosition(inverseTransform * Vec3f(m_meshVertexPositions[meshIndex][vertexIndex * 3], m_meshVertexPositions[meshIndex][vertexIndex * 3 + 1], m_meshVertexPositions[meshIndex][vertexIndex * 3 + 2]));
            vertex.SetNormal((inverseNormalMatrix * Vec4f(m_meshVertexNormals[meshIndex][vertexIndex * 3], m_meshVertexNormals[meshIndex][vertexIndex * 3 + 1], m_meshVertexNormals[meshIndex][vertexIndex * 3 + 2], 0.0f)).GetXYZ());
            vertex.SetUV0(Vec2f(m_meshVertexUvs[meshIndex][vertexIndex * 2], m_meshVertexUvs[meshIndex][vertexIndex * 2 + 1]));
            vertex.SetUV1(uv / (Vec2f { float(atlas->width), float(atlas->height) } + Vec2f(0.5f)));
        }

        // Deallocate memory for data that is no longer needed.
        m_meshVertexPositions[meshIndex].Clear();
        m_meshVertexPositions[meshIndex].Refit();

        m_meshVertexNormals[meshIndex].Clear();
        m_meshVertexNormals[meshIndex].Refit();

        m_meshVertexUvs[meshIndex].Clear();
        m_meshVertexUvs[meshIndex].Refit();

        m_meshIndices[meshIndex].Clear();
        m_meshIndices[meshIndex].Refit();
    }

    xatlas::Destroy(atlas);

    return {};
#else
    return HYP_MAKE_ERROR(Error, "No method to build lightmap");
#endif
}

auto BakeData<LightmapVolume>::ToBitmapIrradiance() const -> BitmapType
{
    Assert(texels.Size() == dimensions.x * dimensions.y, "Invalid UV map size");

    BitmapType bitmap(dimensions.x, dimensions.y);

    for (uint32 x = 0; x < dimensions.x; x++)
    {
        for (uint32 y = 0; y < dimensions.y; y++)
        {
            const uint32 index = x + y * dimensions.x;

            Vec4f color = texels[index].color0;

            if (color.w <= 0.0f)
            {
                continue;
            }

            color /= color.w;

            AssertDebug(!MathUtil::IsNaN(color));

            bitmap.GetPixelReference(x, y).SetRGBA(color);
        }
    }

    return bitmap;
}

auto BakeData<LightmapVolume>::ToBitmapRadiance() const -> BitmapType
{
    Assert(texels.Size() == dimensions.x * dimensions.y, "Invalid UV map size");

    BitmapType bitmap(dimensions.x, dimensions.y);

    for (uint32 x = 0; x < dimensions.x; x++)
    {
        for (uint32 y = 0; y < dimensions.y; y++)
        {
            const uint32 index = x + y * dimensions.x;

            Vec4f color = texels[index].color1;

            if (color.w <= 0.0f)
            {
                continue;
            }

            color /= color.w;

            AssertDebug(!MathUtil::IsNaN(color));

            bitmap.GetPixelReference(x, y).SetRGBA(color);
        }
    }

    return bitmap;
}

} // namespace Baking

} // namespace Hyperion
