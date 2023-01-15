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

    FixedArray<Vector4, 6> &GetPlanes() { return m_planes; }
    const FixedArray<Vector4, 6> &GetPlanes() const { return m_planes; }

    Vector4 &GetPlane(UInt index) { return m_planes[index]; }
    const Vector4 &GetPlane(UInt index) const { return m_planes[index]; }

    const Vector3 &GetCorner(UInt index) { return m_corners[index]; }
    const FixedArray<Vector3, 8> &GetCorners() const { return m_corners; }

    bool ContainsAABB(const BoundingBox &aabb) const;
    bool ContainsBoundingSphere(const BoundingSphere &sphere) const;

    Frustum &SetFromViewProjectionMatrix(const Matrix4 &view_proj);
    Vector3 GetIntersectionPoint(UInt plane_index_0, UInt plane_index_1, UInt plane_index_2) const;


private:
    FixedArray<Vector4, 6> m_planes;
    FixedArray<Vector3, 8> m_corners;
};
} // namespace hyperion

#endif

