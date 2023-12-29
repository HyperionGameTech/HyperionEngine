#ifndef BOUNDING_BOX_H
#define BOUNDING_BOX_H

#include <math/Vector3.hpp>
#include <math/Matrix4.hpp>
#include <math/Transform.hpp>
#include <math/Ray.hpp>
#include <core/lib/FixedArray.hpp>
#include <HashCode.hpp>
#include <Types.hpp>

#include <limits>

namespace hyperion {

class BoundingBox
{
    friend std::ostream &operator<<(std::ostream &out, const BoundingBox &bb);
public:
    static const BoundingBox empty;
    static const BoundingBox infinity;

    BoundingBox();
    BoundingBox(const Vec3f &min, const Vec3f &max);
    BoundingBox(const BoundingBox &other);

    const Vec3f &GetMin() const
        { return min; }

    void SetMin(const Vec3f &min)
        { this->min = min; }
    const Vec3f &GetMax() const
        { return max; }

    void SetMax(const Vec3f &max)
        { this->max = max; }

    FixedArray<Vec3f, 8> GetCorners() const;
    Vec3f GetCorner(UInt index) const;
    
    Vec3f GetCenter() const
        { return (max + min) * 0.5f; }

    void SetCenter(const Vec3f &center);

    Vec3f GetExtent() const
        { return max - min; }

    void SetExtent(const Vec3f &dimensions);

    Float GetRadiusSquared() const;
    Float GetRadius() const;

    BoundingBox operator*(Float scalar) const;
    BoundingBox &operator*=(Float scalar);
    BoundingBox operator/(Float scalar) const;
    BoundingBox &operator/=(Float scalar);
    BoundingBox operator+(const Vec3f &offset) const;
    BoundingBox &operator+=(const Vec3f &offset);
    BoundingBox operator-(const Vec3f &offset) const;
    BoundingBox &operator-=(const Vec3f &offset);
    BoundingBox operator/(const Vec3f &scale) const;
    BoundingBox &operator/=(const Vec3f &scale);
    BoundingBox operator*(const Vec3f &scale) const;
    BoundingBox &operator*=(const Vec3f &scale);
    BoundingBox operator*(const Transform &transform) const;
    BoundingBox &operator*=(const Transform &transform);

    Bool operator==(const BoundingBox &other) const
        { return min == other.min && max == other.max; }

    Bool operator!=(const BoundingBox &other) const
        { return !operator==(other); }

    BoundingBox &Clear();
    
    Bool Empty() const
    { 
        return min == MathUtil::MaxSafeValue<Vec3f>() && 
            max == MathUtil::MinSafeValue<Vec3f>();
    }

    BoundingBox &Extend(const Vec3f &vec);
    BoundingBox &Extend(const BoundingBox &bb);
    
    // do the AABB's intersect at all?
    Bool Intersects(const BoundingBox &other) const;
    // does this AABB completely contain other?
    Bool Contains(const BoundingBox &other) const;
    Bool ContainsPoint(const Vec3f &vec) const;
    Float Area() const;

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(min.GetHashCode());
        hc.Add(max.GetHashCode());

        return hc;
    }

    Vec3f min;
    Vec3f max;
};

} // namespace hyperion

#endif
