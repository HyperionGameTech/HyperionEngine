/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/math/Vector2.hpp>
#include <core/math/Vector4.hpp>
#include <core/math/MathUtil.hpp>

#include <core/object/HypClassUtils.hpp>
#include <core/object/HypClassRegistry.hpp>

namespace hyperion {

// clang-format off
HYP_BEGIN_STRUCT(Vec2f, -1, 0, {})
    HypField(NAME(HYP_STR(x)), &Type::x, offsetof(Type, x)),
    HypField(NAME(HYP_STR(y)), &Type::y, offsetof(Type, y))
HYP_END_STRUCT

HYP_BEGIN_STRUCT(Vec2i, -1, 0, {})
    HypField(NAME(HYP_STR(x)), &Type::x, offsetof(Type, x)),
    HypField(NAME(HYP_STR(y)), &Type::y, offsetof(Type, y))
HYP_END_STRUCT

HYP_BEGIN_STRUCT(Vec2u, -1, 0, {})
    HypField(NAME(HYP_STR(x)), &Type::x, offsetof(Type, x)),
    HypField(NAME(HYP_STR(y)), &Type::y, offsetof(Type, y))
HYP_END_STRUCT
// clang-format on

namespace math {
float Vec2<float>::Distance(const Vec2<float>& other) const
{
    return MathUtil::Sqrt(DistanceSquared(other));
}

float Vec2<float>::DistanceSquared(const Vec2<float>& other) const
{
    float dx = x - other.x;
    float dy = y - other.y;
    return dx * dx + dy * dy;
}

Vec2<float>& Vec2<float>::Normalize()
{
    float len = Length();
    float len_sqr = len * len;
    if (len_sqr == 0 || len_sqr == 1)
    {
        return *this;
    }

    (*this) *= (1.0f / len);
    return *this;
}

Vec2<float>& Vec2<float>::Lerp(const Vec2<float>& to, const float amt)
{
    x = MathUtil::Lerp(x, to.x, amt);
    y = MathUtil::Lerp(y, to.y, amt);
    return *this;
}

float Vec2<float>::Dot(const Vec2<float>& other) const
{
    return x * other.x + y * other.y;
}

Vec2<float> Vec2<float>::Abs(const Vec2<float>& vec)
{
    return Vector2(abs(vec.x), abs(vec.y));
}

Vec2<float> Vec2<float>::Round(const Vec2<float>& vec)
{
    return Vector2(std::round(vec.x), std::round(vec.y));
}

Vec2<float> Vec2<float>::Clamp(const Vec2<float>& vec, float min_value, float max_value)
{
    return Max(min_value, Min(vec, max_value));
}

Vec2<float> Vec2<float>::Min(const Vec2<float>& a, const Vec2<float>& b)
{
    return Vec2<float>(MathUtil::Min(a.x, b.x), MathUtil::Min(a.y, b.y));
}

Vec2<float> Vec2<float>::Max(const Vec2<float>& a, const Vec2<float>& b)
{
    return Vec2<float>(MathUtil::Max(a.x, b.x), MathUtil::Max(a.y, b.y));
}

} // namespace math
} // namespace hyperion
