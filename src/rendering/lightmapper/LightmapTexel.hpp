/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>

#include <core/utilities/Span.hpp>
#include <core/utilities/Result.hpp>

#include <core/reflection/Handle.hpp>

#include <core/math/Transform.hpp>
#include <core/math/Mat4f.hpp>
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

    Mat4f transform;

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

struct LightmapTexel
{
    uint32 triangleIndex = ~0u;
    Vec3f barycentricCoords = Vec3f::Zero();
    Vec2f lightmapUv = Vec2f::Zero();
    Vec4f radiance = Vec4f::Zero();
    Vec4f irradiance = Vec4f::Zero();

    LightmapRay ray;
};

/*! \brief Base class for lightmap texel source, used to trace rays from and store texel data to. */
class LightmapTexelsBase
{
public:
    // HashMap from mesh id to an array of UV indices. Uses dynamic node allocation to reduce number of moves needed when adding or removing elements.
    using MeshToUVIndicesMap = HashMap<ObjId<Mesh>, Array<uint32, DynamicAllocator>>;

    struct TexelRange
    {
        uint32 start = 0;
        uint32 count = 0; // number of consecutive texels
    };

    using MeshToTexelRangesMap = HashMap<ObjId<Mesh>, Array<TexelRange, DynamicAllocator>>;

    uint32 width = 0;
    uint32 height = 0;

    /// Texels in UV space
    Array<LightmapTexel> texels;

    // Mapping from mesh Id to the indices of the UVs that correspond to that mesh.
    MeshToUVIndicesMap meshToUvIndices;

    // Texel indices per mesh
    MeshToTexelRangesMap meshToTexelRanges;

    LightmapTexelsBase() = default;
    virtual ~LightmapTexelsBase() = default;
};

} // namespace hyperion
