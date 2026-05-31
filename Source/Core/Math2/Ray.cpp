/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/
#include <Core/math/Ray.hpp>
#include <Core/math/BoundingBox.hpp>
#include <Core/math/Triangle.hpp>
#include <Core/math/Mat4f.hpp>
#include <Core/math/MathUtil.hpp>

#include <Core/logging/LogChannels.hpp>
#include <Core/logging/Logger.hpp>

#ifndef HYP_TOOL
#include <Ray.generated.inl>
#endif

namespace Hyperion {

CORE_API Ray operator*(const Mat4f& transform, const Ray& ray)
{
    Vec4f transformedPosition = transform.TransformVector(Vec4f(ray.position, 1.0f));
    transformedPosition /= transformedPosition.w;

    Vec4f transformedDirection = transform.TransformVector(Vec4f(ray.direction, 0.0f));

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
        .id = hitId
    });

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
        .id = hitId
    });

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
    float t, u, v;

    Vec3f v0v1 = triangle.GetPoint(1) - triangle.GetPoint(0);
    Vec3f v0v2 = triangle.GetPoint(2) - triangle.GetPoint(0);
    Vec3f pvec = direction.Cross(v0v2);

    float det = v0v1.Dot(pvec);

    // ray and triangle are parallel if det is close to 0
    if (std::fabs(det) < MathUtil::epsilonF)
    {
        return false;
    }

    float invDet = 1.0 / det;

    Vec3f tvec = position - triangle.GetPoint(0);
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

    if (t > 0.0f)
    {
        outResults.AddHit({
            .hitpoint = position + (direction * t),
            .normal = triangle.GetNormal(),
            .barycentricCoords = barycentricCoords,
            .distance = t,
            .id = hitId
        });

        return true;
    }

    return false;
}

} // namespace Hyperion
