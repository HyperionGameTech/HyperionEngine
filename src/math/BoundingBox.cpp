#include "BoundingBox.hpp"

#include <limits>
#include <cmath>

namespace hyperion {

const BoundingBox BoundingBox::empty = BoundingBox();

BoundingBox::BoundingBox()
    : min(MathUtil::MaxSafeValue<Float>()), 
      max(MathUtil::MinSafeValue<Float>())
{
}

BoundingBox::BoundingBox(const Vector3 &min, const Vector3 &max)
    : min(min), 
      max(max)
{
}

BoundingBox::BoundingBox(const BoundingBox &other)
    : min(other.min), 
      max(other.max)
{
}

std::array<Vector3, 8> BoundingBox::GetCorners() const
{
    return {
        Vector3(min.x, min.y, min.z),
        Vector3(max.x, min.y, min.z),
        Vector3(max.x, max.y, min.z),
        Vector3(min.x, max.y, min.z),
        Vector3(min.x, min.y, max.z),
        Vector3(min.x, max.y, max.z),
        Vector3(max.x, max.y, max.z),
        Vector3(max.x, min.y, max.z)
    };
}

Vector3 BoundingBox::GetCorner(UInt index) const
{
    const UInt mask = 1u << index;

    return {
        MathUtil::Lerp(min.x, max.x, Int((mask & 1) != 0)),
        MathUtil::Lerp(min.y, max.y, Int((mask & 2) != 0)),
        MathUtil::Lerp(min.z, max.z, Int((mask & 4) != 0))
    };
}

void BoundingBox::SetCenter(const Vector3 &center)
{
    Vector3 dimensions = GetExtent();

    max = center + dimensions * 0.5f;
    min = center - dimensions * 0.5f;
}

void BoundingBox::SetExtent(const Vector3 &dimensions)
{
    Vector3 center = GetCenter();

    max = center + dimensions * 0.5f;
    min = center - dimensions * 0.5f;
}

// https://github.com/openscenegraph/OpenSceneGraph/blob/master/include/osg/BoundingBox
float BoundingBox::GetRadiusSquared() const
{
    return 0.25f * GetExtent().LengthSquared();
}

float BoundingBox::GetRadius() const
{
    return MathUtil::Sqrt(GetRadiusSquared());
}

BoundingBox &BoundingBox::operator*=(float scalar)
{
    if (Empty()) {
        return *this;
    }

    min *= scalar;
    max *= scalar;

    return *this;
}

BoundingBox BoundingBox::operator*(float scalar) const
{
    if (Empty()) {
        return *this;
    }

    BoundingBox other;

    for (const Vector3 &corner : GetCorners()) {
        other.Extend(corner * scalar);
    }

    return other;
}

BoundingBox BoundingBox::operator/(float scalar) const
{
    return BoundingBox(*this) /= scalar;
}

BoundingBox &BoundingBox::operator/=(float scalar)
{
    if (Empty()) {
        return *this;
    }

    min /= scalar;
    max /= scalar;

    return *this;
}

BoundingBox &BoundingBox::operator*=(const Transform &transform)
{
    if (Empty()) {
        return *this;
    }

    min = transform.GetMatrix() * min;
    max = transform.GetMatrix() * max;

    return *this;
}

BoundingBox BoundingBox::operator*(const Transform &transform) const
{
    return BoundingBox(*this) *= transform;
}

BoundingBox &BoundingBox::Clear()
{
    min = Vector3(MathUtil::MaxSafeValue<Float>());
    max = Vector3(MathUtil::MinSafeValue<Float>());

    return *this;
}

BoundingBox &BoundingBox::Extend(const Vector3 &vec)
{
    min = Vector3::Min(min, vec);
    max = Vector3::Max(max, vec);
    return *this;
}

BoundingBox &BoundingBox::Extend(const BoundingBox &bb)
{
    min = Vector3::Min(min, bb.min);
    max = Vector3::Max(max, bb.max);
    return *this;
}

bool BoundingBox::Intersects(const BoundingBox &other) const
{
    const auto corners = other.GetCorners();

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

bool BoundingBox::Contains(const BoundingBox &other) const
{
    const auto corners = other.GetCorners();

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

bool BoundingBox::ContainsPoint(const Vector3 &vec) const
{
    if (vec.x < min.x) return false;
    if (vec.y < min.y) return false;
    if (vec.z < min.z) return false;
    if (vec.x > max.x) return false;
    if (vec.y > max.y) return false;
    if (vec.z > max.z) return false;

    return true;
}

float BoundingBox::Area() const
{
    Vector3 dimensions(max - min);
    return dimensions.x * dimensions.y * dimensions.z;
}

std::ostream &operator<<(std::ostream &out, const BoundingBox &bb) // output
{
    out << "BoundingBox [max: " << bb.GetMax() << ", min: " << bb.GetMin() << "]";
    return out;
}

} // namespace hyperion
