#ifndef HYPERION_V2_LIGHTMAP_UV_BUILDER_HPP
#define HYPERION_V2_LIGHTMAP_UV_BUILDER_HPP

#include <core/lib/String.hpp>
#include <core/lib/HashMap.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <scene/Entity.hpp>

#include <math/Transform.hpp>
#include <math/Matrix4.hpp>

#include <util/img/Bitmap.hpp>

namespace hyperion::v2 {

struct LightmapEntity
{
    ID<Entity>          entity_id;
    Handle<Mesh>        mesh;
    Handle<Material>    material;
    Matrix4             transform;
};

struct LightmapUVBuilderParams
{
    Array<LightmapEntity>   elements;
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
    ID<Mesh>    mesh_id;
    Matrix4     transform;
    uint        triangle_index;
    Vec3f       barycentric_coords;
    Vec2f       lightmap_uv;
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

        explicit operator bool() const
            { return status == RESULT_OK; }
    };

    LightmapUVBuilder(const LightmapUVBuilderParams &params);
    ~LightmapUVBuilder() = default;

    const Array<LightmapMeshData> &GetMeshData() const
        { return m_mesh_data; }
    
    Result Build();

private:
    LightmapUVBuilderParams m_params;
    Array<LightmapMeshData> m_mesh_data;
};

} // namespace hyperion::v2

#endif