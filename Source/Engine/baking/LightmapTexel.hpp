/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/containers/String.hpp>
#include <Core/containers/Map.hpp>

#include <Core/utilities/Span.hpp>
#include <Core/utilities/Result.hpp>

#include <Core/reflection/Handle.hpp>

#include <Core/math/Transform.hpp>
#include <Core/math/Mat4f.hpp>
#include <Core/math/BoundingBox.hpp>
#include <Core/math/Ray.hpp>

#include <rendering/Vertex.hpp>

#include <util/img/Bitmap.hpp>

namespace Hyperion {

class Mesh;
class MaterialInstance;
class Entity;

namespace Baking {

using BakeVertex = TVertex<VT_Simple | VT_UV1>;

struct BakeEntity
{
    Handle<Entity> entity;
    Handle<Mesh> mesh;
    Handle<MaterialInstance> material;
    Mat4f transformMatrix;
    BoundingBox aabb;
};

struct BakeMesh
{
    Handle<Mesh> mesh;
    Handle<MaterialInstance> material;

    Mat4f transformMatrix;

    Array<BakeVertex> vertices;
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

} // namespace Baking

} // namespace Hyperion
