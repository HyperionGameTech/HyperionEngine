/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <lightmapper/LightmapData.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Texture.hpp>

#include <scene/EnvProbe.hpp>
#include <scene/FogVolume.hpp>

#include <scene/util/VoxelOctree.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <util/NoiseFactory.hpp>

#ifdef HYP_XATLAS
#include <xatlas.h>
#endif

namespace hyperion {

#pragma region LightmapData < LightmapVolume>

LightmapData<LightmapVolume>::LightmapData(Span<const LightmapSubElement> subElements, LightmapVolume* volume)
    : LightmapDataBase(subElements),
      m_volume(volume),
      m_meshVertexPositions(subElements.Size()),
      m_meshVertexNormals(subElements.Size()),
      m_meshVertexUvs(subElements.Size()),
      m_meshIndices(subElements.Size())
{
    // Output mesh data - this will be where we output the computed UVs to be used for tracing
    m_meshData.Resize(subElements.Size());

    for (SizeType i = 0; i < subElements.Size(); i++)
    {
        const LightmapSubElement& subElement = subElements[i];

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

        const MeshDesc& meshDesc = mesh->GetAsset()->GetMeshDesc();

        MeshData meshData = *mesh->GetAsset()->GetMeshData();

        lightmapMeshData.mesh = subElement.mesh;
        lightmapMeshData.material = subElement.material;
        lightmapMeshData.transform = subElement.transform.GetMatrix();

        m_meshVertexPositions[i].Resize(meshData.vertexData.Size() * 3);
        m_meshVertexNormals[i].Resize(meshData.vertexData.Size() * 3);
        m_meshVertexUvs[i].Resize(meshData.vertexData.Size() * 2);

        const SizeType indexSize = GpuElemTypeSize(meshDesc.meshAttributes.indexBufferElemType);

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

        const Mat4f modelMatrix = subElement.transform.GetMatrix();
        const Mat4f normalMatrix = modelMatrix.Inverted().Transpose();

        for (SizeType vertexIndex = 0; vertexIndex < meshData.vertexData.Size(); vertexIndex++)
        {
            const Vec3f position = modelMatrix * meshData.vertexData[vertexIndex].GetPosition();
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

Result LightmapData<LightmapVolume>::Build()
{
    if (m_meshData.Empty())
    {
        return HYP_MAKE_ERROR(Error, "No mesh data to build lightmap UVs from");
    }

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
    dimensions.x = atlas->width;
    dimensions.y = atlas->height;
    dimensions.z = 1;

    texels.Resize(atlas->width * atlas->height);
    m_rays.Resize(atlas->width * atlas->height);

    for (uint32 meshIndex = 0; meshIndex < atlas->meshCount; meshIndex++)
    {
        LightmapMeshData& lightmapMeshData = m_meshData[meshIndex];

        const Mat4f& transform = lightmapMeshData.transform;
        const Mat4f inverseTransform = transform.Inverted();
        const Mat4f normalMatrix = transform.Inverted().Transpose();
        const Mat4f inverseNormalMatrix = normalMatrix.Inverted();

        MeshIndexArray& currentUvIndices = meshToUvIndices[lightmapMeshData.mesh->Id()];

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

                    const uint32 texelIdx = (point.x + atlas->width) % atlas->width
                        + (atlas->height - point.y + atlas->height) % atlas->height * atlas->width;

                    LightmapRay& ray = m_rays[texelIdx];
                    ray = LightmapRay {
                        Ray { position, normal },
                        lightmapMeshData.mesh->Id(),
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

    for (SizeType meshIndex = 0; meshIndex < m_meshData.Size(); meshIndex++)
    {
        LightmapMeshData& lightmapMeshData = m_meshData[meshIndex];
        lightmapMeshData.vertices.Resize(atlas->meshes[meshIndex].vertexCount);
        lightmapMeshData.indices.Resize(atlas->meshes[meshIndex].indexCount);

        const Mat4f inverseTransform = lightmapMeshData.transform.Inverted();
        const Mat4f normalMatrix = lightmapMeshData.transform.Inverted().Transpose();
        const Mat4f inverseNormalMatrix = normalMatrix.Inverted();

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

    return {};
#else
    return HYP_MAKE_ERROR(Error, "No method to build lightmap");
#endif
}

auto LightmapData<LightmapVolume>::ToBitmapIrradiance() const -> BitmapType
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

auto LightmapData<LightmapVolume>::ToBitmapRadiance() const -> BitmapType
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

#pragma endregion LightmapData < LightmapVolume>

#pragma region LightmapData<ReflectionProbe>

Result LightmapData<ReflectionProbe>::Build()
{
    Assert(m_envProbe != nullptr);

    // texels need to be 6*resolution^2 in size
    dimensions = Vec3u(m_envProbe->GetDimensions(), 1);

    AssertDebug(dimensions.Volume() > 0 && dimensions.x == dimensions.y,
        "EnvProbe lightmap dimensions must be square and non-zero! Dimensions: {}", dimensions);

    const SizeType numTexelsPerFace = SizeType(dimensions.x) * SizeType(dimensions.y);
    const SizeType numTexelsTotal = 6 * numTexelsPerFace;

    texels.Resize(numTexelsTotal);
    m_rays.Resize(numTexelsTotal);

    const Vec3f origin = m_envProbe->GetWorldTranslation();

    // Use the same face orientation as render code (Texture::s_cubemapDirections)
    for (uint32 face = 0; face < 6; face++)
    {
        const Vec3f forward = Texture::s_cubemapDirections[face].first;
        const Vec3f up = Texture::s_cubemapDirections[face].second * Vec3f(-1.0f);
        const Vec3f right = forward.Cross(up).Normalize();

        for (uint32 y = 0; y < dimensions.y; y++)
        {
            for (uint32 x = 0; x < dimensions.x; x++)
            {
                const uint32 texelIdx = face * uint32(numTexelsPerFace) + y * dimensions.x + x;

                const float u = (float(x) + 0.5f) / float(dimensions.x) * 2.0f - 1.0f;
                const float v = (float(y) + 0.5f) / float(dimensions.y) * 2.0f - 1.0f;

                const Vec3f dir = (forward + right * u + up * v).Normalize();

                LightmapRay& ray = m_rays[texelIdx];
                ray = LightmapRay {
                    Ray { origin, dir },
                    /* meshId */ ObjId<Mesh>::invalid,
                    /* triangleIndex */ ~0u,
                    /* texelIndex */ texelIdx
                };

                LightmapTexel& texel = texels[texelIdx];
                texel.pRay = &ray;
            }
        }
    }

    return {};
}

auto LightmapData<ReflectionProbe>::ToBitmap() const -> BitmapType
{
    Assert(m_envProbe != nullptr);

    const SizeType numTexelsPerFace = dimensions.x * dimensions.y;

    Assert(texels.Size() == 6 * numTexelsPerFace, "Invalid cubemap size");

    BitmapType bitmap(dimensions.x, dimensions.y * 6);

    for (uint32 face = 0; face < 6; face++)
    {
        for (uint32 y = 0; y < dimensions.y; y++)
        {
            for (uint32 x = 0; x < dimensions.x; x++)
            {
                const uint32 texelIdx = face * numTexelsPerFace + y * dimensions.x + x;
                const uint32 bitmapY = face * dimensions.y + y;

                Vec4f color = texels[texelIdx].color0;

                if (color.w <= 0.0f)
                {
                    continue;
                }

                color /= color.w;

                AssertDebug(!MathUtil::IsNaN(color));

                bitmap.GetPixelReference(x, bitmapY).SetRGBA(color);
            }
        }
    }

    return bitmap;
}

#pragma endregion LightmapData < ReflectionProbe>

#pragma region LightmapData < FogVolume>

static struct FogVolumeNoiseCombinator
{
    NoiseCombinator noiseCombinator;

    FogVolumeNoiseCombinator()
    {
        noiseCombinator
            // Base Density
            .Use<SimplexNoiseGenerator>(0, NoiseCombinator::Mode::ADDITIVE, 0.4f, 0.0f, Vec3f(15.0f))
            // Structure (Mid-Frequency)
            .Use<SimplexNoiseGenerator>(1, NoiseCombinator::Mode::ADDITIVE, 0.3f, 0.0f, Vec3f(60.0f))
            // Grain (High-Frequency)
            .Use<SimplexNoiseGenerator>(2, NoiseCombinator::Mode::ADDITIVE, 0.2f, 0.0f, Vec3f(250.0f))
            // Eraser (Subtractive Worley)
            .Use<WorleyNoiseGenerator>(3, NoiseCombinator::Mode::SUBTRACTIVE, 0.2f, 0.0f, Vec3f(300.0f));
    }
} s_initializer;

static void GenerateNoiseBitmap(typename LightmapData<FogVolume>::NoiseBitmap& noiseBitmap)
{
    for (uint32 z = 0; z < noiseBitmap.GetDepth(); z++)
    {
        for (uint32 y = 0; y < noiseBitmap.GetHeight(); y++)
        {
            for (uint32 x = 0; x < noiseBitmap.GetWidth(); x++)
            {
                const float noiseValue = s_initializer.noiseCombinator.GetNoise(
                    Vec3f(
                        float(x) / float(noiseBitmap.GetWidth()),
                        float(y) / float(noiseBitmap.GetHeight()),
                        float(z) / float(noiseBitmap.GetDepth())));

                noiseBitmap.GetPixelReference(x, y, z).SetComponentFloat(0, noiseValue);
            }
        }
    }
}

Result LightmapData<FogVolume>::Build()
{
    Assert(m_fogVolume != nullptr);

    const BoundingBox localBounds = m_fogVolume->GetLocalBounds();
    const Vec3f localBoundsExtent = localBounds.GetExtent();
    const float maxExtent = localBoundsExtent.Max();

    if (maxExtent < MathUtil::epsilonF)
    {
        dimensions = Vec3u::One();
    }
    else
    {
        const float scale = float(FogVolume::MaxVolumeTextureExtent) / maxExtent;
        dimensions = Vec3u(MathUtil::Max(Vec3f(1.0f), MathUtil::Ceil(localBoundsExtent * scale)));
    }

    if (dimensions.Volume() == 0)
    {
        dimensions = Vec3u::One();
    }

    m_volumeBitmap = VolumeBitmap(
        dimensions.x,
        dimensions.y,
        dimensions.z);

    const Vec3f extentWS = m_fogVolume->GetWorldBounds().GetExtent();
    const Vec3f texelSizeWS = extentWS / Vec3f(dimensions);

    texels.Resize(dimensions.Volume());

    BoundingBox voxelOctreeAabb = m_fogVolume->GetWorldBounds();

    if (!voxelOctreeAabb.IsValid() || !voxelOctreeAabb.IsFinite() || voxelOctreeAabb.IsZero())
    {
        return HYP_MAKE_ERROR(Error, "Invalid fog volume AABB for voxel octree build");
    }

    VoxelOctreeParams octreeParams;
    octreeParams.aabb = voxelOctreeAabb;
    octreeParams.allowResize = false;
    octreeParams.maxDepth = 5;

    m_voxelOctree = MakeUnique<VoxelOctree>();

    auto buildResult = m_voxelOctree->Build(octreeParams, m_fogVolume->GetEntityManager());

    if (buildResult.HasError())
    {
        return buildResult.GetError();
    }

    m_noiseBitmap = NoiseBitmap(
        MaxNoiseBitmapExtent,
        MaxNoiseBitmapExtent,
        MaxNoiseBitmapExtent);

    GenerateNoiseBitmap(m_noiseBitmap);

    return {};
}

#pragma endregion LightmapData < FogVolume>

} // namespace hyperion
