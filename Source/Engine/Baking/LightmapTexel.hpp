/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/String.hpp>
#include <Core/Containers/Map.hpp>

#include <Core/Utilities/Span.hpp>
#include <Core/Utilities/Result.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Transform.hpp>
#include <Core/Math/Mat4f.hpp>
#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/Ray.hpp>

#include <Rendering/Vertex.hpp>

#include <Util/Img/Bitmap.hpp>

namespace Hyperion {

class Mesh;
class Material;
class Entity;

namespace Baking {

struct BakeEntity
{
    Handle<Entity> entity;
    Handle<Mesh> mesh;
    Handle<Material> material;
    Mat4f transformMatrix;
    BoundingBox aabb;
};

struct BakeMesh
{
    Handle<Mesh> mesh;
    Handle<Material> material;

    Mat4f transformMatrix;

    Array<float> vertices;
    Array<uint32> indices;

    // xatlas sub-atlas index, -1 if the vertex isn't in any atlas.
    Array<int32> vertexAtlasIndices;
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

    uint32 chartId = ~0u;
};

} // namespace Baking

} // namespace Hyperion
