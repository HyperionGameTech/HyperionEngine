#include "Ray.hpp"
#include "BoundingBox.hpp"
#include "MathUtil.hpp"

namespace hyperion {

bool Ray::TestAabb(const BoundingBox &aabb) const
{
    RayTestResults out_results;

    return TestAabb(aabb, ~0, out_results);
}

bool Ray::TestAabb(const BoundingBox &aabb, RayTestResults &out_results) const
{
    return TestAabb(aabb, ~0, out_results);
}

bool Ray::TestAabb(const BoundingBox &aabb, RayHitID hit_id, RayTestResults &out_results) const
{
    return TestAabb(aabb, hit_id, nullptr, out_results);
}

bool Ray::TestAabb(const BoundingBox &aabb, RayHitID hit_id, const void *user_data, RayTestResults &out_results) const
{
    if (aabb.Empty()) { // drop out early
        return RayHit::no_hit;
    }

    float t1 = (aabb.min.x - position.x) / direction.x;
    float t2 = (aabb.max.x - position.x) / direction.x;
    float t3 = (aabb.min.y - position.y) / direction.y;
    float t4 = (aabb.max.y - position.y) / direction.y;
    float t5 = (aabb.min.z - position.z) / direction.z;
    float t6 = (aabb.max.z - position.z) / direction.z;

    float tmin = MathUtil::Max(MathUtil::Max(MathUtil::Min(t1, t2), MathUtil::Min(t3, t4)), MathUtil::Min(t5, t6));
    float tmax = MathUtil::Min(MathUtil::Min(MathUtil::Max(t1, t2), MathUtil::Max(t3, t4)), MathUtil::Max(t5, t6));

    // if tmax < 0, ray (line) is intersecting AABB, but whole AABB is behing us
    if (tmax < 0) {
        return false;
    }

    // if tmin > tmax, ray doesn't intersect AABB
    if (tmin > tmax) {
        return false;
    }

    float distance = tmin;

    if (tmin < 0.0f) {
        distance = tmax;
    }

    const auto hitpoint = position + (direction * distance);

    out_results.AddHit(RayHit{
        .hitpoint  = hitpoint,
        .normal    = -direction.Normalized(), // TODO: change to be box normal
        .distance  = distance,
        .id        = hit_id,
        .user_data = user_data
    });

    return true;
}

bool Ray::TestTriangle(const Triangle &triangle) const
{
    RayTestResults out_results;

    return TestTriangle(triangle, ~0, out_results);
}

bool Ray::TestTriangle(const Triangle &triangle, RayTestResults &out_results) const
{
    return TestTriangle(triangle, ~0, out_results);
}

bool Ray::TestTriangle(const Triangle &triangle, RayHitID hit_id, RayTestResults &out_results) const
{
    return TestTriangle(triangle, hit_id, nullptr, out_results);
}

bool Ray::TestTriangle(const Triangle &triangle, RayHitID hit_id, const void *user_data, RayTestResults &out_results) const
{
    float t, u, v;

    Vector3 v0v1 = triangle.GetPoint(1).GetPosition() - triangle.GetPoint(0).GetPosition();
	Vector3 v0v2 = triangle.GetPoint(2).GetPosition() - triangle.GetPoint(0).GetPosition();
	Vector3 pvec = direction.Cross(v0v2);

	float det = v0v1.Dot(pvec);

	// ray and triangle are parallel if det is close to 0
	if (std::fabs(det) < MathUtil::epsilon<float>) {
        return false;
    }

	float inv_det = 1.0 / det;

	Vector3 tvec = position - triangle.GetPoint(0).GetPosition();
	u = tvec.Dot(pvec) * inv_det;

	if (u < 0 || u > 1) {
        return false;
    }

	Vector3 qvec = tvec.Cross(v0v1);
	v = direction.Dot(qvec) * inv_det;

	if (v < 0 || u + v > 1) {
        return false;
    }

	t = v0v2.Dot(qvec) * inv_det;

    if (t > 0.0f) {
        out_results.AddHit({
            .hitpoint  = position + (direction * t),
            .normal    = v0v1.Cross(v0v2),
            .distance  = t,
            .id        = hit_id,
            .user_data = user_data
        });

        return true;
    }

    return false;
}

bool Ray::TestTriangleList(
    const std::vector<Vertex> &vertices,
    const std::vector<UInt32> &indices,
    const Transform &transform
) const
{
    RayTestResults out_results;

    return TestTriangleList(vertices, indices, transform, ~0, out_results);
}

bool Ray::TestTriangleList(
    const std::vector<Vertex> &vertices,
    const std::vector<UInt32> &indices,
    const Transform &transform,
    RayTestResults &out_results
) const
{
    return TestTriangleList(vertices, indices, transform, ~0, out_results);
}

bool Ray::TestTriangleList(
    const std::vector<Vertex> &vertices,
    const std::vector<UInt32> &indices,
    const Transform &transform,
    RayHitID hit_id,
    RayTestResults &out_results
) const
{
    return TestTriangleList(vertices, indices, transform, hit_id, nullptr, out_results);
}

bool Ray::TestTriangleList(
    const std::vector<Vertex> &vertices,
    const std::vector<UInt32> &indices,
    const Transform &transform,
    RayHitID hit_id,
    const void *user_data,
    RayTestResults &out_results
) const
{
    bool intersected = false;
    
    if (indices.size() % 3 != 0) {
        DebugLog(
            LogType::Error,
            "Cannot perform raytest on triangle list because number of indices (%llu) was not divisible by 3\n",
            indices.size()
        );

        return false;
    }

    RayTestResults tmp_results;

    for (size_t i = 0; i < indices.size(); i += 3) {
        Triangle triangle(
            vertices[indices[i]].GetPosition(),
            vertices[indices[i + 1]].GetPosition(),
            vertices[indices[i + 2]].GetPosition()
        );

        triangle *= transform;

        if (TestTriangle(triangle, static_cast<RayHitID>(i), tmp_results)) {
            intersected = true;
        }
    }

    if (intersected) {
        AssertThrow(!tmp_results.Empty());

        auto &first_result = tmp_results.Front();
        first_result.id = hit_id;
        first_result.user_data = user_data;

        out_results.AddHit(first_result);

        return true;
    }

    return false;
}


bool RayTestResults::AddHit(const RayHit &hit)
{
    return Insert(hit).second;
}


} // namespace hyperion
