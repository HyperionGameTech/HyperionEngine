/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_LIGHTMAP_UV_BUILDER_HPP
#define HYPERION_LIGHTMAP_UV_BUILDER_HPP

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>

#include <core/utilities/Span.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <scene/Entity.hpp>

#include <math/Transform.hpp>
#include <math/Matrix4.hpp>

#include <util/img/Bitmap.hpp>

namespace hyperion {

struct LightmapElement
{
    ID<Entity>          entity;
    Handle<Mesh>        mesh;
    Handle<Material>    material;
    Transform           transform;
    BoundingBox         aabb;
};

struct LightmapUVBuilderParams
{
    Span<const LightmapElement> elements;
};

struct LightmapMeshData
{
    ID<Mesh>        mesh_id;

    Matrix4         transform;

    Array<float>    vertex_positions;
    Array<float>    vertex_normals;
    Array<float>    vertex_uvs;

    Array<uint32>   indices;

    Array<Vec2f>    lightmap_uvs;
};

struct LightmapUV
{
    ID<Mesh>    mesh_id = ID<Mesh>::invalid;
    Matrix4     transform = Matrix4::identity;
    uint        triangle_index = ~0u;
    Vec3f       barycentric_coords = Vec3f::Zero();
    Vec2f       lightmap_uv = Vec2f::Zero();
    Vec4f       radiance = Vec4f::Zero();
    Vec4f       irradiance = Vec4f::Zero();
};

struct LightmapUVMap
{
    uint                            width = 0;
    uint                            height = 0;
    Array<LightmapUV>               uvs;
    HashMap<ID<Mesh>, Array<uint>>  mesh_to_uv_indices;
    
    /*! \brief Write the UV map radiance data to RGBA32F format. */
    Bitmap<4, float> ToBitmapRadiance() const;
    /*! \brief Write the UV map irradiance data to RGBA32F format. */
    Bitmap<4, float> ToBitmapIrradiance() const;
};

class LightmapUVBuilder
{
public:
    struct Result
    {
        enum Status { RESULT_OK, RESULT_ERR }   status;

        String                                  message;
        LightmapUVMap                           uv_map;

        Result()
            : status(RESULT_OK),
              message("")
        { }

        Result(Status status, String message)
            : status(status),
              message(std::move(message))
        { }

        Result(Status status, LightmapUVMap uv_map)
            : status(status),
              uv_map(std::move(uv_map))
        { }

        HYP_FORCE_INLINE explicit operator bool() const
            { return status == RESULT_OK; }
    };

    LightmapUVBuilder(const LightmapUVBuilderParams &params);
    ~LightmapUVBuilder() = default;

    HYP_FORCE_INLINE const Array<LightmapMeshData> &GetMeshData() const
        { return m_mesh_data; }
    
    Result Build();

private:
    LightmapUVBuilderParams m_params;
    Array<LightmapMeshData> m_mesh_data;
};

} // namespace hyperion

#endif