#ifndef HYPERION_RAY_H
#define HYPERION_RAY_H

#include "Vector3.hpp"
#include <math/Vertex.hpp>
#include <math/Transform.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

#include <core/lib/FlatSet.hpp>

#include <array>
#include <tuple>

namespace hyperion {

class BoundingBox;
class Triangle;
class RayTestResults;
struct RayHit;

using RayHitID = UInt;

struct Ray
{
    Vector3 position;
    Vector3 direction;

    bool TestAABB(const BoundingBox &aabb) const;
    bool TestAABB(const BoundingBox &aabb, RayTestResults &out_results) const;
    bool TestAABB(const BoundingBox &aabb, RayHitID hit_id, RayTestResults &out_results) const;
    bool TestAABB(const BoundingBox &aabb, RayHitID hit_id, const void *user_data, RayTestResults &out_results) const;

    bool TestTriangle(const Triangle &triangle) const;
    bool TestTriangle(const Triangle &triangle, RayTestResults &out_results) const;
    bool TestTriangle(const Triangle &triangle, RayHitID hit_id, RayTestResults &out_results) const;
    bool TestTriangle(const Triangle &triangle, RayHitID hit_id, const void *user_data, RayTestResults &out_results) const;
    
    bool TestTriangleList(
        const Array<Vertex> &vertices,
        const Array<UInt32> &indices,
        const Transform &transform
    ) const;

    bool TestTriangleList(
        const Array<Vertex> &vertices,
        const Array<UInt32> &indices,
        const Transform &transform,
        RayTestResults &out_results
    ) const;

    bool TestTriangleList(
        const Array<Vertex> &vertices,
        const Array<UInt32> &indices,
        const Transform &transform,
        RayHitID hit_id,
        RayTestResults &out_results
    ) const;

    bool TestTriangleList(
        const Array<Vertex> &vertices,
        const Array<UInt32> &indices,
        const Transform &transform,
        RayHitID hit_id,
        const void *user_data,
        RayTestResults &out_results
    ) const;

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(position.GetHashCode());
        hc.Add(direction.GetHashCode());

        return hc;
    }
};

struct RayHit
{
    static constexpr bool no_hit = false;
    
    Vector3     hitpoint;
    Vector3     normal;
    Float       distance = 0.0f;
    RayHitID    id = ~0u;
    const void *user_data = nullptr;

    bool operator<(const RayHit &other) const
    {
        return std::tie(
            distance,
            hitpoint,
            normal,
            id,
            user_data
        ) < std::tie(
            other.distance,
            other.hitpoint,
            other.normal,
            other.id,
            other.user_data
        );
    }

    bool operator==(const RayHit &other) const
    {
        return distance  == other.distance
            && hitpoint  == other.hitpoint
            && normal    == other.normal
            && id        == other.id
            && user_data == other.user_data;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(distance);
        hc.Add(hitpoint.GetHashCode());
        hc.Add(normal.GetHashCode());
        hc.Add(id);
        hc.Add(user_data);

        return hc;
    }
};

class RayTestResults : public FlatSet<RayHit>
{
public:
    bool AddHit(const RayHit &hit);
};

} // namespace hyperion

#endif
