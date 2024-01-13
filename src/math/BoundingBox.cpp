#include <math/BoundingBox.hpp>
#include <math/MathUtil.hpp>

#include <limits>
#include <cmath>

namespace hyperion {

const BoundingBox BoundingBox::empty = BoundingBox(MathUtil::MaxSafeValue<Vec3f>(), MathUtil::MinSafeValue<Vec3f>());
const BoundingBox BoundingBox::infinity = BoundingBox(-MathUtil::Infinity<Vec3f>(), +MathUtil::Infinity<Vec3f>());

BoundingBox::BoundingBox()
    : min(MathUtil::MaxSafeValue<Vec3f>()),
      max(MathUtil::MinSafeValue<Vec3f>())
{
}

BoundingBox::BoundingBox(const Vec3f &min, const Vec3f &max)
    : min(min), 
      max(max)
{
}

BoundingBox::BoundingBox(const BoundingBox &other)
    : min(other.min), 
      max(other.max)
{
}

FixedArray<Vec3f, 8> BoundingBox::GetCorners() const
{
    return {
        Vec3f(min.x, min.y, min.z),
        Vec3f(max.x, min.y, min.z),
        Vec3f(max.x, max.y, min.z),
        Vec3f(min.x, max.y, min.z),
        Vec3f(min.x, min.y, max.z),
        Vec3f(min.x, max.y, max.z),
        Vec3f(max.x, max.y, max.z),
        Vec3f(max.x, min.y, max.z)
    };
}

Vec3f BoundingBox::GetCorner(UInt index) const
{
    const UInt mask = 1u << index;

    return {
        MathUtil::Lerp(min.x, max.x, Int((mask & 1) != 0)),
        MathUtil::Lerp(min.y, max.y, Int((mask & 2) != 0)),
        MathUtil::Lerp(min.z, max.z, Int((mask & 4) != 0))
    };
}

void BoundingBox::SetCenter(const Vec3f &center)
{
    Vec3f dimensions = GetExtent();

    max = center + dimensions * 0.5f;
    min = center - dimensions * 0.5f;
}

void BoundingBox::SetExtent(const Vec3f &dimensions)
{
    Vec3f center = GetCenter();

    max = center + dimensions * 0.5f;
    min = center - dimensions * 0.5f;
}

// https://github.com/openscenegraph/OpenSceneGraph/blob/master/include/osg/BoundingBox
Float BoundingBox::GetRadiusSquared() const
{
    return 0.25f * GetExtent().LengthSquared();
}

Float BoundingBox::GetRadius() const
{
    return MathUtil::Sqrt(GetRadiusSquared());
}

BoundingBox BoundingBox::operator*(Float scalar) const
{
    BoundingBox other(*this);
    other *= scalar;

    return other;
}

BoundingBox &BoundingBox::operator*=(Float scalar)
{
    if (Empty()) {
        return *this;
    }

    min *= scalar;
    max *= scalar;

    return *this;
}

BoundingBox BoundingBox::operator/(Float scalar) const
{
    return BoundingBox(*this) /= scalar;
}

BoundingBox &BoundingBox::operator/=(Float scalar)
{
    if (Empty()) {
        return *this;
    }

    min /= scalar;
    max /= scalar;

    return *this;
}

BoundingBox BoundingBox::operator+(const Vec3f &scale) const
{
    return BoundingBox(min + scale, max + scale);
}

BoundingBox &BoundingBox::operator+=(const Vec3f &scale)
{
    min += scale;
    max += scale;

    return *this;
}

BoundingBox BoundingBox::operator/(const Vec3f &scale) const
{
    return BoundingBox(min / scale, max / scale);
}

BoundingBox &BoundingBox::operator/=(const Vec3f &scale)
{
    min /= scale;
    max /= scale;

    return *this;
}

BoundingBox BoundingBox::operator*(const Vec3f &scale) const
{
    return BoundingBox(min * scale, max * scale);
}

BoundingBox &BoundingBox::operator*=(const Vec3f &scale)
{
    min *= scale;
    max *= scale;

    return *this;
}

BoundingBox &BoundingBox::operator*=(const Transform &transform)
{
    if (Empty()) {
        return *this;
    }

    auto corners = GetCorners();

    Clear();

    for (auto &corner : corners) {
        corner = transform.GetMatrix() * corner;
        Extend(corner);
    }

    return *this;
}

BoundingBox BoundingBox::operator*(const Transform &transform) const
{
    return BoundingBox(*this) *= transform;
}

BoundingBox &BoundingBox::Clear()
{
    min = Vec3f(MathUtil::MaxSafeValue<Float>());
    max = Vec3f(MathUtil::MinSafeValue<Float>());

    return *this;
}

BoundingBox &BoundingBox::Extend(const Vec3f &vec)
{
    min = Vec3f::Min(min, vec);
    max = Vec3f::Max(max, vec);
    return *this;
}

BoundingBox &BoundingBox::Extend(const BoundingBox &bb)
{
    min = Vec3f::Min(min, bb.min);
    max = Vec3f::Max(max, bb.max);
    return *this;
}

Bool BoundingBox::Intersects(const BoundingBox &other) const
{
    const FixedArray<Vec3f, 8> corners = other.GetCorners();

    if (ContainsPoint(corners[0])) return true;
    if (ContainsPoint(corners[1])) return true;
    if (ContainsPoint(corners[2])) return true;
    if (ContainsPoint(corners[3])) return true;
    if (ContainsPoint(corners[4])) return true;
    if (ContainsPoint(corners[5])) return true;
    if (ContainsPoint(corners[6])) return true;
    if (ContainsPoint(corners[7])) return true;

    return false;
}

Bool BoundingBox::Contains(const BoundingBox &other) const
{
    const FixedArray<Vec3f, 8> corners = other.GetCorners();

    if (!ContainsPoint(corners[0])) return false;
    if (!ContainsPoint(corners[1])) return false;
    if (!ContainsPoint(corners[2])) return false;
    if (!ContainsPoint(corners[3])) return false;
    if (!ContainsPoint(corners[4])) return false;
    if (!ContainsPoint(corners[5])) return false;
    if (!ContainsPoint(corners[6])) return false;
    if (!ContainsPoint(corners[7])) return false;

    return true;
}

Bool BoundingBox::ContainsPoint(const Vec3f &vec) const
{
    if (vec.x < min.x) return false;
    if (vec.y < min.y) return false;
    if (vec.z < min.z) return false;
    if (vec.x > max.x) return false;
    if (vec.y > max.y) return false;
    if (vec.z > max.z) return false;

    return true;
}

Float BoundingBox::Area() const
{
    Vec3f dimensions(max - min);
    return dimensions.x * dimensions.y * dimensions.z;
}

std::ostream &operator<<(std::ostream &out, const BoundingBox &bb) // output
{
    out << "BoundingBox [max: " << bb.GetMax() << ", min: " << bb.GetMin() << "]";
    return out;
}

} // namespace hyperion
