#include <math/Frustum.hpp>

namespace hyperion {

Frustum::Frustum()
{
}

Frustum::Frustum(const Frustum &other)
    : m_planes(other.m_planes),
      m_corners(other.m_corners)
{
}

Frustum::Frustum(const Matrix4 &view_proj)
{
    SetFromViewProjectionMatrix(view_proj);
}

Bool Frustum::ContainsAABB(const BoundingBox &aabb) const
{
    const FixedArray<Vec3f, 8> corners = aabb.GetCorners();

    for (const Vec4f &plane : m_planes) {
        Bool pass = false;

        for (const Vec3f &corner : corners) {
            if (plane.Dot(Vec4f(corner, 1.0f)) > 0.0f) {
                pass = true;
                break;
            }
        }

        if (pass) {
            continue;
        }

        return false;
    }

    return true;
}

Bool Frustum::ContainsBoundingSphere(const BoundingSphere &sphere) const
{
    for (const Vec4f &plane : m_planes) {
        if (plane.Dot(Vec4f(sphere.center, 1.0f)) <= -sphere.radius) {
            return false;
        }
    }

    return true;
}

Frustum &Frustum::SetFromViewProjectionMatrix(const Matrix4 &view_proj)
{
    const Matrix4 mat = view_proj.Transposed();
    
    m_planes[0][0] = mat[0][3] - mat[0][0];
    m_planes[0][1] = mat[1][3] - mat[1][0];
    m_planes[0][2] = mat[2][3] - mat[2][0];
    m_planes[0][3] = mat[3][3] - mat[3][0];
    // m_planes[0].Normalize();

    m_planes[1][0] = mat[0][3] + mat[0][0];
    m_planes[1][1] = mat[1][3] + mat[1][0];
    m_planes[1][2] = mat[2][3] + mat[2][0];
    m_planes[1][3] = mat[3][3] + mat[3][0];
    // m_planes[1].Normalize();

    m_planes[2][0] = mat[0][3] + mat[0][1];
    m_planes[2][1] = mat[1][3] + mat[1][1];
    m_planes[2][2] = mat[2][3] + mat[2][1];
    m_planes[2][3] = mat[3][3] + mat[3][1];
    // m_planes[2].Normalize();

    m_planes[3][0] = mat[0][3] - mat[0][1];
    m_planes[3][1] = mat[1][3] - mat[1][1];
    m_planes[3][2] = mat[2][3] - mat[2][1];
    m_planes[3][3] = mat[3][3] - mat[3][1];
    // m_planes[3].Normalize();

    m_planes[4][0] = mat[0][3] - mat[0][2];
    m_planes[4][1] = mat[1][3] - mat[1][2];
    m_planes[4][2] = mat[2][3] - mat[2][2];
    m_planes[4][3] = mat[3][3] - mat[3][2];
    // m_planes[4].Normalize();

    m_planes[5][0] = mat[0][3] + mat[0][2];
    m_planes[5][1] = mat[1][3] + mat[1][2];
    m_planes[5][2] = mat[2][3] + mat[2][2];
    m_planes[5][3] = mat[3][3] + mat[3][2];
    // m_planes[5].Normalize();

    const Matrix4 clip_to_world = view_proj.Inverted();

    static const FixedArray<Vec3f, 8> corners_ndc {
        Vec3f { -1, -1, 0 },
        Vec3f { -1,  1, 0 },
        Vec3f {  1,  1, 0 },
        Vec3f {  1, -1, 0 },
        Vec3f { -1, -1, 1 },
        Vec3f { -1,  1, 1 },
        Vec3f {  1,  1, 1 },
        Vec3f {  1, -1, 1 }
    };

    for (UInt i = 0; i < 8; i++) {
        Vec4f corner = clip_to_world * Vec4f(corners_ndc[i], 1.0f);
        corner /= corner.w;

        m_corners[i] = corner.GetXYZ();
    }

    return *this;
}

Vec3f Frustum::GetIntersectionPoint(UInt plane_index_0, UInt plane_index_1, UInt plane_index_2) const
{
    const Vec4f planes[3] = { GetPlane(plane_index_0), GetPlane(plane_index_1), GetPlane(plane_index_2) };

    Vec3f bxc = planes[1].GetXYZ().Cross(planes[2].GetXYZ());
    Vec3f cxa = planes[2].GetXYZ().Cross(planes[0].GetXYZ());
    Vec3f axb = planes[0].GetXYZ().Cross(planes[1].GetXYZ());

    Vec3f r = (bxc * -planes[0].w) - (cxa * planes[1].w) - (axb * planes[2].w);

    return r * (1.0f / planes[0].GetXYZ().Dot(bxc));
}

} // namespace hyperion
