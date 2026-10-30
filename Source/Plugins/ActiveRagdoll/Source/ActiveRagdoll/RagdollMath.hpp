/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Math/Vector3.hpp>
#include <Core/Math/Quat4f.hpp>
#include <Core/Math/Mat4f.hpp>
#include <Core/Math/MathUtil.hpp>

#include <Core/Types.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Mat44.h>

#include <cmath>

namespace Hyperion {
namespace RagdollMath {

static constexpr float SupportNormalMinUp = 0.7f;

static constexpr uint32 SoleCorners = 4;
static constexpr float SoleCornerSigns[SoleCorners][2] = {
    { -1.0f, -1.0f },
    { 1.0f, -1.0f },
    { 1.0f, 1.0f },
    { -1.0f, 1.0f },
};

HYP_FORCE_INLINE JPH::Vec3 ToJolt(const Vec3f& v)
{
    return JPH::Vec3(v.x, v.y, v.z);
}

HYP_FORCE_INLINE Vec3f FromJolt(JPH::Vec3Arg v)
{
    return Vec3f(v.GetX(), v.GetY(), v.GetZ());
}

HYP_FORCE_INLINE JPH::Quat ToJolt(const Quat4f& q)
{
    return JPH::Quat(q.x, q.y, q.z, q.w);
}

HYP_FORCE_INLINE Quat4f FromJolt(JPH::QuatArg q)
{
    return Quat4f(q.GetX(), q.GetY(), q.GetZ(), q.GetW());
}

HYP_FORCE_INLINE float SmoothStep(float t)
{
    const float x = MathUtil::Clamp(t, 0.0f, 1.0f);

    return x * x * (3.0f - 2.0f * x);
}

// 1 for +0, so a body exactly on the centre line still gets a side
HYP_FORCE_INLINE float GetSign(float x)
{
    if (std::signbit(x))
    {
        return -1.0f;
    }

    return 1.0f;
}

struct PlanePoint
{
    float x = 0.0f;
    float y = 0.0f;

    HYP_FORCE_INLINE PlanePoint operator+(const PlanePoint& other) const
    {
        return PlanePoint { x + other.x, y + other.y };
    }

    HYP_FORCE_INLINE PlanePoint operator-(const PlanePoint& other) const
    {
        return PlanePoint { x - other.x, y - other.y };
    }

    HYP_FORCE_INLINE PlanePoint operator*(float scale) const
    {
        return PlanePoint { x * scale, y * scale };
    }

    HYP_FORCE_INLINE PlanePoint operator/(float scale) const
    {
        return PlanePoint { x / scale, y / scale };
    }

    HYP_FORCE_INLINE float Dot(const PlanePoint& other) const
    {
        return x * other.x + y * other.y;
    }

    HYP_FORCE_INLINE float Cross(const PlanePoint& other) const
    {
        return x * other.y - y * other.x;
    }

    HYP_FORCE_INLINE float Distance(const PlanePoint& other) const
    {
        const PlanePoint delta = *this - other;

        return std::sqrt(delta.Dot(delta));
    }
};

// up axis and ground plane basis from the physics gravity
struct GravityFrame
{
    JPH::Vec3 up = JPH::Vec3::sAxisY();
    JPH::Vec3 axisU = JPH::Vec3::sAxisX();
    JPH::Vec3 axisV = JPH::Vec3::sAxisZ();
    float gravity = 9.81f;

    static GravityFrame FromGravity(JPH::Vec3Arg gravityVector);

    HYP_FORCE_INLINE JPH::Vec3 Horizontal(JPH::Vec3Arg v) const
    {
        return v - up * v.Dot(up);
    }

    HYP_FORCE_INLINE PlanePoint ToPlane(JPH::Vec3Arg point) const
    {
        return PlanePoint { point.Dot(axisU), point.Dot(axisV) };
    }

    HYP_FORCE_INLINE JPH::Vec3 FromPlane(const PlanePoint& point, float height) const
    {
        return axisU * point.x + axisV * point.y + up * height;
    }
};

// rotation of `child` relative to `parent`, both in world space
HYP_FORCE_INLINE JPH::Quat RelativeRotation(JPH::QuatArg parent, JPH::QuatArg child)
{
    return (parent.Conjugated() * child).Normalized();
}

JPH::Vec3 GetRotationVector(JPH::QuatArg rotation);
JPH::Quat IntegrateRotation(JPH::QuatArg rotation, JPH::Vec3Arg angularVelocity, float time);
JPH::Quat FrameRotation(JPH::Vec3Arg localAxis, JPH::Vec3Arg localHinge, JPH::Vec3Arg worldAxis, JPH::Vec3Arg worldHinge);

// convex hull by monotone chain, counter-clockwise with no repeated closing vertex; sorts `points`
uint32 BuildHull(PlanePoint* points, uint32 count, PlanePoint* outHull);
PlanePoint ClosestOnHull(const PlanePoint* hull, uint32 count, const PlanePoint& p, float& outSignedDistance);
PlanePoint GetHullCentroid(const PlanePoint* hull, uint32 count);

struct TwoBoneIKResult
{
    JPH::Vec3 mid = JPH::Vec3::sZero();
    JPH::Vec3 end = JPH::Vec3::sZero();
};

TwoBoneIKResult SolveTwoBoneIK(JPH::Vec3Arg root, JPH::Vec3Arg target, float upperLength, float lowerLength, JPH::Vec3Arg bendHint);

} // namespace RagdollMath
} // namespace Hyperion
