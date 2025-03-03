/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_LIGHTMAP_UV_BUILDER_HPP
#define HYPERION_LIGHTMAP_UV_BUILDER_HPP

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>

#include <core/utilities/Span.hpp>
#include <core/utilities/Result.hpp>

#include <core/Handle.hpp>

#include <core/math/Transform.hpp>
#include <core/math/Matrix4.hpp>
#include <core/math/BoundingBox.hpp>

#include <util/img/Bitmap.hpp>

namespace hyperion {

class Mesh;
class Material;
class Entity;

struct LightmapElement
{
    Handle<Entity>      entity;
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
    Handle<Mesh>        mesh;
    Handle<Material>    material;

    Matrix4             transform;

    Array<float>        vertex_positions;
    Array<float>        vertex_normals;
    Array<float>        vertex_uvs;

    Array<uint32>       indices;

    Array<Vec2f>        lightmap_uvs;
};

struct LightmapUV
{
    Handle<Mesh>        mesh;
    Handle<Material>    material;
    Matrix4             transform = Matrix4::identity;
    uint32              triangle_index = ~0u;
    Vec3f               barycentric_coords = Vec3f::Zero();
    Vec2f               lightmap_uv = Vec2f::Zero();
    Vec4f               radiance = Vec4f::Zero();
    Vec4f               irradiance = Vec4f::Zero();
};

struct LightmapUVMap
{
    uint32                              width = 0;
    uint32                              height = 0;
    Array<LightmapUV>                   uvs;
    HashMap<ID<Mesh>, Array<uint32>>    mesh_to_uv_indices;
    
    /*! \brief Write the UV map radiance data to RGBA32F format. */
    Bitmap<4, float> ToBitmapRadiance() const;
    /*! \brief Write the UV map irradiance data to RGBA32F format. */
    Bitmap<4, float> ToBitmapIrradiance() const;
};

class LightmapUVBuilder
{
public:
    LightmapUVBuilder(const LightmapUVBuilderParams &params);
    ~LightmapUVBuilder() = default;

    HYP_FORCE_INLINE const Array<LightmapMeshData> &GetMeshData() const
        { return m_mesh_data; }
    
    Result<LightmapUVMap> Build();

private:
    LightmapUVBuilderParams m_params;
    Array<LightmapMeshData> m_mesh_data;
};

} // namespace hyperion

#endif