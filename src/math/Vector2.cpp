#include "Vector2.hpp"
#include "Vector4.hpp"
#include "MathUtil.hpp"
namespace hyperion {

namespace math {
namespace detail {

const Vec2<Float> Vec2<Float>::zero = Vec2<Float>(0.0f);
const Vec2<Float> Vec2<Float>::one = Vec2<Float>(1.0f);

float Vec2<Float>::Min() const
{
    return MathUtil::Min(x, y);
}

float Vec2<Float>::Max() const
{
    return MathUtil::Max(x, y);
}

float Vec2<Float>::Distance(const Vec2<Float> &other) const
{
    return sqrt(DistanceSquared(other));
}

float Vec2<Float>::DistanceSquared(const Vec2<Float> &other) const
{
    float dx = x - other.x;
    float dy = y - other.y;
    return dx * dx + dy * dy;
}

Vec2<Float> &Vec2<Float>::Normalize()
{
    float len = Length();
    float len_sqr = len * len;
    if (len_sqr == 0 || len_sqr == 1) {
        return *this;
    }

    (*this) *= (1.0f / len);
    return *this;
}

Vec2<Float> &Vec2<Float>::Lerp(const Vector2 &to, const float amt)
{
    x = MathUtil::Lerp(x, to.x, amt);
    y = MathUtil::Lerp(y, to.y, amt);
    return *this;
}

Vec2<Float> Vec2<Float>::Abs(const Vec2<Float> &vec)
{
    return Vector2(abs(vec.x), abs(vec.y));
}

Vec2<Float> Vec2<Float>::Round(const Vec2<Float> &vec)
{
    return Vector2(std::round(vec.x), std::round(vec.y));
}

Vec2<Float> Vec2<Float>::Clamp(const Vec2<Float> &vec, float min_value, float max_value)
{
    return Max(min_value, Min(vec, max_value));
}

Vec2<Float> Vec2<Float>::Min(const Vec2<Float> &a, const Vec2<Float> &b)
{
    return Vec2<Float>(MathUtil::Min(a.x, b.x), MathUtil::Min(a.y, b.y));
}

Vec2<Float> Vec2<Float>::Max(const Vec2<Float> &a, const Vec2<Float> &b)
{
    return Vec2<Float>(MathUtil::Max(a.x, b.x), MathUtil::Max(a.y, b.y));
}

Vec2<Float> Vec2<Float>::Zero()
{
    return Vec2<Float>(0, 0);
}

Vec2<Float> Vec2<Float>::One()
{
    return Vec2<Float>(1, 1);
}

Vec2<Float> Vec2<Float>::UnitX()
{
    return Vec2<Float>(1, 0);
}

Vec2<Float> Vec2<Float>::UnitY()
{
    return Vec2<Float>(0, 1);
}

} // namespace detail
} // namespace math

std::ostream &operator<<(std::ostream &out, const Vec2<Float> &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << "]";
    return out;
}

std::ostream &operator<<(std::ostream &out, const Vec2<Int> &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << "]";
    return out;
}

std::ostream &operator<<(std::ostream &out, const Vec2<UInt> &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << "]";
    return out;
}

} // namespace hyperion
