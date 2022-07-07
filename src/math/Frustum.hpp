#ifndef FRUSTUM_H
#define FRUSTUM_H

#include "Matrix4.hpp"
#include "Vector4.hpp"
#include "BoundingBox.hpp"

#include <array>

namespace hyperion {
class Frustum {
public:
    Frustum();
    Frustum(const Frustum &other);
    Frustum(const Matrix4 &view_proj);

    Vector4 &GetPlane(size_t index)             { return m_planes[index]; }
    const Vector4 &GetPlane(size_t index) const { return m_planes[index]; }

    bool ContainsAabb(const BoundingBox &aabb) const;

    Frustum &SetFromViewProjectionMatrix(const Matrix4 &view_proj);
    //Frustum &SetFromAabb(const BoundingBox &aabb);
    //Frustum &SetFromCorners(const std::array<Vector3, 8> &corners);

    std::array<Vector4, 6> m_planes;
};
} // namespace hyperion

#endif

