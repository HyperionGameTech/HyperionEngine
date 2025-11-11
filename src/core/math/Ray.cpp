/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <core/math/Ray.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/Triangle.hpp>
#include <core/math/Mat4f.hpp>
#include <core/math/MathUtil.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#ifndef HYP_BUILDTOOL
#include <Ray.generated.inl>
#endif

namespace hyperion {

HYP_API Ray operator*(const Mat4f& transform, const Ray& ray)
{
    Vec4f transformedPosition = transform * Vec4f(ray.position, 1.0f);
    transformedPosition /= transformedPosition.w;

    Vec4f transformedDirection = transform * Vec4f(ray.direction, 0.0f);

    Ray result;
    result.position = transformedPosition.GetXYZ();
    result.direction = transformedDirection.GetXYZ().Normalized();

    return result;
}

Ray Ray::operator*(const Mat4f& transform) const
{
    Vec4f transformedPosition = Vec4f(position, 1.0f) * transform;
    transformedPosition /= transformedPosition.w;

    Vec4f transformedDirection = Vec4f(direction, 0.0f) * transform;

    Ray result;
    result.position = transformedPosition.GetXYZ();
    result.direction = transformedDirection.GetXYZ().Normalized();

    return result;
}

Optional<RayHit> Ray::TestAABB(const BoundingBox& aabb) const
{
    RayTestResults outResults;

    if (!TestAABB(aabb, ~0, outResults))
    {
        return {};
    }

    return outResults.Front();
}

bool Ray::TestAABB(const BoundingBox& aabb, RayTestResults& outResults) const
{
    return TestAABB(aabb, ~0, outResults);
}

bool Ray::TestAABB(const BoundingBox& aabb, RayHitID hitId, RayTestResults& outResults) const
{
    return TestAABB(aabb, hitId, nullptr, outResults);
}

bool Ray::TestAABB(const BoundingBox& aabb, RayHitID hitId, const void* userData, RayTestResults& outResults) const
{
    if (!aabb.IsValid())
    {
        // drop out early
        return false;
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
    if (tmax < 0)
    {
        return false;
    }

    // if tmin > tmax, ray doesn't intersect AABB
    if (tmin > tmax)
    {
        return false;
    }

    float distance = tmin;

    if (tmin < 0.0f)
    {
        distance = tmax;
    }

    const Vec3f hitpoint = position + (direction * distance);

    outResults.AddHit(RayHit {
        .hitpoint = hitpoint,
        .normal = -direction.Normalized(), // TODO: change to be box normal
        .distance = distance,
        .id = hitId,
        .userData = userData });

    return true;
}

Optional<RayHit> Ray::TestPlane(const Vec3f& position, const Vec3f& normal) const
{
    RayTestResults outResults;

    if (!TestPlane(position, normal, ~0, outResults))
    {
        return {};
    }

    return outResults.Front();
}

bool Ray::TestPlane(const Vec3f& position, const Vec3f& normal, RayTestResults& outResults) const
{
    return TestPlane(position, normal, ~0, outResults);
}

bool Ray::TestPlane(const Vec3f& position, const Vec3f& normal, RayHitID hitId, RayTestResults& outResults) const
{
    return TestPlane(position, normal, hitId, nullptr, outResults);
}

bool Ray::TestPlane(const Vec3f& position, const Vec3f& normal, RayHitID hitId, const void* userData, RayTestResults& outResults) const
{
    const float denom = direction.Dot(normal);

    if (MathUtil::Abs(denom) < MathUtil::epsilonF)
    {
        return false; // Ray is parallel to the plane
    }

    float t = (position - this->position).Dot(normal) / denom;

    if (t < 0.0f)
    {
        return false; // Intersection is behind the ray's origin
    }

    const Vec3f hitpoint = this->position + (direction * t);

    outResults.AddHit(RayHit {
        .hitpoint = hitpoint,
        .normal = normal,
        .distance = t,
        .id = hitId,
        .userData = userData });

    return true;
}

Optional<RayHit> Ray::TestTriangle(const Triangle& triangle) const
{
    RayTestResults outResults;

    if (!TestTriangle(triangle, ~0, outResults))
    {
        return {};
    }

    return outResults.Front();
}

bool Ray::TestTriangle(const Triangle& triangle, RayTestResults& outResults) const
{
    return TestTriangle(triangle, ~0, outResults);
}

bool Ray::TestTriangle(const Triangle& triangle, RayHitID hitId, RayTestResults& outResults) const
{
    return TestTriangle(triangle, hitId, nullptr, outResults);
}

bool Ray::TestTriangle(const Triangle& triangle, RayHitID hitId, const void* userData, RayTestResults& outResults) const
{
    float t, u, v;

    Vec3f v0v1 = triangle.GetPoint(1).GetPosition() - triangle.GetPoint(0).GetPosition();
    Vec3f v0v2 = triangle.GetPoint(2).GetPosition() - triangle.GetPoint(0).GetPosition();
    Vec3f pvec = direction.Cross(v0v2);

    float det = v0v1.Dot(pvec);

    // ray and triangle are parallel if det is close to 0
    if (std::fabs(det) < MathUtil::epsilonF)
    {
        return false;
    }

    float invDet = 1.0 / det;

    Vec3f tvec = position - triangle.GetPoint(0).GetPosition();
    u = tvec.Dot(pvec) * invDet;

    if (u < 0 || u > 1)
    {
        return false;
    }

    Vec3f qvec = tvec.Cross(v0v1);
    v = direction.Dot(qvec) * invDet;

    if (v < 0 || u + v > 1)
    {
        return false;
    }

    t = v0v2.Dot(qvec) * invDet;

    const Vec3f barycentricCoords = Vec3f(1.0f - u - v, u, v);

    const Vec3f normal = triangle.GetPoint(0).GetNormal() * barycentricCoords.x
        + triangle.GetPoint(1).GetNormal() * barycentricCoords.y
        + triangle.GetPoint(2).GetNormal() * barycentricCoords.z;

    if (t > 0.0f)
    {
        outResults.AddHit({ .hitpoint = position + (direction * t),
            .normal = normal,
            .barycentricCoords = barycentricCoords,
            .distance = t,
            .id = hitId,
            .userData = userData });

        return true;
    }

    return false;
}

Optional<RayHit> Ray::TestTriangleList(
    const Array<Vertex>& vertices,
    const Array<uint32>& indices,
    const Transform& transform) const
{
    RayTestResults outResults;

    if (!TestTriangleList(vertices, indices, transform, ~0, outResults))
    {
        return {};
    }

    return outResults.Front();
}

Optional<RayHit> Ray::TestTriangleList(
    const Span<Triangle>& triangles,
    const Transform& transform) const
{
    RayTestResults outResults;

    if (!TestTriangleList(triangles, transform, ~0, outResults))
    {
        return {};
    }

    return outResults.Front();
}

bool Ray::TestTriangleList(
    const Array<Vertex>& vertices,
    const Array<uint32>& indices,
    const Transform& transform,
    RayTestResults& outResults) const
{
    return TestTriangleList(vertices, indices, transform, ~0, outResults);
}

bool Ray::TestTriangleList(
    const Span<Triangle>& triangles,
    const Transform& transform,
    RayTestResults& outResults) const
{
    return TestTriangleList(triangles, transform, ~0, outResults);
}

bool Ray::TestTriangleList(
    const Array<Vertex>& vertices,
    const Array<uint32>& indices,
    const Transform& transform,
    RayHitID hitId,
    RayTestResults& outResults) const
{
    return TestTriangleList(vertices, indices, transform, hitId, nullptr, outResults);
}

bool Ray::TestTriangleList(
    const Span<Triangle>& triangles,
    const Transform& transform,
    RayHitID hitId,
    RayTestResults& outResults) const
{
    return TestTriangleList(triangles, transform, hitId, nullptr, outResults);
}

bool Ray::TestTriangleList(
    const Array<Vertex>& vertices,
    const Array<uint32>& indices,
    const Transform& transform,
    RayHitID hitId,
    const void* userData,
    RayTestResults& outResults) const
{
    bool intersected = false;

    if (indices.Size() % 3 != 0)
    {
        HYP_LOG(Math, Warning, "Cannot perform raytest on triangle list because number of indices ({}) was not divisible by 3", indices.Size());

        return false;
    }

    const Mat4f transformMatrix = transform.GetMatrix();

    RayTestResults tmpResults;

    for (SizeType i = 0; i < indices.Size(); i += 3)
    {
        HYP_CORE_ASSERT(indices[i + 0] < vertices.Size());
        HYP_CORE_ASSERT(indices[i + 1] < vertices.Size());
        HYP_CORE_ASSERT(indices[i + 2] < vertices.Size());

        const Triangle triangle {
            vertices[indices[i + 0]].GetPosition() * transformMatrix,
            vertices[indices[i + 1]].GetPosition() * transformMatrix,
            vertices[indices[i + 2]].GetPosition() * transformMatrix
        };

        if (TestTriangle(triangle, static_cast<RayHitID>(i / 3 /* triangle index */), tmpResults))
        {
            intersected = true;
        }
    }

    if (intersected)
    {
        HYP_CORE_ASSERT(!tmpResults.Empty());

        auto& firstResult = tmpResults.Front();

        // If hitId is set, overwrite the id (which would be set to the mesh index)
        if (hitId != ~0u)
        {
            firstResult.id = hitId;
        }

        firstResult.userData = userData;

        outResults.AddHit(firstResult);

        return true;
    }

    return false;
}

bool Ray::TestTriangleList(
    const Span<Triangle>& triangles,
    const Transform& transform,
    RayHitID hitId,
    const void* userData,
    RayTestResults& outResults) const
{
    bool intersected = false;

    RayTestResults tmpResults;

    for (SizeType i = 0; i < triangles.Size(); i++)
    {
        const Triangle& triangle = triangles[i];

        if (TestTriangle(triangle, static_cast<RayHitID>(i), tmpResults))
        {
            intersected = true;
        }
    }

    if (intersected)
    {
        HYP_CORE_ASSERT(!tmpResults.Empty());

        auto& firstResult = tmpResults.Front();

        // If hitId is set, overwrite the id (which would be set to the mesh index)
        if (hitId != ~0u)
        {
            firstResult.id = hitId;
        }

        firstResult.userData = userData;

        outResults.AddHit(firstResult);

        return true;
    }

    return false;
}

} // namespace hyperion
