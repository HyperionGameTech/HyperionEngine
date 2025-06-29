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
#include <core/math/Vertex.hpp>
#include <core/math/Ray.hpp>

#include <util/img/Bitmap.hpp>

namespace hyperion {

class Mesh;
class Material;
class Entity;

struct LightmapSubElement
{
    Handle<Entity> entity;
    Handle<Mesh> mesh;
    Handle<Material> material;
    Transform transform;
    BoundingBox aabb;
};

struct LightmapUVBuilderParams
{
    Span<const LightmapSubElement> sub_elements;
};

struct LightmapMeshData
{
    Handle<Mesh> mesh;
    Handle<Material> material;

    Matrix4 transform;

    Array<Vertex> vertices;
    Array<uint32> indices;
};

struct LightmapRay
{
    Ray ray;
    ObjId<Mesh> mesh_id;
    uint32 triangle_index;
    uint32 texel_index;

    HYP_FORCE_INLINE bool operator==(const LightmapRay& other) const
    {
        return ray == other.ray
            && mesh_id == other.mesh_id
            && triangle_index == other.triangle_index
            && texel_index == other.texel_index;
    }

    HYP_FORCE_INLINE bool operator!=(const LightmapRay& other) const
    {
        return !(*this == other);
    }
};

static_assert(std::is_trivially_copy_constructible_v<LightmapRay>, "LightmapRay must be trivially copy constructible");
static_assert(std::is_trivially_move_constructible_v<LightmapRay>, "LightmapRay must be trivially move constructible");

struct LightmapUV
{
    Handle<Mesh> mesh;
    Handle<Material> material;
    Matrix4 transform = Matrix4::identity;
    uint32 triangle_index = ~0u;
    Vec3f barycentric_coords = Vec3f::Zero();
    Vec2f lightmap_uv = Vec2f::Zero();
    Vec4f radiance = Vec4f::Zero();
    Vec4f irradiance = Vec4f::Zero();

    LightmapRay ray;
};

struct LightmapUVMap
{
    // HashMap from mesh id to an array of UV indices. Uses dynamic node allocation to reduce number of moves needed when adding or removing elements.
    using MeshToUVIndicesMap = HashMap<ObjId<Mesh>, Array<uint32, DynamicAllocator>, HashTable_DynamicNodeAllocator<KeyValuePair<ObjId<Mesh>, Array<uint32, DynamicAllocator>>>>;

    uint32 width = 0;
    uint32 height = 0;

    /// UVs in texture space with each entry corresponding to a texel in the lightmap.
    Array<LightmapUV> uvs;

    // Mapping from mesh Id to the indices of the UVs that correspond to that mesh.
    MeshToUVIndicesMap mesh_to_uv_indices;

    /*! \brief Write the UV map radiance data to RGBA32F format. */
    Bitmap<4, float> ToBitmapRadiance() const;
    /*! \brief Write the UV map irradiance data to RGBA32F format. */
    Bitmap<4, float> ToBitmapIrradiance() const;
};

class LightmapUVBuilder
{
    using MeshFloatDataArray = Array<float, DynamicAllocator>;
    using MeshIndexArray = Array<uint32, DynamicAllocator>;

public:
    LightmapUVBuilder() = default;

    LightmapUVBuilder(const LightmapUVBuilderParams& params);

    LightmapUVBuilder(const LightmapUVBuilder& other) = default;
    LightmapUVBuilder(LightmapUVBuilder&& other) noexcept = default;
    LightmapUVBuilder& operator=(const LightmapUVBuilder& other) = default;
    LightmapUVBuilder& operator=(LightmapUVBuilder&& other) noexcept = default;

    ~LightmapUVBuilder() = default;

    HYP_FORCE_INLINE const Array<LightmapMeshData>& GetMeshData() const
    {
        return m_mesh_data;
    }

    TResult<LightmapUVMap> Build();

private:
    LightmapUVBuilderParams m_params;
    Array<LightmapMeshData> m_mesh_data;

    // Per element mesh data used for building the UV map
    Array<MeshFloatDataArray, DynamicAllocator> m_mesh_vertex_positions;
    Array<MeshFloatDataArray, DynamicAllocator> m_mesh_vertex_normals;
    Array<MeshFloatDataArray, DynamicAllocator> m_mesh_vertex_uvs;
    Array<MeshIndexArray, DynamicAllocator> m_mesh_indices;
};

} // namespace hyperion

#endif