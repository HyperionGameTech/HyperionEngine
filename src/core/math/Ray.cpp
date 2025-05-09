/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <core/math/Ray.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/Triangle.hpp>
#include <core/math/Matrix4.hpp>
#include <core/math/MathUtil.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_API Ray operator*(const Matrix4 &transform, const Ray &ray)
{
    Vec4f transformed_position = transform * Vec4f(ray.position, 1.0f);
    transformed_position /= transformed_position.w;

    Vec4f transformed_direction = transform * Vec4f(ray.direction, 0.0f);

    Ray result;
    result.position = transformed_position.GetXYZ();
    result.direction = transformed_direction.GetXYZ().Normalized();

    return result;
}

Ray Ray::operator*(const Matrix4 &transform) const
{
    Vec4f transformed_position = Vec4f(position, 1.0f) * transform;
    transformed_position /= transformed_position.w;

    Vec4f transformed_direction = Vec4f(direction, 0.0f) * transform;

    Ray result;
    result.position = transformed_position.GetXYZ();
    result.direction = transformed_direction.GetXYZ().Normalized();

    return result;
}

Optional<RayHit> Ray::TestAABB(const BoundingBox &aabb) const
{
    RayTestResults out_results;

    if (!TestAABB(aabb, ~0, out_results)) {
        return { };
    }

    return out_results.Front();
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

    const Vec3f hitpoint = position + (direction * distance);

    out_results.AddHit(RayHit {
        .hitpoint   = hitpoint,
        .normal     = -direction.Normalized(), // TODO: change to be box normal
        .distance   = distance,
        .id         = hit_id,
        .user_data  = user_data
    });

    return true;
}


Optional<RayHit> Ray::TestPlane(const Vec3f &position, const Vec3f &normal) const
{
    RayTestResults out_results;

    if (!TestPlane(position, normal, ~0, out_results)) {
        return { };
    }

    return out_results.Front();
}

bool Ray::TestPlane(const Vec3f &position, const Vec3f &normal, RayTestResults &out_results) const
{
    return TestPlane(position, normal, ~0, out_results);
}

bool Ray::TestPlane(const Vec3f &position, const Vec3f &normal, RayHitID hit_id, RayTestResults &out_results) const
{
    return TestPlane(position, normal, hit_id, nullptr, out_results);
}

bool Ray::TestPlane(const Vec3f &position, const Vec3f &normal, RayHitID hit_id, const void *user_data, RayTestResults &out_results) const
{
    const float denom = direction.Dot(normal);

    if (MathUtil::Abs(denom) < MathUtil::epsilon_f) {
        return false; // Ray is parallel to the plane
    }

    float t = (position - this->position).Dot(normal) / denom;

    if (t < 0.0f) {
        return false; // Intersection is behind the ray's origin
    }

    const Vec3f hitpoint = this->position + (direction * t);

    out_results.AddHit(RayHit{
        .hitpoint  = hitpoint,
        .normal    = normal,
        .distance  = t,
        .id        = hit_id,
        .user_data = user_data
    });

    return true;
}

Optional<RayHit> Ray::TestTriangle(const Triangle &triangle) const
{
    RayTestResults out_results;

    if (!TestTriangle(triangle, ~0, out_results)) {
        return { };
    }

    return out_results.Front();
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
        HYP_LOG(Math, Warning, "Cannot perform raytest on triangle list because number of indices ({}) was not divisible by 3", indices.Size());

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
