/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <ActiveRagdoll/RagdollMath.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Hyperion {
namespace RagdollMath {

namespace {

PlanePoint ClosestOnSegment(const PlanePoint& a, const PlanePoint& b, const PlanePoint& p)
{
    const PlanePoint edge = b - a;
    const float lengthSq = edge.Dot(edge);

    if (lengthSq < 1e-10f)
    {
        return a;
    }

    const float t = MathUtil::Clamp((p - a).Dot(edge) / lengthSq, 0.0f, 1.0f);

    return a + edge * t;
}

// > 0 when a, b, p turn counter-clockwise
float CrossOffset(const PlanePoint& b, const PlanePoint& p, const PlanePoint& a)
{
    return (b - a).Cross(p - a);
}

JPH::Vec3 GetAnyPerpendicular(JPH::Vec3Arg direction)
{
    JPH::Vec3 axis = JPH::Vec3::sAxisX();

    if (std::abs(direction.Dot(axis)) > 0.9f)
    {
        axis = JPH::Vec3::sAxisY();
    }

    return direction.Cross(axis).Normalized();
}

} // namespace

GravityFrame GravityFrame::FromGravity(JPH::Vec3Arg gravityVector)
{
    GravityFrame frame;

    frame.up = (-gravityVector).NormalizedOr(JPH::Vec3::sAxisY());
    frame.axisU = frame.up.GetNormalizedPerpendicular();
    frame.axisV = frame.up.Cross(frame.axisU);
    frame.gravity = gravityVector.Length();

    return frame;
}

JPH::Vec3 GetRotationVector(JPH::QuatArg rotation)
{
    JPH::Quat shortest = rotation;

    if (shortest.GetW() < 0.0f)
    {
        shortest = -shortest;
    }

    JPH::Vec3 axis;
    float angle = 0.0f;
    shortest.GetAxisAngle(axis, angle);

    return axis * angle;
}

JPH::Quat IntegrateRotation(JPH::QuatArg rotation, JPH::Vec3Arg angularVelocity, float time)
{
    const JPH::Vec3 delta = angularVelocity * time;
    const float angle = delta.Length();

    if (angle < 1e-6f)
    {
        return rotation;
    }

    return (JPH::Quat::sRotation(delta / angle, angle) * rotation).Normalized();
}

JPH::Quat FrameRotation(JPH::Vec3Arg localAxis, JPH::Vec3Arg localHinge, JPH::Vec3Arg worldAxis, JPH::Vec3Arg worldHinge)
{
    const JPH::Vec3 axisLocal = localAxis.Normalized();
    const JPH::Vec3 hingeLocal = (localHinge - axisLocal * localHinge.Dot(axisLocal)).NormalizedOr(axisLocal.GetNormalizedPerpendicular());
    const JPH::Vec3 axisWorld = worldAxis.Normalized();
    const JPH::Vec3 hingeWorld = (worldHinge - axisWorld * worldHinge.Dot(axisWorld)).NormalizedOr(axisWorld.GetNormalizedPerpendicular());

    const JPH::Mat44 local(JPH::Vec4(axisLocal, 0.0f), JPH::Vec4(hingeLocal, 0.0f), JPH::Vec4(axisLocal.Cross(hingeLocal), 0.0f), JPH::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    const JPH::Mat44 world(JPH::Vec4(axisWorld, 0.0f), JPH::Vec4(hingeWorld, 0.0f), JPH::Vec4(axisWorld.Cross(hingeWorld), 0.0f), JPH::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    return (world * local.Transposed3x3()).GetQuaternion().Normalized();
}

uint32 BuildHull(PlanePoint* points, uint32 count, PlanePoint* outHull)
{
    if (count < 3)
    {
        std::copy(points, points + count, outHull);

        return count;
    }

    std::sort(points, points + count, [](const PlanePoint& a, const PlanePoint& b)
        {
            return a.x < b.x || (a.x == b.x && a.y < b.y);
        });

    uint32 size = 0;

    // pops vertices until appending `point` turns strictly counter-clockwise, never below `minSize`
    auto push = [&](const PlanePoint& point, uint32 minSize)
    {
        while (size >= minSize && CrossOffset(outHull[size - 1], point, outHull[size - 2]) <= 0.0f)
        {
            size--;
        }

        outHull[size++] = point;
    };

    for (uint32 i = 0; i < count; i++)
    {
        push(points[i], 2);
    }

    // the upper hull starts one before the rightmost point, which already ends the lower hull
    const uint32 upperMinSize = size + 1;

    for (uint32 i = count - 1; i > 0; i--)
    {
        push(points[i - 1], upperMinSize);
    }

    // the upper pass ends on points[0] again
    return size - 1;
}

PlanePoint ClosestOnHull(const PlanePoint* hull, uint32 count, const PlanePoint& p, float& outSignedDistance)
{
    if (count == 0)
    {
        outSignedDistance = 0.0f;

        return p;
    }

    if (count == 1)
    {
        outSignedDistance = p.Distance(hull[0]);

        return hull[0];
    }

    bool inside = count >= 3;
    PlanePoint closest = hull[0];
    float closestDistance = std::numeric_limits<float>::max();
    uint32 edgeCount = count;

    // a two point hull is one segment, so don't walk it back to the start
    if (count == 2)
    {
        edgeCount = 1;
    }

    for (uint32 i = 0; i < edgeCount; i++)
    {
        const PlanePoint& a = hull[i];
        const PlanePoint& b = hull[(i + 1) % count];

        if (CrossOffset(b, p, a) < 0.0f)
        {
            inside = false;
        }

        const PlanePoint onEdge = ClosestOnSegment(a, b, p);
        const float distance = p.Distance(onEdge);

        if (distance < closestDistance)
        {
            closestDistance = distance;
            closest = onEdge;
        }
    }

    if (inside)
    {
        outSignedDistance = -closestDistance;

        return p;
    }

    outSignedDistance = closestDistance;

    return closest;
}

PlanePoint GetHullCentroid(const PlanePoint* hull, uint32 count)
{
    PlanePoint average;

    for (uint32 i = 0; i < count; i++)
    {
        average = average + hull[i] / float(count);
    }

    // area-weighted centroid of the fan around the vertex average, so vertex density doesn't bias it
    float area = 0.0f;
    PlanePoint weighted;

    for (uint32 i = 0; i < count; i++)
    {
        const PlanePoint a = hull[i] - average;
        const PlanePoint b = hull[(i + 1) % count] - average;
        const float cross = a.Cross(b);

        area += cross;
        weighted = weighted + (a + b) * cross;
    }

    if (std::abs(area) < 1e-8f)
    {
        return average;
    }

    return average + weighted / (3.0f * area);
}

TwoBoneIKResult SolveTwoBoneIK(JPH::Vec3Arg root, JPH::Vec3Arg target, float upperLength, float lowerLength, JPH::Vec3Arg bendHint)
{
    static constexpr float Epsilon = 1e-4f;

    const JPH::Vec3 toTarget = target - root;
    const float distance = toTarget.Length();

    JPH::Vec3 direction;

    if (distance > Epsilon)
    {
        direction = toTarget / distance;
    }
    else
    {
        direction = GetAnyPerpendicular(bendHint);
    }

    const float maxReach = upperLength + lowerLength;
    const float minReach = std::abs(upperLength - lowerLength);
    const float reach = MathUtil::Clamp(distance, minReach + Epsilon, MathUtil::Max(maxReach - Epsilon, Epsilon));

    JPH::Vec3 bend = bendHint - direction * bendHint.Dot(direction);
    const float bendLength = bend.Length();

    if (bendLength > Epsilon)
    {
        bend = bend / bendLength;
    }
    else
    {
        bend = GetAnyPerpendicular(direction);
    }

    const float cosUpper = MathUtil::Clamp((upperLength * upperLength + reach * reach - lowerLength * lowerLength) / (2.0f * upperLength * reach), -1.0f, 1.0f);
    const float sinUpper = std::sqrt(MathUtil::Max(1.0f - cosUpper * cosUpper, 0.0f));

    TwoBoneIKResult result;
    result.mid = root + direction * (upperLength * cosUpper) + bend * (upperLength * sinUpper);
    result.end = root + direction * reach;

    return result;
}

} // namespace RagdollMath
} // namespace Hyperion
