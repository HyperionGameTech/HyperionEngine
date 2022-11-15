#ifndef BOUNDING_BOX_H
#define BOUNDING_BOX_H

#include "Vector3.hpp"
#include "Matrix4.hpp"
#include "Transform.hpp"
#include "Ray.hpp"
#include "../HashCode.hpp"
#include <Types.hpp>

#include <array>
#include <limits>

namespace hyperion {

class BoundingBox
{
    friend std::ostream &operator<<(std::ostream &out, const BoundingBox &bb);
public:
    static const BoundingBox empty;

    BoundingBox();
    BoundingBox(const Vector3 &min, const Vector3 &max);
    BoundingBox(const BoundingBox &other);

    const Vector3 &GetMin() const { return min; }
    void SetMin(const Vector3 &min) { this->min = min; }
    const Vector3 &GetMax() const { return max; }
    void SetMax(const Vector3 &max) { this->max = max; }
    std::array<Vector3, 8> GetCorners() const;
    Vector3 GetCorner(UInt index) const;
    Vector3 GetCenter() const { return (max + min) * 0.5f; }
    void SetCenter(const Vector3 &center);
    Vector3 GetExtent() const { return max - min; }
    void SetExtent(const Vector3 &dimensions);
    float GetRadiusSquared() const;
    float GetRadius() const;

    BoundingBox operator*(float scalar) const;
    BoundingBox &operator*=(float scalar);
    BoundingBox operator/(float scalar) const;
    BoundingBox &operator/=(float scalar);
    BoundingBox operator*(const Transform &transform) const;
    BoundingBox &operator*=(const Transform &transform);
    bool operator==(const BoundingBox &other) const
        { return min == other.min && max == other.max; }
    bool operator!=(const BoundingBox &other) const
        { return !operator==(other); }

    BoundingBox &Clear();
    
    bool Empty() const
    { 
        return min == MathUtil::MaxSafeValue<Vector3>() && 
            max == MathUtil::MinSafeValue<Vector3>();
    }

    BoundingBox &Extend(const Vector3 &vec);
    BoundingBox &Extend(const BoundingBox &bb);
    
    // do the AABB's intersect at all?
    bool Intersects(const BoundingBox &other) const;
    // does this AABB completely contain other?
    bool Contains(const BoundingBox &other) const;
    bool ContainsPoint(const Vector3 &vec) const;
    float Area() const;

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(min.GetHashCode());
        hc.Add(max.GetHashCode());

        return hc;
    }

    Vector3 min;
    Vector3 max;
};

} // namespace hyperion

#endif
