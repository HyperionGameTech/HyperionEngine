/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <math/Frustum.hpp>

namespace hyperion {

static const FixedArray<Vec4f, 8> frustucorners_ndc {
    Vec4f { -1.0f, -1.0f, 0.0f, 1.0f },
    Vec4f { -1.0f,  1.0f, 0.0f, 1.0f },
    Vec4f {  1.0f,  1.0f, 0.0f, 1.0f },
    Vec4f {  1.0f, -1.0f, 0.0f, 1.0f },
    Vec4f { -1.0f, -1.0f, 1.0f, 1.0f },
    Vec4f { -1.0f,  1.0f, 1.0f, 1.0f },
    Vec4f {  1.0f,  1.0f, 1.0f, 1.0f },
    Vec4f {  1.0f, -1.0f, 1.0f, 1.0f }
};

Frustum::Frustum()
{
}

Frustum::Frustum(const Matrix4 &view_proj)
{
    SetFromViewProjectionMatrix(view_proj);
}

bool Frustum::ContainsAABB(const BoundingBox &aabb) const
{
    const FixedArray<Vec3f, 8> corners = aabb.GetCorners();

    for (const Vec4f &plane : planes) {
        if (plane.Dot(Vec4f(corners[0], 1.0f)) > 0.0f) continue;
        if (plane.Dot(Vec4f(corners[1], 1.0f)) > 0.0f) continue;
        if (plane.Dot(Vec4f(corners[2], 1.0f)) > 0.0f) continue;
        if (plane.Dot(Vec4f(corners[3], 1.0f)) > 0.0f) continue;
        if (plane.Dot(Vec4f(corners[4], 1.0f)) > 0.0f) continue;
        if (plane.Dot(Vec4f(corners[5], 1.0f)) > 0.0f) continue;
        if (plane.Dot(Vec4f(corners[6], 1.0f)) > 0.0f) continue;
        if (plane.Dot(Vec4f(corners[7], 1.0f)) > 0.0f) continue;

        return false;
    }

    return true;
}

bool Frustum::ContainsBoundingSphere(const BoundingSphere &sphere) const
{
    for (const Vec4f &plane : planes) {
        if (plane.Dot(Vec4f(sphere.center, 1.0f)) <= -sphere.radius) {
            return false;
        }
    }

    return true;
}

Frustum &Frustum::SetFromViewProjectionMatrix(const Matrix4 &view_proj)
{
    const Matrix4 mat = view_proj.Transposed();
    
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

    const Matrix4 clip_to_world = view_proj.Inverted();

    for (uint i = 0; i < 8; i++) {
        Vec4f corner = clip_to_world * frustucorners_ndc[i];
        corner /= corner.w;

        corners[i] = corner.GetXYZ();
    }

    return *this;
}

Vec3f Frustum::GetIntersectionPoint(uint plane_index_0, uint plane_index_1, uint plane_index_2) const
{
    const Vec4f planes[3] = { GetPlane(plane_index_0), GetPlane(plane_index_1), GetPlane(plane_index_2) };

    Vec3f bxc = planes[1].GetXYZ().Cross(planes[2].GetXYZ());
    Vec3f cxa = planes[2].GetXYZ().Cross(planes[0].GetXYZ());
    Vec3f axb = planes[0].GetXYZ().Cross(planes[1].GetXYZ());

    Vec3f r = (bxc * -planes[0].w) - (cxa * planes[1].w) - (axb * planes[2].w);

    return r * (1.0f / planes[0].GetXYZ().Dot(bxc));
}

} // namespace hyperion
