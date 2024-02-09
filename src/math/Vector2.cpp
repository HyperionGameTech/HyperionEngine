#include "Vector2.hpp"
#include "Vector4.hpp"
#include "MathUtil.hpp"
namespace hyperion {

namespace math {
namespace detail {

const Vec2<float> Vec2<float>::zero = Vec2<float>(0.0f);
const Vec2<float> Vec2<float>::one = Vec2<float>(1.0f);

float Vec2<float>::Min() const
{
    return MathUtil::Min(x, y);
}

float Vec2<float>::Max() const
{
    return MathUtil::Max(x, y);
}

float Vec2<float>::Distance(const Vec2<float> &other) const
{
    return sqrt(DistanceSquared(other));
}

float Vec2<float>::DistanceSquared(const Vec2<float> &other) const
{
    float dx = x - other.x;
    float dy = y - other.y;
    return dx * dx + dy * dy;
}

Vec2<float> &Vec2<float>::Normalize()
{
    float len = Length();
    float len_sqr = len * len;
    if (len_sqr == 0 || len_sqr == 1) {
        return *this;
    }

    (*this) *= (1.0f / len);
    return *this;
}

Vec2<float> &Vec2<float>::Lerp(const Vector2 &to, const float amt)
{
    x = MathUtil::Lerp(x, to.x, amt);
    y = MathUtil::Lerp(y, to.y, amt);
    return *this;
}

Vec2<float> Vec2<float>::Abs(const Vec2<float> &vec)
{
    return Vector2(abs(vec.x), abs(vec.y));
}

Vec2<float> Vec2<float>::Round(const Vec2<float> &vec)
{
    return Vector2(std::round(vec.x), std::round(vec.y));
}

Vec2<float> Vec2<float>::Clamp(const Vec2<float> &vec, float min_value, float max_value)
{
    return Max(min_value, Min(vec, max_value));
}

Vec2<float> Vec2<float>::Min(const Vec2<float> &a, const Vec2<float> &b)
{
    return Vec2<float>(MathUtil::Min(a.x, b.x), MathUtil::Min(a.y, b.y));
}

Vec2<float> Vec2<float>::Max(const Vec2<float> &a, const Vec2<float> &b)
{
    return Vec2<float>(MathUtil::Max(a.x, b.x), MathUtil::Max(a.y, b.y));
}

Vec2<float> Vec2<float>::Zero()
{
    return Vec2<float>(0, 0);
}

Vec2<float> Vec2<float>::One()
{
    return Vec2<float>(1, 1);
}

Vec2<float> Vec2<float>::UnitX()
{
    return Vec2<float>(1, 0);
}

Vec2<float> Vec2<float>::UnitY()
{
    return Vec2<float>(0, 1);
}

} // namespace detail
} // namespace math

std::ostream &operator<<(std::ostream &out, const Vec2<float> &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << "]";
    return out;
}

std::ostream &operator<<(std::ostream &out, const Vec2<int> &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << "]";
    return out;
}

std::ostream &operator<<(std::ostream &out, const Vec2<uint> &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << "]";
    return out;
}

} // namespace hyperion
