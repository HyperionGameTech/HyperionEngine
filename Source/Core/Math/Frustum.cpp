/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Core/Math/Frustum.hpp>

#ifndef HYP_TOOL
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
    for (const Vec4f& plane : planes)
    {
        const Vec4f positiveVertex(
            plane.x >= 0.0f ? aabb.max.x : aabb.min.x,
            plane.y >= 0.0f ? aabb.max.y : aabb.min.y,
            plane.z >= 0.0f ? aabb.max.z : aabb.min.z,
            1.0f);

        if (plane.Dot(positiveVertex) <= 0.0f)
        {
            return false;
        }
    }

    return true;
}

bool Frustum::FullyContainsAABB(const BoundingBox& aabb) const
{
    for (const Vec4f& plane : planes)
    {
        // Mirror of ContainsAABB: the corner that minimizes dot(plane, corner). If it is in front of
        // the plane, all eight corners are.
        const Vec4f negativeVertex(
            plane.x >= 0.0f ? aabb.min.x : aabb.max.x,
            plane.y >= 0.0f ? aabb.min.y : aabb.max.y,
            plane.z >= 0.0f ? aabb.min.z : aabb.max.z,
            1.0f);

        if (plane.Dot(negativeVertex) < 0.0f)
        {
            return false;
        }
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

    planes[4][0] = mat[0][2];
    planes[4][1] = mat[1][2];
    planes[4][2] = mat[2][2];
    planes[4][3] = mat[3][2];
    // planes[4].Normalize();

    planes[5][0] = mat[0][3] - mat[0][2];
    planes[5][1] = mat[1][3] - mat[1][2];
    planes[5][2] = mat[2][3] - mat[2][2];
    planes[5][3] = mat[3][3] - mat[3][2];
    // planes[5].Normalize();

    const Mat4f clipToWorld = viewProj.Inverse();

    for (uint32 i = 0; i < 8; i++)
    {
        Vec4f corner = clipToWorld.TransformVector(s_corners[i]);
        corner /= corner.w;

        corners[i] = corner.GetXYZ();
    }

    return *this;
}

void Frustum::StoreViewProjectionMatrix(Mat4f& outVP) const
{
    Mat4f mat;

    // Reconstruct the transposed matrix rows from the linear combinations of the planes
    for (uint32 j = 0; j < 4; j++)
    {
        // planes[0] = mat[][3] - mat[][0]
        // planes[1] = mat[][3] + mat[][0]
        mat[j][3] = 0.5f * (planes[0][j] + planes[1][j]);
        mat[j][0] = 0.5f * (planes[1][j] - planes[0][j]);

        // planes[2] = mat[][3] + mat[][1]
        // planes[3] = mat[][3] - mat[][1]
        mat[j][1] = 0.5f * (planes[2][j] - planes[3][j]);

        // planes[4] = mat[][2]             (near plane)
        // planes[5] = mat[][3] - mat[][2]  (far plane)
        mat[j][2] = planes[4][j];
    }

    outVP = mat.Transpose();
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

HYP_NODISCARD Frustum Frustum::SubFrustum(const float inNearRatio, const float inFarRatio) const
{
    Frustum sub;

    for (int i = 0; i < 4; ++i)
    {
        sub.corners[i] = MathUtil::Lerp(corners[i], corners[i + 4], inNearRatio);
        sub.corners[i + 4] = MathUtil::Lerp(corners[i], corners[i + 4], inFarRatio);
    }

    sub.planes[0] = planes[0];
    sub.planes[1] = planes[1];
    sub.planes[2] = planes[2];
    sub.planes[3] = planes[3];

    sub.planes[4] = Vec4f(planes[4].GetXYZ(), 0.0f);
    sub.planes[4].w = -sub.planes[4].GetXYZ().Dot(sub.corners[0]);

    sub.planes[5] = Vec4f(planes[5].GetXYZ(), 0.0f);
    sub.planes[5].w = -sub.planes[5].GetXYZ().Dot(sub.corners[4]);

    return sub;
}

} // namespace Hyperion
