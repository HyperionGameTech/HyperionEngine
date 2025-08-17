/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#include <rendering/Mesh.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#ifdef HYP_XATLAS
#include <xatlas.h>
#endif

namespace hyperion {

#pragma region LightmapUVMap

Bitmap_RGBA16F LightmapUVMap::ToBitmapRadiance() const
{
    Assert(uvs.Size() == width * height, "Invalid UV map size");

    Bitmap_RGBA16F bitmap(width, height);

    for (uint32 x = 0; x < width; x++)
    {
        for (uint32 y = 0; y < height; y++)
        {
            const uint32 index = x + y * width;

            Vec4f color = uvs[index].radiance;

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

Bitmap_RGBA16F LightmapUVMap::ToBitmapIrradiance() const
{
    Assert(uvs.Size() == width * height, "Invalid UV map size");

    Bitmap_RGBA16F bitmap(width, height);

    for (uint32 x = 0; x < width; x++)
    {
        for (uint32 y = 0; y < height; y++)
        {
            const uint32 index = x + y * width;

            Vec4f color = uvs[index].irradiance;

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

#pragma endregion LightmapUVMap

#pragma region LightmapUVBuilder

LightmapUVBuilder::LightmapUVBuilder(const LightmapUVBuilderParams& params)
    : m_params(params),
      m_meshVertexPositions(m_params.subElements.Size()),
      m_meshVertexNormals(m_params.subElements.Size()),
      m_meshVertexUvs(m_params.subElements.Size()),
      m_meshIndices(m_params.subElements.Size())
{
    // Output mesh data - this will be where we output the computed UVs to be used for tracing
    m_meshData.Resize(m_params.subElements.Size());

    for (SizeType i = 0; i < m_params.subElements.Size(); i++)
    {
        const LightmapSubElement& subElement = m_params.subElements[i];

        LightmapMeshData& lightmapMeshData = m_meshData[i];

        if (!subElement.mesh)
        {
            HYP_LOG(Lightmap, Warning, "Sub-element {} has no mesh, skipping", i);

            continue;
        }

        const Handle<Mesh>& mesh = subElement.mesh;

        if (!mesh->GetAsset())
        {
            HYP_LOG(Lightmap, Error, "Sub-element {} has no streamed mesh data, skipping", i);

            continue;
        }

        ResourceHandle resourceHandle(*mesh->GetAsset()->GetResource());

        if (!resourceHandle)
        {
            return;
        }

        MeshData meshData = *mesh->GetAsset()->GetMeshData();

        lightmapMeshData.mesh = subElement.mesh;
        lightmapMeshData.material = subElement.material;
        lightmapMeshData.transform = subElement.transform.GetMatrix();

        m_meshVertexPositions[i].Resize(meshData.vertexData.Size() * 3);
        m_meshVertexNormals[i].Resize(meshData.vertexData.Size() * 3);
        m_meshVertexUvs[i].Resize(meshData.vertexData.Size() * 2);

        const SizeType indexSize = GpuElemTypeSize(meshData.desc.meshAttributes.indexBufferElemType);

        m_meshIndices[i].Resize(meshData.indexData.Size() / indexSize);

        if (indexSize == sizeof(uint32))
        {
            Memory::MemCpy(m_meshIndices[i].Data(), meshData.indexData.Data(), meshData.indexData.Size());
        }
        else
        {
            for (SizeType j = 0; j < meshData.indexData.Size(); j += indexSize)
            {
                Memory::MemCpy(&m_meshIndices[i][j / indexSize], meshData.indexData.Data() + j, MathUtil::Min(indexSize, sizeof(uint32)));
            }
        }

        const Matrix4 normalMatrix = subElement.transform.GetMatrix().Inverted().Transpose();

        for (SizeType vertexIndex = 0; vertexIndex < meshData.vertexData.Size(); vertexIndex++)
        {
            const Vec3f position = subElement.transform.GetMatrix() * meshData.vertexData[vertexIndex].GetPosition();
            const Vec3f normal = (normalMatrix * Vec4f(meshData.vertexData[vertexIndex].GetNormal(), 0.0f)).GetXYZ().Normalize();
            const Vec2f uv = meshData.vertexData[vertexIndex].GetTexCoord0();

            m_meshVertexPositions[i][vertexIndex * 3] = position.x;
            m_meshVertexPositions[i][vertexIndex * 3 + 1] = position.y;
            m_meshVertexPositions[i][vertexIndex * 3 + 2] = position.z;

            m_meshVertexNormals[i][vertexIndex * 3] = normal.x;
            m_meshVertexNormals[i][vertexIndex * 3 + 1] = normal.y;
            m_meshVertexNormals[i][vertexIndex * 3 + 2] = normal.z;

            m_meshVertexUvs[i][vertexIndex * 2] = uv.x;
            m_meshVertexUvs[i][vertexIndex * 2 + 1] = uv.y;
        }
    }
}

TResult<LightmapUVMap> LightmapUVBuilder::Build()
{
    if (m_meshData.Empty())
    {
        return HYP_MAKE_ERROR(Error, "No mesh data to build lightmap UVs from");
    }

    LightmapUVMap uvMap;

#ifdef HYP_XATLAS
    xatlas::Atlas* atlas = xatlas::Create();

    for (SizeType meshIndex = 0; meshIndex < m_meshData.Size(); meshIndex++)
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
    uvMap.width = atlas->width;
    uvMap.height = atlas->height;
    uvMap.uvs.Resize(atlas->width * atlas->height);

    for (uint32 meshIndex = 0; meshIndex < atlas->meshCount; meshIndex++)
    {
        LightmapMeshData& lightmapMeshData = m_meshData[meshIndex];

        const Matrix4& transform = lightmapMeshData.transform;
        const Matrix4 inverseTransform = transform.Inverted();
        const Matrix4 normalMatrix = transform.Inverted().Transpose();
        const Matrix4 inverseNormalMatrix = normalMatrix.Inverted();

        MeshIndexArray& currentUvIndices = uvMap.meshToUvIndices[lightmapMeshData.mesh->Id()];

        const xatlas::Mesh& atlasMesh = atlas->meshes[meshIndex];

        Assert(m_meshIndices[meshIndex].Size() == atlasMesh.indexCount,
            "Mesh index size does not match atlas mesh index count! Mesh index count: %zu, Atlas index count: %u",
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

            const Vec2i clamp { int(uvMap.width - 1), int(uvMap.height - 1) };

            Vec2i bboxmin { int(uvMap.width - 1), int(uvMap.height - 1) };
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

                    Assert(triangleIndices[0] * 3 < m_meshVertexPositions[meshIndex].Size());
                    Assert(triangleIndices[1] * 3 < m_meshVertexPositions[meshIndex].Size());
                    Assert(triangleIndices[2] * 3 < m_meshVertexPositions[meshIndex].Size());

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

                    const uint32 uvIndex = (point.x + atlas->width) % atlas->width
                        + (atlas->height - point.y + atlas->height) % atlas->height * atlas->width;

                    LightmapUV& lightmapUv = uvMap.uvs[uvIndex];
                    lightmapUv.mesh = lightmapMeshData.mesh;
                    lightmapUv.material = lightmapMeshData.material;
                    lightmapUv.transform = lightmapMeshData.transform;
                    lightmapUv.triangleIndex = i / 3;
                    lightmapUv.barycentricCoords = barycentricCoords;
                    lightmapUv.lightmapUv = Vec2f(point) / Vec2f { float(atlas->width), float(atlas->height) };
                    lightmapUv.ray = LightmapRay {
                        Ray { position, normal },
                        lightmapMeshData.mesh->Id(),
                        triangleIndex,
                        uvIndex
                    };

                    currentUvIndices.PushBack(uvIndex);
                }
            }
        }
    }

    for (SizeType meshIndex = 0; meshIndex < m_meshData.Size(); meshIndex++)
    {
        LightmapMeshData& lightmapMeshData = m_meshData[meshIndex];
        lightmapMeshData.vertices.Resize(atlas->meshes[meshIndex].vertexCount);
        lightmapMeshData.indices.Resize(atlas->meshes[meshIndex].indexCount);

        const Matrix4 inverseTransform = lightmapMeshData.transform.Inverted();
        const Matrix4 normalMatrix = lightmapMeshData.transform.Inverted().Transpose();
        const Matrix4 inverseNormalMatrix = normalMatrix.Inverted();

        for (uint32 j = 0; j < atlas->meshes[meshIndex].indexCount; j++)
        {
            lightmapMeshData.indices[j] = atlas->meshes[meshIndex].indexArray[j];

            const uint32 vertexIndex = atlas->meshes[meshIndex].vertexArray[atlas->meshes[meshIndex].indexArray[j]].xref;
            const Vec2f uv = {
                atlas->meshes[meshIndex].vertexArray[atlas->meshes[meshIndex].indexArray[j]].uv[0],
                atlas->meshes[meshIndex].vertexArray[atlas->meshes[meshIndex].indexArray[j]].uv[1]
            };

            Vertex& vertex = lightmapMeshData.vertices[lightmapMeshData.indices[j]];

            vertex.SetPosition(inverseTransform * Vec3f(m_meshVertexPositions[meshIndex][vertexIndex * 3], m_meshVertexPositions[meshIndex][vertexIndex * 3 + 1], m_meshVertexPositions[meshIndex][vertexIndex * 3 + 2]));
            vertex.SetNormal((inverseNormalMatrix * Vec4f(m_meshVertexNormals[meshIndex][vertexIndex * 3], m_meshVertexNormals[meshIndex][vertexIndex * 3 + 1], m_meshVertexNormals[meshIndex][vertexIndex * 3 + 2], 0.0f)).GetXYZ());
            vertex.SetTexCoord0(Vec2f(m_meshVertexUvs[meshIndex][vertexIndex * 2], m_meshVertexUvs[meshIndex][vertexIndex * 2 + 1]));
            vertex.SetTexCoord1(uv / (Vec2f { float(atlas->width), float(atlas->height) } + Vec2f(0.5f)));
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

    return std::move(uvMap);
#else
    return HYP_MAKE_ERROR(Error, "No method to build lightmap");
#endif
}

#pragma endregion LightmapUVBuilder

} // namespace hyperion