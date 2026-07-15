/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Baking/LightmapVolume/LightmapVolumeBakeData.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>

#include <Scene/LightmapVolume.hpp>

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

        const VertexArrayView vertexData = mesh->GetVertexData(0);
        const Span<const ubyte> indexData = mesh->GetIndexData(0);

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
            const float* srcVertexOffset = vertexData.floatData + (vertexIndex * vertexSizeInFloats);

            size_t offset = 0;

            Vec3f position;
            Vec3f normal;
            Vec2f uv0;

            if (vertexData.layoutDesc.mask & VT_Position)
            {
                const TVertexPacket<VT_Position>* packet = reinterpret_cast<const TVertexPacket<VT_Position>*>(srcVertexOffset + offset);
                position = modelMatrix.TransformVector(packet->GetPosition());

                offset += sizeof(TVertexPacket<VT_Position>) / sizeof(float);
            }

            if (vertexData.layoutDesc.mask & VT_Normal)
            {
                const TVertexPacket<VT_Normal>* packet = reinterpret_cast<const TVertexPacket<VT_Normal>*>(srcVertexOffset + offset);
                normal = (normalMatrix.TransformVector(Vec4f(packet->GetNormal(), 0.0f))).GetXYZ().Normalize();

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
    // xatlas::SetPrint([](const char* str, ...) -> int
    //     {
    //         va_list args;
    //         va_start(args, str);

    //        char buffer[1024] {};
    //        int sprintfResult = snprintf(buffer, 1024, str, args);

    //        HYP_LOG(Lightmap, Debug, "{}", buffer);
    //
    //        va_end(args);

    //        return sprintfResult;
    //    },
    //    true);

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
    packOptions.resolution = 2048;
    packOptions.padding = 4;
    packOptions.bilinear = true;
    // packOptions.maxChartSize = 256;
    // packOptions.texelsPerUnit = 8.0f;
    // packOptions.padding = 4;
    // //packOptions.resolution = 512;
    // packOptions.bilinear = true;

    xatlas::ComputeCharts(atlas);
    xatlas::PackCharts(atlas, packOptions);

    const uint32 numAtlases = MathUtil::Max(1u, atlas->atlasCount);
    atlasCount = numAtlases;

    // write lightmap data
    dimensions.x = atlas->width;
    dimensions.y = atlas->height;
    dimensions.z = 1;

    const uint32 texelsPerAtlas = atlas->width * atlas->height;

    texels.Resize(numAtlases * texelsPerAtlas);
    m_rays.Resize(numAtlases * texelsPerAtlas);

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
                        (inverseNormalMatrix.TransformVector(Vec4f(Vec3f(m_meshVertexNormals[meshIndex][triangleIndices[0] * 3], m_meshVertexNormals[meshIndex][triangleIndices[0] * 3 + 1], m_meshVertexNormals[meshIndex][triangleIndices[0] * 3 + 2]), 0.0f))).GetXYZ(),
                        (inverseNormalMatrix.TransformVector(Vec4f(Vec3f(m_meshVertexNormals[meshIndex][triangleIndices[0] * 3], m_meshVertexNormals[meshIndex][triangleIndices[0] * 3 + 1], m_meshVertexNormals[meshIndex][triangleIndices[0] * 3 + 2]), 0.0f))).GetXYZ(),
                        (inverseNormalMatrix.TransformVector(Vec4f(Vec3f(m_meshVertexNormals[meshIndex][triangleIndices[0] * 3], m_meshVertexNormals[meshIndex][triangleIndices[0] * 3 + 1], m_meshVertexNormals[meshIndex][triangleIndices[0] * 3 + 2]), 0.0f))).GetXYZ(),
                    };

                    const Vec3f position = vertexPositions[0] * barycentricCoords.x
                        + vertexPositions[1] * barycentricCoords.y
                        + vertexPositions[2] * barycentricCoords.z;

                    const Vec3f normal = (normalMatrix.TransformVector(Vec4f((vertexNormals[0] * barycentricCoords.x + vertexNormals[1] * barycentricCoords.y + vertexNormals[2] * barycentricCoords.z), 0.0f))).GetXYZ().Normalize();

                    const uint32 texelIdx = ((point.x + atlas->width) % atlas->width
                        + (atlas->height - point.y + atlas->height) % atlas->height * atlas->width)
                        + uint32(atlasIndex) * texelsPerAtlas;

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

        const VertexInputLayoutDesc prevInputLayout = bakeMesh.mesh->GetMeshDesc().meshAttributes.inputLayout;
        VertexInputLayoutDesc newInputLayout { uint8(prevInputLayout.mask | VT_UV1) };

        const size_t prevVertexStrideFloats = prevInputLayout.VertexSize() / sizeof(float);
        const size_t newVertexStrideFloats = newInputLayout.VertexSize() / sizeof(float);

        bakeMesh.vertices.Resize(atlas->meshes[meshIndex].vertexCount * newVertexStrideFloats);
        bakeMesh.indices.Resize(atlas->meshes[meshIndex].indexCount);
        bakeMesh.vertexAtlasIndices.Resize(atlas->meshes[meshIndex].vertexCount);

        for (uint32 v = 0; v < atlas->meshes[meshIndex].vertexCount; v++)
        {
            bakeMesh.vertexAtlasIndices[v] = atlas->meshes[meshIndex].vertexArray[v].atlasIndex;
        }

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

            const size_t vertexDataOffsetFloats = bakeMesh.indices[j] * newVertexStrideFloats;

            float* vertexDataFloat = bakeMesh.vertices.Data() + vertexDataOffsetFloats;

            if (prevInputLayout.mask & VT_Position)
            {
                TVertexPacket<VT_Position>* packet = reinterpret_cast<TVertexPacket<VT_Position>*>(vertexDataFloat);
                packet->SetPosition(inverseTransform.TransformVector(Vec3f(m_meshVertexPositions[meshIndex][vertexIndex * 3], m_meshVertexPositions[meshIndex][vertexIndex * 3 + 1], m_meshVertexPositions[meshIndex][vertexIndex * 3 + 2])));

                vertexDataFloat += sizeof(TVertexPacket<VT_Position>) / sizeof(float);
            }

            if (prevInputLayout.mask & VT_Normal)
            {
                TVertexPacket<VT_Normal>* packet = reinterpret_cast<TVertexPacket<VT_Normal>*>(vertexDataFloat);
                packet->SetNormal((inverseNormalMatrix.TransformVector(Vec4f(m_meshVertexNormals[meshIndex][vertexIndex * 3], m_meshVertexNormals[meshIndex][vertexIndex * 3 + 1], m_meshVertexNormals[meshIndex][vertexIndex * 3 + 2], 0.0f))).GetXYZ());

                vertexDataFloat += sizeof(TVertexPacket<VT_Normal>) / sizeof(float);
            }

            if (prevInputLayout.mask & VT_UV0)
            {
                TVertexPacket<VT_UV0>* packet = reinterpret_cast<TVertexPacket<VT_UV0>*>(vertexDataFloat);
                packet->SetUV0(Vec2f(m_meshVertexUvs[meshIndex][vertexIndex * 2], m_meshVertexUvs[meshIndex][vertexIndex * 2 + 1]));

                vertexDataFloat += sizeof(TVertexPacket<VT_UV0>) / sizeof(float);
            }

            { // UV1
                TVertexPacket<VT_UV1>* packet = reinterpret_cast<TVertexPacket<VT_UV1>*>(vertexDataFloat);
                packet->SetUV1(uv / (Vec2f { float(atlas->width), float(atlas->height) } + Vec2f(0.5f)));

                vertexDataFloat += sizeof(TVertexPacket<VT_UV1>) / sizeof(float);
            }

            // @TODO Handle other vertex data fields -- may need to read from the prev mesh data again in order to do that
        }

        // Deallocate memory for data that is no longer needed.
        m_meshVertexPositions[meshIndex].Clear();
        m_meshVertexNormals[meshIndex].Clear();
        m_meshVertexUvs[meshIndex].Clear();
        m_meshIndices[meshIndex].Clear();
    }

    xatlas::Destroy(atlas);

    return {};
#else
    return HYP_MAKE_ERROR(Error, "No method to build lightmap");
#endif
}

void BakeData<LightmapVolume>::Blur()
{
    static constexpr int KernelRadius = 3;
    static constexpr float SigmaPosition = 0.5f;
    static constexpr float SigmaNormal = 0.25f;

    const uint32 width = dimensions.x;
    const uint32 height = dimensions.y;

    if (width == 0 || height == 0 || texels.Empty())
    {
        return;
    }

    const float sigmaSpRcp = 1.0f / (2.0f * float(KernelRadius) * float(KernelRadius));
    const float sigmaPosRcp = 1.0f / (2.0f * SigmaPosition * SigmaPosition);
    const float sigmaNrmRcp = 1.0f / (2.0f * SigmaNormal * SigmaNormal);

    const uint32 numTexels = width * height;

    for (uint32 atlasIndex = 0; atlasIndex < atlasCount; atlasIndex++)
    {
        const uint32 baseOffset = atlasIndex * numTexels;

        Array<Vec4f> norm0(numTexels);
        Array<Vec4f> norm1(numTexels);

        for (uint32 i = 0; i < numTexels; i++)
        {
            if (!texels[baseOffset + i].pRay)
            {
                continue;
            }

            if (texels[baseOffset + i].color0.w > 0.0f)
            {
                norm0[i] = texels[baseOffset + i].color0 / texels[baseOffset + i].color0.w;
                norm0[i].w = 1.0f;
            }

            if (texels[baseOffset + i].color1.w > 0.0f)
            {
                norm1[i] = texels[baseOffset + i].color1 / texels[baseOffset + i].color1.w;
                norm1[i].w = 1.0f;
            }
        }

        Array<Vec4f> out0(numTexels);
        Array<Vec4f> out1(numTexels);

        for (uint32 cy = 0; cy < height; cy++)
        {
            for (uint32 cx = 0; cx < width; cx++)
            {
                const uint32 centerIdx = cx + cy * width;

                if (!texels[baseOffset + centerIdx].pRay)
                {
                    continue;
                }

                const Vec3f centerPos = texels[baseOffset + centerIdx].pRay->ray.position;
                const Vec3f centerNrm = texels[baseOffset + centerIdx].pRay->ray.direction;

                Vec4f accum0 = Vec4f::Zero();
                Vec4f accum1 = Vec4f::Zero();
                float totalW = 0.0f;

                const int nx0 = MathUtil::Max(0, int(cx) - KernelRadius);
                const int nx1 = MathUtil::Min(int(width) - 1, int(cx) + KernelRadius);
                const int ny0 = MathUtil::Max(0, int(cy) - KernelRadius);
                const int ny1 = MathUtil::Min(int(height) - 1, int(cy) + KernelRadius);

                for (int ny = ny0; ny <= ny1; ny++)
                {
                    for (int nx = nx0; nx <= nx1; nx++)
                    {
                        const uint32 nbIdx = uint32(nx) + uint32(ny) * width;

                        if (!texels[baseOffset + nbIdx].pRay)
                        {
                            continue;
                        }

                        const float dx = float(nx - int(cx));
                        const float dy = float(ny - int(cy));
                        const float wSpatial = MathUtil::Exp(-(dx * dx + dy * dy) * sigmaSpRcp);

                        const Vec3f posDiff = texels[baseOffset + nbIdx].pRay->ray.position - centerPos;
                        const float wPos = MathUtil::Exp(-posDiff.Dot(posDiff) * sigmaPosRcp);

                        const float nDot = MathUtil::Clamp(centerNrm.Dot(texels[baseOffset + nbIdx].pRay->ray.direction), -1.0f, 1.0f);
                        const float wNrm = MathUtil::Exp(-(1.0f - nDot) * sigmaNrmRcp);

                        const float w = wSpatial * wPos * wNrm;

                        accum0 += norm0[nbIdx] * w;
                        accum1 += norm1[nbIdx] * w;
                        totalW += w;
                    }
                }

                if (totalW > 0.0f)
                {
                    out0[centerIdx] = accum0 / totalW;
                    out1[centerIdx] = accum1 / totalW;
                }
            }
        }

        for (uint32 i = 0; i < numTexels; i++)
        {
            if (!texels[baseOffset + i].pRay)
            {
                continue;
            }

            texels[baseOffset + i].color0 = out0[i];
            texels[baseOffset + i].color1 = out1[i];
        }
    }
}

void BakeData<LightmapVolume>::Dilate()
{
    static constexpr int NumPasses = 5;

    const uint32 width = dimensions.x;
    const uint32 height = dimensions.y;

    if (width == 0 || height == 0 || texels.Empty())
    {
        return;
    }

    const uint32 numTexels = width * height;

    static constexpr int offsets[8][2] = {
        { -1, -1 }, { 0, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 0, 1 }, { 1, 1 }
    };

    for (uint32 atlasIndex = 0; atlasIndex < atlasCount; atlasIndex++)
    {
        const uint32 baseOffset = atlasIndex * numTexels;

        Array<Vec4f> cur0(numTexels);
        Array<Vec4f> cur1(numTexels);

        for (uint32 i = 0; i < numTexels; i++)
        {
            cur0[i] = texels[baseOffset + i].color0;
            cur1[i] = texels[baseOffset + i].color1;
        }

        for (int pass = 0; pass < NumPasses; pass++)
        {
            Array<Vec4f> next0(numTexels);
            Array<Vec4f> next1(numTexels);

            for (uint32 i = 0; i < numTexels; i++)
            {
                next0[i] = cur0[i];
                next1[i] = cur1[i];
            }

            bool anyChanged = false;

            for (uint32 cy = 0; cy < height; cy++)
            {
                for (uint32 cx = 0; cx < width; cx++)
                {
                    const uint32 idx = cx + cy * width;

                    if (texels[baseOffset + idx].pRay != nullptr)
                    {
                        continue;
                    }

                    if (cur0[idx].w > 0.0f)
                    {
                        continue;
                    }

                    Vec4f accum0 = Vec4f::Zero();
                    Vec4f accum1 = Vec4f::Zero();
                    int count = 0;

                    for (int k = 0; k < 8; k++)
                    {
                        const int nx = int(cx) + offsets[k][0];
                        const int ny = int(cy) + offsets[k][1];

                        if (nx < 0 || ny < 0 || nx >= int(width) || ny >= int(height))
                        {
                            continue;
                        }

                        const uint32 nbIdx = uint32(nx) + uint32(ny) * width;

                        if (cur0[nbIdx].w <= 0.0f)
                        {
                            continue;
                        }

                        accum0 += cur0[nbIdx];
                        accum1 += cur1[nbIdx];
                        ++count;
                    }

                    if (count > 0)
                    {
                        next0[idx] = accum0 / float(count);
                        next1[idx] = accum1 / float(count);
                        anyChanged = true;
                    }
                }
            }

            cur0 = std::move(next0);
            cur1 = std::move(next1);

            if (!anyChanged)
            {
                break;
            }
        }

        for (uint32 i = 0; i < numTexels; i++)
        {
            if (texels[baseOffset + i].pRay == nullptr)
            {
                texels[baseOffset + i].color0 = cur0[i];
                texels[baseOffset + i].color1 = cur1[i];
            }
        }
    }
}

auto BakeData<LightmapVolume>::ToBitmapIrradiance(uint32 atlasIndex) const -> BitmapType
{
    Assert(atlasIndex < atlasCount, "Atlas index out of bounds");
    Assert(texels.Size() >= dimensions.x * dimensions.y * atlasCount, "Invalid UV map size");

    const uint32 baseOffset = atlasIndex * dimensions.x * dimensions.y;

    BitmapType bitmap(dimensions.x, dimensions.y);

    for (uint32 x = 0; x < dimensions.x; x++)
    {
        for (uint32 y = 0; y < dimensions.y; y++)
        {
            const uint32 index = baseOffset + x + y * dimensions.x;

            Vec4f color = texels[index].color0;

            if (color.w <= 0.0f)
            {
                continue;
            }

            color /= color.w;

            AssertDebug(!MathUtil::IsNaN(color));

            color = MathUtil::Clamp(color, Vec4f::Zero(), Vec4f::One());

            bitmap.GetPixelReference(x, y).SetRGBA(color);
        }
    }

    return bitmap;
}

auto BakeData<LightmapVolume>::ToBitmapRadiance(uint32 atlasIndex) const -> BitmapType
{
    Assert(atlasIndex < atlasCount, "Atlas index out of bounds");
    Assert(texels.Size() >= dimensions.x * dimensions.y * atlasCount, "Invalid UV map size");

    const uint32 baseOffset = atlasIndex * dimensions.x * dimensions.y;

    BitmapType bitmap(dimensions.x, dimensions.y);

    for (uint32 x = 0; x < dimensions.x; x++)
    {
        for (uint32 y = 0; y < dimensions.y; y++)
        {
            const uint32 index = baseOffset + x + y * dimensions.x;

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
