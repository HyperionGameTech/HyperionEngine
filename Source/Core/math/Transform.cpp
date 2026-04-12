/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/math/Transform.hpp>

#ifndef HYP_TOOL
#include <Transform.generated.inl>
#endif

namespace Hyperion {

const Transform Transform::identity {};

Transform::Transform()
    : translation(Vec3f::Zero()),
      scale(Vec3f::One()),
      rotation(Quat4f::Identity())
{
}

Transform::Transform(const Vec3f& translation, const Vec3f& scale)
    : translation(translation),
      scale(scale),
      rotation(Quat4f::Identity())
{
}

Transform::Transform(const Vec3f& translation, const Vec3f& scale, const Quat4f& rotation)
    : translation(translation),
      scale(scale),
      rotation(rotation)
{
}

Transform::Transform(const Vec3f& translation)
    : Transform(translation, Vec3f::One(), Quat4f::Identity())
{
}

Transform Transform::GetInverse() const
{
    return {
        -translation,
        Vec3f(1.0f) / scale,
        rotation.Inverse()
    };
}

Transform Transform::operator*(const Transform& other) const
{
    return {
        translation + (rotation * (scale * other.translation)),
        scale * other.scale,
        rotation * other.rotation
    };
}

Transform& Transform::operator*=(const Transform& other)
{
    return *this = *this * other;
}

} // namespace Hyperion
