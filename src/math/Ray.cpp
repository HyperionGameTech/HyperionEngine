/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <math/Ray.hpp>
#include <math/BoundingBox.hpp>
#include <math/Triangle.hpp>
#include <math/MathUtil.hpp>

namespace hyperion {

bool Ray::TestAABB(const BoundingBox &aabb) const
{
    RayTestResults out_results;

    return TestAABB(aabb, ~0, out_results);
}

bool Ray::TestAABB(const BoundingBox &aabb, RayTestResults &out_results) const
{
    return TestAABB(aabb, ~0, out_results);
}

bool Ray::TestAABB(const BoundingBox &aabb, RayHitID hit_id, RayTestResults &out_results) const
{
    return TestAABB(aabb, hit_id, nullptr, out_results);
}

bool Ray::TestAABB(const BoundingBox &aabb, RayHitID hit_id, const void *user_data, RayTestResults &out_results) const
{
    if (!aabb.IsValid()) { // drop out early
        return RayHit::no_hit;
    }

    const float t1 = (aabb.min.x - position.x) / direction.x;
    const float t2 = (aabb.max.x - position.x) / direction.x;
    const float t3 = (aabb.min.y - position.y) / direction.y;
    const float t4 = (aabb.max.y - position.y) / direction.y;
    const float t5 = (aabb.min.z - position.z) / direction.z;
    const float t6 = (aabb.max.z - position.z) / direction.z;

    const float tmin = MathUtil::Max(MathUtil::Max(MathUtil::Min(t1, t2), MathUtil::Min(t3, t4)), MathUtil::Min(t5, t6));
    const float tmax = MathUtil::Min(MathUtil::Min(MathUtil::Max(t1, t2), MathUtil::Max(t3, t4)), MathUtil::Max(t5, t6));

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

    out_results.AddHit(RayHit {
        .hitpoint = hitpoint,
        .normal = -direction.Normalized(), // TODO: change to be box normal
        .distance = distance,
        .id = hit_id,
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

    Vec3f v0v1 = triangle.GetPoint(1).GetPosition() - triangle.GetPoint(0).GetPosition();
	Vec3f v0v2 = triangle.GetPoint(2).GetPosition() - triangle.GetPoint(0).GetPosition();
	Vec3f pvec = direction.Cross(v0v2);

	float det = v0v1.Dot(pvec);

	// ray and triangle are parallel if det is close to 0
	if (std::fabs(det) < MathUtil::epsilon_f) {
        return false;
    }

	float inv_det = 1.0 / det;

	Vec3f tvec = position - triangle.GetPoint(0).GetPosition();
	u = tvec.Dot(pvec) * inv_det;

	if (u < 0 || u > 1) {
        return false;
    }

	Vec3f qvec = tvec.Cross(v0v1);
	v = direction.Dot(qvec) * inv_det;

	if (v < 0 || u + v > 1) {
        return false;
    }

	t = v0v2.Dot(qvec) * inv_det;

    const Vec3f barycentric_coords = Vec3f(1.0f - u - v, u, v);

    const Vec3f normal = triangle.GetPoint(0).GetNormal() * barycentric_coords.x
        + triangle.GetPoint(1).GetNormal() * barycentric_coords.y
        + triangle.GetPoint(2).GetNormal() * barycentric_coords.z;

    if (t > 0.0f) {
        out_results.AddHit({
            .hitpoint           = position + (direction * t),
            .normal             = normal,
            .barycentric_coords = barycentric_coords,
            .distance           = t,
            .id                 = hit_id,
            .user_data          = user_data
        });

        return true;
    }

    return false;
}

Optional<RayHit> Ray::TestTriangleList(
    const Array<Vertex> &vertices,
    const Array<uint32> &indices,
    const Transform &transform
) const
{
    RayTestResults out_results;

    if (!TestTriangleList(vertices, indices, transform, ~0, out_results)) {
        return { };
    }

    return out_results.Front();
}

Optional<RayHit> Ray::TestTriangleList(
    const Array<Triangle> &triangles,
    const Transform &transform
) const
{
    RayTestResults out_results;

    if (!TestTriangleList(triangles, transform, ~0, out_results)) {
        return { };
    }

    return out_results.Front();
}

bool Ray::TestTriangleList(
    const Array<Vertex> &vertices,
    const Array<uint32> &indices,
    const Transform &transform,
    RayTestResults &out_results
) const
{
    return TestTriangleList(vertices, indices, transform, ~0, out_results);
}

bool Ray::TestTriangleList(
    const Array<Triangle> &triangles,
    const Transform &transform,
    RayTestResults &out_results
) const
{
    return TestTriangleList(triangles, transform, ~0, out_results);
}

bool Ray::TestTriangleList(
    const Array<Vertex> &vertices,
    const Array<uint32> &indices,
    const Transform &transform,
    RayHitID hit_id,
    RayTestResults &out_results
) const
{
    return TestTriangleList(vertices, indices, transform, hit_id, nullptr, out_results);
}

bool Ray::TestTriangleList(
    const Array<Triangle> &triangles,
    const Transform &transform,
    RayHitID hit_id,
    RayTestResults &out_results
) const
{
    return TestTriangleList(triangles, transform, hit_id, nullptr, out_results);
}

bool Ray::TestTriangleList(
    const Array<Vertex> &vertices,
    const Array<uint32> &indices,
    const Transform &transform,
    RayHitID hit_id,
    const void *user_data,
    RayTestResults &out_results
) const
{
    bool intersected = false;
    
    if (indices.Size() % 3 != 0) {
        DebugLog(
            LogType::Error,
            "Cannot perform raytest on triangle list because number of indices (%llu) was not divisible by 3\n",
            indices.Size()
        );

        return false;
    }

    RayTestResults tmp_results;

    for (SizeType i = 0; i < indices.Size(); i += 3) {
#ifdef HYP_DEBUG_MODE
        AssertThrow(indices[i + 0] < vertices.Size());
        AssertThrow(indices[i + 1] < vertices.Size());
        AssertThrow(indices[i + 2] < vertices.Size());
#endif

        const Triangle triangle {
            vertices[indices[i + 0]].GetPosition() * transform.GetMatrix(),
            vertices[indices[i + 1]].GetPosition() * transform.GetMatrix(),
            vertices[indices[i + 2]].GetPosition() * transform.GetMatrix()
        };

        if (TestTriangle(triangle, static_cast<RayHitID>(i / 3 /* triangle index */), tmp_results)) {
            intersected = true;
        }
    }

    if (intersected) {
        AssertThrow(!tmp_results.Empty());

        auto &first_result = tmp_results.Front();

        // If hit_id is set, overwrite the id (which would be set to the mesh index)
        if (hit_id != ~0u) {
            first_result.id = hit_id;
        }
        
        first_result.user_data = user_data;

        out_results.AddHit(first_result);

        return true;
    }

    return false;
}

bool Ray::TestTriangleList(
    const Array<Triangle> &triangles,
    const Transform &transform,
    RayHitID hit_id,
    const void *user_data,
    RayTestResults &out_results
) const
{
    bool intersected = false;

    RayTestResults tmp_results;

    for (SizeType i = 0; i < triangles.Size(); i++) {
        const Triangle &triangle = triangles[i];

        if (TestTriangle(triangle, static_cast<RayHitID>(i), tmp_results)) {
            intersected = true;
        }
    }

    if (intersected) {
        AssertThrow(!tmp_results.Empty());

        auto &first_result = tmp_results.Front();

        // If hit_id is set, overwrite the id (which would be set to the mesh index)
        if (hit_id != ~0u) {
            first_result.id = hit_id;
        }
        
        first_result.user_data = user_data;

        out_results.AddHit(first_result);

        return true;
    }

    return false;
}

} // namespace hyperion
