#ifndef FRUSTUM_H
#define FRUSTUM_H

#include <math/Matrix4.hpp>
#include <math/Vector4.hpp>
#include <math/BoundingBox.hpp>
#include <math/BoundingSphere.hpp>

#include <core/lib/FixedArray.hpp>

#include <Types.hpp>

namespace hyperion {
class Frustum
{
public:
    Frustum();
    Frustum(const Frustum &other);
    Frustum(const Matrix4 &view_proj);

    FixedArray<Vec4f, 6> &GetPlanes()
        { return m_planes; }

    const FixedArray<Vec4f, 6> &GetPlanes() const
        { return m_planes; }

    Vec4f &GetPlane(UInt index)
        { return m_planes[index]; }

    const Vec4f &GetPlane(UInt index) const
        { return m_planes[index]; }

    const Vec3f &GetCorner(UInt index)
        { return m_corners[index]; }

    const FixedArray<Vec3f, 8> &GetCorners() const
        { return m_corners; }

    Bool ContainsAABB(const BoundingBox &aabb) const;
    Bool ContainsBoundingSphere(const BoundingSphere &sphere) const;

    Frustum &SetFromViewProjectionMatrix(const Matrix4 &view_proj);
    Vec3f GetIntersectionPoint(UInt plane_index_0, UInt plane_index_1, UInt plane_index_2) const;

private:
    FixedArray<Vec4f, 6>    m_planes;
    FixedArray<Vec3f, 8>    m_corners;
};
} // namespace hyperion

#endif

