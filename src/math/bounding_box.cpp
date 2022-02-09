#include "bounding_box.h"

#include <limits>
#include <cmath>

namespace hyperion {

BoundingBox::BoundingBox()
    : m_min(std::numeric_limits<float>::max()), 
      m_max(std::numeric_limits<float>::lowest())
{
}

BoundingBox::BoundingBox(const Vector3 &min, const Vector3 &max)
    : m_min(min), 
      m_max(max)
{
}

BoundingBox::BoundingBox(const BoundingBox &other)
    : m_min(other.m_min), 
      m_max(other.m_max)
{
}

std::array<Vector3, 8> BoundingBox::GetCorners() const
{
    return std::array<Vector3, 8> {
        Vector3(m_min.x, m_min.y, m_min.z),
        Vector3(m_max.x, m_min.y, m_min.z),
        Vector3(m_max.x, m_max.y, m_min.z),
        Vector3(m_min.x, m_max.y, m_min.z),
        Vector3(m_min.x, m_min.y, m_max.z),
        Vector3(m_min.x, m_max.y, m_max.z),
        Vector3(m_max.x, m_max.y, m_max.z),
        Vector3(m_max.x, m_min.y, m_max.z)
    };
}

void BoundingBox::SetCenter(const Vector3 &center)
{
    Vector3 dimensions = GetDimensions();

    m_max = center + dimensions * 0.5f;
    m_min = center - dimensions * 0.5f;
}

BoundingBox &BoundingBox::operator*=(double scalar)
{
    m_min *= scalar;
    m_max *= scalar;

    return *this;
}

BoundingBox &BoundingBox::operator*=(const Transform &transform)
{
    m_min *= transform.GetScale();
    m_max *= transform.GetScale();

    m_min *= transform.GetRotation();
    m_max *= transform.GetRotation();

    m_min += transform.GetTranslation();
    m_max += transform.GetTranslation();

    return *this;
}

BoundingBox &BoundingBox::Clear()
{
    m_min = Vector3(std::numeric_limits<float>::max());
    m_max = Vector3(std::numeric_limits<float>::lowest());
    return *this;
}

BoundingBox &BoundingBox::Extend(const Vector3 &vec)
{
    m_min = Vector3::Min(m_min, vec);
    m_max = Vector3::Max(m_max, vec);
    return *this;
}

BoundingBox &BoundingBox::Extend(const BoundingBox &bb)
{
    m_min = Vector3::Min(m_min, bb.m_min);
    m_max = Vector3::Max(m_max, bb.m_max);
    return *this;
}

BoundingBox BoundingBox::operator*(double scalar) const
{
    BoundingBox other;

    for (const Vector3 &corner : GetCorners()) {
        other.Extend(corner * scalar);
    }

    return other;
}

BoundingBox BoundingBox::operator*(const Transform &transform) const
{
    BoundingBox other;

    for (const Vector3 &corner : GetCorners()) {
        other.Extend(corner * transform.GetMatrix());
    }

    return other;
}

bool BoundingBox::IntersectRay(const Ray &ray, RaytestHit &out) const
{
    const float epsilon = MathUtil::EPSILON;

    if (m_max == Vector3(std::numeric_limits<float>::lowest()) && 
        m_min == Vector3(std::numeric_limits<float>::max())) {
        // early detect if box is empty
        return false;
    }

    Vector3 hit_min(std::numeric_limits<float>::lowest());
    Vector3 hit_max(std::numeric_limits<float>::max());

    if (std::fabs(ray.m_direction.x) < epsilon) {
        if (ray.m_position.x < m_min.x || ray.m_position.x > m_max.x) {
            return false; // no collision
        }
    } else {
        hit_min.x = (m_min.x - ray.m_position.x) / ray.m_direction.x;
        hit_max.x = (m_max.x - ray.m_position.x) / ray.m_direction.x;
        if (hit_min.x > hit_max.x) {
            std::swap(hit_min.x, hit_max.x);
        }
    }

    if (std::fabs(ray.m_direction.y) < epsilon) {
        if (ray.m_position.y < m_min.y ||
            ray.m_position.y > m_max.y) {
            return false; // no collision
        }
    } else {
        hit_min.y = (m_min.y - ray.m_position.y) / ray.m_direction.y;
        hit_max.y = (m_max.y - ray.m_position.y) / ray.m_direction.y;

        if (hit_min.y > hit_max.y) {
            std::swap(hit_min.y, hit_max.y);
        }

        if (hit_min.x > hit_max.y || hit_min.y > hit_max.x) {
            return false; // no collision
        }

        if (hit_min.y > hit_min.x) hit_min.x = hit_min.y;
        if (hit_max.y < hit_max.x) hit_max.x = hit_max.y;
    }

    if (std::fabs(ray.m_direction.z) < epsilon) {
        if (ray.m_position.z < m_min.z || ray.m_position.z > m_max.z) {
            return false; // no collision
        }
    } else {
        hit_min.z = (m_min.z - ray.m_position.z) / ray.m_direction.z;
        hit_max.z = (m_max.z - ray.m_position.z) / ray.m_direction.z;

        if (hit_min.z > hit_max.z) {
            std::swap(hit_min.z, hit_max.z);
        }

        if (hit_min.x > hit_max.z || hit_min.z > hit_max.x) {
            return false; // no collision
        }

        if (hit_min.z > hit_min.x) hit_min.x = hit_min.z;
        if (hit_max.z < hit_max.x) hit_max.x = hit_max.z;
    }

    out.hitpoint = ray.m_position + (ray.m_direction * hit_min.x);

    return true;
}

bool BoundingBox::ContainsPoint(const Vector3 &vec) const
{
    if (vec.x < m_min.x) return false;
    if (vec.y < m_min.y) return false;
    if (vec.z < m_min.z) return false;
    if (vec.x > m_max.x) return false;
    if (vec.y > m_max.y) return false;
    if (vec.z > m_max.z) return false;

    return true;
}

double BoundingBox::Area() const
{
    Vector3 dimensions(m_max - m_min);
    return dimensions.x * dimensions.y * dimensions.z;
}

std::ostream &operator<<(std::ostream &out, const BoundingBox &bb) // output
{
    out << "BoundingBox [max: " << bb.GetMax() << ", min: " << bb.GetMin() << "]";
    return out;
}

} // namespace hyperion
