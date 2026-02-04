/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/math/Frustum.hpp>

#ifndef HYP_BUILDTOOL
#include <Frustum.generated.inl>
#endif

namespace Hyperion {

static const FixedArray<Vec4f, 8> s_corners {
    Vec4f { -1.0f, -1.0f, 0.0f, 1.0f },
    Vec4f { -1.0f, 1.0f, 0.0f, 1.0f },
    Vec4f { 1.0f, 1.0f, 0.0f, 1.0f },
    Vec4f { 1.0f, -1.0f, 0.0f, 1.0f },
    Vec4f { -1.0f, -1.0f, 1.0f, 1.0f },
    Vec4f { -1.0f, 1.0f, 1.0f, 1.0f },
    Vec4f { 1.0f, 1.0f, 1.0f, 1.0f },
    Vec4f { 1.0f, -1.0f, 1.0f, 1.0f }
};

Frustum::Frustum()
{
}

Frustum::Frustum(const Mat4f& viewProj)
{
    SetFromViewProjectionMatrix(viewProj);
}

bool Frustum::ContainsAABB(const BoundingBox& aabb) const
{
    const FixedArray<Vec3f, 8> corners = aabb.GetCorners();

    for (const Vec4f& plane : planes)
    {
        if (plane.Dot(Vec4f(corners[0], 1.0f)) > 0.0f)
            continue;
        if (plane.Dot(Vec4f(corners[1], 1.0f)) > 0.0f)
            continue;
        if (plane.Dot(Vec4f(corners[2], 1.0f)) > 0.0f)
            continue;
        if (plane.Dot(Vec4f(corners[3], 1.0f)) > 0.0f)
            continue;
        if (plane.Dot(Vec4f(corners[4], 1.0f)) > 0.0f)
            continue;
        if (plane.Dot(Vec4f(corners[5], 1.0f)) > 0.0f)
            continue;
        if (plane.Dot(Vec4f(corners[6], 1.0f)) > 0.0f)
            continue;
        if (plane.Dot(Vec4f(corners[7], 1.0f)) > 0.0f)
            continue;

        return false;
    }

    return true;
}

bool Frustum::FullyContainsAABB(const BoundingBox& aabb) const
{
    const FixedArray<Vec3f, 8> corners = aabb.GetCorners();

    for (const Vec4f& plane : planes)
    {
        if (plane.Dot(Vec4f(corners[0], 1.0f)) < 0.0f)
            return false;
        if (plane.Dot(Vec4f(corners[1], 1.0f)) < 0.0f)
            return false;
        if (plane.Dot(Vec4f(corners[2], 1.0f)) < 0.0f)
            return false;
        if (plane.Dot(Vec4f(corners[3], 1.0f)) < 0.0f)
            return false;
        if (plane.Dot(Vec4f(corners[4], 1.0f)) < 0.0f)
            return false;
        if (plane.Dot(Vec4f(corners[5], 1.0f)) < 0.0f)
            return false;
        if (plane.Dot(Vec4f(corners[6], 1.0f)) < 0.0f)
            return false;
        if (plane.Dot(Vec4f(corners[7], 1.0f)) < 0.0f)
            return false;
    }

    return true;
}

bool Frustum::ContainsBoundingSphere(const BoundingSphere& sphere) const
{
    for (const Vec4f& plane : planes)
    {
        if (plane.Dot(Vec4f(sphere.center, 1.0f)) <= -sphere.radius)
        {
            return false;
        }
    }

    return true;
}

bool Frustum::ContainsPoint(const Vec3f& point) const
{
    for (const Vec4f& plane : planes)
    {
        if (plane.Dot(Vec4f(point, 1.0f)) < 0.0f)
        {
            return false;
        }
    }

    return true;
}

Frustum& Frustum::SetFromViewProjectionMatrix(const Mat4f& viewProj)
{
    const Mat4f mat = viewProj.Transpose();

    planes[0][0] = mat[0][3] - mat[0][0];
    planes[0][1] = mat[1][3] - mat[1][0];
    planes[0][2] = mat[2][3] - mat[2][0];
    planes[0][3] = mat[3][3] - mat[3][0];
    // planes[0].Normalize();

    planes[1][0] = mat[0][3] + mat[0][0];
    planes[1][1] = mat[1][3] + mat[1][0];
    planes[1][2] = mat[2][3] + mat[2][0];
    planes[1][3] = mat[3][3] + mat[3][0];
    // planes[1].Normalize();

    planes[2][0] = mat[0][3] + mat[0][1];
    planes[2][1] = mat[1][3] + mat[1][1];
    planes[2][2] = mat[2][3] + mat[2][1];
    planes[2][3] = mat[3][3] + mat[3][1];
    // planes[2].Normalize();

    planes[3][0] = mat[0][3] - mat[0][1];
    planes[3][1] = mat[1][3] - mat[1][1];
    planes[3][2] = mat[2][3] - mat[2][1];
    planes[3][3] = mat[3][3] - mat[3][1];
    // planes[3].Normalize();

    planes[4][0] = mat[0][3] - mat[0][2];
    planes[4][1] = mat[1][3] - mat[1][2];
    planes[4][2] = mat[2][3] - mat[2][2];
    planes[4][3] = mat[3][3] - mat[3][2];
    // planes[4].Normalize();

    planes[5][0] = mat[0][3] + mat[0][2];
    planes[5][1] = mat[1][3] + mat[1][2];
    planes[5][2] = mat[2][3] + mat[2][2];
    planes[5][3] = mat[3][3] + mat[3][2];
    // planes[5].Normalize();

    const Mat4f clipToWorld = viewProj.Inverse();

    for (uint32 i = 0; i < 8; i++)
    {
        Vec4f corner = clipToWorld * s_corners[i];
        corner /= corner.w;

        corners[i] = corner.GetXYZ();
    }

    return *this;
}

Vec3f Frustum::GetIntersectionPoint(uint32 planeIndex0, uint32 planeIndex1, uint32 planeIndex2) const
{
    const Vec4f planes[3] = { GetPlane(planeIndex0), GetPlane(planeIndex1), GetPlane(planeIndex2) };

    Vec3f bxc = planes[1].GetXYZ().Cross(planes[2].GetXYZ());
    Vec3f cxa = planes[2].GetXYZ().Cross(planes[0].GetXYZ());
    Vec3f axb = planes[0].GetXYZ().Cross(planes[1].GetXYZ());

    Vec3f r = (bxc * -planes[0].w) - (cxa * planes[1].w) - (axb * planes[2].w);

    return r * (1.0f / planes[0].GetXYZ().Dot(bxc));
}

} // namespace Hyperion
