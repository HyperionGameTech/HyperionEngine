#ifndef BOUNDING_BOX_H
#define BOUNDING_BOX_H

#include <math/Vector3.hpp>
#include <math/Matrix4.hpp>
#include <math/Transform.hpp>
#include <math/Ray.hpp>
#include <core/lib/FixedArray.hpp>
#include <core/lib/CMemory.hpp>
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

    HYP_FORCE_INLINE
    const Vec3f &GetMin() const
        { return min; }

    HYP_FORCE_INLINE
    void SetMin(const Vec3f &min)
        { this->min = min; }

    HYP_FORCE_INLINE
    const Vec3f &GetMax() const
        { return max; }

    HYP_FORCE_INLINE
    void SetMax(const Vec3f &max)
        { this->max = max; }

    FixedArray<Vec3f, 8> GetCorners() const;
    Vec3f GetCorner(UInt index) const;
    
    HYP_FORCE_INLINE
    Vec3f GetCenter() const
        { return (max + min) * 0.5f; }

    void SetCenter(const Vec3f &center);

    HYP_FORCE_INLINE
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

    HYP_FORCE_INLINE
    Bool operator==(const BoundingBox &other) const
        { return min == other.min && max == other.max; }

    HYP_FORCE_INLINE
    Bool operator!=(const BoundingBox &other) const
        { return !operator==(other); }

    BoundingBox &Clear();
    
    HYP_FORCE_INLINE
    Bool Empty() const
        { return Memory::MemCmp(this, &empty, sizeof(BoundingBox)) == 0; }

    BoundingBox &Extend(const Vec3f &vec);
    BoundingBox &Extend(const BoundingBox &bb);
    
    // do the AABB's intersect at all?
    Bool Intersects(const BoundingBox &other) const;
    // does this AABB completely contain other?
    Bool Contains(const BoundingBox &other) const;
    Bool ContainsPoint(const Vec3f &vec) const;
    Float Area() const;

    HYP_FORCE_INLINE
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
