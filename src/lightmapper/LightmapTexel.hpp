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
    Vec4f color0 = Vec4f::Zero();
    Vec4f color1 = Vec4f::Zero();

    LightmapRay* pRay = nullptr;
};

} // namespace hyperion
