/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Baking/LightmapTexel.hpp>

#include <Core/Types.hpp>

#include <Core/Math/Ray.hpp>
#include <Core/Math/Mat4f.hpp>
#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/Transform.hpp>
#include <Core/Math/Triangle.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/FlatSet.hpp>
#include <Core/Containers/Array.hpp>

#include <Rendering/Vertex.hpp>

#include <Scene/BVH.hpp>

namespace Hyperion {

class Entity;

namespace Baking {

struct BakeEntity;

struct LightmapRayHit : RayHit
{
    Handle<Entity> entity;
    Triangle triangle;

    LightmapRayHit() = default;

    LightmapRayHit(const RayHit& rayHit, const Handle<Entity>& entity, const Triangle& triangle)
        : RayHit(rayHit),
          entity(entity),
          triangle(triangle)
    {
    }

    LightmapRayHit(const LightmapRayHit& other) = default;
    LightmapRayHit& operator=(const LightmapRayHit& other) = default;

    LightmapRayHit(LightmapRayHit&& other) noexcept
        : RayHit(static_cast<RayHit&&>(std::move(other))),
          entity(std::move(other.entity)),    // NOLINT(bugprone-use-after-move)
          triangle(std::move(other.triangle)) // NOLINT(bugprone-use-after-move)
    {
    }

    LightmapRayHit& operator=(LightmapRayHit&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        RayHit::operator=(static_cast<RayHit&&>(std::move(other)));
        entity = std::move(other.entity);     // NOLINT(bugprone-use-after-move)
        triangle = std::move(other.triangle); // NOLINT(bugprone-use-after-move)

        return *this;
    }

    virtual ~LightmapRayHit() = default;

    bool operator==(const LightmapRayHit& other) const
    {
        return static_cast<const RayHit&>(*this) == static_cast<const RayHit&>(other)
            && entity == other.entity
            && triangle == other.triangle;
    }

    bool operator!=(const LightmapRayHit& other) const
    {
        return static_cast<const RayHit&>(*this) != static_cast<const RayHit&>(other)
            || entity != other.entity
            || triangle != other.triangle;
    }

    bool operator<(const LightmapRayHit& other) const
    {
        if (static_cast<const RayHit&>(*this) < static_cast<const RayHit&>(other))
        {
            return true;
        }

        if (entity < other.entity)
        {
            return true;
        }

        if (entity == other.entity && triangle.GetPosition() < other.triangle.GetPosition())
        {
            return true;
        }

        return false;
    }
}

} // namespace Baking
} // namespace Hyperion
