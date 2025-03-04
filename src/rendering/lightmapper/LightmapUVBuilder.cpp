/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/lightmapper/LightmapUVBuilder.hpp>
#include <rendering/RenderMesh.hpp>

#include <scene/Mesh.hpp>

#include <streaming/StreamedMeshData.hpp>

#ifdef HYP_XATLAS
#include <xatlas.h>
#endif

namespace hyperion {

#pragma region LightmapUVMap

Bitmap<4, float> LightmapUVMap::ToBitmapRadiance() const
{
    AssertThrowMsg(uvs.Size() == width * height, "Invalid UV map size");

    Bitmap<4, float> bitmap(width, height);

    for (uint32 x = 0; x < width; x++) {
        for (uint32 y = 0; y < height; y++) {
            const uint32 index = x + y * width;

            Vec4f color = uvs[index].radiance;

            if (color.w >= 1.0f) {
                color /= color.w;
            }

            bitmap.GetPixelAtIndex(index).SetRGBA({
                color.x,
                color.y,
                color.z,
                color.w
            });
        }
    }

    return bitmap;
}

Bitmap<4, float> LightmapUVMap::ToBitmapIrradiance() const
{
    AssertThrowMsg(uvs.Size() == width * height, "Invalid UV map size");

    Bitmap<4, float> bitmap(width, height);

    for (uint32 x = 0; x < width; x++) {
        for (uint32 y = 0; y < height; y++) {
            const uint32 index = x + y * width;

            Vec4f color = uvs[index].irradiance;

            if (color.w >= 1.0f) {
                color /= color.w;
            }

            bitmap.GetPixelAtIndex(index).SetRGBA({
                color.x,
                color.y,
                color.z,
                color.w
            });
        }
    }

    return bitmap;
}

#pragma endregion LightmapUVMap

#pragma region LightmapUVBuilder

LightmapUVBuilder::LightmapUVBuilder(const LightmapUVBuilderParams &params)
    : m_params(params)
{
    m_mesh_data.Resize(params.elements.Size());

    for (SizeType i = 0; i < params.elements.Size(); i++) {
        const LightmapElement &element = params.elements[i];

        LightmapMeshData &lightmap_mesh_data = m_mesh_data[i];

        if (!element.mesh || !element.mesh->GetStreamedMeshData()) {
            continue;
        }

        const Handle<Mesh> &mesh = element.mesh;

        StreamedMeshData *streamed_mesh_data = mesh->GetStreamedMeshData();
        AssertThrow(streamed_mesh_data != nullptr);

        ResourceHandle resource_handle(*streamed_mesh_data);

        lightmap_mesh_data.mesh = element.mesh;

        lightmap_mesh_data.material = element.material;

        lightmap_mesh_data.transform = element.transform.GetMatrix();

        lightmap_mesh_data.vertex_positions.Resize(streamed_mesh_data->GetMeshData().vertices.Size() * 3);
        lightmap_mesh_data.vertex_normals.Resize(streamed_mesh_data->GetMeshData().vertices.Size() * 3);
        lightmap_mesh_data.vertex_uvs.Resize(streamed_mesh_data->GetMeshData().vertices.Size() * 2);

        lightmap_mesh_data.indices = streamed_mesh_data->GetMeshData().indices;

        lightmap_mesh_data.lightmap_uvs.Resize(streamed_mesh_data->GetMeshData().vertices.Size());

        const Matrix4 normal_matrix = element.transform.GetMatrix().Inverted().Transpose();

        for (SizeType i = 0; i < streamed_mesh_data->GetMeshData().vertices.Size(); i++) {
            const Vec3f position = element.transform.GetMatrix() * streamed_mesh_data->GetMeshData().vertices[i].GetPosition();
            const Vec3f normal = (normal_matrix * Vec4f(streamed_mesh_data->GetMeshData().vertices[i].GetNormal(), 0.0f)).GetXYZ().Normalize();
            const Vec2f uv = streamed_mesh_data->GetMeshData().vertices[i].GetTexCoord0();

            lightmap_mesh_data.vertex_positions[i * 3] = position.x;
            lightmap_mesh_data.vertex_positions[i * 3 + 1] = position.y;
            lightmap_mesh_data.vertex_positions[i * 3 + 2] = position.z;

            lightmap_mesh_data.vertex_normals[i * 3] = normal.x;
            lightmap_mesh_data.vertex_normals[i * 3 + 1] = normal.y;
            lightmap_mesh_data.vertex_normals[i * 3 + 2] = normal.z;

            lightmap_mesh_data.vertex_uvs[i * 2] = uv.x;
            lightmap_mesh_data.vertex_uvs[i * 2 + 1] = uv.y;
        }
    }
}

Result<LightmapUVMap> LightmapUVBuilder::Build()
{
    if (!m_params.elements) {
        return HYP_MAKE_ERROR(Error, "No elements to build lightmap");
    }

    LightmapUVMap uv_map;

#ifdef HYP_XATLAS
    xatlas::Atlas *atlas = xatlas::Create();

    for (SizeType i = 0; i < m_mesh_data.Size(); i++) {
        const LightmapMeshData &lightmap_mesh_data = m_mesh_data[i];
    
        xatlas::MeshDecl mesh_decl;
        mesh_decl.indexData = lightmap_mesh_data.indices.Data();
        mesh_decl.indexFormat = xatlas::IndexFormat::UInt32;
        mesh_decl.indexCount = uint32(lightmap_mesh_data.indices.Size());
        mesh_decl.vertexCount = uint32(lightmap_mesh_data.vertex_positions.Size() / 3);
        mesh_decl.vertexPositionData = lightmap_mesh_data.vertex_positions.Data();
        mesh_decl.vertexPositionStride = sizeof(float) * 3;
        mesh_decl.vertexNormalData = lightmap_mesh_data.vertex_normals.Data();
        mesh_decl.vertexNormalStride = sizeof(float) * 3;
        mesh_decl.vertexUvData = lightmap_mesh_data.vertex_uvs.Data();
        mesh_decl.vertexUvStride = sizeof(float) * 2;

        xatlas::AddMeshError error = xatlas::AddMesh(atlas, mesh_decl);

        if (error != xatlas::AddMeshError::Success) {
            xatlas::Destroy(atlas);

            DebugLog(LogType::Error, "Error adding mesh: %s\n", xatlas::StringForEnum(error));

            return HYP_MAKE_ERROR(Error, "Error adding mesh");
        }

        xatlas::AddMeshJoin(atlas);
    }

    xatlas::PackOptions pack_options { };
    // pack_options.maxChartSize = 2048;
    pack_options.resolution = 1024; // testing
    // pack_options.padding = 8;
    // pack_options.texelsPerUnit = 128.0f;
    pack_options.bilinear = true;
    //pack_options.blockAlign = true;
    pack_options.bruteForce = true;
    pack_options.rotateCharts = true;

    xatlas::ComputeCharts(atlas);
    xatlas::PackCharts(atlas, pack_options);
    
    // write lightmap data
    uv_map.width = atlas->width;
    uv_map.height = atlas->height;
    uv_map.uvs.Resize(atlas->width * atlas->height);

    for (uint32 mesh_index = 0; mesh_index < atlas->meshCount; mesh_index++) {
        AssertThrow(mesh_index < m_mesh_data.Size());

        const xatlas::Mesh &atlas_mesh = atlas->meshes[mesh_index];
        
        for (uint32 i = 0; i < atlas_mesh.indexCount; i += 3) {
            bool skip = false;
            int atlas_index = -1;
            FixedArray<Pair<uint32, Vec2i>, 3> verts;

            for (uint32 j = 0; j < 3; j++) {
                // Get UV coordinates for each edge
                const xatlas::Vertex &v = atlas_mesh.vertexArray[atlas_mesh.indexArray[i + j]];

                if (v.atlasIndex == -1) {
                    skip = true;
                
                    break;
                }
                
                atlas_index = v.atlasIndex;

                verts[j] = { v.xref, { int(v.uv[0]), int(v.uv[1]) } };
            }

            if (skip) {
                continue;
            }

            m_mesh_data[mesh_index].lightmap_uvs[verts[0].first] = Vec2f(verts[0].second) / Vec2f { float(atlas->width), float(atlas->height) };
            m_mesh_data[mesh_index].lightmap_uvs[verts[1].first] = Vec2f(verts[1].second) / Vec2f { float(atlas->width), float(atlas->height) };
            m_mesh_data[mesh_index].lightmap_uvs[verts[2].first] = Vec2f(verts[2].second) / Vec2f { float(atlas->width), float(atlas->height) };

            const Vec2i pts[3] = { verts[0].second, verts[1].second, verts[2].second };
            
            const Vec2i clamp { int(uv_map.width - 1), int(uv_map.height - 1) };

            Vec2i bboxmin { int(uv_map.width - 1), int(uv_map.height - 1) }; 
            Vec2i bboxmax { 0, 0 }; 

            for (int j = 0; j < 3; j++) { 
                bboxmin.x = MathUtil::Max(0, MathUtil::Min(bboxmin.x, pts[j].x));
	            bboxmin.y = MathUtil::Max(0, MathUtil::Min(bboxmin.y, pts[j].y));

	            bboxmax.x = MathUtil::Min(clamp.x, MathUtil::Max(bboxmax.x, pts[j].x));
	            bboxmax.y = MathUtil::Min(clamp.y, MathUtil::Max(bboxmax.y, pts[j].y));
            }

            Vec2i point;

            for (point.x = bboxmin.x; point.x <= bboxmax.x; point.x++) { 
                for (point.y = bboxmin.y; point.y <= bboxmax.y; point.y++) { 
                    const Vec3f bc_screen = MathUtil::CalculateBarycentricCoordinates(Vec2f(pts[0]), Vec2f(pts[1]), Vec2f(pts[2]), Vec2f(point));

                    if (bc_screen.x < 0 || bc_screen.y < 0 || bc_screen.z < 0) {
                        continue;
                    }

                    const uint32 index = (point.x + atlas->width) % atlas->width
                        + (atlas->height - point.y + atlas->height) % atlas->height * atlas->width;

                    uv_map.uvs[index] = {
                        m_mesh_data[mesh_index].mesh,                                       // mesh
                        m_mesh_data[mesh_index].material,                                   // material
                        m_mesh_data[mesh_index].transform,                                  // transform
                        i / 3,                                                              // triangle_index
                        bc_screen,                                                          // barycentric_coords
                        Vec2f(point) / Vec2f { float(atlas->width), float(atlas->height) }  // lightmap_uv
                    };

                    uv_map.mesh_to_uv_indices[m_mesh_data[mesh_index].mesh.GetID()].PushBack(index);
                } 
            }
        }
    }

    for (SizeType mesh_index = 0; mesh_index < m_mesh_data.Size(); mesh_index++) {
        const LightmapMeshData &lightmap_mesh_data = m_mesh_data[mesh_index];
        const LightmapElement &element = m_params.elements[mesh_index];

        const Handle<Mesh> &mesh = element.mesh;
        AssertThrow(mesh.IsValid());

        ResourceHandle resource_handle(*mesh->GetStreamedMeshData());

        MeshData new_mesh_data;
        new_mesh_data.vertices.Resize(atlas->meshes[mesh_index].vertexCount);
        new_mesh_data.indices.Resize(atlas->meshes[mesh_index].indexCount);

        for (uint32 j = 0; j < atlas->meshes[mesh_index].indexCount; j++) {
            new_mesh_data.indices[j] = atlas->meshes[mesh_index].indexArray[j];

            new_mesh_data.vertices[new_mesh_data.indices[j]] = mesh->GetStreamedMeshData()->GetMeshData().vertices[atlas->meshes[mesh_index].vertexArray[atlas->meshes[mesh_index].indexArray[j]].xref];
            new_mesh_data.vertices[new_mesh_data.indices[j]].texcoord1 = Vec2f {
                float(atlas->meshes[mesh_index].vertexArray[atlas->meshes[mesh_index].indexArray[j]].uv[0]) / float(atlas->width),
                float(atlas->meshes[mesh_index].vertexArray[atlas->meshes[mesh_index].indexArray[j]].uv[1]) / float(atlas->height)
            };
        }

        resource_handle.Reset();

        mesh->SetStreamedMeshData(StreamedMeshData::FromMeshData(std::move(new_mesh_data)));
    }

    xatlas::Destroy(atlas);

    return std::move(uv_map);
#else
    return HYP_MAKE_ERROR(Error, "No method to build lightmap");
#endif
}

#pragma endregion LightmapUVBuilder

} // namespace hyperion