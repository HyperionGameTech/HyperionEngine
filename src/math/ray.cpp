#include "ray.h"
#include "bounding_box.h"
#include "math_util.h"

namespace hyperion {


bool Ray::TestAabb(const BoundingBox &aabb, RayTestResults &out_results) const
{
    return TestAabb(aabb, ~0, out_results);
}

bool Ray::TestAabb(const BoundingBox &aabb, int hit_id, RayTestResults &out_results) const
{
    return TestAabb(aabb, hit_id, nullptr, out_results);
}

bool Ray::TestAabb(const BoundingBox &aabb, int hit_id, const void *user_data, RayTestResults &out_results) const
{
    if (aabb.Empty()) { // drop out early
        return RayHit::no_hit;
    }

    auto hit_min = MathUtil::MinSafeValue<Vector3>();
    auto hit_max = MathUtil::MaxSafeValue<Vector3>();

    if (MathUtil::Abs(direction.x) < MathUtil::epsilon<float>) {
        if (position.x < aabb.min.x || position.x > aabb.max.x) {
            return RayHit::no_hit; // no collision
        }
    } else {
        hit_min.x = (aabb.min.x - position.x) / direction.x;
        hit_max.x = (aabb.max.x - position.x) / direction.x;
        if (hit_min.x > hit_max.x) {
            std::swap(hit_min.x, hit_max.x);
        }
    }

    if (MathUtil::Abs(direction.y) < MathUtil::epsilon<float>) {
        if (position.y < aabb.min.y ||
            position.y > aabb.max.y) {
            return RayHit::no_hit; // no collision
        }
    } else {
        hit_min.y = (aabb.min.y - position.y) / direction.y;
        hit_max.y = (aabb.max.y - position.y) / direction.y;

        if (hit_min.y > hit_max.y) {
            std::swap(hit_min.y, hit_max.y);
        }

        if (hit_min.x > hit_max.y || hit_min.y > hit_max.x) {
            return RayHit::no_hit; // no collision
        }

        if (hit_min.y > hit_min.x) hit_min.x = hit_min.y;
        if (hit_max.y < hit_max.x) hit_max.x = hit_max.y;
    }

    if (MathUtil::Abs(direction.z) < MathUtil::epsilon<float>) {
        if (position.z < aabb.min.z || position.z > aabb.max.z) {
            return RayHit::no_hit; // no collision
        }
    } else {
        hit_min.z = (aabb.min.z - position.z) / direction.z;
        hit_max.z = (aabb.max.z - position.z) / direction.z;

        if (hit_min.z > hit_max.z) {
            std::swap(hit_min.z, hit_max.z);
        }

        if (hit_min.x > hit_max.z || hit_min.z > hit_max.x) {
            return RayHit::no_hit; // no collision
        }

        if (hit_min.z > hit_min.x) hit_min.x = hit_min.z;
        if (hit_max.z < hit_max.x) hit_max.x = hit_max.z;
    }

    const auto hitpoint = position + (direction * hit_min.x);

    out_results.AddHit(RayHit{
        .hitpoint  = hitpoint,
        .normal    = -direction.Normalized(), // TODO: change to be box normal
        .distance  = hitpoint.Distance(position),
        .id        = hit_id,
        .user_data = user_data
    });

    return true;
}

bool RayTestResults::AddHit(const RayHit &hit)
{
    if (cumulative) {
        Insert(hit);

        return true;
    }
    
    FlatSet::operator=({hit});
    
    return true;
}


} // namespace hyperion
