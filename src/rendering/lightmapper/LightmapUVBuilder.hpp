/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>

#include <core/utilities/Span.hpp>
#include <core/utilities/Result.hpp>

#include <core/object/Handle.hpp>

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
    Span<const LightmapSubElement> subElements;
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
    ObjId<Mesh> meshId;
    uint32 triangleIndex;
    uint32 texelIndex;

    HYP_FORCE_INLINE bool operator==(const LightmapRay& other) const
    {
        return ray == other.ray
            && meshId == other.meshId
            && triangleIndex == other.triangleIndex
            && texelIndex == other.texelIndex;
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
    uint32 triangleIndex = ~0u;
    Vec3f barycentricCoords = Vec3f::Zero();
    Vec2f lightmapUv = Vec2f::Zero();
    Vec4f radiance = Vec4f::Zero();
    Vec4f irradiance = Vec4f::Zero();

    LightmapRay ray;
};

using LightmapAtlasBitmap = Bitmap_RGBA8;

struct LightmapUVMap
{
    // HashMap from mesh id to an array of UV indices. Uses dynamic node allocation to reduce number of moves needed when adding or removing elements.
    using MeshToUVIndicesMap = HashMap<ObjId<Mesh>, Array<uint32, DynamicAllocator>>;

    uint32 width = 0;
    uint32 height = 0;

    /// UVs in texture space with each entry corresponding to a texel in the lightmap.
    Array<LightmapUV> uvs;

    // Mapping from mesh Id to the indices of the UVs that correspond to that mesh.
    MeshToUVIndicesMap meshToUvIndices;

    LightmapAtlasBitmap ToBitmapRadiance() const;
    LightmapAtlasBitmap ToBitmapIrradiance() const;
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
        return m_meshData;
    }

    TResult<LightmapUVMap> Build();

private:
    LightmapUVBuilderParams m_params;
    Array<LightmapMeshData> m_meshData;

    // Per element mesh data used for building the UV map
    Array<MeshFloatDataArray, DynamicAllocator> m_meshVertexPositions;
    Array<MeshFloatDataArray, DynamicAllocator> m_meshVertexNormals;
    Array<MeshFloatDataArray, DynamicAllocator> m_meshVertexUvs;
    Array<Array<uint32>, DynamicAllocator> m_meshIndices;
};

} // namespace hyperion
